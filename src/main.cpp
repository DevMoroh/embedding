#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "spi.h"
#include "bme280_spi.h"
#include "led.h"
#include "i2c.h"
#include "oled.h"

#define LED_OUT             GPIO_NUM_16

// Шина I2C для OLED
#define I2C_MASTER_SCL_IO   GPIO_NUM_9
#define I2C_MASTER_SDA_IO   GPIO_NUM_8
#define I2C_MASTER_NUM      I2C_NUM_0

// Період оновлення показників BME280
#define BME280_UPDATE_PERIOD_US   (1000 * 1000)

static const char *TAG = "BME280_SPI";

extern spi_device_handle_t spi;

// Стан, з яким працює колбек таймера. Статичний, бо колбек
// викликається вже після виходу з app_main і її локальні змінні зникнуть
static oled_handle_t oled = NULL;
static Bme280Calib calib = {};
static Led *led = nullptr;
static bool led_on = false;
static esp_timer_handle_t update_timer = NULL;

// Налаштовує BME280 на безперервні вимірювання (normal mode),
// щоб не чекати завершення кожного вимірювання, як у forced mode
static esp_err_t bme280_start_normal_mode(spi_device_handle_t dev)
{
    // 0xF2 ctrl_hum: osrs_h = 001 - вологість x1
    esp_err_t ret = spi_write_register(dev, 0xF2, 0x01);
    if (ret != ESP_OK) {
        return ret;
    }

    // 0xF5 config: t_sb = 101 (1000 мс між вимірюваннями), фільтр вимкнено.
    // Дані оновлюються раз на секунду, тож два зчитування поспіль у bme280_read
    // майже завжди однакові й на справжньому датчику
    ret = spi_write_register(dev, 0xF5, 0xA0);
    if (ret != ESP_OK) {
        return ret;
    }

    // 0xF4 ctrl_meas: osrs_t = 001, osrs_p = 001, mode = 11 (normal)
    // Записується після ctrl_hum, щоб налаштування вологості застосувалися
    return spi_write_register(dev, 0xF4, 0x27);
}

// Скільки разів пробувати отримати два однакові зчитування поспіль
#define BME280_READ_ATTEMPTS   3

// Читає сирі дані одним пакетом 0xF7..0xFE і повертає компенсовані значення.
// Дані читаються двічі й приймаються, лише якщо обидва зчитування збігаються:
static esp_err_t bme280_read(spi_device_handle_t dev, const Bme280Calib *calib,
                             Bme280Compensated *out)
{
    // 0xF7..0xF9 - тиск, 0xFA..0xFC - температура (по 20 біт), 0xFD..0xFE - вологість (16 біт)
    uint8_t raw[8] = {};
    uint8_t check[8] = {};
    esp_err_t ret = ESP_ERR_INVALID_RESPONSE;
    for (int attempt = 0; attempt < BME280_READ_ATTEMPTS; attempt++) {
        ret = spi_read_registers(dev, 0xF7, raw, sizeof(raw));
        if (ret == ESP_OK) {
            ret = spi_read_registers(dev, 0xF7, check, sizeof(check));
        }
        // перевіряємо чи два зчитування однакові
        if (ret == ESP_OK && memcmp(raw, check, sizeof(raw)) == 0) {
            break;
        }
        ESP_LOGW(TAG, "BME280 data mismatch, retry %d", attempt + 1);
        ret = ESP_ERR_INVALID_RESPONSE;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    int32_t pressure = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | ((int32_t)raw[2] >> 4);
    int32_t temperature = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | ((int32_t)raw[5] >> 4);
    int32_t humidity = ((int32_t)raw[6] << 8) | (int32_t)raw[7];

    *out = bme280_compensate(temperature, pressure, humidity, calib);
    return ESP_OK;
}

// Колбек esp_timer (раз на секунду): читає BME280 і оновлює OLED.
// Виконується в задачі esp_timer (ESP_TIMER_TASK), тому SPI та I2C тут дозволені;
// робота займає кілька мілісекунд, інші таймери на цей час чекають
static void update_timer_cb(void *arg)
{
    Bme280Compensated value = {};
    esp_err_t ret = bme280_read(spi, &calib, &value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BME280 read failed: %s", esp_err_to_name(ret));
        oled_printf(oled, 2, 1, "%-16s", "Read error");
        return;
    }

    ESP_LOGI(TAG, "T: %.1f C, RH: %.0f %%, P: %.0f hPa",
             value.temperature_c, value.humidity_rh, value.pressure_hpa);

    // Рядок дисплея - до 16 символів; доповнюємо пробілами, щоб стерти старий текст.
    // 0xB0 - символ градуса в шрифті дисплея
    //    рядок 2 - температура, рядок 4 - вологість, рядок 6 - тиск
    char line[17];
    snprintf(line, sizeof(line), "T:  %.1f\xB0" "C", value.temperature_c);
    oled_printf(oled, 2, 1, "%-16s", line);
    snprintf(line, sizeof(line), "RH: %.0f%%", value.humidity_rh);
    oled_printf(oled, 4, 1, "%-16s", line);
    snprintf(line, sizeof(line), "P:  %.0f hPa", value.pressure_hpa);
    oled_printf(oled, 6, 1, "%-16s", line);

    // Світлодіод перемикається на кожному оновленні
    led_on = !led_on;
    if (led_on) {
        led->on();
    } else {
        led->off();
    }
}

extern "C" void app_main() {
    static Led status_led(LED_OUT);
    led = &status_led;

    // 1. Шина I2C і OLED 128x64 (адреса 0x3C)
    i2c_master_bus_handle_t bus = NULL;
    i2c_bus_settings_t bus_cfg = {};
    bus_cfg.port = I2C_MASTER_NUM;
    bus_cfg.sda_pin = I2C_MASTER_SDA_IO;
    bus_cfg.scl_pin = I2C_MASTER_SCL_IO;
    bus_cfg.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_bus_init(&bus_cfg, &bus));

    oled_settings_t oled_cfg = OLED_SETTINGS_128X64_DEFAULT();
    esp_err_t ret = oled_init(bus, &oled_cfg, &oled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_ERROR_CHECK(oled_clear(oled));
    oled_print(oled, 0, 1, false, "BME280 (SPI)");

    // 2. SPI і BME280 (емулятор на STM32)
    ESP_ERROR_CHECK(spi_initialize(&spi));

    // BME280: регістр chip_id = 0xD0, очікуване значення 0x60
    uint8_t chip_id = 0;
    ret = spi_read_registers(spi, 0xD0, &chip_id, 1);
    if (ret != ESP_OK || chip_id != 0x60) {
        ESP_LOGE(TAG, "BME280 not found (chip ID=0x%02X)", chip_id);
        oled_print(oled, 3, 1, false, "Sensor not found");
        return;
    }
    ESP_LOGI(TAG, "Chip ID: 0x%02X", chip_id);

    ESP_ERROR_CHECK(bme280_read_calibration(spi, &calib));
    ESP_ERROR_CHECK(bme280_start_normal_mode(spi));

    // 3. Періодичний таймер: раз на секунду читає датчик і оновлює дисплей
    const esp_timer_create_args_t timer_args = {
        .callback = update_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "bme280_update",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &update_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_timer, BME280_UPDATE_PERIOD_US));

    // Далі все робить таймер; app_main можна завершити
    ESP_LOGI(TAG, "Update timer started (%d ms)", BME280_UPDATE_PERIOD_US / 1000);
}
