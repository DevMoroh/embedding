#ifndef ADC_H
#define ADC_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// Ініціалізація каналів АЦП
HAL_StatusTypeDef ADC_Init(const uint32_t *channels, uint32_t count);

// Асинхронний запуск послідовності перетворень через DMA
HAL_StatusTypeDef ADC_StartSequenceDMA(uint32_t *values, uint32_t count);

// Зчитування послідовності значень з АЦП через DMA
HAL_StatusTypeDef ADC_ReadSequence(uint32_t *values, uint32_t count);

// Максимальна кількість вимірів для усереднення (довжина послідовності ADC1)
#define ADC_MAX_AVERAGE_SAMPLES 16U

// Ініціалізація одного каналу з усередненням: послідовність із samples
// перетворень того самого каналу (1..ADC_MAX_AVERAGE_SAMPLES)
HAL_StatusTypeDef ADC_InitAveraged(uint32_t channel, uint32_t samples);

// Асинхронний запуск samples перетворень через DMA у внутрішній буфер модуля;
// завершення - HAL_ADC_ConvCpltCallback
HAL_StatusTypeDef ADC_StartAveragedDMA(void);

// Середнє значення останньої завершеної послідовності
uint32_t ADC_GetAverage(void);

// Обробник переривання DMA2 Stream 0
void ADC_DMA_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif
