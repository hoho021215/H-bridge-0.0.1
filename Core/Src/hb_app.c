#include "main.h"
#include "hb_app.h"
#include "hb_config.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

volatile HB_Request hb_request = {0.0f, 0U};
volatile HB_Status hb_status = {0};
static HB_Control control;
static volatile uint32_t initialized = 0U;
static volatile uint32_t sampling_started = 0U;
static volatile uint32_t last_adc_ms = 0U;
static volatile uint32_t outputs_armed = 0U;

/* CCR=0 alone turns both low-side MOSFETs ON. MOE must be cleared to stop. */
static void kill_outputs(void)
{
    CLEAR_BIT(TIM1->BDTR, TIM_BDTR_MOE);
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    outputs_armed = 0U;
}

static void publish_status(void)
{
    hb_status.state = (uint32_t)control.state;
    hb_status.faults = control.faults;
    hb_status.calibrated = control.calibrated;
    hb_status.hardware_verified = control.hardware_verified;
    hb_status.gates_enabled = control.gates_enabled;
    hb_status.adc_irq_count = control.sample_count;
    hb_status.last_adc_ms = last_adc_ms;
    hb_status.adc_raw = control.adc_raw;
    hb_status.offset_count = control.offset_count;
    hb_status.current_A = control.current_A;
    hb_status.filtered_A = control.filtered_A;
    hb_status.reference_A = control.reference_A;
    hb_status.duty = control.duty;
}

static void trip(uint32_t cause)
{
    kill_outputs(); /* Hardware off first, then bookkeeping. */
    hb_request.run = 0U;
    HB_Control_Trip(&control, cause);
    publish_status();
}

static uint32_t configuration_valid(void)
{
    uint32_t timer_hz = HAL_RCC_GetPCLK2Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE2) != 0U) timer_hz *= 2U;
    const uint32_t idle_bits = TIM_CR2_OIS1 | TIM_CR2_OIS1N
                             | TIM_CR2_OIS2 | TIM_CR2_OIS2N;
    const uint32_t polarities = TIM_CCER_CC1P | TIM_CCER_CC1NP
                              | TIM_CCER_CC2P | TIM_CCER_CC2NP;
    const uint32_t pwm1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1;
    return hadc1.Instance == ADC1 && htim1.Instance == TIM1
        && (RCC->CFGR3 & RCC_CFGR3_TIM1SW) == 0U
        && timer_hz == HB_TIMER_CLOCK_HZ
        && TIM1->PSC == 0U && TIM1->ARR == HB_PWM_PERIOD_TICKS - 1U
        && TIM1->RCR == 0U
        && (TIM1->CR1 & (TIM_CR1_CMS | TIM_CR1_DIR | TIM_CR1_CKD)) == 0U
        && (TIM1->CR2 & idle_bits) == 0U
        && (TIM1->CCER & polarities) == 0U
        && (TIM1->CCMR1 & (TIM_CCMR1_CC1S | TIM_CCMR1_CC2S)) == 0U
        && (TIM1->CCMR1 & TIM_CCMR1_OC1M) == pwm1
        && (TIM1->CCMR1 & TIM_CCMR1_OC2M) == (pwm1 << 8U)
        && (TIM1->BDTR & TIM_BDTR_DTG) == HB_DEADTIME_TICKS
        && (TIM1->BDTR & (TIM_BDTR_OSSI | TIM_BDTR_OSSR))
             == (TIM_BDTR_OSSI | TIM_BDTR_OSSR)
        && (TIM1->BDTR & TIM_BDTR_AOE) == 0U
        && (ADC1->SQR1 & (ADC_SQR1_L | ADC_SQR1_SQ1))
              == (1U << ADC_SQR1_SQ1_Pos)
        && (ADC1->DIFSEL & (1U << 1U)) == 0U /* Channel 1 single ended */
        && hadc1.Init.ClockPrescaler == ADC_CLOCK_SYNC_PCLK_DIV2
        && hadc1.Init.Resolution == ADC_RESOLUTION_12B
        && hadc1.Init.DataAlign == ADC_DATAALIGN_RIGHT
        && hadc1.Init.ScanConvMode == ADC_SCAN_DISABLE
        && hadc1.Init.NbrOfConversion == 1U
        && hadc1.Init.ContinuousConvMode == DISABLE
        && hadc1.Init.DiscontinuousConvMode == DISABLE
        && hadc1.Init.ExternalTrigConv == ADC_EXTERNALTRIGCONV_T1_TRGO
        && hadc1.Init.ExternalTrigConvEdge == ADC_EXTERNALTRIGCONVEDGE_RISING
        && HB_DUTY_MAX > 0.0f && HB_DUTY_MAX <= 0.50f
        && HB_CURRENT_COMMAND_MAX_A > 0.0f
        && HB_SOFTWARE_TRIP_A > HB_CURRENT_COMMAND_MAX_A;
}

