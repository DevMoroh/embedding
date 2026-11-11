#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "logger";

static const char *const LEVEL_NAMES[] = {"Info", "Warning", "Error"};

struct logger_s {
    at24c32_handle_t eeprom;
    uint16_t count;       /* кількість логів у кільці, 0..128 */
    uint16_t last_slot;   /* сторінка останнього логу (дійсна, якщо count > 0) */
    uint16_t last_number; /* номер останнього логу (дійсний, якщо count > 0) */
};

/* Номер, що йде після number: ... #998 -> #999 -> #0 -> #1 ... */
static uint16_t next_number(uint16_t number)
{
    return (uint16_t)((number + 1U) % LOGGER_NUMBER_MODULO);
}

static uint16_t next_slot(uint16_t slot)
{
    return (uint16_t)((slot + 1U) % LOGGER_SLOT_COUNT);
}

static uint16_t prev_slot(uint16_t slot)
{
    return (uint16_t)((slot + LOGGER_SLOT_COUNT - 1U) % LOGGER_SLOT_COUNT);
}

/*
 * Розбирає запис слота: "#<1-3 цифри> ...", обов'язково з '\0' в межах 32 байт.
 * Чиста сторінка (0xFF) або сміття - не лог.
 */
static bool parse_number(const uint8_t *record, uint16_t *number)
{
    if (record[0] != '#' || memchr(record, '\0', LOGGER_RECORD_SIZE) == NULL) {
        return false;
    }

    uint16_t value = 0;
    size_t i = 1;
    while (i <= 3U && record[i] >= '0' && record[i] <= '9') {
        value = (uint16_t)(value * 10U + (record[i] - '0'));
        i++;
    }
    if (i == 1U || record[i] != ' ') {
        return false;
    }

    *number = value;
    return true;
}

/* Найстаріший лог: на count - 1 кроків назад від останнього. */
static uint16_t first_slot(const struct logger_s *ctx)
{
    return (uint16_t)((ctx->last_slot + LOGGER_SLOT_COUNT - (ctx->count - 1U)) % LOGGER_SLOT_COUNT);
}

static uint16_t first_number(const struct logger_s *ctx)
{
    return (uint16_t)((ctx->last_number + LOGGER_NUMBER_MODULO - (ctx->count - 1U)) % LOGGER_NUMBER_MODULO);
}

/* Сторінка, в якій лежить лог з номером number; false, якщо його немає в кільці. */
static bool slot_of(const struct logger_s *ctx, uint16_t number, uint16_t *slot)
{
    if (ctx->count == 0U || number >= LOGGER_NUMBER_MODULO) {
        return false;
    }

    const uint16_t offset = (uint16_t)((number + LOGGER_NUMBER_MODULO - first_number(ctx)) % LOGGER_NUMBER_MODULO);
    if (offset >= ctx->count) {
        return false;
    }

    *slot = (uint16_t)((first_slot(ctx) + offset) % LOGGER_SLOT_COUNT);
    return true;
}

/*
 * Пошук останнього логу.
 * Логи пишуться в сусідні слоти з номерами, що зростають на 1 (по модулю 1000).
 * Тому останній лог - це слот, за яким іде порожній слот або слот, номер якого
 * не є продовженням (там лежить старий лог, записаний 128 логів тому).
 * Оскільки 1000 не ділиться на 128, повне коло з 128 послідовних номерів неможливе,
 * і такий слот завжди існує. Якщо кандидатів кілька (пошкоджені дані) -
 * беремо той, у якого найдовший ланцюжок попередніх логів.
 */
static esp_err_t scan(struct logger_s *ctx)
{
    uint16_t numbers[LOGGER_SLOT_COUNT];
    bool valid[LOGGER_SLOT_COUNT];
    uint8_t record[LOGGER_RECORD_SIZE];

    for (uint16_t slot = 0; slot < LOGGER_SLOT_COUNT; slot++) {
        const esp_err_t err = at24c32_read_page(ctx->eeprom, slot, record);
        if (err != ESP_OK) {
            return err;
        }
        valid[slot] = parse_number(record, &numbers[slot]);
    }

    ctx->count = 0;
    uint16_t candidates = 0;
    for (uint16_t slot = 0; slot < LOGGER_SLOT_COUNT; slot++) {
        const uint16_t next = next_slot(slot);
        if (!valid[slot] || (valid[next] && numbers[next] == next_number(numbers[slot]))) {
            continue;
        }

        // Довжина ланцюжка послідовних номерів назад від цього слота
        uint16_t length = 1;
        uint16_t current = slot;
        while (length < LOGGER_SLOT_COUNT) {
            const uint16_t prev = prev_slot(current);
            if (!valid[prev] || next_number(numbers[prev]) != numbers[current]) {
                break;
            }
            current = prev;
            length++;
        }

        candidates++;
        if (length > ctx->count) {
            ctx->count = length;
            ctx->last_slot = slot;
            ctx->last_number = numbers[slot];
        }
    }

    if (candidates > 1U) {
        ESP_LOGW(TAG, "Found %u log chains, using the longest one", (unsigned int)candidates);
    }
    return ESP_OK;
}

