#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Button debounce FSM (ESP-IDF port of buttonFSM from esp32_button_debounce_FSM).
 * The button shorts the pin to GND, the pin uses the internal pull-up: pressed = LOW.
 * Call button_update() periodically from a task, not from an ISR.
 */

typedef enum {
    BUTTON_STATE_IDLE = 0,
    BUTTON_STATE_DEBOUNCE_PRESS,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_DEBOUNCE_RELEASE
} button_state_t;

typedef void (*button_callback_t)(void *arg);

typedef struct {
    gpio_num_t gpio;
    button_state_t state;
    uint32_t debounce_ms;
    uint32_t last_change_ms;
    button_callback_t pressed_cb;
    button_callback_t released_cb;
    void *arg;
} button_t;

esp_err_t button_init(button_t *button, gpio_num_t gpio, uint32_t debounce_ms,
                      button_callback_t pressed_cb, button_callback_t released_cb, void *arg);
esp_err_t button_update(button_t *button);
esp_err_t button_is_pressed(const button_t *button, bool *pressed);

#ifdef __cplusplus
}
#endif

#endif
