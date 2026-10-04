#include <stdio.h>
#include <stdbool.h>
#include "main.h"
#include "printf/usb_printf.h"
#include "pwm/pwm.h"
#include "servo/servo.h"
#include "adc/adc.h"

#define SERVO_GPIO_PORT PWM_PORT_B
#define SERVO_GPIO_PIN 4
#define ADC_MAX_VALUE 4095U

extern "C" void main_cpp() {
    bool error = false;

    PwmDriver_t servo_pwm = {};
    Servo_t servo;

    if (!Pwm_InitByPin(&servo_pwm, SERVO_GPIO_PORT,
                SERVO_GPIO_PIN, SERVO_FREQUENCY_HZ, 0)) {
        error = true;
        printf("PWM init failed\n");
    }

    if (!error && !Servo_Init(&servo, &servo_pwm)) {
        error = true;
        printf("Servo init failed\n");
    }

    const uint32_t adc_channels[] = {ADC_CHANNEL_5};
    uint32_t adc_values[1];

    if (ADC_Init(adc_channels, 1) != HAL_OK) {
        Error_Handler();
    }

    while (1) {
        if (error) {
            printf("Init error\n");
        } else {
            if (ADC_ReadSequence(adc_values, 1) == HAL_OK) {
                const uint16_t angle = (uint16_t)
                    ((adc_values[0] * SERVO_MAX_ANGLE + ADC_MAX_VALUE / 2U) /
                     ADC_MAX_VALUE);
                Servo_SetAngle(&servo, angle);
                printf("ADC PA5: %lu | Servo: %u deg\n", adc_values[0], angle);
            }
        }

        HAL_Delay(20);
    }
}
