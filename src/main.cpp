#include <Arduino.h>
#include "ledControl.h"

constexpr uint8_t LED_1_PIN = 16;
constexpr uint8_t LED_2_PIN = 17;
constexpr uint8_t LED_3_PIN = 11;

static ledControl_t led_1;
static ledControl_t led_2;
static ledControl_t led_3;

void setup() {
    Serial.begin(115200);

    ledControl_init(&led_1, LED_1_PIN, true, LED_MODE_BLINK, 200, 200);
    ledControl_init(&led_2, LED_2_PIN, true, LED_MODE_BLINK, 500, 500);
    ledControl_init(&led_3, LED_3_PIN, true, LED_MODE_BLINK, 1000, 1000);

}

void loop() {

    ledControl_update(&led_1);
    ledControl_update(&led_2);
    ledControl_update(&led_3);

}