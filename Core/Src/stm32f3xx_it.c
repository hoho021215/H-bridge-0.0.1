#include "main.h"
#include "stm32f3xx_it.h"
#include "hb_app.h"

static void Trap(void)
{
    HB_App_EmergencyStop();
    __disable_irq();
    while (1) { }
}
void NMI_Handler(void) { Trap(); }
void HardFault_Handler(void) { Trap(); }
void MemManage_Handler(void) { Trap(); }
void BusFault_Handler(void) { Trap(); }
void UsageFault_Handler(void) { Trap(); }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }
void SysTick_Handler(void)
{
    HAL_IncTick();
    HB_App_SafetyTick1ms();
}
void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}
