#ifndef AT24C32_DRIVER_H
#define AT24C32_DRIVER_H

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* AT24C32: 32 Кбіт = 4096 байт, 128 сторінок по 32 байти, 16-бітна адреса комірки. */
#define AT24C32_I2C_ADDRESS_DEFAULT 0x50U /* A2 = A1 = A0 = 0 */
#define AT24C32_SIZE                4096U
#define AT24C32_PAGE_SIZE           32U
#define AT24C32_PAGE_COUNT          (AT24C32_SIZE / AT24C32_PAGE_SIZE)

/* Непрозорий дескриптор AT24C32. */
typedef struct at24c32_s *at24c32_handle_t;

/*
 * Перевіряє відповідь мікросхеми на шині та додає її як пристрій I2C (100 кГц).
 * bus     - дескриптор шини з i2c_bus_init()
 * address - I2C-адреса 0x50..0x57 залежно від ніжок A0-A2
 * eeprom  - [out] дескриптор створеного пристрою
 * Повертає ESP_ERR_NOT_FOUND, якщо EEPROM не відповідає.
 */
esp_err_t at24c32_init(i2c_master_bus_handle_t bus, uint16_t address, at24c32_handle_t *eeprom);

/* Видаляє пристрій; шина I2C залишається відкритою. */
esp_err_t at24c32_deinit(at24c32_handle_t eeprom);

/* Читає 32-байтну сторінку page (0..127) в буфер data. */
esp_err_t at24c32_read_page(at24c32_handle_t eeprom, uint16_t page, uint8_t *data);

/*
 * Записує 32 байти в сторінку page (0..127) і чекає завершення
 * внутрішнього циклу запису (до 10 мс за даташитом).
 */
esp_err_t at24c32_write_page(at24c32_handle_t eeprom, uint16_t page, const uint8_t *data);

/* Послідовне читання size байт з адреси address (з переходом через межі сторінок). */
esp_err_t at24c32_read(at24c32_handle_t eeprom, uint16_t address, uint8_t *data, size_t size);

#endif
