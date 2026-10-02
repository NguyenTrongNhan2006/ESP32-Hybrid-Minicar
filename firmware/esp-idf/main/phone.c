#include "phone.h"
#include "board.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#define PHONE_STOP_BIT BIT0
#define DISCONNECT_BIT BIT1
#define COMMAND_MAX_AGE_MS 100U
typedef struct {
    char text[SERIAL_LINE_SIZE];
    uint32_t session, sequence, epoch, received_ms;
} phone_command_t;
static QueueHandle_t commands;
static EventGroupHandle_t events;
static portMUX_TYPE status_lock = portMUX_INITIALIZER_UNLOCKED;
static vehicle_status_t snapshot;
static uint32_t ack_session, ack_sequence;
static bool ack_accepted;
extern const unsigned char web_start[] asm("_binary_web_html_start");
extern const unsigned char web_end[] asm("_binary_web_html_end");

static void publish_status(void) {
    vehicle_status_t current = vehicle_status();
    portENTER_CRITICAL(&status_lock);
    snapshot = current;
    portEXIT_CRITICAL(&status_lock);
}

static bool header_u32(httpd_req_t* req, const char* name, uint32_t* value) {
    char text[12];
    if (httpd_req_get_hdr_value_str(req, name, text, sizeof(text)) != ESP_OK || !text[0]) return false;
    uint64_t n = 0;
    for (const char* p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        n = n * 10 + (unsigned)(*p - '0');
        if (n > UINT32_MAX) return false;
    }
    *value = (uint32_t)n;
    return true;
}

static esp_err_t home_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'");
    return httpd_resp_send(req, (const char*)web_start, web_end - web_start - 1);
}

static esp_err_t command_handler(httpd_req_t* req) {
    phone_command_t cmd = {0};
    if (req->content_len < 1 || req->content_len >= sizeof(cmd.text) ||
        !header_u32(req, "X-Control-Session", &cmd.session) || !cmd.session ||
        !header_u32(req, "X-Control-Seq", &cmd.sequence) || !cmd.sequence ||
        !header_u32(req, "X-Control-Epoch", &cmd.epoch)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid command frame");
    }
    size_t received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, cmd.text + received, req->content_len - received);
        if (n <= 0) return httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Incomplete body");
        received += (size_t)n;
    }
    for (size_t i = 0; i < received; ++i) {
        if (cmd.text[i] < 32 || (unsigned char)cmd.text[i] > 126) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ASCII line only");
        }
    }
    cmd.received_ms = board_millis();
    // STOP bypasses a full command queue. Outputs are changed by the control task.
    if (strcmp(cmd.text, "STOP") == 0) xEventGroupSetBits(events, PHONE_STOP_BIT);
    else if (xQueueSend(commands, &cmd, 0) != pdTRUE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Command queue full");
    }
    httpd_resp_set_status(req, "202 Accepted");
    return httpd_resp_sendstr(req, "Queued; read /api/status for controller acknowledgement");
}

static void json_escape(const char* input, char* output, size_t capacity) {
    size_t n = 0;
    for (; *input && n + 3 < capacity; ++input) {
        if (*input == '"' || *input == '\\') output[n++] = '\\';
        if (*input == '\n' || *input == '\r') { output[n++] = ' '; continue; }
        output[n++] = *input;
    }
    output[n] = '\0';
}

static esp_err_t status_handler(httpd_req_t* req) {
    vehicle_status_t status;
    uint32_t session, sequence;
    bool accepted;
    portENTER_CRITICAL(&status_lock);
    status = snapshot; session = ack_session; sequence = ack_sequence; accepted = ack_accepted;
    portEXIT_CRITICAL(&status_lock);
    char message[260], json[620];
    json_escape(status.reply, message, sizeof(message));
    snprintf(json, sizeof(json),
        "{\"state\":\"%s\",\"battery_mv\":%" PRIu32 ",\"owner\":%" PRIu32
        ",\"epoch\":%" PRIu32 ",\"steer\":%d,\"gas\":%d,\"esc\":%d,\"last_stop\":\"%s\","
        "\"ack_session\":%" PRIu32 ",\"ack_seq\":%" PRIu32 ",\"accepted\":%s,\"message\":\"%s\"}",
        status.state == RUNNING ? "RUNNING" : status.state == ARMED ? "ARMED" : "STOPPED",
        status.battery_mv, status.owner, status.stop_count, status.steer_deg, status.gas_percent,
        status.esc_percent, status.last_stop, session, sequence, accepted ? "true" : "false", message);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static void wifi_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
    (void)arg; (void)base; (void)data;
    if (id == WIFI_EVENT_AP_STADISCONNECTED) xEventGroupSetBits(events, DISCONNECT_BIT);
}

