#ifndef HB_CONFIG_H
#define HB_CONFIG_H

/* Set to 1 only after completing docs/BRINGUP_KO.md on this assembled board.
 * This is a local bring-up setting, not a measured hardware guarantee. */
#ifndef HB_HARDWARE_VERIFIED
#define HB_HARDWARE_VERIFIED 0U
#endif

/* IC3 fitted part must be TMCS1108A3BQDRQ1 (Value says A3, lib_id says A2). */
#define HB_ADC_VREF_V              3.3f
#define HB_SENSOR_SUPPLY_V         3.3f
#define HB_SENSOR_SENSITIVITY_V_A   0.200f
#define HB_CURRENT_SIGN            1.0f
#define HB_ADC_MAX_COUNT           4095U

#define HB_PWM_HZ                  20000U
#define HB_TIMER_CLOCK_HZ          72000000U
#define HB_PWM_PERIOD_TICKS        3600U
#define HB_ADC_TRIGGER_TICKS       2700U
#define HB_DEADTIME_TICKS          72U   /* CKD=DIV1, 72 MHz => 1 us */

/* PROVISIONAL assumptions, NOT motor datasheet ratings. See MOTOR_ASSUMPTIONS. */
#define HB_ASSUMED_BUS_V           12.0f
#define HB_ASSUMED_MOTOR_RATED_A    0.5f
#define HB_ASSUMED_MOTOR_STALL_A    2.0f
#define HB_ASSUMED_MOTOR_R_OHM      6.0f
#define HB_ASSUMED_MOTOR_L_H        0.001f

/* Initial experiment limits and gains; must be tuned to measured hardware. */
#define HB_CURRENT_COMMAND_MAX_A   0.5f
#define HB_SOFTWARE_TRIP_A          1.0f
#define HB_DUTY_MAX                 0.50f
#define HB_PI_KP_DUTY_PER_A         0.05f
#define HB_PI_KI_DUTY_PER_A_S       5.0f
#define HB_CURRENT_LPF_ALPHA        0.2f
#define HB_REFERENCE_SLEW_A_S       0.5f
#define HB_POLARITY_TRIP_A         (-0.2f)

#define HB_SENSOR_SETTLE_SAMPLES   200U  /* 10 ms, gates off */
#define HB_OFFSET_SAMPLES          1024U /* 51.2 ms, true zero current required */
#define HB_OFFSET_MIN_COUNT        1800U
#define HB_OFFSET_MAX_COUNT        2300U
#define HB_OFFSET_MAX_SPAN_COUNT   64U
#define HB_ADC_RAIL_LOW_COUNT      124U
#define HB_ADC_RAIL_HIGH_COUNT     3971U
#define HB_BOOTSTRAP_SAMPLES        100U  /* 5 ms, both low-side FETs on */
#define HB_ADC_TIMEOUT_MS           3U

#if HB_PWM_PERIOD_TICKS != (HB_TIMER_CLOCK_HZ / HB_PWM_HZ)
#error "PWM clock and period are inconsistent"
#endif
#if HB_ADC_TRIGGER_TICKS >= HB_PWM_PERIOD_TICKS
#error "ADC trigger must be inside the PWM period"
#endif
#endif
