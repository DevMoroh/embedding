#ifndef PLAYER_H
#define PLAYER_H

#include "pwm/pwm.h"
#include "player/notes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Неблокуючий програвач мелодій поверх модуля sine (порт player з ESP32-версії).
 * Замість esp_timer використовує HAL_GetTick(): Player_Update() треба викликати
 * в головному циклі, вона перемикає ноти, коли спливає їхня тривалість.
 */

typedef struct {
    uint32_t frequency_hz;  // NOTE_REST (0) = пауза
    uint32_t duration_ms;
} Tone_t;

typedef struct {
    PwmDriver_t *pwm;
    const Tone_t *sequence;
    size_t length;
    size_t index;
    uint32_t note_start_ms;
    bool repeat;
    bool playing;
    bool initialized;
} Player_t;

bool Player_Init(Player_t *player, PwmDriver_t *pwm);
/* Мелодія має жити весь час програвання (static const). Попередня зупиняється. */
bool Player_Play(Player_t *player, const Tone_t *notes, size_t count, bool repeat);
void Player_Stop(Player_t *player);
bool Player_IsPlaying(const Player_t *player);
void Player_Update(Player_t *player);

#define PLAYER_LENGTH(notes) (sizeof(notes) / sizeof((notes)[0]))

#endif
