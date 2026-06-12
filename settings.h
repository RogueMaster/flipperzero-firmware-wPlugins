#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t lang; // 0 = espanol, 1 = ingles
    bool sound;
    bool vibro;
    bool led;
    uint8_t volume; // indice 0..3 -> 25..100%
    uint16_t tone_hz;
    uint8_t wpm; // 5..35
    uint16_t dit_ms; // soltar OK antes de esto = punto
    uint16_t letter_gap_ms; // pausa que confirma letra (0 = solo manual)
    uint16_t word_gap_ms; // pausa que agrega espacio (0 = desactivado)
} MorseSettings;

void morse_settings_default(MorseSettings* s);
void morse_settings_load(MorseSettings* s);
void morse_settings_save(const MorseSettings* s);
float morse_settings_volume_f(const MorseSettings* s);
