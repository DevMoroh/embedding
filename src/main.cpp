#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "notes.h"
#include "pwm.h"
#include "sine.h"

static const char *TAG = "main";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Audio demo started; PWM output on GPIO 15");
    esp_err_t ret = ESP_OK;

    // Timer callbacks keep using these objects after app_main returns.
    static pwm_t buzzer_pwm = {};
    pwm_config_t buzzer_cfg = {
        .gpio = GPIO_NUM_15,
        .channel = LEDC_CHANNEL_0,
        .timer = LEDC_TIMER_0,
        .frequency_hz = 10000,
        .resolution = LEDC_TIMER_12_BIT,
        .duty = 0,
        .inverted = false
    };

    ret = pwm_init(&buzzer_pwm, &buzzer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "pwm_init failed: %s", esp_err_to_name(ret));
        return;
    }

    // Assignment example: C D E | C D E | C D E.
    // Each entry is {frequency_hz, duration_ms}; durations use 50 ms ticks.
    static const tone_t melody[] = {
        {NOTE_C4, 200},
        {NOTE_D4, 200},
        {NOTE_E4, 400},
        {NOTE_REST, 200},

        {NOTE_C4, 200},
        {NOTE_D4, 200},
        {NOTE_E4, 400},
        {NOTE_REST, 200},

        {NOTE_C4, 200},
        {NOTE_D4, 200},
        {NOTE_E4, 400},
        {NOTE_REST, 3000}, // Silence before repeating the melody.
    };

    static player_t player = {};
    ret = player_init(&player, &buzzer_pwm);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "player_init failed: %s", esp_err_to_name(ret));
        pwm_deinit(&buzzer_pwm);
        return;
    }

    ret = player_play_sequence(&player, melody, sizeof(melody) / sizeof(melody[0]), true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "player_play_sequence failed: %s", esp_err_to_name(ret));
        player_deinit(&player);
        pwm_deinit(&buzzer_pwm);
        return;
    }

    ESP_LOGI(TAG, "Playing C D E | C D E | C D E; app_main returns immediately");
}
