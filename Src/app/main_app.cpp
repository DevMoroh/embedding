#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "main.h"

// Період блимання вбудованого світлодіода PC13
#define LED_BLINK_PERIOD_MS 500U

// Кнопка KEY на BlackPill: PA0, при натисканні замикає на GND.
// Скільки мілісекунд стан кнопки має бути стабільним, щоб ми йому повірили
#define KEY_DEBOUNCE_MS     20U

// PC13 - вихід push-pull (вбудований світлодіод, світиться при 0)
static void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;       // тактування порту C

    GPIOC->MODER &= ~GPIO_MODER_MODER13;       // скидаємо 2 біти режиму PC13
    GPIOC->MODER |= GPIO_MODER_MODER13_0;      // 01 - вихід
    GPIOC->OTYPER &= ~GPIO_OTYPER_OT13;        // 0 - push-pull
    GPIOC->PUPDR &= ~GPIO_PUPDR_PUPD13;        // 00 - без підтяжок
}

// PA0 - вхід з підтяжкою вгору: відпущена = 1, натиснута = 0
static void key_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;       // тактування порту A

    GPIOA->MODER &= ~GPIO_MODER_MODER0;        // 00 - вхід
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPD0;         // скидаємо 2 біти підтяжки
    GPIOA->PUPDR |= GPIO_PUPDR_PUPD0_0;        // 01 - підтяжка вгору
}

// true, якщо кнопка натиснута (PA0 = 0)
static bool key_is_pressed(void)
{
    return (GPIOA->IDR & GPIO_IDR_ID0) == 0U;
}

// Перемикає PC13. BSRR змінює пін атомарно: BS13 - встановити в 1, BR13 - скинути в 0
static void led_toggle(void)
{
    if (GPIOC->ODR & GPIO_ODR_OD13)
    {
        GPIOC->BSRR = GPIO_BSRR_BR13;
    }
    else
    {
        GPIOC->BSRR = GPIO_BSRR_BS13;
    }
}

extern "C" void main_cpp()
{
    // Тут ініціалізація застосунку (викликається один раз з main.c)
    printf("[APP] Started\r\n");

    led_init();
    key_init();

    // Стан кнопки: "сирий" (як читаємо зараз) і "стабільний" (після антидребезгу)
    bool key_raw = key_is_pressed();
    bool key_stable = key_raw;
    uint32_t key_changed_at = HAL_GetTick();

    uint32_t last_blink = HAL_GetTick();

    while (1)
    {
        // Блимаємо світлодіодом без HAL_Delay, щоб цикл не блокувався
        if ((HAL_GetTick() - last_blink) >= LED_BLINK_PERIOD_MS)
        {
            last_blink = HAL_GetTick();
            led_toggle();
        }

        // Кнопка: чекаємо, поки стан протримається KEY_DEBOUNCE_MS, і лише тоді реагуємо
        bool key_now = key_is_pressed();
        if (key_now != key_raw)
        {
            key_raw = key_now;              // стан змінився - запам'ятовуємо час
            key_changed_at = HAL_GetTick();
        }
        else if ((key_raw != key_stable) &&
                 ((HAL_GetTick() - key_changed_at) >= KEY_DEBOUNCE_MS))
        {
            key_stable = key_raw;           // стан стабільний - приймаємо його
            if (key_stable)
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
