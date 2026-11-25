/* Host model of hardware-facing stop/start behavior, not an electrical test.
 * Run each named case in a new process so app static state starts as at boot. */
#include "main.h"
#include "hb_app.h"
#include "hb_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

TIM_TypeDef fake_tim1;
ADC_TypeDef fake_adc1;
RCC_TypeDef fake_rcc;
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim1;
static uint32_t now_ms, irq_mask, raw_adc = 2048U, pclk2 = 72000000U;
static unsigned adc_starts, timer_starts, calibrations, sample_number;
static HAL_StatusTypeDef adc_start_result = HAL_OK;
static HAL_StatusTypeDef timer_start_result = HAL_OK;

static uint32_t moe(void) { return TIM1->BDTR & TIM_BDTR_MOE; }
uint32_t HAL_RCC_GetPCLK2Freq(void) { return pclk2; }
uint32_t HAL_GetTick(void) { return now_ms; }
uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1U; }
void __set_PRIMASK(uint32_t mask) { irq_mask = mask; }
void __WFI(void) { }

HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer,
                                          const TIM_OC_InitTypeDef *config,
                                          uint32_t channel)
{
    assert(timer->Instance == TIM1 && channel == TIM_CHANNEL_4);
    assert(moe() == 0U);
    assert(config->OCMode == TIM_OCMODE_PWM2);
    assert(config->Pulse == HB_ADC_TRIGGER_TICKS);
    assert(config->OCPolarity == TIM_OCPOLARITY_HIGH);
    timer->Instance->CCR4 = config->Pulse;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIMEx_MasterConfigSynchronization(
    TIM_HandleTypeDef *timer, const TIM_MasterConfigTypeDef *config)
{
    assert(timer->Instance == TIM1 && moe() == 0U);
    assert(config->MasterOutputTrigger == TIM_TRGO_OC4REF);
    assert(config->MasterOutputTrigger2 == TIM_TRGO2_RESET);
    assert(config->MasterSlaveMode == TIM_MASTERSLAVEMODE_DISABLE);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *adc, uint32_t mode)
{
    assert(adc->Instance == ADC1 && mode == ADC_SINGLE_ENDED);
    assert(moe() == 0U);
    calibrations++;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *adc)
{
    assert(adc->Instance == ADC1 && moe() == 0U);
    adc_starts++;
    return adc_start_result;
}
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *timer)
{
    assert(timer->Instance == TIM1 && moe() == 0U);
    timer_starts++;
    return timer_start_result;
}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *adc)
{
    assert(adc->Instance == ADC1);
    return raw_adc;
}

static void setup_registers(void)
{
    memset(&fake_tim1, 0, sizeof fake_tim1);
    memset(&fake_adc1, 0, sizeof fake_adc1);
    memset(&fake_rcc, 0, sizeof fake_rcc);
    memset(&hadc1, 0, sizeof hadc1);
    htim1.Instance = TIM1;
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.NbrOfConversion = 1U;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_TRGO;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    TIM1->ARR = HB_PWM_PERIOD_TICKS - 1U;
    TIM1->CCMR1 = 0x00006060U; /* CH1 and CH2 both PWM1, output mode */
    TIM1->BDTR = TIM_BDTR_OSSI | TIM_BDTR_OSSR | HB_DEADTIME_TICKS;
    ADC1->SQR1 = 1U << ADC_SQR1_SQ1_Pos;
}

static void sample(uint16_t raw)
{
    raw_adc = raw;
    now_ms = sample_number++ / (HB_PWM_HZ / 1000U);
    HAL_ADC_ConvCpltCallback(&hadc1);
}

static void calibrate(void)
{
    assert(hb_status.faults == 0U && moe() == 0U);
    for (unsigned k = 0; k < HB_SENSOR_SETTLE_SAMPLES + HB_OFFSET_SAMPLES; ++k) {
        sample(2048U);
        assert(moe() == 0U && hb_status.gates_enabled == 0U);
    }
    assert(hb_status.state == HB_STATE_IDLE);
    assert(hb_status.calibrated == 1U);
    assert(hb_status.offset_count == 2048.0f);
    sample(2048U); /* Fresh idle run=0 after calibration permits a later start. */
}

static void request_start(void)
{
    hb_request.current_A = 0.2f;
    hb_request.run = 1U;
    sample(2048U);
}

