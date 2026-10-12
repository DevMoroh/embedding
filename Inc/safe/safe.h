#ifndef SAFE_H
#define SAFE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Логіка "сейфа" без HAL - тільки цифри, напрямки і спроби, тому її можна
 * зібрати і перевірити на ПК.
 *
 * Введення коду:
 *  - тік в одному напрямку: поточна цифра +1 (9 -> 0 по колу);
 *  - перший тік у протилежному напрямку: поточна цифра підтверджена, наступна = 0;
 *  - зміна напрямку після останньої цифри: перевірка коду;
 *  - кнопка: скидання введеного коду, це теж витрачена спроба.
 */

#define SAFE_CODE_LENGTH 4
#define SAFE_MAX_ATTEMPTS 3

typedef enum {
    SAFE_READY,     // чекаємо перший тік нової спроби
    SAFE_ENTERING,  // набираємо цифри
    SAFE_OPEN,      // код вірний
    SAFE_LOCKED,    // спроби вичерпано, до перезавантаження
} SafeState_t;

typedef enum {
    SAFE_EVENT_NONE,
    SAFE_EVENT_FIRST_DIGIT,    // почалася спроба, digits[0] = 1
    SAFE_EVENT_DIGIT_CHANGED,  // поточна цифра змінилась
    SAFE_EVENT_NEXT_DIGIT,     // попередню цифру підтверджено, нова = 0
    SAFE_EVENT_WRONG,          // код невірний, спробу витрачено
    SAFE_EVENT_RESET,          // кнопка, спробу витрачено
    SAFE_EVENT_OPENED,         // код вірний
    SAFE_EVENT_CLOSED,         // кнопка у стані OPEN: закрити сейф
    SAFE_EVENT_LOCKED,         // остання спроба витрачена
} SafeEvent_t;

typedef struct {
    uint8_t code[SAFE_CODE_LENGTH];    // секретний код
    uint8_t digits[SAFE_CODE_LENGTH];  // введені цифри
    uint8_t digit_index;               // яку цифру зараз набираємо
    int8_t direction;                  // +1 / -1, напрямок поточної цифри
    uint8_t attempts_left;
    SafeState_t state;
} Safe_t;

void Safe_Init(Safe_t *safe, const uint8_t code[SAFE_CODE_LENGTH]);
SafeEvent_t Safe_OnTick(Safe_t *safe, int8_t direction);
SafeEvent_t Safe_OnButton(Safe_t *safe);

#endif
