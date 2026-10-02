#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static adc_oneshot_unit_handle_t adc;
static bool ready, uart_ready;
static bool configured[4];
static unsigned last_pulse[4];
static char tx[512];
static size_t tx_head, tx_tail;
static portMUX_TYPE estop_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool estop_edge;
static const unsigned pins[4] = {PIN_STEER, PIN_GAS, PIN_ESC, PIN_RCEXL_TEST};
static const unsigned safe_us[4] = {1500, 500, ESC_STOP_US, RCEXL_STOP_TEST_US};

static void estop_isr(void* arg) {
    (void)arg;
    portENTER_CRITICAL_ISR(&estop_lock);
    estop_edge = true;
    portEXIT_CRITICAL_ISR(&estop_lock);
}

static void hardware_fault(void) {
    ready = false; // Latched until reboot. Best effort safe targets on working channels.
    for (int i = 0; i < 4; ++i) {
        if (configured[i]) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, vehicle_pulse_duty(safe_us[i]));
            ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i);
            last_pulse[i] = safe_us[i];
        }
    }
    gpio_set_level(LED_FAILSAFE, 1);
    gpio_set_level(LED_ARMED, 0);
    gpio_set_level(LED_RUNNING, 0);
}

bool board_init(void) {
    ready = false;
    const gpio_config_t leds = {
        .pin_bit_mask = (1ULL << LED_ARMED) | (1ULL << LED_FAILSAFE) | (1ULL << LED_RUNNING),
        .mode = GPIO_MODE_OUTPUT
    };
    if (gpio_config(&leds) != ESP_OK) return false;
    gpio_set_level(LED_FAILSAFE, 1);
    const gpio_config_t estop = {
        .pin_bit_mask = 1ULL << PIN_ESTOP, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    if (gpio_config(&estop) != ESP_OK) return false;
    if (gpio_install_isr_service(0) != ESP_OK ||
        gpio_isr_handler_add(PIN_ESTOP, estop_isr, NULL) != ESP_OK) return false;
    const uart_config_t uart_cfg = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    if (uart_param_config(UART_NUM_0, &uart_cfg) != ESP_OK ||
        uart_set_pin(UART_NUM_0, 1, 3, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK ||
        uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0) != ESP_OK) return false;
    uart_ready = true;

    const adc_oneshot_unit_init_cfg_t adc_cfg = {.unit_id = ADC_UNIT_1};
    const adc_oneshot_chan_cfg_t channel_cfg = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};
    if (adc_oneshot_new_unit(&adc_cfg, &adc) != ESP_OK ||
        adc_oneshot_config_channel(adc, ADC_CHANNEL_6, &channel_cfg) != ESP_OK) return false;
    // ESP32 GPIO34 = ADC1 channel 6. Raw mapping is still a potentiometer test.
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_16_BIT,
        .timer_num = LEDC_TIMER_0, .freq_hz = PWM_FREQUENCY_HZ, .clk_cfg = LEDC_USE_APB_CLK
    };
    if (ledc_timer_config(&timer) != ESP_OK) return false;
    for (int i = 0; i < 4; ++i) {
        const ledc_channel_config_t channel = {
            .gpio_num = (int)pins[i], .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)i, .timer_sel = LEDC_TIMER_0,
            .duty = vehicle_pulse_duty(safe_us[i]), .hpoint = 0
        };
        if (ledc_channel_config(&channel) != ESP_OK) { hardware_fault(); return false; }
        configured[i] = true;
        last_pulse[i] = safe_us[i];
    }
    ready = true;
    return true;
}

bool board_is_ready(void) { return ready; }
uint32_t board_millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
bool board_estop_pressed(void) {
    portENTER_CRITICAL(&estop_lock);
    bool edge = estop_edge;
    estop_edge = false;
    portEXIT_CRITICAL(&estop_lock);
    // A short observed LOW pulse also stops the vehicle. Release never re-arms.
    return edge || gpio_get_level(PIN_ESTOP) == 0;
}
int board_adc_raw(void) {
    int raw = -1;
    if (adc == NULL || adc_oneshot_read(adc, ADC_CHANNEL_6, &raw) != ESP_OK) return -1;
    return raw;
}
void board_gpio_write(unsigned pin, int value) {
    if (gpio_set_level((gpio_num_t)pin, (uint32_t)value) != ESP_OK) hardware_fault();
}
void board_pulse(unsigned pin, unsigned pulse_us) {
    for (int i = 0; i < 4; ++i) {
        if (pins[i] != pin || !configured[i]) continue;
        if (!ready) pulse_us = safe_us[i];
        if (pulse_us == last_pulse[i]) return;
        if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i, vehicle_pulse_duty(pulse_us)) != ESP_OK ||
            ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)i) != ESP_OK) {
            hardware_fault();
            return;
        }
        last_pulse[i] = pulse_us;
        return;
    }
}
void board_angle(unsigned pin, unsigned angle) { board_pulse(pin, vehicle_angle_us(angle)); }
size_t board_rx_available(void) {
    size_t count = 0;
    if (uart_ready) uart_get_buffered_data_len(UART_NUM_0, &count);
    return count;
}
int board_rx_read(void) {
    unsigned char byte;
    return uart_ready && uart_read_bytes(UART_NUM_0, &byte, 1, 0) == 1 ? byte : -1;
}
void board_tx_try(const char* data, size_t length) {
    size_t used = (tx_head + sizeof(tx) - tx_tail) % sizeof(tx);
    if (!uart_ready || length > sizeof(tx) - used - 1) return;
    for (size_t i = 0; i < length; ++i) {
        tx[tx_head] = data[i];
        tx_head = (tx_head + 1) % sizeof(tx);
    }
}
void board_service_tx(void) {
    if (!uart_ready || tx_head == tx_tail) return;
    size_t count = tx_head > tx_tail ? tx_head - tx_tail : sizeof(tx) - tx_tail;
    if (count > 128) count = 128;
    // tx_buffer_size=0: uart_tx_chars writes only what fits, never waits for TX.
    int sent = uart_tx_chars(UART_NUM_0, tx + tx_tail, (uint32_t)count);
    if (sent > 0) tx_tail = (tx_tail + (size_t)sent) % sizeof(tx);
}