esp_err_t logger_init(at24c32_handle_t eeprom, logger_handle_t *logger)
{
    if (eeprom == NULL || logger == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *logger = NULL;

    struct logger_s *ctx = (struct logger_s *)calloc(1U, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    ctx->eeprom = eeprom;

    const esp_err_t err = scan(ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM scan failed: %s", esp_err_to_name(err));
        free(ctx);
        return err;
    }

    if (ctx->count == 0U) {
        ESP_LOGI(TAG, "No logs found, next log #0 -> page 0 (0x000)");
    } else {
        const uint16_t slot = next_slot(ctx->last_slot);
        ESP_LOGI(TAG, "Last log #%u at page %u (0x%03X), next log #%u -> page %u (0x%03X)",
                 (unsigned int)ctx->last_number, (unsigned int)ctx->last_slot,
                 (unsigned int)(ctx->last_slot * LOGGER_RECORD_SIZE),
                 (unsigned int)next_number(ctx->last_number), (unsigned int)slot,
                 (unsigned int)(slot * LOGGER_RECORD_SIZE));
    }

    *logger = ctx;
    return ESP_OK;
}

esp_err_t logger_deinit(logger_handle_t logger)
{
    if (logger == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    free(logger);
    return ESP_OK;
}

esp_err_t logger_write(logger_handle_t logger, logger_level_t level, const char *format, ...)
{
    if (logger == NULL || format == NULL || level > LOGGER_ERROR) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t slot = (logger->count == 0U) ? 0U : next_slot(logger->last_slot);
    const uint16_t number = (logger->count == 0U) ? 0U : next_number(logger->last_number);

    // Нулі до кінця сторінки: "#185 Info: Temp 25C\0....\0"
    char record[LOGGER_RECORD_SIZE] = {};
    int length = snprintf(record, sizeof(record), "#%u %s: ", (unsigned int)number, LEVEL_NAMES[level]);
    if (length < (int)sizeof(record)) {
        va_list args;
        va_start(args, format);
        vsnprintf(&record[length], sizeof(record) - (size_t)length, format, args);
        va_end(args);
    }

    const esp_err_t err = at24c32_write_page(logger->eeprom, slot, (const uint8_t *)record);
    if (err != ESP_OK) {
        return err;
    }

    logger->last_slot = slot;
    logger->last_number = number;
    if (logger->count < LOGGER_SLOT_COUNT) {
        logger->count++;
    }

    printf("%s\n", record);
    return ESP_OK;
}

esp_err_t logger_read(logger_handle_t logger, uint16_t number, char *buffer)
{
    if (logger == NULL || buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t slot = 0;
    if (!slot_of(logger, number, &slot)) {
        return ESP_ERR_NOT_FOUND;
    }

    const esp_err_t err = at24c32_read_page(logger->eeprom, slot, (uint8_t *)buffer);
    if (err != ESP_OK) {
        return err;
    }

    // Захист від розсинхронізації з вмістом EEPROM
    uint16_t stored = 0;
    if (!parse_number((const uint8_t *)buffer, &stored) || stored != number) {
        ESP_LOGE(TAG, "Page %u does not contain log #%u", (unsigned int)slot, (unsigned int)number);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

uint16_t logger_count(logger_handle_t logger)
{
    return (logger == NULL) ? 0U : logger->count;
}

bool logger_range(logger_handle_t logger, uint16_t *first, uint16_t *last)
{
    if (logger == NULL || logger->count == 0U) {
        return false;
    }
    if (first != NULL) {
        *first = first_number(logger);
    }
    if (last != NULL) {
        *last = logger->last_number;
    }
    return true;
}

esp_err_t logger_clear(logger_handle_t logger)
{
    if (logger == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t empty[LOGGER_RECORD_SIZE];
    memset(empty, 0xFF, sizeof(empty));
    for (uint16_t slot = 0; slot < LOGGER_SLOT_COUNT; slot++) {
        const esp_err_t err = at24c32_write_page(logger->eeprom, slot, empty);
        if (err != ESP_OK) {
            return err;
        }
    }

    logger->count = 0;
    ESP_LOGI(TAG, "All %u pages erased", (unsigned int)LOGGER_SLOT_COUNT);
    return ESP_OK;
}

void logger_print_info(logger_handle_t logger)
{
    if (logger == NULL || logger->count == 0U) {
        printf("Logs: 0 of %u\n", (unsigned int)LOGGER_SLOT_COUNT);
        return;
    }

    printf("Logs: %u of %u, first #%u (page %u), last #%u (page %u)\n",
           (unsigned int)logger->count, (unsigned int)LOGGER_SLOT_COUNT,
           (unsigned int)first_number(logger), (unsigned int)first_slot(logger),
           (unsigned int)logger->last_number, (unsigned int)logger->last_slot);
}

esp_err_t logger_print(logger_handle_t logger, uint16_t number)
{
    char record[LOGGER_RECORD_SIZE];
    const esp_err_t err = logger_read(logger, number, record);
    if (err == ESP_OK) {
        printf("%s\n", record);
    } else {
        printf("#%u: %s\n", (unsigned int)number,
               (err == ESP_ERR_NOT_FOUND) ? "not found" : esp_err_to_name(err));
    }
    return err;
}

esp_err_t logger_print_range(logger_handle_t logger, uint16_t first, uint16_t last)
{
    if (logger == NULL || first >= LOGGER_NUMBER_MODULO || last >= LOGGER_NUMBER_MODULO) {
        return ESP_ERR_INVALID_ARG;
    }

    // Ідемо від first до last по колу номерів; відсутні в пам'яті номери пропускаємо
    printf("Logs #%u..#%u:\n", (unsigned int)first, (unsigned int)last);
    uint16_t slot = 0;
    for (uint16_t number = first;; number = next_number(number)) {
        if (slot_of(logger, number, &slot)) {
            const esp_err_t err = logger_print(logger, number);
            if (err != ESP_OK) {
                return err;
            }
        }
        if (number == last) {
            break;
        }
    }
    return ESP_OK;
}

esp_err_t logger_print_all(logger_handle_t logger)
{
    if (logger == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t first = 0;
    uint16_t last = 0;
    if (!logger_range(logger, &first, &last)) {
        printf("No logs\n");
        return ESP_OK;
    }
    return logger_print_range(logger, first, last);
}
