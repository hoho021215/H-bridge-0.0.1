/* Standalone HAL initialization reference for NUCLEO-F303ZE.
 * hbridge_f303ze.ioc describes the same configuration; it has not been run
 * through CubeMX GUI generation. Keep the USER CODE calls when regenerating.
 */
#include "main.h"
/* USER CODE BEGIN Includes */
#include "hb_app.h"
/* USER CODE END Includes */

ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim1;
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM1_Init();
    /* USER CODE BEGIN 2 */
    HB_App_Init();
    /* USER CODE END 2 */
    while (1) {
        /* USER CODE BEGIN 3 */
        HB_App_Poll();
        /* USER CODE END 3 */
    }
}

void SystemClock_Config(void)
{
    /* ST-LINK의 8 MHz 외부 클럭을 PLL x9로 올려 MCU/TIM1을 72 MHz로 설정합니다. */
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clocks = {0};
    RCC_PeriphCLKInitTypeDef peripheral = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_BYPASS;
    osc.PLL.PREDIV = RCC_PREDIV_DIV1;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV2;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
    /* Explicitly avoid the alternate PLL x2 TIM1 clock path. */
    peripheral.PeriphClockSelection = RCC_PERIPHCLK_TIM1;
    peripheral.Tim1ClockSelection = RCC_TIM1CLK_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&peripheral) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    /* All four driver inputs are low before assigning the timer AF. */
    HAL_GPIO_WritePin(HB_GATE_GPIO_PORT, HB_GATE_GPIO_PINS, GPIO_PIN_RESET);
    gpio.Pin = HB_GATE_GPIO_PINS;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(HB_GATE_GPIO_PORT, &gpio);
}

static void MX_ADC1_Init(void)
{
    /* PA0의 센서 전압을 12 bit로 변환합니다. 연속 자유 변환 대신 TIM1 TRGO마다 한 번 측정합니다. */
    ADC_MultiModeTypeDef multi = {0};
    ADC_ChannelConfTypeDef channel = {0};
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_TRGO;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1U;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();
    multi.Mode = ADC_MODE_INDEPENDENT;
    if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multi) != HAL_OK) Error_Handler();
    channel.Channel = ADC_CHANNEL_1;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SingleDiff = ADC_SINGLE_ENDED;
    channel.SamplingTime = ADC_SAMPLETIME_19CYCLES_5;
    channel.OffsetNumber = ADC_OFFSET_NONE;
    channel.Offset = 0U;
    if (HAL_ADC_ConfigChannel(&hadc1, &channel) != HAL_OK) Error_Handler();
}

static void MX_TIM1_Init(void)
{
    /* PSC=0, ARR=3599: 72 MHz/3600=20 kHz. CH1/CH2는 상/하측 상보 출력입니다. */
    TIM_ClockConfigTypeDef clock = {0};
    TIM_MasterConfigTypeDef master = {0};
    TIM_OC_InitTypeDef output = {0};
    TIM_BreakDeadTimeConfigTypeDef safety = {0};
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0U;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = HB_PWM_PERIOD_COUNTS - 1U;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0U;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK) Error_Handler();
    clock.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim1, &clock) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) Error_Handler();
    master.MasterOutputTrigger = TIM_TRGO_OC4REF;
    master.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &master) != HAL_OK) Error_Handler();

    output.OCMode = TIM_OCMODE_PWM1;
    output.Pulse = 0U;
    output.OCPolarity = TIM_OCPOLARITY_HIGH;
    output.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    output.OCFastMode = TIM_OCFAST_DISABLE;
    output.OCIdleState = TIM_OCIDLESTATE_RESET;
    output.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &output, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &output, TIM_CHANNEL_2) != HAL_OK) Error_Handler();
    output.OCMode = TIM_OCMODE_PWM2;
    /* CH4는 외부 핀 없이 주기 시작 후 37.5 us에 ADC를 시작하는 내부 신호입니다. */
    output.Pulse = HB_ADC_SAMPLE_COMPARE;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &output, TIM_CHANNEL_4) != HAL_OK) Error_Handler();

    safety.OffStateRunMode = TIM_OSSR_ENABLE;
    safety.OffStateIDLEMode = TIM_OSSI_ENABLE;
    safety.LockLevel = TIM_LOCKLEVEL_OFF;
    safety.DeadTime = HB_DEADTIME_TICKS; /* 상/하측 모두 OFF인 전환 간격. 현재 72 ticks=1 us. */
    safety.BreakState = TIM_BREAK_DISABLE;
    safety.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    safety.BreakFilter = 0U;
    safety.Break2State = TIM_BREAK2_DISABLE;
    safety.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
    safety.Break2Filter = 0U;
    safety.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &safety) != HAL_OK) Error_Handler();
    CLEAR_BIT(TIM1->BDTR, TIM_BDTR_MOE); /* 초기화만으로 모터가 켜지지 않게 출력은 차단합니다. */
    HAL_TIM_MspPostInit(&htim1);
    /* No HAL_TIM_PWM_Start here: it would set MOE. HB_App starts the
     * counter with HAL_TIM_Base_Start and owns every output transition. */
}

void Error_Handler(void)
{
    HB_App_EmergencyStop();
    __disable_irq();
    while (1) { }
}
