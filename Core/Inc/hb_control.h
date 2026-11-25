#ifndef HB_CONTROL_H
#define HB_CONTROL_H
#include <stdint.h>

typedef enum {
    HB_STATE_SENSOR_SETTLE = 0,
    HB_STATE_OFFSET_CALIBRATION,
    HB_STATE_IDLE,
    HB_STATE_BOOTSTRAP,
    HB_STATE_RUNNING,
    HB_STATE_FAULT
} HB_State;

typedef enum {
    HB_FAULT_NONE             = 0U,
    HB_FAULT_ADC_RAIL         = 1U << 0,
    HB_FAULT_OFFSET           = 1U << 1,
    HB_FAULT_OVERCURRENT      = 1U << 2,
    HB_FAULT_COMMAND          = 1U << 3,
    HB_FAULT_NOT_VERIFIED     = 1U << 4,
    HB_FAULT_ADC_DRIVER       = 1U << 5,
    HB_FAULT_CURRENT_POLARITY = 1U << 6,
    HB_FAULT_ADC_TIMEOUT      = 1U << 7,
    HB_FAULT_CONFIGURATION    = 1U << 8,
    HB_FAULT_PLATFORM         = 1U << 9
} HB_Fault;

typedef struct {
    HB_State state;
    uint32_t faults;
    uint32_t sample_count;
    uint32_t phase_count;
    uint32_t offset_sum;
    uint32_t offset_min;
    uint32_t offset_max;
    uint32_t calibrated;
    uint32_t hardware_verified;
    uint32_t saw_idle_run_low;
    uint32_t gates_enabled;
    uint16_t adc_raw;
    float offset_count;
    float current_A;
    float filtered_A;
    float reference_A;
    float integrator;
    float duty;
} HB_Control;

/* One owner: ADC ISR. Calls from other contexts need a short critical section. */
void HB_Control_Init(HB_Control *control, uint32_t hardware_verified);
void HB_Control_Step(HB_Control *control, uint16_t adc_raw,
                     float command_A, uint32_t run_request);
void HB_Control_Trip(HB_Control *control, uint32_t faults);
#endif
