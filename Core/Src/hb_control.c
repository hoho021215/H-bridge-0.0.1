#include "hb_control.h"
#include "hb_config.h"

static float clamp(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static void stop_output(HB_Control *c)
{
    c->gates_enabled = 0U;
    c->duty = 0.0f;
    c->reference_A = 0.0f;
    c->integrator = 0.0f;
}

void HB_Control_Init(HB_Control *c, uint32_t hardware_verified)
{
    HB_Control initial = {0};
    *c = initial;
    c->state = HB_STATE_SENSOR_SETTLE;
    c->offset_count = (float)HB_ADC_MAX_COUNT * 0.5f;
    c->offset_min = HB_ADC_MAX_COUNT;
    c->hardware_verified = hardware_verified != 0U ? 1U : 0U;
}

void HB_Control_Trip(HB_Control *c, uint32_t faults)
{
    c->faults |= faults;
    c->state = HB_STATE_FAULT;
    stop_output(c);
}

void HB_Control_Step(HB_Control *c, uint16_t raw,
                     float command_A, uint32_t run_request)
{
    c->sample_count++;
    c->adc_raw = raw;
    if (c->faults != 0U) {
        stop_output(c);
        return;
    }
    if (raw < HB_ADC_RAIL_LOW_COUNT || raw > HB_ADC_RAIL_HIGH_COUNT) {
        HB_Control_Trip(c, HB_FAULT_ADC_RAIL);
        return;
    }

    if (c->state == HB_STATE_SENSOR_SETTLE) {
        if (++c->phase_count >= HB_SENSOR_SETTLE_SAMPLES) {
            c->phase_count = 0U;
            c->state = HB_STATE_OFFSET_CALIBRATION;
        }
        return;
    }
    if (c->state == HB_STATE_OFFSET_CALIBRATION) {
        c->offset_sum += raw;
        if (raw < c->offset_min) c->offset_min = raw;
        if (raw > c->offset_max) c->offset_max = raw;
        if (++c->phase_count < HB_OFFSET_SAMPLES) return;
        c->offset_count = (float)c->offset_sum / (float)HB_OFFSET_SAMPLES;
        if (c->offset_count < (float)HB_OFFSET_MIN_COUNT ||
            c->offset_count > (float)HB_OFFSET_MAX_COUNT ||
            c->offset_max - c->offset_min > HB_OFFSET_MAX_SPAN_COUNT) {
            HB_Control_Trip(c, HB_FAULT_OFFSET);
            return;
        }
        c->calibrated = 1U;
        c->phase_count = 0U;
        c->state = HB_STATE_IDLE;
        return;
    }

    c->current_A = HB_CURRENT_SIGN * ((float)raw - c->offset_count)
                 * HB_ADC_VREF_V
                 / ((float)HB_ADC_MAX_COUNT * HB_SENSOR_SENSITIVITY_V_A);
    c->filtered_A += HB_CURRENT_LPF_ALPHA * (c->current_A - c->filtered_A);

    /* Raw sample, before the software feedback filter. */
    if (c->current_A > HB_SOFTWARE_TRIP_A ||
        c->current_A < -HB_SOFTWARE_TRIP_A) {
        HB_Control_Trip(c, HB_FAULT_OVERCURRENT);
        return;
    }
    /* Ordered comparisons also reject NaN and +/-infinity. */
    if (!(command_A >= 0.0f && command_A <= HB_CURRENT_COMMAND_MAX_A)) {
        HB_Control_Trip(c, HB_FAULT_COMMAND);
        return;
    }
    if (run_request == 0U) {
        c->saw_idle_run_low = 1U;
        c->state = HB_STATE_IDLE;
        c->phase_count = 0U;
        stop_output(c);
        return;
    }
    if (command_A == 0.0f) {
        c->state = HB_STATE_IDLE;
        c->phase_count = 0U;
        stop_output(c);
        return;
    }
    /* A request held high across power-up/calibration does not auto-start. */
    if (c->saw_idle_run_low == 0U) return;
    if (c->hardware_verified == 0U) {
        HB_Control_Trip(c, HB_FAULT_NOT_VERIFIED);
        return;
    }
    if (c->current_A < HB_POLARITY_TRIP_A) {
        HB_Control_Trip(c, HB_FAULT_CURRENT_POLARITY);
        return;
    }

    if (c->state == HB_STATE_IDLE) {
        c->state = HB_STATE_BOOTSTRAP;
        c->phase_count = 0U;
        c->gates_enabled = 1U;
        return; /* Duty zero => Q2/Q4 on, Q1/Q3 off. */
    }
    if (c->state == HB_STATE_BOOTSTRAP) {
        if (++c->phase_count >= HB_BOOTSTRAP_SAMPLES) {
            c->state = HB_STATE_RUNNING;
        }
        return;
    }

    const float ts = 1.0f / (float)HB_PWM_HZ;
    const float slew = HB_REFERENCE_SLEW_A_S * ts;
    c->reference_A += clamp(command_A - c->reference_A, -slew, slew);
    const float error = c->reference_A - c->filtered_A;
    const float candidate_i = clamp(c->integrator + HB_PI_KI_DUTY_PER_A_S
                                   * error * ts, -HB_DUTY_MAX, HB_DUTY_MAX);
    const float candidate_u = HB_PI_KP_DUTY_PER_A * error + candidate_i;
    if (!((candidate_u > HB_DUTY_MAX && error > 0.0f) ||
          (candidate_u < 0.0f && error < 0.0f))) {
        c->integrator = candidate_i;
    }
    c->duty = clamp(HB_PI_KP_DUTY_PER_A * error + c->integrator,
                    0.0f, HB_DUTY_MAX);
}