void HB_App_Init(void)
{
    kill_outputs();
    HB_Control_Init(&control, HB_HARDWARE_VERIFIED);
    hb_request.current_A = 0.0f;
    hb_request.run = 0U;
    initialized = 1U;
    if (configuration_valid() == 0U) {
        trip(HB_FAULT_CONFIGURATION);
        return;
    }

    /* CH4 is an internal reference only; never call HAL_TIM_PWM_Start here:
     * on an advanced timer it sets MOE, including the connected CH1/CH2. */
    TIM_OC_InitTypeDef sample = {0};
    sample.OCMode = TIM_OCMODE_PWM2;
    sample.Pulse = HB_ADC_TRIGGER_TICKS;
    sample.OCPolarity = TIM_OCPOLARITY_HIGH;
    sample.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sample, TIM_CHANNEL_4) != HAL_OK) {
        trip(HB_FAULT_CONFIGURATION);
        return;
    }
    TIM_MasterConfigTypeDef master = {0};
    master.MasterOutputTrigger = TIM_TRGO_OC4REF;
    master.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &master) != HAL_OK) {
        trip(HB_FAULT_CONFIGURATION);
        return;
    }

    SET_BIT(TIM1->CCMR1, TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE);
    /* Active-high main AND complementary outputs. MOE stays off. */
    SET_BIT(TIM1->CCER, TIM_CCER_CC1E | TIM_CCER_CC1NE
                       | TIM_CCER_CC2E | TIM_CCER_CC2NE);
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    TIM1->CNT = 0U;
    TIM1->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        trip(HB_FAULT_ADC_DRIVER);
        return;
    }

    last_adc_ms = HAL_GetTick();
    publish_status();
    if (HAL_ADC_Start_IT(&hadc1) != HAL_OK) {
        trip(HB_FAULT_ADC_DRIVER);
        return;
    }
    sampling_started = 1U;
    if (HAL_TIM_Base_Start(&htim1) != HAL_OK) {
        sampling_started = 0U;
        trip(HB_FAULT_CONFIGURATION);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1 || initialized == 0U) return;
    const uint16_t raw = (uint16_t)HAL_ADC_GetValue(hadc);
    last_adc_ms = HAL_GetTick();
    const float command = hb_request.current_A;
    const uint32_t run = hb_request.run;
    HB_Control_Step(&control, raw, command, run);
    if (control.gates_enabled == 0U) {
        kill_outputs();
    } else {
        if (outputs_armed == 0U) {
            /* One-time timer rephase while all gates are off. The following
             * five-ms bootstrap phase contains no PI current regulation. */
            TIM1->CCR1 = 0U;
            TIM1->CCR2 = 0U;
            TIM1->EGR = TIM_EGR_UG;
            __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
            SET_BIT(TIM1->BDTR, TIM_BDTR_MOE);
            outputs_armed = 1U;
        }
        TIM1->CCR1 = (uint32_t)(control.duty * (float)HB_PWM_PERIOD_TICKS);
        TIM1->CCR2 = 0U; /* Q3 off, Q4 on. Forward current only. */
    }
    if (control.faults != 0U) hb_request.run = 0U;
    publish_status();
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) trip(HB_FAULT_ADC_DRIVER);
}

void HB_App_SafetyTick1ms(void)
{
    if (sampling_started == 0U) return;
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    if ((uint32_t)(HAL_GetTick() - last_adc_ms) > HB_ADC_TIMEOUT_MS) {
        trip(HB_FAULT_ADC_TIMEOUT);
    }
    __set_PRIMASK(mask);
}

void HB_App_Poll(void)
{
    /* Commands are a debugger/application mailbox. Never force run=1 here. */
    __WFI();
}

void HB_App_EmergencyStop(void)
{
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    trip(HB_FAULT_PLATFORM);
    __set_PRIMASK(mask);
}
