#include "hb_control.h"
#include "hb_config.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void calibrate(HB_Control *c, uint32_t verified)
{
    HB_Control_Init(c, verified);
    for (uint32_t i = 0; i < HB_SENSOR_SETTLE_SAMPLES + HB_OFFSET_SAMPLES; ++i) {
        HB_Control_Step(c, 2048U, 0.0f, 0U);
        assert(c->gates_enabled == 0U);
    }
    assert(c->calibrated == 1U && c->faults == 0U);
    assert(c->state == HB_STATE_IDLE && c->offset_count == 2048.0f);
    HB_Control_Step(c, 2048U, 0.0f, 0U); /* Explicit idle run-low. */
}

static void test_calibration_and_interlock(void)
{
    HB_Control c;
    HB_Control_Init(&c, 1U);
    for (uint32_t k = 0; k < HB_SENSOR_SETTLE_SAMPLES + HB_OFFSET_SAMPLES + 10U; ++k)
        HB_Control_Step(&c, 2048U, 0.1f, 1U);
    assert(c.calibrated == 1U && c.state == HB_STATE_IDLE);
    assert(c.gates_enabled == 0U); /* Held-high request cannot auto-start. */
    HB_Control_Step(&c, 2048U, 0.1f, 0U);
    HB_Control_Step(&c, 2048U, 0.1f, 1U);
    assert(c.state == HB_STATE_BOOTSTRAP && c.gates_enabled == 1U);
    for (uint32_t k = 0; k < HB_BOOTSTRAP_SAMPLES; ++k) {
        assert(c.duty == 0.0f);
        HB_Control_Step(&c, 2048U, 0.1f, 1U);
    }
    assert(c.state == HB_STATE_RUNNING && c.duty == 0.0f);
    HB_Control_Step(&c, 2048U, 0.1f, 1U);
    assert(c.reference_A > 0.0f && c.duty > 0.0f);
    HB_Control_Step(&c, 2048U, 0.1f, 0U);
    assert(c.state == HB_STATE_IDLE && c.gates_enabled == 0U && c.duty == 0.0f);
    HB_Control_Step(&c, 2048U, 0.1f, 1U);
    assert(c.state == HB_STATE_BOOTSTRAP); /* Every restart recharges. */
    puts("PASS zero calibration, no auto-start, precharge and restart");
}

static void test_faults(void)
{
    HB_Control c;
    calibrate(&c, 0U);
    HB_Control_Step(&c, 2048U, 0.1f, 1U);
    assert(c.faults == HB_FAULT_NOT_VERIFIED && c.gates_enabled == 0U);
    HB_Control_Step(&c, 2048U, 0.0f, 0U);
    assert(c.state == HB_STATE_FAULT); /* Stays latched. */
    calibrate(&c, 1U);
    HB_Control_Step(&c, 2400U, 0.1f, 1U);
    assert(c.faults == HB_FAULT_OVERCURRENT && c.gates_enabled == 0U);
    calibrate(&c, 1U);
    HB_Control_Step(&c, 1980U, 0.1f, 1U);
    assert(c.faults == HB_FAULT_CURRENT_POLARITY);
    const float invalid[] = {-0.1f, HB_CURRENT_COMMAND_MAX_A + 0.1f,
                             NAN, INFINITY, -INFINITY};
    for (unsigned k = 0; k < sizeof(invalid) / sizeof(invalid[0]); ++k) {
        calibrate(&c, 1U);
        HB_Control_Step(&c, 2048U, invalid[k], 1U);
        assert(c.faults == HB_FAULT_COMMAND && c.gates_enabled == 0U);
    }
    HB_Control_Init(&c, 1U);
    HB_Control_Step(&c, 4095U, 0.0f, 0U);
    assert(c.faults == HB_FAULT_ADC_RAIL);
    HB_Control_Init(&c, 1U);
    for (uint32_t k = 0; k < HB_SENSOR_SETTLE_SAMPLES + HB_OFFSET_SAMPLES; ++k)
        HB_Control_Step(&c, 1000U, 0.0f, 0U);
    assert(c.faults == HB_FAULT_OFFSET);
    HB_Control_Init(&c, 1U);
    for (uint32_t k = 0; k < HB_SENSOR_SETTLE_SAMPLES + HB_OFFSET_SAMPLES; ++k)
        HB_Control_Step(&c, (uint16_t)(k % 2U ? 2100U : 2000U), 0.0f, 0U);
    assert(c.faults == HB_FAULT_OFFSET);
    puts("PASS verification guard, latched faults, raw overcurrent, sign, NaN and calibration errors");
}

static void test_saturation(void)
{
    HB_Control c;
    calibrate(&c, 1U);
    for (unsigned k = 0; k < 80000U; ++k)
        HB_Control_Step(&c, 2048U, 0.5f, 1U);
    assert(c.faults == 0U && fabsf(c.duty - HB_DUTY_MAX) < 0.00001f);
    assert(fabsf(c.integrator) <= HB_DUTY_MAX);
    HB_Control_Step(&c, 2048U, 0.0f, 1U);
    assert(c.gates_enabled == 0U && c.integrator == 0.0f && c.reference_A == 0.0f);
    puts("PASS unreachable target saturation, bounded integral and zero-command stop");
}

static void test_averaged_motor_model(void)
{
    HB_Control c;
    calibrate(&c, 1U);
    double current = 0.0;
    double first = 0.0;
    const double dt = 1.0 / (double)HB_PWM_HZ;
    for (unsigned k = 0; k < 80000U; ++k) {
        /* Averaged RL model, one-tick output delay, approximate 1 us loss.
         * Not a switching, mechanical, RC-filter or real-motor validation. */
        const double emf = k >= 40000U ? 2.4 : 0.0;
        double effective_duty = (double)c.duty - 1.0e-6 * HB_PWM_HZ;
        if (effective_duty < 0.0 || c.gates_enabled == 0U) effective_duty = 0.0;
        current += dt * (HB_ASSUMED_BUS_V * effective_duty
                         - HB_ASSUMED_MOTOR_R_OHM * current - emf)
                         / HB_ASSUMED_MOTOR_L_H;
        if (current < 0.0) current = 0.0; /* Forward averaged model only. */
        long raw = lround(2048.0 + current * HB_SENSOR_SENSITIVITY_V_A
                         / HB_ADC_VREF_V * HB_ADC_MAX_COUNT);
        assert(raw >= 0 && raw <= (long)HB_ADC_MAX_COUNT);
        HB_Control_Step(&c, (uint16_t)raw, 0.2f, 1U);
        assert(c.faults == 0U && isfinite(c.duty));
        assert(c.duty >= 0.0f && c.duty <= HB_DUTY_MAX);
        if (k == 39999U) first = current;
    }
    assert(fabs(first - 0.2) < 0.01);
    assert(fabs(current - 0.2) < 0.01);
    printf("PASS provisional RL model: 0.200 A command -> %.4f A; after 2.4 V back-EMF step -> %.4f A\n",
           first, current);
}

int main(void)
{
    test_calibration_and_interlock();
    test_faults();
    test_saturation();
    test_averaged_motor_model();
    return 0;
}
