#ifndef HB_APP_H
#define HB_APP_H
#include <stdint.h>
#include "hb_control.h"

/* 사용자 입력: 응용 코드 또는 Live Expressions에서 변경하는 지령입니다.
 * 시작할 때 run=0에서 전류 지령을 입력한 다음 run=1을 요청합니다. */
typedef struct {
    float current_A;       /* 전류 지령 [A]. 허용 범위는 0~HB_CURRENT_COMMAND_MAX_A입니다. */
    uint32_t run;          /* 0: 네 게이트 입력 LOW로 정지 요청, 1: 기동/운전 요청 */
} HB_Request;

/* 확인용 출력: 제어 코드가 갱신합니다. 값을 수정해도 출력 지령이 되지 않습니다. */
typedef struct {
    uint32_t state;             /* HB_State 동작 상태: 보정/정지/충전/운전/오류 등 */
    uint32_t faults;            /* HB_Fault 오류 비트. 여러 원인이 함께 표시될 수 있습니다. */
    uint32_t calibrated;        /* 1: 센서 무전류 영점 보정 완료 */
    uint32_t hardware_verified; /* 출력 허용 설정값. 실장/파형을 자동 검증했다는 뜻은 아닙니다. */
    uint32_t gates_enabled;     /* 제어기의 출력 허용 요청 상태. 실제 게이트 파형의 측정값은 아닙니다. */
    uint32_t adc_irq_count;     /* 제어기에 전달된 ADC 표본의 누적 개수 */
    uint32_t last_adc_ms;       /* 마지막 ADC 콜백 시각 [ms], HAL tick 기준 */
    uint32_t adc_raw;           /* ADC 원시값 [count], 12 bit 기준 0~4095 */
    float offset_count;        /* 실제 전류 0 A일 때 측정한 ADC 평균 [count] */
    float current_A;           /* 영점/감도를 적용한 전류 [A], 소프트웨어 필터 적용 전 */
    float filtered_A;          /* PI 제어 입력으로 사용하는 필터 전류 [A] */
    float reference_A;         /* 변화율 제한을 거친 내부 전류 지령 [A] */
    float duty;                /* PI 계산 듀티 [0~1 비율]. 0.10=10%, 입력용 변수가 아닙니다. */
} HB_Status;

/* extern은 다른 파일에서 참조하기 위한 선언이며, 실제 변수는 hb_app.c에 있습니다. */
extern volatile HB_Request hb_request;
/* 응용 코드/디버거에서는 읽기 전용입니다. 여러 필드를 읽는 동안 갱신될 수 있습니다. */
extern volatile HB_Status hb_status;

void HB_App_Init(void);          /* 주변장치 설정 후 호출: 출력 OFF, ADC 보정/동기 측정 시작 */
void HB_App_Poll(void);          /* main의 while(1)에서 호출: 인터럽트를 기다립니다. */
void HB_App_SafetyTick1ms(void); /* SysTick에서 호출: ADC 콜백이 장시간 멈추면 정지합니다. */
void HB_App_EmergencyStop(void); /* CPU 예외/Error_Handler 등에서 MOE를 해제하고 오류를 기록합니다. */
#endif
