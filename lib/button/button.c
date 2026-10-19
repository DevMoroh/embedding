#include "button.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "button";

static uint32_t button_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

esp_err_t button_init(button_t *button, gpio_num_t gpio, uint32_t debounce_ms,
                      button_callback_t pressed_cb, button_callback_t released_cb, void *arg)
{
    if (button == NULL || !GPIO_IS_VALID_GPIO(gpio)) {
        ESP_LOGE(TAG, "Invalid button arguments");
        return ESP_ERR_INVALID_ARG;
    }

    button->gpio = gpio;
    button->state = BUTTON_STATE_IDLE;
    button->debounce_ms = debounce_ms;
    button->last_change_ms = 0;
    button->pressed_cb = pressed_cb;
    button->released_cb = released_cb;
    button->arg = arg;

    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    const esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO%d configuration failed: %s", gpio, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Button ready: GPIO%d, debounce %lu ms", gpio, (unsigned long)debounce_ms);
    return ESP_OK;
}

esp_err_t button_update(button_t *button)
{
    if (button == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool is_low = (gpio_get_level(button->gpio) == 0);
    const uint32_t now = button_now_ms();

    switch (button->state) {
        case BUTTON_STATE_IDLE:
            if (is_low) {
                button->state = BUTTON_STATE_DEBOUNCE_PRESS;
                button->last_change_ms = now;
            }
            break;

        case BUTTON_STATE_DEBOUNCE_PRESS:
            if ((now - button->last_change_ms) >= button->debounce_ms) {
                if (is_low) {
                    button->state = BUTTON_STATE_PRESSED;
                    if (button->pressed_cb != NULL) {
                        button->pressed_cb(button->arg);
                    }
                } else {
                    /* Short glitch, not a real press. */
                    button->state = BUTTON_STATE_IDLE;
                }
            }
            break;

        case BUTTON_STATE_PRESSED:
            if (!is_low) {
                button->state = BUTTON_STATE_DEBOUNCE_RELEASE;
                button->last_change_ms = now;
            }
            break;

        case BUTTON_STATE_DEBOUNCE_RELEASE:
            if ((now - button->last_change_ms) >= button->debounce_ms) {
                if (!is_low) {
                    button->state = BUTTON_STATE_IDLE;
                    if (button->released_cb != NULL) {
                        button->released_cb(button->arg);
                    }
                } else {
                    /* Bounce on release, the button is still held. */
                    button->state = BUTTON_STATE_PRESSED;
                }
            }
            break;

        default:
            button->state = BUTTON_STATE_IDLE;
            break;
    }

    return ESP_OK;
}

esp_err_t button_is_pressed(const button_t *button, bool *pressed)
{
    if (button == NULL || pressed == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *pressed = (button->state == BUTTON_STATE_PRESSED) ||
               (button->state == BUTTON_STATE_DEBOUNCE_RELEASE);
    return ESP_OK;
}
