#ifndef STM32F3xx_HAL_CONF_H
#define STM32F3xx_HAL_CONF_H
#ifdef __cplusplus
extern "C" {
#endif
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#ifndef HSE_VALUE
#define HSE_VALUE 8000000U
#endif
#define HSE_STARTUP_TIMEOUT 100U
#ifndef HSI_VALUE
#define HSI_VALUE 8000000U
#endif
#define LSI_VALUE 40000U
#define LSE_VALUE 32768U
#define LSE_STARTUP_TIMEOUT 5000U
#define EXTERNAL_CLOCK_VALUE 8000000U
#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY 3U
#define USE_RTOS 0U
#define PREFETCH_ENABLE 1U
#define USE_HAL_ADC_REGISTER_CALLBACKS 0U
#define USE_HAL_TIM_REGISTER_CALLBACKS 0U

#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_dma.h"
#include "stm32f3xx_hal_cortex.h"
#include "stm32f3xx_hal_adc.h"
#include "stm32f3xx_hal_flash.h"
#include "stm32f3xx_hal_pwr.h"
#include "stm32f3xx_hal_tim.h"
#define assert_param(expr) ((void)0U)
#ifdef __cplusplus
}
#endif
#endif
