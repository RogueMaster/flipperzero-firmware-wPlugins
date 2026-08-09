#pragma once

#include <notification/notification.h>
#include <stdint.h>

#include "progress.h"

#define MAX_SEQUENCE 12

typedef struct TonePlayer TonePlayer;

TonePlayer* tone_player_alloc(NotificationApp* notifications, const EarSettings* settings);
void tone_player_free(TonePlayer* player);

/** Play a run of notes back to back. Anything already sounding is cut short.
 *
 * Chords arrive here as arpeggios rather than block chords: the speaker can
 * only hold one frequency at a time.
 *
 * @param gap_ms  silence between notes; 0 uses the default
 */
void tone_player_play_sequence(
    TonePlayer* player,
    const uint8_t* midi_notes,
    uint8_t count,
    uint16_t gap_ms);

/** Convenience wrapper for the two-note interval case. */
void tone_player_play_interval(TonePlayer* player, uint8_t first_midi, uint8_t second_midi);

/** Play a single note (used by the reference screen). */
void tone_player_play_note(TonePlayer* player, uint8_t midi_note);

void tone_player_stop(TonePlayer* player);
