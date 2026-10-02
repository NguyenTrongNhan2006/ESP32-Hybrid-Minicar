#include "board.h"
#include "controller.h"
#include "phone.h"
#include <stdio.h>
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void) {
    board_init();
    vehicle_init();
    if (!phone_init()) printf("PHONE unavailable; USB Serial control still available.\n");
    // Watchdog monitors the task that actually polls safety and applies outputs.
    // Reset is not proof of real ESC/ignition shutdown; verify hardware behavior.
    const esp_task_wdt_config_t watchdog = {
        .timeout_ms = 2000, .idle_core_mask = (1U << portNUM_PROCESSORS) - 1U,
        .trigger_panic = true
    };
    esp_err_t result = esp_task_wdt_reconfigure(&watchdog);
    if (result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_init(&watchdog);
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    for (;;) {
        vehicle_poll();
        phone_poll();
        vehicle_poll();
        board_service_tx();
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(1);
    }
}
