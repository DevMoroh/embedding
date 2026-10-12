#include "safe/safe.h"

#include <stddef.h>
#include <string.h>

static void Safe_StartAttempt(Safe_t *safe) {
    memset(safe->digits, 0, sizeof(safe->digits));
    safe->digit_index = 0;
    safe->direction = 0;
    safe->state = SAFE_READY;
}

/* Wrong code and reset both cost one attempt. */
static SafeEvent_t Safe_UseAttempt(Safe_t *safe, SafeEvent_t event) {
    if (safe->attempts_left > 0) {
        safe->attempts_left--;
    }

    if (safe->attempts_left == 0) {
        safe->state = SAFE_LOCKED;
        return SAFE_EVENT_LOCKED;
    }

    Safe_StartAttempt(safe);
    return event;
}

static SafeEvent_t Safe_Check(Safe_t *safe) {
    if (memcmp(safe->digits, safe->code, SAFE_CODE_LENGTH) == 0) {
        safe->state = SAFE_OPEN;
        return SAFE_EVENT_OPENED;
    }
    return Safe_UseAttempt(safe, SAFE_EVENT_WRONG);
}

void Safe_Init(Safe_t *safe, const uint8_t code[SAFE_CODE_LENGTH]) {
    if (safe == NULL || code == NULL) {
        return;
    }

    memcpy(safe->code, code, SAFE_CODE_LENGTH);
    safe->attempts_left = SAFE_MAX_ATTEMPTS;
    Safe_StartAttempt(safe);
}

SafeEvent_t Safe_OnTick(Safe_t *safe, int8_t direction) {
    if (safe == NULL || direction == 0) {
        return SAFE_EVENT_NONE;
    }

    if (safe->state == SAFE_READY) {
        /* The very first tick picks the direction and counts as +1. */
        safe->direction = direction;
        safe->digit_index = 0;
        safe->digits[0] = 1;
        safe->state = SAFE_ENTERING;
        return SAFE_EVENT_FIRST_DIGIT;
    }

    if (safe->state != SAFE_ENTERING) {
        return SAFE_EVENT_NONE;
    }

    if (direction == safe->direction) {
        uint8_t *digit = &safe->digits[safe->digit_index];
        *digit = (uint8_t)((*digit + 1U) % 10U);
        return SAFE_EVENT_DIGIT_CHANGED;
    }

    /* Direction changed: confirm the current digit. */
    safe->direction = direction;
    if (safe->digit_index == SAFE_CODE_LENGTH - 1) {
        return Safe_Check(safe);
    }

    safe->digit_index++;
    safe->digits[safe->digit_index] = 0;
    return SAFE_EVENT_NEXT_DIGIT;
}

SafeEvent_t Safe_OnButton(Safe_t *safe) {
    if (safe == NULL) {
        return SAFE_EVENT_NONE;
    }

    switch (safe->state) {
        case SAFE_READY:
        case SAFE_ENTERING:
            return Safe_UseAttempt(safe, SAFE_EVENT_RESET);
        case SAFE_OPEN:
            safe->attempts_left = SAFE_MAX_ATTEMPTS;
            Safe_StartAttempt(safe);
            return SAFE_EVENT_CLOSED;
        default:
            return SAFE_EVENT_NONE;
    }
}
