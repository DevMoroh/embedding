#include "at24c32.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "i2c.h"

#define AT24C32_I2C_SPEED_HZ      100000U
#define AT24C32_I2C_TIMEOUT_MS    100U
#define AT24C32_WRITE_CYCLE_US    20000  /* tWR = 10 мс, беремо із запасом */
#define AT24C32_POLL_INTERVAL_US  500U

static const char *TAG = "at24c32";

struct at24c32_s {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    uint16_t address;
};

/*
 * Під час внутрішнього циклу запису EEPROM не відповідає (NACK) на свою адресу.
 * Опитуємо її (ACK polling), доки не відповість, але не довше AT24C32_WRITE_CYCLE_US.
 */
static esp_err_t wait_write_complete(at24c32_handle_t eeprom)
{
    const int64_t deadline = esp_timer_get_time() + AT24C32_WRITE_CYCLE_US;
    do {
        esp_rom_delay_us(AT24C32_POLL_INTERVAL_US);
        if (i2c_bus_probe(eeprom->bus, eeprom->address, AT24C32_I2C_TIMEOUT_MS) == ESP_OK) {
            return ESP_OK;
        }
    } while (esp_timer_get_time() < deadline);

    ESP_LOGE(TAG, "Write cycle timeout");
    return ESP_ERR_TIMEOUT;
}

/* Створює дескриптор AT24C32 на шині I2C. */
esp_err_t at24c32_init(i2c_master_bus_handle_t bus, uint16_t address, at24c32_handle_t *eeprom)
{
    if (bus == NULL || eeprom == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *eeprom = NULL;

    // Перевірка зв'язку: мікросхема має підтвердити свою адресу
    esp_err_t err = i2c_bus_probe(bus, address, AT24C32_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No ACK from 0x%02X: %s", (unsigned int)address, esp_err_to_name(err));
        return ESP_ERR_NOT_FOUND;
    }

    struct at24c32_s *ctx = (struct at24c32_s *)calloc(1U, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_device_settings_t dev_cfg = {};
    dev_cfg.address = address;
    dev_cfg.scl_speed_hz = AT24C32_I2C_SPEED_HZ;
    err = i2c_device_add(bus, &dev_cfg, &ctx->device);
    if (err != ESP_OK) {
        free(ctx);
        return err;
    }

    ctx->bus = bus;
    ctx->address = address;
    *eeprom = ctx;
    ESP_LOGI(TAG, "AT24C32 ready at 0x%02X", (unsigned int)address);
    return ESP_OK;
}

/* Видаляє пристрій і звільняє дескриптор. */
esp_err_t at24c32_deinit(at24c32_handle_t eeprom)
{
    if (eeprom == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t err = i2c_device_remove(eeprom->device);
    free(eeprom);
    return err;
}

/* Послідовне читання: адреса комірки (старший, молодший байт), потім дані. */
esp_err_t at24c32_read(at24c32_handle_t eeprom, uint16_t address, uint8_t *data, size_t size)
{
    if (eeprom == NULL || data == NULL || size == 0U ||
        (size_t)address + size > AT24C32_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t addr[2] = {(uint8_t)(address >> 8), (uint8_t)(address & 0xFFU)};
    const esp_err_t err = i2c_write_read(eeprom->device, addr, sizeof(addr),
                                         data, size, AT24C32_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Read 0x%03X failed: %s", (unsigned int)address, esp_err_to_name(err));
    }
    return err;
}

/* Читає одну сторінку, адреса вирівняна на 32 байти. */
esp_err_t at24c32_read_page(at24c32_handle_t eeprom, uint16_t page, uint8_t *data)
{
    if (page >= AT24C32_PAGE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    return at24c32_read(eeprom, (uint16_t)(page * AT24C32_PAGE_SIZE), data, AT24C32_PAGE_SIZE);
}

/* Page write: адреса комірки + 32 байти даних однією транзакцією. */
esp_err_t at24c32_write_page(at24c32_handle_t eeprom, uint16_t page, const uint8_t *data)
{
    if (eeprom == NULL || data == NULL || page >= AT24C32_PAGE_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t address = (uint16_t)(page * AT24C32_PAGE_SIZE);
    uint8_t buffer[2U + AT24C32_PAGE_SIZE];
    buffer[0] = (uint8_t)(address >> 8);
    buffer[1] = (uint8_t)(address & 0xFFU);
    memcpy(&buffer[2], data, AT24C32_PAGE_SIZE);

    const esp_err_t err = i2c_write(eeprom->device, buffer, sizeof(buffer), AT24C32_I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Write page %u failed: %s", (unsigned int)page, esp_err_to_name(err));
        return err;
    }
    return wait_write_complete(eeprom);
}
