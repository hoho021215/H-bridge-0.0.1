#ifndef HB_CONFIG_H
#define HB_CONFIG_H

/* 전류제어 설정 모음. #define 값을 바꾸면 다시 빌드하고 MCU에 다운로드해야 합니다.
 * 듀티는 0~1의 비율이며 0.50은 50%입니다. 운전 중 지령은 hb_request로 입력합니다. */

/* 출력 허용 설정: 0=측정/영점 보정만 허용, 1=기동 요청에 따른 구동 허용.
 * docs/BRINGUP_KO.md의 배선/파형 확인 후 변경합니다. 자동 하드웨어 진단값이 아닙니다. */
#ifndef HB_HARDWARE_VERIFIED
#define HB_HARDWARE_VERIFIED 0U
#endif

/* 전류 센서/ADC 환산 기준. 실제 실장 IC3가 A3B인지 확인합니다.
 * 회로 Value는 A3, library ID는 A2로 서로 다릅니다. */
#define HB_ADC_VREF_V              3.3f   /* ADC 기준 전압 [V]. 실제 VDDA/VREF+와 맞춰야 전류 환산이 정확합니다. */
#define HB_SENSOR_SUPPLY_V         3.3f   /* 센서 전원 가정값 [V]. 전원 자동 측정/보정 기능은 아닙니다. */
#define HB_SENSOR_SENSITIVITY_V_A   0.200f /* A3B 감도 [V/A]. 전류 1 A 변화에 센서 출력이 0.200 V 변합니다. */
#define HB_CURRENT_SIGN            1.0f   /* 전류 부호. 현재 OUT+ -> 모터 -> OUT- 방향이 양수입니다. */
#define HB_ADC_MAX_COUNT           4095U  /* 12 bit ADC 최대값. ADC count를 전압으로 바꿀 때 사용합니다. */

/* PWM/ADC 동기 타이밍. 실제 초기화 값과 일치해야 합니다.
 * 데드타임 HB_DEADTIME_TICKS는 중복 없이 main.h에서만 정의합니다. */
#define HB_PWM_HZ                  20000U    /* PWM/정상 제어 주파수 [Hz]. 20 kHz -> 제어 주기 50 us. */
#define HB_TIMER_CLOCK_HZ          72000000U /* TIM1 입력 클럭 [Hz]. 주파수/실제 설정 일치 검사에 사용합니다. */
#define HB_PWM_PERIOD_TICKS        3600U     /* PWM 한 주기의 count 수 [tick]. ARR=3599, 듀티 20% -> CCR1=720. */
#define HB_ADC_TRIGGER_TICKS       2700U     /* ADC 시작 위치 [tick]. 2700/72 MHz=37.5 us, CH4 PWM2/TRGO로 생성. */

/* 모터 임시 모델값: 실제 정격/실측값이 아닙니다. MOTOR_ASSUMPTIONS_KO.md 참고.
 * 아래 가정을 바꿔도 전류 지령 상한/과전류 차단값이 자동으로 바뀌지는 않습니다. */
#define HB_ASSUMED_BUS_V           12.0f  /* 모터 버스 전압 가정 [V]. 별도 게이트 전원 15 V와 구분합니다. */
#define HB_ASSUMED_MOTOR_RATED_A    0.5f   /* 정격 전류 가정 [A]. 초기 모델용이며 실제 허용 전류를 뜻하지 않습니다. */
#define HB_ASSUMED_MOTOR_STALL_A    2.0f   /* 회전이 멈췄을 때의 스톨 전류 가정 [A]. 실제 구속 시험값 아님. */
#define HB_ASSUMED_MOTOR_R_OHM      6.0f   /* 권선 저항 가정 [ohm]. 12 V / 스톨 2 A로 산출한 RL 시험 모델값. */
#define HB_ASSUMED_MOTOR_L_H        0.001f /* 권선 인덕턴스 가정 [H]. 0.001 H=1 mH, RL 모의 시험에 사용합니다. */

