#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "uart.h"
#include "button.h"
#include "rgb_led.h"

/* BOOT button on YD-ESP32-S3: GPIO0, pressed = LOW. */
#define BUTTON_GPIO GPIO_NUM_0
#define BUTTON_DEBOUNCE_MS 30U

/* Onboard WS2812 RGB LED. */
#define RGB_LED_GPIO GPIO_NUM_48
#define RGB_ON_RED 0
#define RGB_ON_GREEN 40
#define RGB_ON_BLUE 0

/* Commands shared with STM32: one ASCII byte. */
#define CMD_LED_ON '1'
#define CMD_LED_OFF '0'

#define LOOP_PERIOD_TICKS 1  // 10 ms at CONFIG_FREERTOS_HZ=100

static const char *TAG = "app";

typedef struct {
    bool remote_led_on;  // state of the LED on the STM32 side
} ButtonContext_t;

static button_t button;
static ButtonContext_t button_ctx = {.remote_led_on = false};

/* Called from button_update() in the main task: logging and UART are safe here. */
static void on_button_pressed(void *arg)
{
    ButtonContext_t *ctx = (ButtonContext_t *)arg;

    ctx->remote_led_on = !ctx->remote_led_on;
    const uint8_t command = ctx->remote_led_on ? CMD_LED_ON : CMD_LED_OFF;

    const esp_err_t err = uart_transmit(&command, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Button -> TX '%c' (STM32 LED %s)", command,
                 ctx->remote_led_on ? "ON" : "OFF");
    } else {
        ESP_LOGE(TAG, "UART transmit error: %s", esp_err_to_name(err));
    }
}

static void handle_command(uint8_t command)
{
    switch (command) {
        case CMD_LED_ON:
            rgb_led_set(RGB_ON_RED, RGB_ON_GREEN, RGB_ON_BLUE);
            ESP_LOGI(TAG, "RX '%c' -> RGB ON", command);
            break;
        case CMD_LED_OFF:
            rgb_led_set(0, 0, 0);
            ESP_LOGI(TAG, "RX '%c' -> RGB OFF", command);
            break;
        default:
            ESP_LOGW(TAG, "RX unknown byte 0x%02X, ignored", (unsigned int)command);
            break;
    }
}

extern "C" void app_main()
{
    esp_err_t err = uart_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART initialization failed: %s", esp_err_to_name(err));
        return;
    }

    err = rgb_led_init(RGB_LED_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RGB LED initialization failed: %s", esp_err_to_name(err));
        return;
    }

    err = button_init(&button, BUTTON_GPIO, BUTTON_DEBOUNCE_MS, on_button_pressed, NULL, &button_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Ready: STM32 button -> RGB LED, BOOT button -> STM32 LED");

    uint8_t received_byte = 0;
    while (1) {
        button_update(&button);

        /* Timeout 0: take what is already in the RX buffer, do not block the button FSM. */
        err = uart_receive(&received_byte, 1, 0);
        if (err == ESP_OK) {
            handle_command(received_byte);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "UART receive error: %s", esp_err_to_name(err));
        }

        vTaskDelay(LOOP_PERIOD_TICKS);
    }
}
