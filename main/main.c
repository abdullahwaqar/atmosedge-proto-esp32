#include "air_sensors.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "atmosedge-proto";

static air_metrics_t g_last_metrics;
static SemaphoreHandle_t g_metrics_mutex;

static esp_err_t metrics_get_handler(httpd_req_t *req) {
    air_metrics_t m;

    if (xSemaphoreTake(g_metrics_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        m = g_last_metrics;
        xSemaphoreGive(g_metrics_mutex);
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "metrics locked");
        return ESP_FAIL;
    }

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "{"
                       "\"co2_ppm\":%.1f,"
                       "\"temperature_c\":%.2f,"
                       "\"humidity_rh\":%.2f,"
                       "\"voc_index\":%d,"
                       "\"nox_index\":%d,"
                       "\"pressure_hpa\":%.2f"
                       "}",
                       m.co2_ppm, m.temperature_c, m.humidity_rh, m.voc_index,
                       m.nox_index, m.pressure_hpa);

    if (len < 0 || len >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "snprintf error");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

static void metrics_task(void *arg) {
    air_metrics_t m;
    while (1) {
        if (air_sensors_read(&m)) {
            if (xSemaphoreTake(g_metrics_mutex, portMAX_DELAY) == pdTRUE) {
                g_last_metrics = m;
                xSemaphoreGive(g_metrics_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // sample every 2s
    }
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t metrics_uri = {.uri = "/metrics",
                                   .method = HTTP_GET,
                                   .handler = metrics_get_handler,
                                   .user_ctx = NULL};
        httpd_register_uri_handler(server, &metrics_uri);
    }
    return server;
}

static void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = "YOUR_SSID",
                .password = "YOUR_PASS",
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init_sta();

    if (!air_sensors_init()) {
        ESP_LOGE(TAG, "Failed to init sensors");
    }

    g_metrics_mutex = xSemaphoreCreateMutex();
    air_metrics_t zero = {0};
    g_last_metrics = zero;

    xTaskCreate(metrics_task, "metrics_task", 4096, NULL, 5, NULL);
    start_webserver();

    ESP_LOGI(TAG, "HTTP server started, GET /metrics for data");
}
