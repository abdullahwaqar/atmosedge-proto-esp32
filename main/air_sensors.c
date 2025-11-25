#include "air_sensors.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"

#define I2C_PORT I2C_NUM_0
#define I2C_SDA GPIO_NUM_8
#define I2C_SCL GPIO_NUM_9
#define I2C_FREQ 400000

static const char *TAG = "air-sensors";

/**
 * I2C Helper Functions
 */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &conf));
    return i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

/**
 * SCD41 (CO2/Temp/Humidity)
 */
static esp_err_t scd41_read(float *co2, float *temp, float *rh) {
    uint8_t cmd[] = {0xEC, 0x05};
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_PORT, 0x62, cmd, 2,
                                               100 / portTICK_PERIOD_MS));
    vTaskDelay(pdMS_TO_TICKS(5));

    uint8_t data[6];
    ESP_ERROR_CHECK(i2c_master_read_from_device(I2C_PORT, 0x62, data, 6,
                                                100 / portTICK_PERIOD_MS));

    uint16_t co2_raw = (data[0] << 8) | data[1];
    uint16_t t_raw = (data[2] << 8) | data[3];
    uint16_t rh_raw = (data[4] << 8) | data[5];

    *co2 = co2_raw;
    *temp = -45 + 175 * (t_raw / 65535.0);
    *rh = 100 * (rh_raw / 65535.0);
    return ESP_OK;
}

/**
 * SGP41 (VOC/NOx)
 */
static esp_err_t sgp41_read(int *voc, int *nox) {
    uint8_t cmd[] = {0x26, 0x0F};
    ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_PORT, 0x59, cmd, 2,
                                               100 / portTICK_PERIOD_MS));
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t data[6];
    ESP_ERROR_CHECK(i2c_master_read_from_device(I2C_PORT, 0x59, data, 6,
                                                100 / portTICK_PERIOD_MS));

    *voc = (data[0] << 8) | data[1];
    *nox = (data[3] << 8) | data[4];
    return ESP_OK;
}

/**
 * LPS22 (Pressure)
 */
static esp_err_t lps22_read(float *pressure) {
    uint8_t reg = 0x28 | 0x80; // Auto-increment
    uint8_t data[3];

    ESP_ERROR_CHECK(i2c_master_write_read_device(I2C_PORT, 0x5C, &reg, 1, data,
                                                 3, 100 / portTICK_PERIOD_MS));

    int32_t raw = (int32_t)(data[2] << 16 | data[1] << 8 | data[0]);
    *pressure = raw / 4096.0f;
    return ESP_OK;
}

/**
 * Init sensors
 */
bool air_sensors_init(void) {
    ESP_LOGI(TAG, "Initializing I2C and sensors...");

    if (i2c_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed");
        return false;
    }

    // Start SCD41 continuous measurement
    uint8_t start_cmd[] = {0x21, 0xB1};
    i2c_master_write_to_device(I2C_PORT, 0x62, start_cmd, 2,
                               100 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Sensors initialized");
    return true;
}

bool air_sensors_read(air_metrics_t *out) {
    if (!out) {
        return false;
    }

    if (scd41_read(&out->co2_ppm, &out->temperature_c, &out->humidity_rh) !=
        ESP_OK) {
        ESP_LOGE(TAG, "Failed to read SCD41");
        return false;
    }

    if (sgp41_read(&out->voc_index, &out->nox_index) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read SGP41");
        return false;
    }

    if (lps22_read(&out->pressure_hpa) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read LPS22");
        return false;
    }

    ESP_LOGI(TAG,
             "CO2: %.1f ppm, Temp: %.2f C, RH: %.2f%%, VOC: %d, NOx: %d, "
             "Pressure: %.2f hPa",
             out->co2_ppm, out->temperature_c, out->humidity_rh, out->voc_index,
             out->nox_index, out->pressure_hpa);

    return true;
}
