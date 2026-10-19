#ifndef RGB_LED_DRIVER_H
#define RGB_LED_DRIVER_H

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Single WS2812 RGB LED driven by the RMT peripheral (YD-ESP32-S3 onboard LED: GPIO48). */
esp_err_t rgb_led_init(gpio_num_t gpio);
esp_err_t rgb_led_set(uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
}
#endif

#endif
