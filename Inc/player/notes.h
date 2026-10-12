#ifndef NOTES_H
#define NOTES_H

#include <stdint.h>

// Equal-tempered notes (A4 = 440 Hz), rounded to whole hertz.
enum note_frequency_t : uint32_t {
    NOTE_REST = 0,
    NOTE_C4 = 262,
    NOTE_D4 = 294,
    NOTE_E4 = 330,
    NOTE_F4 = 349,
    NOTE_G4 = 392,
    NOTE_A4 = 440,
    NOTE_B4 = 494,
    NOTE_C5 = 523,
    NOTE_E5 = 659,
    NOTE_G5 = 784,
    NOTE_A5 = 880,
};

#endif // NOTES_H
