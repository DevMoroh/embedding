#include <Arduino.h>

constexpr int LED_OUT = 15;
constexpr int BUTTON_IN = 4;
constexpr int DELAY = 500000;

enum class LedState : uint8_t {
    ON = 1,
    OFF = 0,
};

enum class LedMode: uint8_t {
    Blinking,
    AlwaysOn,
    AlwaysOff,
};

class LedConfig {
public:
    static constexpr uint32_t SERIAL_BAUD = 115200;
    static constexpr uint32_t DEBOUNCE_US = 30000;
};

class Led {
    private:
        uint8_t pin_;
        LedState state_ = LedState::OFF;

    public:
        explicit Led(uint8_t pin): pin_(pin) 
        {
        }

        void init() {
            pinMode(pin_, OUTPUT);
            set(LedState::OFF);
        }

        void set(LedState state) {
            state_ = state;
            digitalWrite(pin_, state_ == LedState::ON ? HIGH : LOW);
        }

        void toggle() {
            set(state_ == LedState::ON ? LedState::OFF : LedState::ON);
        }

};

volatile bool buttonPressed = false;

void onButtonInterrupt() {
    buttonPressed = true;
}

Led& getLedInstance() {
    static Led led(LED_OUT);
    return led;
}

void setup() {
    Serial.begin(LedConfig::SERIAL_BAUD);

    getLedInstance().init();

    pinMode(BUTTON_IN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_IN), onButtonInterrupt, FALLING);
}

void loop() {

    const uint32_t loopStartUs = micros();

    static LedMode mode = LedMode::Blinking;
    static uint32_t lastBlinkUs = 0;
    static uint32_t lastButtonUs = 0;

    static uint32_t loopCounter = 0;

    if (buttonPressed) {
        noInterrupts();
        buttonPressed = false;
        interrupts();

        if (loopStartUs - lastButtonUs >= LedConfig::DEBOUNCE_US) {
            lastButtonUs = loopStartUs;

            switch (mode) {
                case LedMode::Blinking:
                    mode = LedMode::AlwaysOn;
                    getLedInstance().set(LedState::ON);
                    Serial.println("Mode: Always ON");
                    break;
                case LedMode::AlwaysOn:
                    mode = LedMode::AlwaysOff;
                    getLedInstance().set(LedState::OFF);
                    Serial.println("Mode: Always OFF");
                    break;
                case LedMode::AlwaysOff:
                    mode = LedMode::Blinking;
                    lastBlinkUs = loopStartUs; // Reset blink timer
                    break;     
            }

            
        }
    }

    if (mode == LedMode::Blinking && loopStartUs - lastBlinkUs >= DELAY) {
        lastBlinkUs = loopStartUs;
        getLedInstance().toggle();
    }

}
