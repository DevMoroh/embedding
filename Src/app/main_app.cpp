#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "uart/uart.h"
#include "button/button.h"

static UART_HandleTypeDef huart1;


typedef struct
{
    UART_HandleTypeDef *huart;   // через який UART слати
    bool led_on;                 // поточний стан
} KeyContext_t;

static Button_t key;
static uint32_t led_off_at = 0;

static void OnKeyPressed(void *arg)
{
    KeyContext_t *ctx = (KeyContext_t *)arg;
    // наприклад: перемкнути стан і відправити '1' / '0' по UART
    ctx->led_on = !ctx->led_on;
    uint8_t tx_byte = ctx->led_on ? '1' : '0';
    HAL_StatusTypeDef status = UART_Transmit(ctx->huart, &tx_byte, 1, 100);
    if (status == HAL_OK)
    {
        printf("[KEY] TX: '%c' -> LED %s\n", tx_byte, ctx->led_on ? "ON" : "OFF");
    }
    else
    {
        printf("[KEY] TX failed (status=%d)\n", (int)status);
    }
}

extern "C" void main_cpp()
{
    uint8_t received_byte = 0;

    HAL_StatusTypeDef status = UART_Init(&huart1, USART1, 115200);
    if (status != HAL_OK)
    {
        printf("[UART] USART1 initialization failed (status=%d)\n", (int)status);
        Error_Handler();
    }

    printf("[UART] USART1 initialized: 115200 baud, 8N1\n");

    static KeyContext_t key_ctx = { .huart = &huart1, .led_on = false };
    if (Button_Init(&key, GPIOA, GPIO_PIN_0, 30, OnKeyPressed, NULL, &key_ctx) != HAL_OK)
    {
        printf("[KEY] initialization failed\n");
        Error_Handler();
    }

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); // Ensure LED is off initially
    while (1)
    {
        Button_Update(&key);
        if (led_off_at != 0 && (int32_t)(HAL_GetTick() - led_off_at) >= 0)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            led_off_at = 0;
        }
        status = UART_Receive(&huart1, &received_byte, 1, 1);
        if (status == HAL_OK)
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            led_off_at = HAL_GetTick() + 50;

            printf("[UART] RX: 0x%02X - '%c'\n", received_byte, received_byte);
        }
        else if (status != HAL_TIMEOUT)
        {
            printf("[UART] Receive failed (status=%d)\r\n", (int)status);
            Error_Handler();
        }
    }
}
