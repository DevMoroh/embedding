#include <Arduino.h>

constexpr uint8_t RELAY_NO_PIN = 9;
constexpr uint8_t RELAY_NC_PIN = 10;
constexpr uint8_t OUTPUT_PIN = 8;
constexpr uint32_t RELAY_TOGGLE_INTERVAL_US = 1000000; // 1000 ms
constexpr uint32_t RELAY_CONTACT_DEBOUNCE_US = 100000; // 100 ms
constexpr uint8_t MEASUREMENTS_COUNT = 10;
uint32_t relayNcTimes[MEASUREMENTS_COUNT] = {};

volatile bool relayNoTriggered = false;
volatile bool relayNcTriggered = false;
volatile uint32_t relayNcTriggerUs = 0;


void isr_relay_no() {
    relayNoTriggered = true;
}
void isr_relay_nc() {
    relayNcTriggered = true;
    relayNcTriggerUs = micros();
}

void setup() {
    Serial.begin(115200);

    pinMode(RELAY_NO_PIN, INPUT_PULLUP);
    pinMode(RELAY_NC_PIN, INPUT_PULLUP);
    pinMode(OUTPUT_PIN, OUTPUT);

    attachInterrupt(digitalPinToInterrupt(RELAY_NO_PIN), isr_relay_no, FALLING);
    attachInterrupt(digitalPinToInterrupt(RELAY_NC_PIN), isr_relay_nc, FALLING);
}



void loop() {
    const uint32_t loopStartUs = micros();
    static uint32_t lastDelayUs = 0;
    static uint32_t lastDelayRelayUs = 0;
    static uint8_t counter = 0;
    static bool testFinished = false;
    static uint32_t relayOffCommandUs = 0;
    static uint8_t measurementCounter = 0;

    static bool outputState = false;

    if (relayNoTriggered) {
         relayNoTriggered = false;
        if (digitalRead(RELAY_NO_PIN) == LOW) {
            Serial.println("[ISR] Relay NO triggered");
        }
    }
    if (relayNcTriggered) {
        relayNcTriggered = false;
        if (digitalRead(RELAY_NC_PIN) == LOW) {
            if (loopStartUs - lastDelayRelayUs >= RELAY_CONTACT_DEBOUNCE_US && !testFinished) {
                uint32_t relayNcTime = relayNcTriggerUs - relayOffCommandUs;
                relayNcTimes[measurementCounter] = relayNcTime;
                Serial.print("[NC] Measurement #");
                    Serial.print(measurementCounter + 1);
                ++measurementCounter;
                Serial.println();
                Serial.println("[ISR] Relay NC triggered");
                lastDelayRelayUs = loopStartUs;
            }
        }
    }

    if (loopStartUs - lastDelayUs >= RELAY_TOGGLE_INTERVAL_US && !testFinished) {
        Serial.print("outputState: ");
        Serial.println(outputState);
        if (outputState) {
            digitalWrite(OUTPUT_PIN, HIGH);
            counter++;
            Serial.print("[ISR NO] Counter: ");
            Serial.println(counter);
        } else {
            relayOffCommandUs = micros(); // record the time when we send the command to turn off the relay
            digitalWrite(OUTPUT_PIN, LOW);
        }
        outputState = !outputState;
        lastDelayUs = loopStartUs;
    }

    if (measurementCounter == MEASUREMENTS_COUNT && !testFinished) {
        testFinished = true;

        uint64_t sum = 0;

        for (uint8_t i = 0; i < MEASUREMENTS_COUNT; ++i) {
            sum += relayNcTimes[i];
        }

        const float average =
            static_cast<float>(sum) / MEASUREMENTS_COUNT;

        Serial.println();
        Serial.println("===== RESULT =====");

        Serial.print("Average NC trigger time: ");
        Serial.print(average / 1000.0f); // Convert to milliseconds
        Serial.println(" ms");
    }

}
