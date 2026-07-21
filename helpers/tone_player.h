#pragma once

#include <notification/notification.h>
#include <stdint.h>

#include "progress.h"

typedef struct TonePlayer TonePlayer;

TonePlayer* tone_player_alloc(NotificationApp* notifications, const EarSettings* settings);
void tone_player_free(TonePlayer* player);

/* Play two notes back to back. Any sound already playing is cut short. */
void tone_player_play_interval(TonePlayer* player, uint8_t first_midi, uint8_t second_midi);

/* Play a single note (used by the reference screen). */
void tone_player_play_note(TonePlayer* player, uint8_t midi_note);

void tone_player_stop(TonePlayer* player);
