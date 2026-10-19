#include "rgb_led.h"

#include <stddef.h>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

/* 10 MHz -> one RMT tick = 0.1 us. WS2812: bit0 = 0.3 us HIGH + 0.9 us LOW,
 * bit1 = 0.9 us HIGH + 0.3 us LOW. */
#define RGB_LED_RESOLUTION_HZ 10000000
#define RGB_LED_T0H_TICKS 3
#define RGB_LED_T0L_TICKS 9
#define RGB_LED_T1H_TICKS 9
#define RGB_LED_T1L_TICKS 3
#define RGB_LED_TX_TIMEOUT_MS 100

static const char *TAG = "rgb_led";

static rmt_channel_handle_t rgb_channel = NULL;
static rmt_encoder_handle_t rgb_encoder = NULL;

esp_err_t rgb_led_init(gpio_num_t gpio)
{
    const rmt_tx_channel_config_t channel_config = {
        .gpio_num = gpio,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RGB_LED_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    esp_err_t err = rmt_new_tx_channel(&channel_config, &rgb_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT channel creation failed: %s", esp_err_to_name(err));
        return err;
    }

    const rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = {
            .duration0 = RGB_LED_T0H_TICKS,
            .level0 = 1,
            .duration1 = RGB_LED_T0L_TICKS,
            .level1 = 0,
        },
        .bit1 = {
            .duration0 = RGB_LED_T1H_TICKS,
            .level0 = 1,
            .duration1 = RGB_LED_T1L_TICKS,
            .level1 = 0,
        },
        .flags.msb_first = 1,
    };

    err = rmt_new_bytes_encoder(&encoder_config, &rgb_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT encoder creation failed: %s", esp_err_to_name(err));
        return err;
    }

    err = rmt_enable(rgb_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT enable failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "RGB LED ready: GPIO%d", gpio);
    return rgb_led_set(0, 0, 0);
}

esp_err_t rgb_led_set(uint8_t red, uint8_t green, uint8_t blue)
{
    if (rgb_channel == NULL || rgb_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* WS2812 expects green first. */
    const uint8_t grb[3] = {green, red, blue};
    const rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };

    esp_err_t err = rmt_transmit(rgb_channel, rgb_encoder, grb, sizeof(grb), &transmit_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT transmit failed: %s", esp_err_to_name(err));
        return err;
    }

    return rmt_tx_wait_all_done(rgb_channel, RGB_LED_TX_TIMEOUT_MS);
}
