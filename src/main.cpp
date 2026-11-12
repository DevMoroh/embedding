#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "i2c.h"
#include "at24c32.h"
#include "logger.h"

static const char *TAG = "app";

// Піни та порт шини I2C
#define I2C_MASTER_SCL_IO  GPIO_NUM_9
#define I2C_MASTER_SDA_IO  GPIO_NUM_8
#define I2C_MASTER_NUM     I2C_NUM_0

// Рівні виводів A2..A0 мікросхеми AT24C32: 0 -> адреса 0x50
#define EEPROM_A_PINS      0U

// Скільки логів дописати понад 128 при першому запуску, щоб показати перезапис найстаріших
#define DEMO_OVERFLOW_LOGS 10U
// Період запису "вимірювань" у головному циклі
#define MEASURE_PERIOD_MS  10000

extern "C" void app_main()
{
    vTaskDelay(pdMS_TO_TICKS(2000));   // чекаємо, поки підключиться монітор
    ESP_LOGI(TAG, "Start");

    // 0. Діагностика: у вільному стані обидві лінії мають бути в 1 (підтяжка)
    const gpio_config_t probe_cfg = {
        .pin_bit_mask = (1ULL << I2C_MASTER_SDA_IO) | (1ULL << I2C_MASTER_SCL_IO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&probe_cfg);
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_LOGI(TAG, "Idle levels: SDA=%d SCL=%d",
             gpio_get_level(I2C_MASTER_SDA_IO), gpio_get_level(I2C_MASTER_SCL_IO));

    // 1. Створюємо шину I2C
    i2c_master_bus_handle_t bus = NULL;
    i2c_bus_settings_t bus_cfg = {};
    bus_cfg.port = I2C_MASTER_NUM;
    bus_cfg.sda_pin = I2C_MASTER_SDA_IO;
    bus_cfg.scl_pin = I2C_MASTER_SCL_IO;
    bus_cfg.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_bus_init(&bus_cfg, &bus));

    // Сканування шини: AT24C32 - 0x50, DS1307 - 0x68
    printf("I2C scan:");
    for (uint16_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_bus_probe(bus, addr, 50) == ESP_OK) {
            printf(" 0x%02X", (unsigned int)addr);
        }
    }
    printf("\n");

    // 2. EEPROM AT24C32 (адреса 0x50)
    at24c32_handle_t eeprom = NULL;
    esp_err_t err = at24c32_init(bus, EEPROM_A_PINS, &eeprom);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AT24C32 init failed: %s", esp_err_to_name(err));
        return;
    }

    // 3. Логер: перевіряємо зв'язок з EEPROM, скануємо слоти і відновлюємо позицію останнього логу
    logger_handle_t logger = NULL;
    err = logger_init(eeprom, &logger);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Logger init failed: %s", esp_err_to_name(err));
        return;
    }
    logger_print_info(logger);

    // 4. Запис логів різних рівнів; довгий текст обрізається до 32 байт
    printf("\n--- Write ---\n");
    logger_write(logger, LOGGER_INFO, "Power on");
    logger_write(logger, LOGGER_INFO, "Temp %dC", 25);
    logger_write(logger, LOGGER_WARNING, "Press %dhPa", 1013);
    logger_write(logger, LOGGER_ERROR, "Failed to read memory block");

    // 5. Перший запуск: заповнюємо всі 128 слотів і ще DEMO_OVERFLOW_LOGS зверху,
    //    щоб найстаріші логи перезаписались (ring buffer)
    if (logger_count(logger) < LOGGER_SLOT_COUNT) {
        printf("\n--- Fill ring buffer ---\n");
        const uint16_t to_write = (uint16_t)(LOGGER_SLOT_COUNT - logger_count(logger) + DEMO_OVERFLOW_LOGS);
        for (uint16_t i = 0; i < to_write; i++) {
            logger_write(logger, LOGGER_INFO, "Fill %u", (unsigned int)i);
        }
    }

    // 6. Читання: кількість, перший/останній номер, лог за номером, діапазон, усі логи
    uint16_t first = 0;
    uint16_t last = 0;
    logger_range(logger, &first, &last);

    printf("\n--- Info ---\n");
    logger_print_info(logger);

    printf("\n--- Log by number ---\n");
    logger_print(logger, (uint16_t)((last + LOGGER_NUMBER_MODULO - 3U) % LOGGER_NUMBER_MODULO));
    // Номер перед найстарішим уже перезаписаний -> "not found"
    logger_print(logger, (uint16_t)((first + LOGGER_NUMBER_MODULO - 1U) % LOGGER_NUMBER_MODULO));

    printf("\n--- Range: last 5 ---\n");
    logger_print_range(logger, (uint16_t)((last + LOGGER_NUMBER_MODULO - 4U) % LOGGER_NUMBER_MODULO), last);

    printf("\n--- All ---\n");
    logger_print_all(logger);

    // 7. Головний цикл: періодично пишемо "виміри" температури й тиску
    printf("\n--- Measurements every %d s ---\n", MEASURE_PERIOD_MS / 1000);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MEASURE_PERIOD_MS));
        const int temp = 20 + (int)(esp_random() % 10U);
        const int press = 1000 + (int)(esp_random() % 30U);
        logger_write(logger, LOGGER_INFO, "Temp %dC Press %dhPa", temp, press);
        logger_print_info(logger);
    }
}
