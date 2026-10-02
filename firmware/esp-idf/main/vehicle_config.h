#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Fixed GPIO map. D0/D1 refer to Logic Analyzer channels, not ESP32 flash pins.
#define PIN_STEER 18
#define PIN_GAS 19
#define PIN_ESC 23
#define PIN_RCEXL_TEST 26
#define PIN_ESTOP 27
#define PIN_BATTERY 34
#define LED_ARMED 25
#define LED_FAILSAFE 32
#define LED_RUNNING 33
#define LOW 0
#define HIGH 1
#define TIMEOUT_MS 500U // Bench setting requested by user; Wokwi remains 5000 ms.
#define BATTERY_SAMPLE_MS 20U
#define LOW_BATTERY_MV 10200U
#define ESC_STOP_US 1000
#define ESC_MAX_TEST_US 1600
// Test markers only: not verified real RCEXL ON/OFF pulse widths.
#define RCEXL_STOP_TEST_US 1000
#define RCEXL_RUN_TEST_US 1500
#define STEER_CENTER_DEG 90
#define GAS_CLOSED_DEG 0
#define GAS_MAX_TEST_DEG 135
#define SERIAL_LINE_SIZE 48U
#define SERIAL_BYTES_PER_LOOP 32U
#define PWM_PERIOD_US 20000U
#define PWM_FREQUENCY_HZ 50U
#define PWM_RESOLUTION_BITS 16U

static inline uint32_t vehicle_pulse_duty(unsigned pulse_us) {
    return (pulse_us * (1U << PWM_RESOLUTION_BITS) + PWM_PERIOD_US / 2U) / PWM_PERIOD_US;
}
static inline unsigned vehicle_angle_us(unsigned degrees) {
    return 500U + degrees * 2000U / 180U;
}
