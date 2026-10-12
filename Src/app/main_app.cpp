#include <stdio.h>
#include <stdbool.h>
#include "main.h"
#include "printf/usb_printf.h"
#include "pwm/pwm.h"
#include "player/player.h"
#include "encoder/encoder.h"
#include "safe/safe.h"

#define ENCODER_A_PORT GPIOA
#define ENCODER_A_PIN GPIO_PIN_0
#define ENCODER_B_PORT GPIOA
#define ENCODER_B_PIN GPIO_PIN_1
#define ENCODER_BUTTON_PORT GPIOA
#define ENCODER_BUTTON_PIN GPIO_PIN_2
#define ENCODER_DEBOUNCE_NS 2000000

/* "Замок": світлодіод на платі (PC13, активний LOW) світиться, коли сейф відкрито. */
#define LED_PORT GPIOC
#define LED_PIN GPIO_PIN_13

static const uint8_t kSecretCode[SAFE_CODE_LENGTH] = {3, 1, 0, 2};

static const Tone_t kMelodyOpen[] = {
    {NOTE_C5, 120}, {NOTE_E5, 120}, {NOTE_G5, 120}, {NOTE_REST, 60}, {NOTE_C5, 120}, {NOTE_G5, 400},
};
static const Tone_t kMelodyWrong[] = {
    {NOTE_E4, 200}, {NOTE_REST, 60}, {NOTE_C4, 400},
};
static const Tone_t kMelodyAlarm[] = {
    {NOTE_A5, 250}, {NOTE_E5, 250},
};

/* PC13 вже налаштований як вихід у MX_GPIO_Init() (main.c). */
static void Lock_SetOpen(bool open) {
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, open ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void PrintPrompt(const Safe_t *safe) {
    printf("\nAttempt %u/%u. Code: ",
           (unsigned)(SAFE_MAX_ATTEMPTS - safe->attempts_left + 1), (unsigned)SAFE_MAX_ATTEMPTS);
}

/* Друкує результат події. Поточна цифра змінюється на місці через '\b'. */
static void HandleEvent(const Safe_t *safe, SafeEvent_t event, Player_t *player) {
    unsigned digit = safe->digits[safe->digit_index];

    switch (event) {
        case SAFE_EVENT_FIRST_DIGIT:
            printf("%u", digit);
            break;
        case SAFE_EVENT_DIGIT_CHANGED:
            printf("\b%u", digit);
            break;
        case SAFE_EVENT_NEXT_DIGIT:
            printf(" %u", digit);
            break;
        case SAFE_EVENT_WRONG:
            printf("  -> WRONG. Attempts left: %u\n", (unsigned)safe->attempts_left);
            Player_Play(player, kMelodyWrong, PLAYER_LENGTH(kMelodyWrong), false);
            PrintPrompt(safe);
            break;
        case SAFE_EVENT_RESET:
            printf("  -> RESET. Attempts left: %u\n", (unsigned)safe->attempts_left);
            PrintPrompt(safe);
            break;
        case SAFE_EVENT_OPENED:
            printf("  -> OPEN. Press the button to close.\n");
            Lock_SetOpen(true);
            Player_Play(player, kMelodyOpen, PLAYER_LENGTH(kMelodyOpen), false);
            break;
        case SAFE_EVENT_CLOSED:
            printf("Closed.\n");
            Lock_SetOpen(false);
            PrintPrompt(safe);
            break;
        case SAFE_EVENT_LOCKED:
            printf("\n!!! No attempts left. LOCKED until reset. !!!\n");
            Player_Play(player, kMelodyAlarm, PLAYER_LENGTH(kMelodyAlarm), true);
            break;
        default:
            break;
    }
}

extern "C" void main_cpp() {
    bool error = false;

    /* Без буфера: інакше рядок без '\n' (цифри коду) не дійде до консолі. */
    setvbuf(stdout, NULL, _IONBF, 0);

    HAL_Delay(1000); // Затримка для налаштування послідовного інтерфейса

    PwmDriver_t pwm_buzzer;
    if (!Pwm_InitByPin(&pwm_buzzer, PWM_PORT_B, 4, NOTE_A4, 0)) {
        error = true;
        printf("PWM init failed\n");
    }

    Player_t player;
    if (!Player_Init(&player, &pwm_buzzer)) {
        error = true;
        printf("Player init failed\n");
    }

    EncoderCtx_t encoder = {0};
    if (!Encoder_Init(&encoder, ENCODER_A_PORT, ENCODER_A_PIN,
                      ENCODER_B_PORT, ENCODER_B_PIN,
                      ENCODER_BUTTON_PORT, ENCODER_BUTTON_PIN,
                      ENCODER_DEBOUNCE_NS)) {
        error = true;
        printf("Encoder init failed\n");
    }

    Lock_SetOpen(false);

    while (error) {
        printf("Init error\n");
        HAL_Delay(1000);
    }

    Safe_t safe;
    Safe_Init(&safe, kSecretCode);

    printf("\n=== SAFE ===\n");
    printf("Turn to change a digit, reverse to confirm it. Button = reset.\n");
    PrintPrompt(&safe);

    int32_t last_position = 0;
    bool last_pressed = false;
    Encoder_GetButton(&encoder, &last_pressed);

    while (safe.state != SAFE_LOCKED) {
        int32_t position = 0;
        bool pressed = false;
        Encoder_Read(&encoder, &position);
        Encoder_GetButton(&encoder, &pressed);

        /* Кожен тік окремо: при швидкому обертанні за 10 мс їх може бути кілька. */
        while (position != last_position && safe.state != SAFE_LOCKED) {
            int8_t direction = (position > last_position) ? 1 : -1;
            last_position += direction;
            HandleEvent(&safe, Safe_OnTick(&safe, direction), &player);
        }

        /* Лише момент натискання, а не весь час, поки кнопка утримується. */
        if (pressed && !last_pressed) {
            HandleEvent(&safe, Safe_OnButton(&safe), &player);
        }
        last_pressed = pressed;

        Player_Update(&player);
        HAL_Delay(10);
    }

    /* Спроби вичерпано: ігноруємо енкодер, грає тривога до перезавантаження. */
    Encoder_Deinit(&encoder);
    while (1) {
        Player_Update(&player);
        HAL_Delay(10);
    }
}
