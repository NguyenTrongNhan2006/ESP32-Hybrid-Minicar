#pragma once
#include "vehicle_config.h"
bool board_init(void);
bool board_is_ready(void);
uint32_t board_millis(void);
bool board_estop_pressed(void);
int board_adc_raw(void);
void board_gpio_write(unsigned pin, int value);
void board_angle(unsigned pin, unsigned angle);
void board_pulse(unsigned pin, unsigned pulse_us);
size_t board_rx_available(void);
int board_rx_read(void);
void board_tx_try(const char* data, size_t length);
void board_service_tx(void);
