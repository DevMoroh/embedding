#include <Arduino.h>

#define BUTTON_IN  6
#define LED_OUT    8

int buttonPreviousState;

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_OUT, OUTPUT);
    pinMode(BUTTON_IN, INPUT);
    digitalWrite(LED_OUT, LOW);
}

void loop() {
    int buttonState = digitalRead(BUTTON_IN);

    if (buttonState == LOW) {
        digitalWrite(LED_OUT, HIGH);
        if (buttonPreviousState != LOW) {
            Serial.println("Button pressed");
        }
        buttonPreviousState = LOW;
    } else {
        digitalWrite(LED_OUT, LOW);
        if (buttonPreviousState != HIGH) {
            Serial.println("Button released");
        }
        buttonPreviousState = HIGH;
    }
    delay(100);
}