/* 초기 시험 제한값/PI 이득. 실제 전원/모터/센서 응답을 측정해 조정합니다.
 * Kp가 커지면 같은 순간 오차에 대한 듀티 변화가 커지고, Ki가 커지면 오차가 더 빨리 누적됩니다.
 * 지나치게 큰 이득은 전류 진동/과도 응답을 만들 수 있으므로 초기값부터 실측하며 조정합니다.
 * ADC 기반 소프트웨어 정지는 별도의 하드웨어 단락 보호를 대신하지 않습니다. */
#define HB_CURRENT_COMMAND_MAX_A   0.5f   /* 전류 지령 상한 [A]. 음수 또는 이 값을 넘는 지령은 오류로 정지. */
#define HB_SOFTWARE_TRIP_A          1.0f   /* 필터 전 측정 전류의 절댓값이 이 값 [A]을 초과하면 오류 유지 정지. */
#define HB_DUTY_MAX                 0.50f  /* PI 듀티 상한 [0~1]. 50%에 고정하는 명령이 아니며 현재 0.50 초과는 거부. */
#define HB_PI_KP_DUTY_PER_A         0.05f  /* 비례 이득 [duty/A]. 현재 전류 오차에 즉시 비례하는 듀티 보정량. */
#define HB_PI_KI_DUTY_PER_A_S       5.0f   /* 적분 이득 [duty/(A*s)]. 오차*제어주기를 누적해 지속 오차를 줄입니다. */
#define HB_CURRENT_LPF_ALPHA        0.2f   /* 전류 제어용 필터 계수. 새 측정값 20% + 이전 필터값 80%. 과전류 검사는 필터 전. */
#define HB_REFERENCE_SLEW_A_S       0.5f   /* 전류 지령 변화율 제한 [A/s]. 단, 지령 0/정지 요청은 즉시 출력 OFF. */
#define HB_POLARITY_TRIP_A         (-0.2f) /* 양의 구동 요청 중 측정 전류가 이 값 [A]보다 작으면 방향 오류로 정지. */

/* 보정/기동 시간은 20 kHz ADC 표본 수를 기준으로 정합니다. */
#define HB_SENSOR_SETTLE_SAMPLES   200U  /* 센서 안정 대기 [표본]. 약 10 ms 동안 모든 게이트 입력 LOW. */
#define HB_OFFSET_SAMPLES          1024U /* 영점 평균 [표본]. 약 51.2 ms, 모터가 멈추고 실제 전류가 0이어야 합니다. */
#define HB_OFFSET_MIN_COUNT        1800U /* 허용 영점 ADC 평균의 하한 [count]. 3.3 V 센서의 중간 전압 근처인지 확인. */
#define HB_OFFSET_MAX_COUNT        2300U /* 허용 영점 ADC 평균의 상한 [count]. */
#define HB_OFFSET_MAX_SPAN_COUNT   64U   /* 영점 수집 중 최대값-최소값의 허용 폭 [count]. 초과 시 보정 불안정 오류. */
#define HB_ADC_RAIL_LOW_COUNT      124U  /* ADC 입력이 하단 레일에 가까운지 판단하는 경계 [count]. 미만이면 정지. */
#define HB_ADC_RAIL_HIGH_COUNT     3971U /* ADC 입력이 상단 레일에 가까운지 판단하는 경계 [count]. 초과하면 정지. */
#define HB_BOOTSTRAP_SAMPLES        100U /* 부트스트랩 충전 [표본]. 약 5 ms 동안 Q1/Q3 OFF, Q2/Q4 ON. */
#define HB_ADC_TIMEOUT_MS           3U   /* 마지막 ADC 시각과의 tick 차이가 이 값 [ms]을 초과하면 정지. PWM 매 주기 감시는 아님. */

/* 서로 모순되는 PWM 주기/ADC 위치는 컴파일 단계에서 거부합니다. */
#if HB_PWM_PERIOD_TICKS != (HB_TIMER_CLOCK_HZ / HB_PWM_HZ)
#error "PWM clock and period are inconsistent"
#endif
#if HB_ADC_TRIGGER_TICKS >= HB_PWM_PERIOD_TICKS
#error "ADC trigger must be inside the PWM period"
#endif
#endif
