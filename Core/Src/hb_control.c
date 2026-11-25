#include "hb_control.h"
#include "hb_config.h"

/* 값 x를 lo~hi 안으로 제한합니다. 출력/적분/지령 변화량의 공통 제한 함수입니다. */
static float clamp(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static void stop_output(HB_Control *c)
{
    /* 계산 상태만 정지합니다. 실제 네 입력 LOW 처리는 App의 MOE 차단에서 수행합니다. */
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
        /* 시작 직후 약 10 ms는 측정값이 안정되도록 기다리며 출력을 켜지 않습니다. */
        if (++c->phase_count >= HB_SENSOR_SETTLE_SAMPLES) {
            c->phase_count = 0U;
            c->state = HB_STATE_OFFSET_CALIBRATION;
        }
        return;
    }
    if (c->state == HB_STATE_OFFSET_CALIBRATION) {
        /* 실제 0 A에서 1024개 ADC값을 평균합니다. 1.65 V/2048을 고정 영점으로 쓰지 않습니다. */
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

    /* ADC count -> 전류 [A]: 영점 차분 * ADC 기준 전압 / (최대 count * 센서 V/A).
     * raw를 float로 바꾼 뒤 빼므로 영점 아래의 음수 전류도 표현할 수 있습니다. */
    c->current_A = HB_CURRENT_SIGN * ((float)raw - c->offset_count)
                 * HB_ADC_VREF_V
                 / ((float)HB_ADC_MAX_COUNT * HB_SENSOR_SENSITIVITY_V_A);
    /* 제어용 1차 필터: 새 필터값 = 0.8*이전값 + 0.2*이번 측정값. */
    c->filtered_A += HB_CURRENT_LPF_ALPHA * (c->current_A - c->filtered_A);

    /* 과전류는 위 소프트웨어 필터 전 값으로 검사해 추가 필터 지연을 피합니다.
     * 센서 자체/R13-C20/ADC 변환 지연까지 없애는 하드웨어 보호는 아닙니다. */
    if (c->current_A > HB_SOFTWARE_TRIP_A ||
        c->current_A < -HB_SOFTWARE_TRIP_A) {
        HB_Control_Trip(c, HB_FAULT_OVERCURRENT);
        return;
    }
    /* 정상 전류 지령은 0~상한 [A]. 이 비교는 NaN과 +/-무한대도 거부합니다. */
    if (!(command_A >= 0.0f && command_A <= HB_CURRENT_COMMAND_MAX_A)) {
        HB_Control_Trip(c, HB_FAULT_COMMAND);
        return;
    }
    if (run_request == 0U) {
        /* 정상 정지 요청: 이후 새 시작을 허용하되 듀티/적분/내부 지령은 모두 0으로 만듭니다. */
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
    /* 부팅/보정 중 run=1이 들어 있어도 자동 시작하지 않습니다.
     * 보정 후 run=0을 한 번 확인하고 새 기동 요청을 받아야 합니다. */
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
        return; /* 듀티 0 + 출력 허용 => Q1/Q3 OFF, Q2/Q4 ON으로 부트스트랩 충전. */
    }
    if (c->state == HB_STATE_BOOTSTRAP) {
        /* 약 5 ms 충전 중 PI를 계산하지 않습니다. 매번 재기동 시 이 단계를 거칩니다. */
        if (++c->phase_count >= HB_BOOTSTRAP_SAMPLES) {
            c->state = HB_STATE_RUNNING;
        }
        return;
    }

    /* 지령 ramp: 1초 변화량[A/s] * 주기[s]만큼만 내부 지령을 갱신합니다.
     * 지령 0/정지 요청은 위에서 즉시 OFF 처리하므로 이 ramp를 기다리지 않습니다. */
    const float ts = 1.0f / (float)HB_PWM_HZ;
    const float slew = HB_REFERENCE_SLEW_A_S * ts;
    c->reference_A += clamp(command_A - c->reference_A, -slew, slew);
    const float error = c->reference_A - c->filtered_A; /* 전류 오차 [A]. 양수면 전류를 더 올려야 합니다. */
    /* 적분 후보 [duty] = 이전 적분 + Ki[duty/(A*s)] * 오차[A] * 주기[s]. */
    const float candidate_i = clamp(c->integrator + HB_PI_KI_DUTY_PER_A_S
                                   * error * ts, -HB_DUTY_MAX, HB_DUTY_MAX);
    const float candidate_u = HB_PI_KP_DUTY_PER_A * error + candidate_i;
    /* Anti-windup: 듀티 상/하한을 더 벗어나게 만드는 방향으로는 적분하지 않습니다. */
    if (!((candidate_u > HB_DUTY_MAX && error > 0.0f) ||
          (candidate_u < 0.0f && error < 0.0f))) {
        c->integrator = candidate_i;
    }
    /* 최종 PI 출력 = Kp*오차 + 적분항, 0~50% 범위로 제한해 App에 전달합니다. */
    c->duty = clamp(HB_PI_KP_DUTY_PER_A * error + c->integrator,
                    0.0f, HB_DUTY_MAX);
}
