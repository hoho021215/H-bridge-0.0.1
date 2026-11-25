#ifndef HB_CONTROL_H
#define HB_CONTROL_H
#include <stdint.h>

/* 제어 상태. 순서대로 0~5이며 hb_status.state에서 확인할 수 있습니다. */
typedef enum {
    HB_STATE_SENSOR_SETTLE = 0,  /* 센서 안정 대기: 모든 게이트 입력 LOW. */
    HB_STATE_OFFSET_CALIBRATION, /* 실제 무전류 상태의 ADC 평균을 영점으로 수집. */
    HB_STATE_IDLE,               /* 보정 완료/기동 대기: 출력 OFF. */
    HB_STATE_BOOTSTRAP,          /* 상측 OFF/양 하측 ON으로 상측 드라이버 전원을 충전. */
    HB_STATE_RUNNING,            /* 양의 전류 지령에 대한 PI 제어. */
    HB_STATE_FAULT               /* 오류 유지 상태: 출력 OFF, 재부팅 전 자동 재기동 없음. */
} HB_State;

/* 오류 비트 마스크. 여러 원인이 있으면 OR로 함께 기록됩니다. */
typedef enum {
    HB_FAULT_NONE             = 0U,      /* 정상: 오류 없음. */
    HB_FAULT_ADC_RAIL         = 1U << 0,  /* ADC 측정값이 입력 범위 끝에 근접. */
    HB_FAULT_OFFSET           = 1U << 1,  /* 영점 평균 범위 또는 표본 흔들림 이상. */
    HB_FAULT_OVERCURRENT      = 1U << 2,  /* 필터 전 전류의 절댓값이 차단값 초과. */
    HB_FAULT_COMMAND          = 1U << 3,  /* 음수/상한 초과/NaN/무한대 전류 지령. */
    HB_FAULT_NOT_VERIFIED     = 1U << 4,  /* 출력 허용 guard=0인 상태에서 구동 요청. */
    HB_FAULT_ADC_DRIVER       = 1U << 5,  /* HAL ADC 자체 보정/시작/변환 처리 오류. */
    HB_FAULT_CURRENT_POLARITY = 1U << 6,  /* 양의 구동 요청에 대해 예상과 반대인 전류 검출. */
    HB_FAULT_ADC_TIMEOUT      = 1U << 7,  /* ADC 콜백 중단 감시의 시간 한계 초과. */
    HB_FAULT_CONFIGURATION    = 1U << 8,  /* 클럭/PWM/ADC 설정 검사 또는 타이머 설정/시작 실패. */
    HB_FAULT_PLATFORM         = 1U << 9   /* Error_Handler/CPU 예외/비상 정지 함수 호출. */
} HB_Fault;

/* 제어기 내부 상태. 계산 결과를 저장하며 실제 레지스터 적용은 hb_app.c가 담당합니다. */
typedef struct {
    HB_State state;             /* 현재 동작 단계. */
    uint32_t faults;            /* 유지 중인 HB_Fault 비트들의 OR 값. */
    uint32_t sample_count;      /* 처리한 ADC 표본 누계. */
    uint32_t phase_count;       /* 현재 대기/보정/충전 단계에서 처리한 표본 수. */
    uint32_t offset_sum;        /* 영점 평균 계산용 ADC 합계 [count]. */
    uint32_t offset_min;        /* 영점 수집 중 최소 ADC값 [count]. */
    uint32_t offset_max;        /* 영점 수집 중 최대 ADC값 [count]. */
    uint32_t calibrated;        /* 1=무전류 영점 보정 완료. */
    uint32_t hardware_verified; /* 1=출력 허용 설정. 실제 파형을 자동 측정한 결과가 아닙니다. */
    uint32_t saw_idle_run_low;  /* 보정 후 run=0을 한 번 확인했는지: 부팅 중 run=1 자동 기동 방지. */
    uint32_t gates_enabled;     /* 출력 허용 요청. 실제 MOE/게이트 입력은 App이 적용합니다. */
    uint16_t adc_raw;           /* 이번 ADC 원시 측정값 [count]. */
    float offset_count;        /* 전류 0 A에 해당하는 ADC 평균값 [count]. */
    float current_A;           /* 영점/감도로 환산한 전류 [A], 소프트웨어 필터 전. */
    float filtered_A;          /* PI 피드백용으로 필터링한 전류 [A]. */
    float reference_A;         /* 변화율 제한을 적용한 내부 전류 지령 [A]. */
    float integrator;          /* Ki가 적용된 누적 듀티 보정량 [duty], 제한 범위 내 유지. */
    float duty;                /* 계산된 듀티비. 0~HB_DUTY_MAX이며 관측된 실제 펄스폭은 아닙니다. */
} HB_Control;

/* 평상시 ADC ISR 한 곳이 이 상태를 변경합니다.
 * 다른 문맥에서 오류를 기록할 때는 짧은 임계 구역으로 인터럽트와의 경합을 막습니다. */
/* 계산 상태 초기화. 이 함수 자체는 ADC/PWM 하드웨어를 시작하지 않습니다. */
void HB_Control_Init(HB_Control *control, uint32_t hardware_verified);
/* ADC 표본 하나로 보정/보호/기동 상태/PI를 갱신합니다. 정상 주기 50 us. */
void HB_Control_Step(HB_Control *control, uint16_t adc_raw,
                     float command_A, uint32_t run_request);
/* 오류 비트를 유지하고 출력 허용/지령/적분/듀티를 0으로 초기화합니다. */
void HB_Control_Trip(HB_Control *control, uint32_t faults);
#endif
