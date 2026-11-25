#ifndef HB_APP_H
#define HB_APP_H
#include <stdint.h>
#include "hb_control.h"

typedef struct {
    float current_A;       /* Set while run=0, then set run=1. */
    uint32_t run;          /* 0: coast/stop (all four gates low), 1: request run */
} HB_Request;

typedef struct {
    uint32_t state;
    uint32_t faults;
    uint32_t calibrated;
    uint32_t hardware_verified;
    uint32_t gates_enabled;
    uint32_t adc_irq_count;
    uint32_t last_adc_ms;
    uint32_t adc_raw;
    float offset_count;
    float current_A;
    float filtered_A;
    float reference_A;
    float duty;
} HB_Status;

extern volatile HB_Request hb_request;
/* Read-only to application/debugger. Multi-field reads are not atomic. */
extern volatile HB_Status hb_status;

void HB_App_Init(void);
void HB_App_Poll(void);
void HB_App_SafetyTick1ms(void);
void HB_App_EmergencyStop(void);
#endif