bool phone_init(void) {
    snapshot = vehicle_status();
    commands = xQueueCreate(8, sizeof(phone_command_t));
    events = xEventGroupCreate();
    if (!commands || !events) return false;
    // Do not erase NVS automatically: it can contain other user data.
    if (nvs_flash_init() != ESP_OK || esp_netif_init() != ESP_OK ||
        esp_event_loop_create_default() != ESP_OK || !esp_netif_create_default_wifi_ap()) return false;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK || esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) return false;
    wifi_config_t cfg = {0};
    strcpy((char*)cfg.ap.ssid, "ESP32-Hybrid");
    snprintf((char*)cfg.ap.password, sizeof(cfg.ap.password), "MC-%08" PRIX32, esp_random());
    cfg.ap.ssid_len = strlen((char*)cfg.ap.ssid);
    cfg.ap.channel = 1; cfg.ap.max_connection = 1;
    cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, wifi_event, NULL) != ESP_OK ||
        esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK || esp_wifi_set_config(WIFI_IF_AP, &cfg) != ESP_OK ||
        esp_wifi_start() != ESP_OK) return false;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.recv_wait_timeout = 1;
    config.send_wait_timeout = 1;
    config.lru_purge_enable = true;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) return false;
    const httpd_uri_t home = {.uri = "/", .method = HTTP_GET, .handler = home_handler};
    const httpd_uri_t command = {.uri = "/api/command", .method = HTTP_POST, .handler = command_handler};
    const httpd_uri_t status = {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler};
    if (httpd_register_uri_handler(server, &home) != ESP_OK ||
        httpd_register_uri_handler(server, &command) != ESP_OK ||
        httpd_register_uri_handler(server, &status) != ESP_OK) return false;
    // Startup only, before the controller accepts any ARM/RUN command.
    printf("\nPHONE: WiFi ESP32-Hybrid | password %s | http://192.168.4.1\n", cfg.ap.password);
    printf("Password changes after reset. Phone UI: ARM then hold RUN. Release = STOP.\n");
    return true;
}

void phone_poll(void) {
    if (!commands || !events) return;
    EventBits_t flags = xEventGroupClearBits(events, PHONE_STOP_BIT | DISCONNECT_BIT);
    vehicle_status_t status = vehicle_status();
    if ((flags & PHONE_STOP_BIT) || ((flags & DISCONNECT_BIT) && status.owner != 0)) {
        vehicle_stop((flags & PHONE_STOP_BIT) ? "PHONE_STOP" : "PHONE_DISCONNECTED");
        xQueueReset(commands);
    }
    phone_command_t cmd;
    for (unsigned i = 0; i < 4 && xQueueReceive(commands, &cmd, 0) == pdTRUE; ++i) {
        status = vehicle_status();
        bool valid = cmd.epoch == status.stop_count &&
            (uint32_t)(board_millis() - cmd.received_ms) <= COMMAND_MAX_AGE_MS;
        bool accepted = valid && vehicle_execute(cmd.text, cmd.session, cmd.sequence);
        portENTER_CRITICAL(&status_lock);
        ack_session = cmd.session; ack_sequence = cmd.sequence; ack_accepted = accepted;
        portEXIT_CRITICAL(&status_lock);
        if (vehicle_status().stop_count != status.stop_count) { xQueueReset(commands); break; }
    }
    publish_status();
}
