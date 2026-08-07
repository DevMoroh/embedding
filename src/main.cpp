#include <Arduino.h>

#define RED_LED_OUT 15
#define BLUE_LED_OUT 16

bool ledState = false;

void blinkLed(int pinLow, int pinHigh) {
    digitalWrite(pinLow, LOW);
    digitalWrite(pinHigh, HIGH);
    delay(200);
    digitalWrite(pinHigh, LOW);
    delay(200);
    digitalWrite(pinHigh, HIGH);
    delay(200);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(RED_LED_OUT, OUTPUT);
    digitalWrite(RED_LED_OUT, LOW);
    pinMode(BLUE_LED_OUT, OUTPUT);
    digitalWrite(BLUE_LED_OUT, LOW);
}

void loop() {

    if (ledState) {
        blinkLed(BLUE_LED_OUT, RED_LED_OUT);
        Serial.println("RED LED OFF");
        Serial.println("BLUE LED ON");
    } else {
        blinkLed(RED_LED_OUT, BLUE_LED_OUT);
        Serial.println("RED LED ON");
        Serial.println("BLUE LED OFF");
    }
    ledState = !ledState;
    delay(500);
}
