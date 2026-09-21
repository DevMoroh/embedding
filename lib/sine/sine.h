#ifndef LIB_SINE_SINE_H
#define LIB_SINE_SINE_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "pwm.h"

#define SINE_TABLE_SIZE 32
#define PLAYER_TICK_MS 50U

typedef struct {
	pwm_t *pwm;
	esp_timer_handle_t timer;
	uint32_t frequency_hz;
	uint32_t pwm_frequency_hz;
	uint32_t sample_period_us;
	uint8_t table_index;
	bool initialized;
	bool running;
} sine_t;

typedef struct {
	uint32_t frequency_hz;
	uint32_t duration_ms;
} tone_t;

typedef struct {
	sine_t sine;
	esp_timer_handle_t tick_timer;
	const tone_t *sequence;
	size_t sequence_len;
	size_t sequence_index;
	uint32_t remaining_ms;
	bool initialized;
	bool playing;
	bool repeat;
} player_t;

esp_err_t sine_init(sine_t *sine, pwm_t *pwm, uint32_t frequency_hz);
esp_err_t sine_deinit(sine_t *sine);
esp_err_t sine_start(sine_t *sine);
esp_err_t sine_stop(sine_t *sine);
esp_err_t sine_set_frequency(sine_t *sine, uint32_t frequency_hz);

esp_err_t player_init(player_t *player, pwm_t *pwm);
esp_err_t player_deinit(player_t *player);
esp_err_t player_play_note(player_t *player, uint32_t frequency_hz, uint32_t duration_ms);
// Objects and notes must outlive playback. Durations round up to PLAYER_TICK_MS.
// Stop playback before starting another note/sequence. Call from a serialized
// task context (not an ISR); the player does not create a FreeRTOS task.
esp_err_t player_play_sequence(player_t *player, const tone_t *notes, size_t note_count,
	bool repeat = false);
esp_err_t player_stop(player_t *player);

#endif // LIB_SINE_SINE_H
