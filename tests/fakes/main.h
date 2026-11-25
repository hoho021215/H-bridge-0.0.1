#ifndef HB_TEST_FAKE_MAIN_H
#define HB_TEST_FAKE_MAIN_H

/* Register/driver model for host regression tests only. It does not model
 * MOSFET waveforms, timer shadow latching, ADC timing or interrupt latency. */
#include <stdint.h>

typedef struct {
    volatile uint32_t CR1, CR2, CCMR1, CCMR2, CCER, BDTR;
    volatile uint32_t PSC, ARR, RCR, CCR1, CCR2, CCR4, CNT, EGR, SR;
} TIM_TypeDef;
typedef struct { volatile uint32_t SQR1, DIFSEL; } ADC_TypeDef;
typedef struct { volatile uint32_t CFGR, CFGR3; } RCC_TypeDef;

extern TIM_TypeDef fake_tim1;
extern ADC_TypeDef fake_adc1;
extern RCC_TypeDef fake_rcc;
#define TIM1 (&fake_tim1)
#define ADC1 (&fake_adc1)
#define RCC  (&fake_rcc)

typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct {
    uint32_t ClockPrescaler, Resolution, DataAlign, ScanConvMode;
    uint32_t NbrOfConversion, ContinuousConvMode, DiscontinuousConvMode;
    uint32_t ExternalTrigConv, ExternalTrigConvEdge;
} ADC_InitTypeDef;
typedef struct { ADC_TypeDef *Instance; ADC_InitTypeDef Init; } ADC_HandleTypeDef;
typedef struct { uint32_t OCMode, Pulse, OCPolarity, OCFastMode; } TIM_OC_InitTypeDef;
typedef struct {
    uint32_t MasterOutputTrigger, MasterOutputTrigger2, MasterSlaveMode;
} TIM_MasterConfigTypeDef;
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;

/* Bit positions follow the pinned STM32F303xE CMSIS header. */
#define TIM_CR1_DIR       0x00000010U
#define TIM_CR1_CMS       0x00000060U
#define TIM_CR1_CKD       0x00000300U
#define TIM_CR2_OIS1      0x00000100U
#define TIM_CR2_OIS1N     0x00000200U
#define TIM_CR2_OIS2      0x00000400U
#define TIM_CR2_OIS2N     0x00000800U
#define TIM_CCMR1_CC1S    0x00000003U
#define TIM_CCMR1_CC2S    0x00000300U
#define TIM_CCMR1_OC1PE   0x00000008U
#define TIM_CCMR1_OC2PE   0x00000800U
#define TIM_CCMR1_OC1M    0x00010070U
#define TIM_CCMR1_OC2M    0x01007000U
#define TIM_CCMR1_OC1M_1  0x00000020U
#define TIM_CCMR1_OC1M_2  0x00000040U
#define TIM_CCER_CC1E     0x00000001U
#define TIM_CCER_CC1P     0x00000002U
#define TIM_CCER_CC1NE    0x00000004U
#define TIM_CCER_CC1NP    0x00000008U
#define TIM_CCER_CC2E     0x00000010U
#define TIM_CCER_CC2P     0x00000020U
#define TIM_CCER_CC2NE    0x00000040U
#define TIM_CCER_CC2NP    0x00000080U
#define TIM_BDTR_DTG      0x000000FFU
#define TIM_BDTR_OSSI     0x00000400U
#define TIM_BDTR_OSSR     0x00000800U
#define TIM_BDTR_AOE      0x00004000U
#define TIM_BDTR_MOE      0x00008000U
#define TIM_EGR_UG        0x00000001U
#define TIM_FLAG_UPDATE   0x00000001U
#define RCC_CFGR_PPRE2    0x00003800U
#define RCC_CFGR3_TIM1SW  0x00000100U
#define ADC_SQR1_L        0x0000000FU
#define ADC_SQR1_SQ1      0x000007C0U
#define ADC_SQR1_SQ1_Pos  6U

#define DISABLE 0U
#define ADC_CLOCK_SYNC_PCLK_DIV2         0x00020000U
#define ADC_RESOLUTION_12B               0U
#define ADC_DATAALIGN_RIGHT              0U
#define ADC_SCAN_DISABLE                 0U
#define ADC_EXTERNALTRIGCONV_T1_TRGO     0x00000240U
#define ADC_EXTERNALTRIGCONVEDGE_RISING  0x00000400U
#define ADC_SINGLE_ENDED                 0U
#define TIM_OCMODE_PWM2                  0x00000070U
#define TIM_OCPOLARITY_HIGH              0U
#define TIM_OCFAST_DISABLE               0U
#define TIM_CHANNEL_4                    0x0000000CU
#define TIM_TRGO_OC4REF                  0x00000070U
#define TIM_TRGO2_RESET                  0U
#define TIM_MASTERSLAVEMODE_DISABLE       0U

#define CLEAR_BIT(reg, bits) ((reg) &= ~(bits))
#define SET_BIT(reg, bits)   ((reg) |= (bits))
#define __HAL_TIM_CLEAR_FLAG(timer, flag) ((timer)->Instance->SR &= ~(flag))

uint32_t HAL_RCC_GetPCLK2Freq(void);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer,
                                          const TIM_OC_InitTypeDef *config,
                                          uint32_t channel);
HAL_StatusTypeDef HAL_TIMEx_MasterConfigSynchronization(
    TIM_HandleTypeDef *timer, const TIM_MasterConfigTypeDef *config);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *adc, uint32_t mode);
HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *adc);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *timer);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *adc);
uint32_t HAL_GetTick(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t mask);
void __WFI(void);

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc);
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc);
#endif
