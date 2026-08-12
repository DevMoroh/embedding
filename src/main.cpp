#include <Arduino.h>

#define BUTTON_IN 16
#define DEBOUNCE_US 30000

volatile uint32_t button_counter = 0;
volatile uint32_t last_isr_us = 0;
uint32_t last_count = 0;

void IRAM_ATTR button_isr() {
    uint32_t now = micros();

    if (now - last_isr_us >= DEBOUNCE_US) {
        button_counter++;
        last_isr_us = now;
    }
}

void setup() {
    pinMode(BUTTON_IN, INPUT_PULLUP);
    Serial.begin(115200);

    // Trigger interrupt on falling edge (button press for pull-up wiring).
    attachInterrupt(digitalPinToInterrupt(BUTTON_IN), button_isr, FALLING);
}

void loop() {
    if (button_counter != last_count) {
        last_count = button_counter;
        Serial.print("Button Pressed! Count: ");
        Serial.println(last_count);
    }

    delay(10);
}
