#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        GPIO_InitTypeDef gpio = {0};
        __HAL_RCC_ADC12_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_0;
        gpio.Mode = GPIO_MODE_ANALOG;
        gpio.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio);
        HAL_NVIC_SetPriority(ADC1_2_IRQn, 1U, 0U);
        HAL_NVIC_EnableIRQ(ADC1_2_IRQn);
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) __HAL_RCC_TIM1_CLK_ENABLE();
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) __HAL_RCC_TIM1_CLK_ENABLE();
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        GPIO_InitTypeDef gpio = {0};
        __HAL_RCC_GPIOE_CLK_ENABLE();
        gpio.Pin = HB_GATE_GPIO_PINS;
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_PULLDOWN;
        gpio.Speed = GPIO_SPEED_FREQ_HIGH;
        gpio.Alternate = GPIO_AF2_TIM1;
        HAL_GPIO_Init(GPIOE, &gpio);
    }
}
