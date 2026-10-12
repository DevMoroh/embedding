#include "player/player.h"
#include "sine/sine.h"

static void Player_StartNote(Player_t *player) {
    const Tone_t *note = &player->sequence[player->index];
    if (note->frequency_hz == NOTE_REST) {
        Sine_Stop();
    } else {
        Sine_Init(player->pwm, note->frequency_hz);
    }
    player->note_start_ms = HAL_GetTick();
}

bool Player_Init(Player_t *player, PwmDriver_t *pwm) {
    if (player == NULL || pwm == NULL || !pwm->initialized) {
        return false;
    }

    player->pwm = pwm;
    player->sequence = NULL;
    player->length = 0;
    player->index = 0;
    player->note_start_ms = 0;
    player->repeat = false;
    player->playing = false;
    player->initialized = true;
    return true;
}

bool Player_Play(Player_t *player, const Tone_t *notes, size_t count, bool repeat) {
    if (player == NULL || !player->initialized || notes == NULL || count == 0) {
        return false;
    }

    Player_Stop(player);

    player->sequence = notes;
    player->length = count;
    player->index = 0;
    player->repeat = repeat;
    player->playing = true;
    Player_StartNote(player);
    return true;
}

void Player_Stop(Player_t *player) {
    if (player == NULL || !player->initialized) {
        return;
    }

    player->playing = false;
    player->sequence = NULL;
    player->length = 0;
    player->index = 0;
    Sine_Stop();
}

bool Player_IsPlaying(const Player_t *player) {
    return (player != NULL) ? player->playing : false;
}

void Player_Update(Player_t *player) {
    if (player == NULL || !player->playing) {
        return;
    }

    if (HAL_GetTick() - player->note_start_ms < player->sequence[player->index].duration_ms) {
        return;
    }

    player->index++;
    if (player->index >= player->length) {
        if (!player->repeat) {
            Player_Stop(player);
            return;
        }
        player->index = 0;
    }
    Player_StartNote(player);
}
