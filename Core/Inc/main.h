#ifndef MAIN_H
#define MAIN_H
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32f3xx_hal.h"

extern ADC_HandleTypeDef hadc1; /* ADC1 설정/상태 핸들. 실제 변수는 main.c에 있으며 센서 측정에 사용합니다. */
extern TIM_HandleTypeDef htim1; /* TIM1 설정/상태 핸들. 상보 PWM과 ADC 트리거를 생성합니다. */

#define HB_GATE_GPIOA_PINS GPIO_PIN_7 /* PA7: TIM1_CH1N / J1.3 */
#define HB_GATE_GPIOC_PINS (GPIO_PIN_0 | GPIO_PIN_1) /* PC0: TIM1_CH1 / J1.1, PC1: TIM1_CH2 / J1.4 */
#define HB_GATE_GPIOE_PINS GPIO_PIN_10 /* PE10: TIM1_CH2N / J1.6 */
#define HB_PWM_PERIOD_COUNTS 3600U /* TIM1 초기화용 주기 count [tick]. ARR=이 값-1, 72 MHz에서 20 kHz. */
#define HB_ADC_SAMPLE_COMPARE 2700U /* TIM1 CH4 초기 비교값 [tick]. 주기 시작 37.5 us 후 ADC 트리거. */

/* USER CODE BEGIN Private defines */
/* 데드타임의 유일한 정의: TIM1 초기화와 App 설정 검사가 함께 사용합니다.
 * CKD=DIV1, 72 MHz에서 DTG=72는 1 us입니다. 상/하측 전환 사이에 모두 OFF인 시간을 삽입합니다. */
#define HB_DEADTIME_TICKS 72U /* TIM1 BDTR.DTG 설정값. 이 선형 구간에서 1 tick=약 13.9 ns. */
/* USER CODE END Private defines */

void SystemClock_Config(void); /* HSE 8 MHz -> PLL 72 MHz 등 MCU/주변장치 클럭 설정. */
void Error_Handler(void); /* 초기화 실패 등의 오류에서 출력 차단 후 대기. */
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim); /* 네 게이트 핀에 TIM1 대체 기능을 설정. */
#ifdef __cplusplus
}
#endif
#endif