static void check_latched(uint32_t cause)
{
    assert((hb_status.faults & cause) != 0U);
    assert(hb_status.state == HB_STATE_FAULT);
    assert(hb_status.gates_enabled == 0U && moe() == 0U);
    assert(TIM1->CCR1 == 0U && TIM1->CCR2 == 0U);
    hb_request.current_A = 0.1f;
    hb_request.run = 1U;
    sample(2048U);
    assert((hb_status.faults & cause) != 0U && moe() == 0U);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *name = argv[1];
    setup_registers();
    if (strcmp(name, "bad_mode") == 0) TIM1->CCMR1 |= 0x00001000U; /* CH2 PWM2 */
    if (strcmp(name, "bad_rank") == 0) ADC1->SQR1 = 2U << ADC_SQR1_SQ1_Pos;
    if (strcmp(name, "bad_differential") == 0) ADC1->DIFSEL = 1U << 1U;
    if (strcmp(name, "bad_clock") == 0) RCC->CFGR3 = RCC_CFGR3_TIM1SW;
    if (strcmp(name, "bad_frequency") == 0) pclk2 = 36000000U;
    if (strcmp(name, "apb_prescaler") == 0) {
        RCC->CFGR = 0x00002000U; /* PPRE2 divisor >1 => timer clock doubles. */
        pclk2 = 36000000U;
    }
    if (strcmp(name, "adc_start_failure") == 0) adc_start_result = HAL_ERROR;
    if (strcmp(name, "timer_start_failure") == 0) timer_start_result = HAL_ERROR;
    HB_App_Init();

    if (strncmp(name, "bad_", 4U) == 0) {
        assert(hb_status.faults == HB_FAULT_CONFIGURATION);
        assert(moe() == 0U && adc_starts == 0U && timer_starts == 0U);
    } else if (strcmp(name, "adc_start_failure") == 0) {
        assert(adc_starts == 1U && timer_starts == 0U);
        check_latched(HB_FAULT_ADC_DRIVER);
    } else if (strcmp(name, "timer_start_failure") == 0) {
        assert(adc_starts == 1U && timer_starts == 1U);
        check_latched(HB_FAULT_CONFIGURATION);
    } else if (strcmp(name, "init") == 0 || strcmp(name, "apb_prescaler") == 0) {
        assert(adc_starts == 1U && timer_starts == 1U && calibrations == 1U);
        assert(moe() == 0U && hb_status.faults == 0U);
        assert(TIM1->CCR4 == HB_ADC_TRIGGER_TICKS);
        assert((TIM1->CCER & 0x55U) == 0x55U); /* Four connected outputs enabled,
                                              * all still gated off by MOE. */
        calibrate();
    } else if (strcmp(name, "guard") == 0) {
        calibrate();
        request_start();
        if (HB_HARDWARE_VERIFIED == 0U) {
            check_latched(HB_FAULT_NOT_VERIFIED);
        } else {
            assert(hb_status.state == HB_STATE_BOOTSTRAP && moe() != 0U);
            assert(TIM1->CCR1 == 0U && TIM1->CCR2 == 0U);
        }
    } else if (strcmp(name, "run_stop") == 0) {
        assert(HB_HARDWARE_VERIFIED != 0U);
        calibrate();
        request_start();
        for (unsigned k = 0; k < HB_BOOTSTRAP_SAMPLES; ++k) {
            assert(moe() != 0U && TIM1->CCR1 == 0U && TIM1->CCR2 == 0U);
            sample(2048U);
        }
        assert(hb_status.state == HB_STATE_RUNNING && hb_status.duty == 0.0f);
        for (unsigned k = 0; k < 500U; ++k) sample(2048U);
        assert(TIM1->CCR1 > 0U && TIM1->CCR2 == 0U && moe() != 0U);
        hb_request.run = 0U;
        sample(2048U);
        assert(moe() == 0U && hb_status.state == HB_STATE_IDLE);
        assert(TIM1->CCR1 == 0U && TIM1->CCR2 == 0U);
        assert(hb_status.duty == 0.0f && hb_status.gates_enabled == 0U);
        assert(adc_starts == 1U && timer_starts == 1U);
    } else if (strcmp(name, "adc_error") == 0) {
        calibrate();
        if (HB_HARDWARE_VERIFIED != 0U) request_start();
        HAL_ADC_ErrorCallback(&hadc1);
        check_latched(HB_FAULT_ADC_DRIVER);
    } else if (strcmp(name, "adc_timeout") == 0) {
        calibrate();
        if (HB_HARDWARE_VERIFIED != 0U) request_start();
        now_ms = hb_status.last_adc_ms + HB_ADC_TIMEOUT_MS;
        HB_App_SafetyTick1ms();
        assert(hb_status.faults == 0U); /* Strictly greater than 3 ms. */
        ++now_ms;
        HB_App_SafetyTick1ms();
        assert(irq_mask == 0U);
        check_latched(HB_FAULT_ADC_TIMEOUT);
    } else if (strcmp(name, "emergency") == 0) {
        calibrate();
        if (HB_HARDWARE_VERIFIED != 0U) request_start();
        HB_App_EmergencyStop();
        assert(irq_mask == 0U);
        check_latched(HB_FAULT_PLATFORM);
    } else if (strcmp(name, "primask") == 0) {
        irq_mask = 1U;
        HB_App_SafetyTick1ms();
        assert(irq_mask == 1U);
        HB_App_EmergencyStop();
        assert(irq_mask == 1U);
    } else {
        (void)fprintf(stderr, "Unknown app test case: %s\n", name);
        return 2;
    }
    (void)printf("PASS app verified=%u case=%s\n", (unsigned)HB_HARDWARE_VERIFIED, name);
    return 0;
}
