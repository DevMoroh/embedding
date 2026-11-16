#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "adc/adc.h"
#include "uart/uart.h"

#define LED_INBUILT_PIN   GPIO_PIN_13
#define LED_INBUILT_PORT  GPIOC
#define LED_PERIOD_MS     500U

#define ADC_CHANNEL_COUNT 1U
#define ADC_PERIOD_MS     200U   // як часто запускати нове перетворення
#define ADC_VREF_MV       3300U
#define ADC_MAX_VALUE     4095U  // 12 біт

static UART_HandleTypeDef huart1;

// Буфер, куди DMA2 Stream0 складає результат ADC1 (PA5)
static uint32_t adc_values[ADC_CHANNEL_COUNT];
static volatile bool adc_data_ready;
static volatile bool adc_error;

// Буфер, з якого DMA2 Stream7 передає рядок у USART1; не змінюється під час передавання
static char uart_tx_buffer[64];

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        adc_data_ready = true;
    }
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1) {
        adc_error = true;
    }
}

static uint32_t to_millivolts(uint32_t raw)
{
    return raw * ADC_VREF_MV / ADC_MAX_VALUE;
}

extern "C" void main_cpp()
{
    // USART1: TX - PA9, RX - PA10, 115200 8N1; передавання через DMA2 Stream7
    if (UART_Init(&huart1, USART1, 115200) != HAL_OK) {
        printf("[UART] USART1 initialization failed\n");
        Error_Handler();
    }
    printf("[UART] USART1 initialized: 115200 baud, 8N1, TX DMA\n");

    // ADC1: PA5 (IN5); результат через DMA2 Stream0
    const uint32_t adc_channels[ADC_CHANNEL_COUNT] = {ADC_CHANNEL_5};
    if (ADC_Init(adc_channels, ADC_CHANNEL_COUNT) != HAL_OK) {
        printf("[ADC] initialization failed\n");
        Error_Handler();
    }
    printf("[ADC] ADC1 initialized: PA5, DMA\n");

    static const char hello[] = "\r\nADC -> UART DMA demo\r\n";
    UART_TransmitDMA(&huart1, reinterpret_cast<const uint8_t *>(hello), sizeof(hello) - 1U);

    if (ADC_StartSequenceDMA(adc_values, ADC_CHANNEL_COUNT) != HAL_OK) {
        Error_Handler();
    }

    bool adc_start_pending = false;
    uint32_t adc_next_start = 0;
    uint32_t led_next_toggle = 0;

    while (1) {
        const uint32_t now = HAL_GetTick();

        if (adc_error) {
            printf("[ADC] DMA error\n");
            Error_Handler();
        }

        // Дані готові: формуємо рядок і віддаємо його DMA, щойно UART звільниться
        if (adc_data_ready && UART_IsTxReady(&huart1)) {
            adc_data_ready = false;

            const int length = snprintf(uart_tx_buffer, sizeof(uart_tx_buffer),
                                        "ADC PA5: %4lu (%4lu mV)\r\n",
                                        static_cast<unsigned long>(adc_values[0]),
                                        static_cast<unsigned long>(to_millivolts(adc_values[0])));
            if (length > 0) {
                UART_TransmitDMA(&huart1, reinterpret_cast<const uint8_t *>(uart_tx_buffer),
                                 static_cast<uint16_t>(length));
            }

            adc_start_pending = true;
            adc_next_start = now + ADC_PERIOD_MS;
        }

        // Наступне перетворення через ADC_PERIOD_MS
        if (adc_start_pending && (int32_t)(now - adc_next_start) >= 0) {
            adc_start_pending = false;
            if (ADC_StartSequenceDMA(adc_values, ADC_CHANNEL_COUNT) != HAL_OK) {
                Error_Handler();
            }
        }

        // Світлодіод блимає без HAL_Delay, щоб не блокувати цикл
        if ((int32_t)(now - led_next_toggle) >= 0) {
            led_next_toggle = now + LED_PERIOD_MS;
            HAL_GPIO_TogglePin(LED_INBUILT_PORT, LED_INBUILT_PIN);
        }
    }
}
