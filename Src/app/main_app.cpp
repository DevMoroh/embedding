#include <stdint.h>
#include <stdio.h>
#include "main.h"

// Період блимання вбудованого світлодіода PC13
#define LED_BLINK_PERIOD_MS 500U

// Кнопка KEY на BlackPill: PA0, при натисканні замикає на GND
#define KEY_GPIO_PORT       GPIOA
#define KEY_GPIO_PIN        GPIO_PIN_0
// Скільки мілісекунд стан кнопки має бути стабільним, щоб ми йому повірили
#define KEY_DEBOUNCE_MS     20U

static void key_init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = KEY_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(KEY_GPIO_PORT, &gpio);
}

extern "C" void main_cpp()
{
    // Тут ініціалізація застосунку (викликається один раз з main.c)
    printf("[APP] Started\r\n");

    key_init();

    // Стан кнопки: "сирий" (як читаємо зараз) і "стабільний" (після антидребезгу)
    GPIO_PinState key_raw = HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN);
    GPIO_PinState key_stable = key_raw;
    uint32_t key_changed_at = HAL_GetTick();

    uint32_t last_blink = HAL_GetTick();

    while (1)
    {
        // Блимаємо світлодіодом без HAL_Delay, щоб цикл не блокувався
        if ((HAL_GetTick() - last_blink) >= LED_BLINK_PERIOD_MS)
        {
            last_blink = HAL_GetTick();
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }

         // Кнопка: чекаємо, поки стан протримається KEY_DEBOUNCE_MS, і лише тоді реагуємо
        GPIO_PinState key_now = HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN);
        if (key_now != key_raw)
        {
            key_raw = key_now;              // стан змінився - запам'ятовуємо час
            key_changed_at = HAL_GetTick();
        }
        else if ((key_raw != key_stable) &&
                 ((HAL_GetTick() - key_changed_at) >= KEY_DEBOUNCE_MS))
        {
            key_stable = key_raw;           // стан стабільний - приймаємо його
            if (key_stable == GPIO_PIN_RESET)
            {
                printf("[KEY] pressed (t=%lu ms)\r\n", HAL_GetTick());
            }
            else
            {
                printf("[KEY] released (t=%lu ms)\r\n", HAL_GetTick());
            }
        }
    }
}
