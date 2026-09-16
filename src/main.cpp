#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pwm.h"
#include "adc.h"

constexpr gpio_num_t LED_OUT = GPIO_NUM_16;
constexpr gpio_num_t MOTOR_OUT = GPIO_NUM_15;
static const char *TAG = "MAIN";
constexpr adc_channel_t adc_channel_3 = ADC_CHANNEL_3;  // GPIO4
constexpr adc_channel_t adc_channel_4 = ADC_CHANNEL_4;  // GPIO5

extern "C" void app_main() {
    pwm_t led_pwm = {};
    pwm_config_t led_cfg = {
        .gpio = LED_OUT,
        .channel = LEDC_CHANNEL_0,
        .timer = LEDC_TIMER_0,
        .frequency_hz = 1000,
        .resolution = LEDC_TIMER_8_BIT,
        .duty = 0,
        .inverted = false,
    };

    ESP_ERROR_CHECK(pwm_init(&led_pwm, &led_cfg));

    pwm_t motor_pwm = {};
    pwm_config_t motor_cfg = {
        .gpio = MOTOR_OUT,
        .channel = LEDC_CHANNEL_1,
        .timer = LEDC_TIMER_1,
        .frequency_hz = 1000,
        .resolution = LEDC_TIMER_8_BIT,
        .duty = 0,
        .inverted = false,
    };

    ESP_ERROR_CHECK(pwm_init(&motor_pwm, &motor_cfg));

       // Oneshot context for ADC_UNIT_1
    adc_oneshot_ctx_t adc_ctx;
    adc_oneshot_init(&adc_ctx,
                     ADC_UNIT_1,
                     ADC_RTC_CLK_SRC_DEFAULT,
                     ADC_ULP_MODE_DISABLE);

    // Channel 1 configuration (GPIO4 on ESP32-S3)
    adc_oneshot_config(&adc_ctx,
                       adc_channel_3,
                       ADC_ATTEN_DB_12,
                       ADC_BITWIDTH_DEFAULT,
                       true);

    // Channel 2 configuration (GPIO5 on ESP32-S3)
    adc_oneshot_config(&adc_ctx,
                       adc_channel_4,
                       ADC_ATTEN_DB_12,
                       ADC_BITWIDTH_DEFAULT,
                       true);

    while (1) {

        int raw_ch1 = 0;
        int volt_ch1 = 0;

        adc_oneshot_read_raw(&adc_ctx, adc_channel_3, &raw_ch1);
        adc_oneshot_read_voltage(&adc_ctx, adc_channel_3, &volt_ch1);
        
        int percent_ch_1 = (volt_ch1 * 100) / 3300;
        if (percent_ch_1 < 0) { percent_ch_1 = 0; }
        if (percent_ch_1 > 100) { percent_ch_1 = 100; }

        ESP_ERROR_CHECK(pwm_set_percent(&led_pwm, (uint8_t)percent_ch_1));

        // Channel 2 measurements
        int raw_ch2 = 0;
        int volt_ch2 = 0;
        adc_oneshot_read_raw(&adc_ctx, adc_channel_4, &raw_ch2);
        adc_oneshot_read_voltage(&adc_ctx, adc_channel_4, &volt_ch2);
        
        int percent_ch_2 = (volt_ch2 * 100) / 3300;
        if (percent_ch_2 < 0) { percent_ch_2 = 0; }
        if (percent_ch_2 > 100) { percent_ch_2 = 100; }

        ESP_ERROR_CHECK(pwm_set_percent(&motor_pwm, (uint8_t)percent_ch_2));


        ESP_LOGI(TAG, "CH%d: Raw=%d, Volt=%d mV | CH%d: Raw=%d, Volt=%d mV",
                 adc_channel_3, raw_ch1, volt_ch1,
                 adc_channel_4, raw_ch2, volt_ch2);
        printf("PWM check: LED=%d%%, MOTOR=%d%%\n", percent_ch_1, percent_ch_2);
        vTaskDelay(pdMS_TO_TICKS(200));    
    }
}
