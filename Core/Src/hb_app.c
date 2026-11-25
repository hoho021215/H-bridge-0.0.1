#include "main.h"
#include "hb_app.h"
#include "hb_config.h"

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

/* 외부에서는 hb_request만 입력하고 hb_status를 확인합니다.
 * 실제 제어 상태와 PWM 갱신은 주로 ADC 인터럽트가 담당합니다. */
volatile HB_Request hb_request = {0.0f, 0U};
volatile HB_Status hb_status = {0};
static HB_Control control;
static volatile uint32_t initialized = 0U;
static volatile uint32_t sampling_started = 0U;
static volatile uint32_t last_adc_ms = 0U;
static volatile uint32_t outputs_armed = 0U;

/* 상보 PWM에서는 CCR=0만 쓰면 하측 Q2/Q4가 켜집니다.
 * MOE를 해제해야 설정된 idle LOW 상태로 네 게이트 입력을 모두 끕니다. */
static void kill_outputs(void)
{
    CLEAR_BIT(TIM1->BDTR, TIM_BDTR_MOE);
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    outputs_armed = 0U;
}

/* 제어기 내부 값을 디버거/응용 코드가 읽을 상태 변수로 복사합니다. */
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
    kill_outputs(); /* 실제 출력을 먼저 끈 뒤 지령/오류 상태를 갱신합니다. */
    hb_request.run = 0U;
    HB_Control_Trip(&control, cause);
    publish_status();
}

/* 코드가 가정한 클럭, PWM1/상보 극성, idle LOW, 데드타임, ADC 입력/트리거를
 * 확인합니다. 잘못 생성된 주변장치 설정으로 출력을 켜지 않기 위한 검사입니다. */
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
        && (ADC1->DIFSEL & (1U << 1U)) == 0U /* ADC 채널 1은 single-ended 입력 */
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
    /* 부팅 시 출력과 지령을 끈 상태에서 시작하며, run=1을 자동 요청하지 않습니다. */
    kill_outputs();
    HB_Control_Init(&control, HB_HARDWARE_VERIFIED);
    hb_request.current_A = 0.0f;
    hb_request.run = 0U;
    initialized = 1U;
    if (configuration_valid() == 0U) {
        trip(HB_FAULT_CONFIGURATION);
        return;
    }

    /* CH4는 ADC 시작 시점만 만드는 내부 신호입니다. PWM2/CCR4=2700이면
     * 50 us 주기의 37.5 us 지점에서 상승하며, 외부 CH4 핀은 필요 없습니다.
     * HAL_TIM_PWM_Start는 연결된 CH1/CH2의 MOE까지 켜므로 여기서 호출하지 않습니다. */
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

    /* compare preload: ISR에서 쓴 듀티를 다음 PWM update부터 적용합니다. */
    SET_BIT(TIM1->CCMR1, TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE);
    /* 상측/하측 출력 채널을 모두 준비하지만 MOE는 아직 OFF로 유지합니다. */
    SET_BIT(TIM1->CCER, TIM_CCER_CC1E | TIM_CCER_CC1NE
                       | TIM_CCER_CC2E | TIM_CCER_CC2NE);
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
    TIM1->CNT = 0U;
    TIM1->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
    /* STM32 ADC 자체 보정입니다. 센서의 무전류 영점 평균은 이후 제어기가 수행합니다. */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        trip(HB_FAULT_ADC_DRIVER);
        return;
    }

    last_adc_ms = HAL_GetTick();
    publish_status();
    /* 외부 트리거 대기는 한 번만 시작합니다. 이후 TIM1이 ADC 변환을 반복 요청합니다. */
    if (HAL_ADC_Start_IT(&hadc1) != HAL_OK) {
        trip(HB_FAULT_ADC_DRIVER);
        return;
    }
    sampling_started = 1U;
    /* counter만 시작해 ADC 트리거를 발생시킵니다. 이 호출은 MOE를 켜지 않습니다. */
    if (HAL_TIM_Base_Start(&htim1) != HAL_OK) {
        sampling_started = 0U;
        trip(HB_FAULT_CONFIGURATION);
    }
}

/* 정상 주기에서는 ADC 완료마다 20 kHz(50 us)로 호출됩니다.
 * 측정값 취득 -> 상태/보호/PI 계산 -> 실제 PWM 반영 순서입니다. */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1 || initialized == 0U) return;
    const uint16_t raw = (uint16_t)HAL_ADC_GetValue(hadc);
    last_adc_ms = HAL_GetTick();
    /* 한 제어 주기에서 사용할 입력을 각각 한 번 읽습니다. */
    const float command = hb_request.current_A;
    const uint32_t run = hb_request.run;
    HB_Control_Step(&control, raw, command, run);
    if (control.gates_enabled == 0U) {
        kill_outputs();
    } else {
        if (outputs_armed == 0U) {
            /* 기동할 때만 모든 게이트 OFF 상태에서 타이머 위상을 다시 맞춥니다.
             * CCR1/2=0으로 MOE를 켜면 상측 OFF/양 하측 ON이 되어 bootstrap을
             * 충전합니다. 이어지는 5 ms 충전 상태에서는 PI 전류제어를 하지 않습니다. */
            TIM1->CCR1 = 0U;
            TIM1->CCR2 = 0U;
            TIM1->EGR = TIM_EGR_UG;
            __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
            SET_BIT(TIM1->BDTR, TIM_BDTR_MOE);
            outputs_armed = 1U;
        }
        /* duty는 0~1 비율입니다. 예: 0.20 x 3600 = CCR1 720(비교값 기준 20%). */
        TIM1->CCR1 = (uint32_t)(control.duty * (float)HB_PWM_PERIOD_TICKS);
        TIM1->CCR2 = 0U; /* 우측은 Q3 OFF/Q4 ON을 유지합니다. 양(+) 전류 구동만 지원합니다. */
    }
    if (control.faults != 0U) hb_request.run = 0U;
    publish_status();
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    /* HAL ADC 오류도 출력 차단과 reset까지 유지되는 오류 상태로 처리합니다. */
    if (hadc->Instance == ADC1) trip(HB_FAULT_ADC_DRIVER);
}

void HB_App_SafetyTick1ms(void)
{
    if (sampling_started == 0U) return;
    /* ADC ISR과 오류 상태를 동시에 바꾸지 않도록 잠시 인터럽트를 막습니다.
     * 종료 시 원래 PRIMASK를 복원하여 기존 인터럽트 상태를 보존합니다. */
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    /* 3 ms 초과 ADC 중단 감시입니다. PWM 한 주기(50 us)의 누락이나
     * ADC 이후 12.5 us 계산 마감 초과를 검사하는 기능은 아닙니다. */
    if ((uint32_t)(HAL_GetTick() - last_adc_ms) > HB_ADC_TIMEOUT_MS) {
        trip(HB_FAULT_ADC_TIMEOUT);
    }
    __set_PRIMASK(mask);
}

void HB_App_Poll(void)
{
    /* PI는 ADC ISR에서 실행하므로 여기서는 인터럽트를 기다립니다.
     * 이 루프에서 run=1을 강제로 반복 대입하면 정지 요청을 덮어쓰게 됩니다. */
    __WFI();
}

void HB_App_EmergencyStop(void)
{
    /* Error_Handler/CPU 예외용 정지입니다. MOE를 끄고 플랫폼 오류를 기록합니다. */
    const uint32_t mask = __get_PRIMASK();
    __disable_irq();
    trip(HB_FAULT_PLATFORM);
    __set_PRIMASK(mask);
}
