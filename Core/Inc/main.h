#ifndef MAIN_H
#define MAIN_H
#ifdef __cplusplus
extern "C" {
#endif
#include "stm32f3xx_hal.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

#define HB_GATE_GPIO_PORT GPIOE
#define HB_GATE_GPIO_PINS (GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11)
#define HB_PWM_PERIOD_COUNTS 3600U
#define HB_ADC_SAMPLE_COMPARE 2700U
#define HB_DEADTIME_TICKS 72U

void SystemClock_Config(void);
void Error_Handler(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
#ifdef __cplusplus
}
#endif
#endif
