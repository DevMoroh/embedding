#include "uart/uart.h"
#include "main.h"

static DMA_HandleTypeDef uart_tx_dma;
static UART_HandleTypeDef *uart1_handle = NULL;

HAL_StatusTypeDef UART_Init(UART_HandleTypeDef *huart,
                            USART_TypeDef *instance,
                            uint32_t baud_rate)
{
  if ((huart == NULL) || (instance != USART1) || (baud_rate == 0))
  {
    return HAL_ERROR;
  }

  huart->Instance = instance;
  huart->Init.BaudRate = baud_rate;
  huart->Init.WordLength = UART_WORDLENGTH_8B;
  huart->Init.StopBits = UART_STOPBITS_1;
  huart->Init.Parity = UART_PARITY_NONE;
  huart->Init.Mode = UART_MODE_TX_RX;
  huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart->Init.OverSampling = UART_OVERSAMPLING_16;

  HAL_StatusTypeDef status = HAL_UART_Init(huart);
  if (status == HAL_OK)
  {
    uart1_handle = huart;
  }
  return status;
}

HAL_StatusTypeDef UART_Transmit(UART_HandleTypeDef *huart,
                                uint8_t *data,
                                uint16_t size,
                                uint32_t timeout)
{
  if ((huart == NULL) || (data == NULL) || (size == 0))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(huart, data, size, timeout);
}

HAL_StatusTypeDef UART_Receive(UART_HandleTypeDef *huart,
                               uint8_t *data,
                               uint16_t size,
                               uint32_t timeout)
{
  if ((huart == NULL) || (data == NULL) || (size == 0))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Receive(huart, data, size, timeout);
}

HAL_StatusTypeDef UART_TransmitDMA(UART_HandleTypeDef *huart,
                                   const uint8_t *data,
                                   uint16_t size)
{
  if ((huart == NULL) || (data == NULL) || (size == 0))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit_DMA(huart, (uint8_t *)data, size);
}

bool UART_IsTxReady(UART_HandleTypeDef *huart)
{
  return (huart != NULL) && (huart->gState == HAL_UART_STATE_READY);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef gpio_init = {0};

  if (huart->Instance != USART1)
  {
    return;
  }

  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  gpio_init.Mode = GPIO_MODE_AF_PP;
  /* Idle UART line is HIGH: keeps RX from picking up noise when nothing drives it. */
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_init.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &gpio_init);

  /* USART1_TX: DMA2 Stream7, Channel 4 (RM0383, таблиця запитів DMA2) */
  __HAL_RCC_DMA2_CLK_ENABLE();
  uart_tx_dma.Instance = DMA2_Stream7;
  uart_tx_dma.Init.Channel = DMA_CHANNEL_4;
  uart_tx_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
  uart_tx_dma.Init.PeriphInc = DMA_PINC_DISABLE;
  uart_tx_dma.Init.MemInc = DMA_MINC_ENABLE;
  uart_tx_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  uart_tx_dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  uart_tx_dma.Init.Mode = DMA_NORMAL;
  uart_tx_dma.Init.Priority = DMA_PRIORITY_LOW;
  uart_tx_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  if (HAL_DMA_Init(&uart_tx_dma) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_LINKDMA(huart, hdmatx, uart_tx_dma);

  HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

  /* Після DMA HAL чекає прапорець TC у перериванні USART, щоб звільнити UART */
  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance != USART1)
  {
    return;
  }

  __HAL_RCC_USART1_CLK_DISABLE();
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
  HAL_DMA_DeInit(huart->hdmatx);
  HAL_NVIC_DisableIRQ(DMA2_Stream7_IRQn);
  HAL_NVIC_DisableIRQ(USART1_IRQn);
}

void UART_IRQHandler(UART_HandleTypeDef *huart)
{
  if (huart != NULL)
  {
    HAL_UART_IRQHandler(huart);
  }
}

void UART_USART1_IRQHandler(void)
{
  UART_IRQHandler(uart1_handle);
}

void UART_DMA_TX_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&uart_tx_dma);
}
