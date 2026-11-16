#ifndef UART_H
#define UART_H

#include <stdbool.h>

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

HAL_StatusTypeDef UART_Init(UART_HandleTypeDef *huart,
                            USART_TypeDef *instance,
                            uint32_t baud_rate);
HAL_StatusTypeDef UART_Transmit(UART_HandleTypeDef *huart,
                                uint8_t *data,
                                uint16_t size,
                                uint32_t timeout);
HAL_StatusTypeDef UART_Receive(UART_HandleTypeDef *huart,
                               uint8_t *data,
                               uint16_t size,
                               uint32_t timeout);

// Асинхронне передавання через DMA2 Stream7; буфер data не можна змінювати,
// доки UART_IsTxReady() не поверне true
HAL_StatusTypeDef UART_TransmitDMA(UART_HandleTypeDef *huart,
                                   const uint8_t *data,
                                   uint16_t size);

// true, якщо попереднє передавання завершене і можна запускати наступне
bool UART_IsTxReady(UART_HandleTypeDef *huart);

void UART_IRQHandler(UART_HandleTypeDef *huart);

// Обробники переривань USART1 і DMA2 Stream7 (USART1_TX)
void UART_USART1_IRQHandler(void);
void UART_DMA_TX_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_H */
