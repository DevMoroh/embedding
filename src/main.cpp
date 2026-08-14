#include <Arduino.h>

#define ADC_IN 4

void setup() {
    Serial.begin(115200);

    analogReadResolution(12);
    pinMode(ADC_IN, INPUT);
    analogSetPinAttenuation(ADC_IN, ADC_11db);

    Serial.println();
    Serial.println("ADC measurement started");
}

void loop() {
    uint32_t voltageRaw = analogRead(ADC_IN);

    float calculatedVoltage = (voltageRaw / 4095.0f) * 3.1f;

    uint32_t voltageMilliVolts = analogReadMilliVolts(ADC_IN);

    float measuredVoltage = voltageMilliVolts / 1000.0f;

    float absoluteError = abs(calculatedVoltage - measuredVoltage);

    float relativeError = 0.0f;

    if (measuredVoltage > 0.0f) {
        relativeError =
            (absoluteError / measuredVoltage) * 100.0f;
    }

    Serial.println("------------------------------------------------");
    Serial.println("| ADC Raw | Calculated | Calibrated | Error   |");
    Serial.println("|         | Voltage V  | Voltage V  | %       |");
    Serial.println("------------------------------------------------");

    Serial.print("| ");
    Serial.print(voltageRaw);

    Serial.print("\t | ");
    Serial.print(calculatedVoltage, 3);

    Serial.print("\t | ");
    Serial.print(measuredVoltage, 3);

    Serial.print("\t | ");
    Serial.print(relativeError, 2);

    Serial.println(" % |");

    Serial.println("------------------------------------------------");
    Serial.println();

    delay(500);
}