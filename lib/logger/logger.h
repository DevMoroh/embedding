#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <stdint.h>

#include "at24c32.h"
#include "esp_err.h"

/*
 * Кільцевий логер в EEPROM AT24C32.
 * Кожен лог - рівно одна сторінка 32 байти: текст "#<номер> <рівень>: <повідомлення>",
 * доповнений '\0' до 32 байт. Номер логу 0..999, після #999 іде #0.
 * 128 слотів (сторінок); коли всі зайняті, новий лог перезаписує найстаріший.
 */
#define LOGGER_RECORD_SIZE   AT24C32_PAGE_SIZE_BYTES                          /* 32 байти, разом з '\0' */
#define LOGGER_SLOT_COUNT    (AT24C32_SIZE_BYTES / AT24C32_PAGE_SIZE_BYTES)  /* 128 логів */
#define LOGGER_NUMBER_MODULO 1000U                                       /* номери #0..#999 */

typedef enum {
    LOGGER_INFO,
    LOGGER_WARNING,
    LOGGER_ERROR,
} logger_level_t;

/* Непрозорий дескриптор логера. */
typedef struct logger_s *logger_handle_t;

/*
 * Перевіряє зв'язок з EEPROM, сканує всі слоти, знаходить останній записаний лог (з урахуванням
 * переходу #999 -> #0) і встановлює вказівник на наступний запис.
 * eeprom - дескриптор з at24c32_init()
 * logger - [out] дескриптор логера
 */
esp_err_t logger_init(at24c32_handle_t eeprom, logger_handle_t *logger);

/* Звільняє дескриптор логера; EEPROM залишається ініціалізованою. */
esp_err_t logger_deinit(logger_handle_t logger);

/*
 * Форматує повідомлення, додає номер і рівень, записує в наступний слот
 * і виводить рядок через printf(). Текст довший за 31 символ обрізається.
 */
esp_err_t logger_write(logger_handle_t logger, logger_level_t level, const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/*
 * Читає лог з номером number у buffer (не менше LOGGER_RECORD_SIZE байт).
 * Повертає ESP_ERR_NOT_FOUND, якщо такого логу в пам'яті немає.
 */
esp_err_t logger_read(logger_handle_t logger, uint16_t number, char *buffer);

/* Кількість збережених логів (0..128). */
uint16_t logger_count(logger_handle_t logger);

/* Номери найстарішого та останнього логу; false, якщо логів немає. */
bool logger_range(logger_handle_t logger, uint16_t *first, uint16_t *last);

/* Стирає всі слоти (заповнює 0xFF); наступний лог отримає номер #0. */
esp_err_t logger_clear(logger_handle_t logger);

/* Виводить через printf() кількість логів, перший і останній номер та їхні сторінки. */
void logger_print_info(logger_handle_t logger);

/* Виводить лог за номером. */
esp_err_t logger_print(logger_handle_t logger, uint16_t number);

/* Виводить логи з номерами від first до last включно (можливий перехід #999 -> #0). */
esp_err_t logger_print_range(logger_handle_t logger, uint16_t first, uint16_t last);

/* Виводить усі збережені логи від найстарішого до останнього. */
esp_err_t logger_print_all(logger_handle_t logger);

#endif
