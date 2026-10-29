# STM32 BlackPill template

Базовий шаблон проєкту для STM32F411CE (плата BlackPill) на STM32Cube HAL і PlatformIO.

## Що є в шаблоні

- тактування 96 МГц від зовнішнього кварца 25 МГц (`SystemClock_Config` у `Src/main.c`);
- світлодіод PC13 як вихід (`MX_GPIO_Init`);
- USB CDC (віртуальний COM-порт через Type-C) і перенаправлення `printf` у нього (`Src/print/usb_printf.c`);
- точка входу застосунку `main_cpp()` у `Src/app/main_app.cpp`: виводить `[APP] Started` і блимає PC13 без `HAL_Delay`.

Код застосунку додається в `Src/app/main_app.cpp`, власні модулі — у `Src/<module>/` і `Inc/<module>/`.

## Структура

```
Src/main.c              - згенерований CubeMX старт: HAL, тактування, GPIO, USB, виклик main_cpp()
Src/app/main_app.cpp    - код застосунку
Src/print/usb_printf.c  - printf через USB CDC
Inc/                    - заголовки, stm32f4xx_hal_conf.h
Drivers/, Middlewares/  - HAL, CMSIS, USB Device Library
STM32CubeIDE/           - startup, linker-скрипти, проєкт CubeIDE
stm32CubeMX_test.ioc    - конфігурація CubeMX
```

## Збірка та завантаження

```
pio run
pio run -t upload        # через ST-Link
pio device monitor       # лог printf з USB CDC
```
