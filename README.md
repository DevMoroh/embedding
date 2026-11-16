# Beetroot EMB 4

Навчальний проєкт для BlackPill зі STM32F411CE: читання АЦП через DMA і відправка значень у консоль по UART теж через DMA.

## Що демонструє проєкт

- ADC1 у режимі сканування перетворює канали PA5 (IN5) і PA6 (IN6). Результати DMA2 Stream0 складає в буфер без участі процесора; переривання DMA викликає `HAL_ADC_ConvCpltCallback`, який позначає, що дані готові.
- Основний цикл формує рядок зі значеннями і запускає передавання `HAL_UART_Transmit_DMA` — USART1_TX обслуговує DMA2 Stream7. Новий рядок формується тільки після завершення попереднього передавання, тож буфер не перезаписується під час роботи DMA.
- Наступне перетворення АЦП запускається кожні 200 мс; світлодіод PC13 блимає без `HAL_Delay`, цикл не блокується.

Приклад виводу в UART-терміналі:

```
ADC -> UART DMA demo
ADC PA5: 2047 (1649 mV) | ADC PA6:  512 ( 412 mV)
ADC PA5: 2051 (1652 mV) | ADC PA6:  515 ( 415 mV)
```

## Конфігурація

- MCU: STM32F411CEUx
- Framework: STM32Cube HAL
- Збірка та завантаження: PlatformIO, ST-Link
- АЦП: ADC1, 12 біт, PA5 та PA6, DMA2 Stream0 Channel 0
- UART: USART1, 115200 baud, 8N1, PA9 (TX), PA10 (RX), передавання DMA2 Stream7 Channel 4
- Статус ініціалізації виводиться через `printf()` у USB CDC

Під'єднайте потенціометри (або інше джерело 0–3,3 В) до PA5 і PA6. UART-адаптер під'єднайте перехресно: RX адаптера до PA9, TX адаптера до PA10, а також з'єднайте GND. Термінал: 115200 baud, 8 біт даних, без парності, 1 стоп-біт.

ADC1, USART1 і DMA налаштовуються в коді модулів, а не в CubeMX (`.ioc` їх не містить).

## Структура

- `Src/app/main_app.cpp` — запуск АЦП, формування рядка і відправка по UART
- `Src/adc/adc.cpp` — ADC1 + DMA2 Stream0 (код з уроку, [vladimirvk2007/Beetroot_EMB_4](https://github.com/vladimirvk2007/Beetroot_EMB_4/tree/stm32_cubemx_adc_drv_dma))
- `Src/uart/uart.c` — USART1, блокувальне приймання/передавання та передавання через DMA2 Stream7
- `Src/stm32f4xx_it.c` — переривання DMA2 Stream0, DMA2 Stream7 і USART1
- `Src/main.c` — запуск HAL, системного тактування та периферії
