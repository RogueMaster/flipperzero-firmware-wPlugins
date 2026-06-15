#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <notification/notification.h>

#include "settings.h"

typedef enum {
    MorsePlayerStateIdle,
    MorsePlayerStatePlaying,
    MorsePlayerStatePaused,
    MorsePlayerStateFinished,
} MorsePlayerState;

typedef struct MorsePlayer MorsePlayer;
typedef void (*MorsePlayerCallback)(void* context);

// El reproductor lee sonido/vibracion/LED/volumen/tono/WPM de `settings`
// en vivo, asi que los cambios aplican durante la reproduccion.
MorsePlayer* morse_player_alloc(NotificationApp* notification, const MorseSettings* settings);
void morse_player_free(MorsePlayer* player);

// El callback se invoca desde el hilo de reproduccion cada vez que cambia
// el estado (posicion, simbolo, pausa, fin).
void morse_player_set_callback(MorsePlayer* player, MorsePlayerCallback callback, void* context);

// `text` debe seguir siendo valido mientras dure la reproduccion.
void morse_player_start(MorsePlayer* player, const char* text);
void morse_player_stop(MorsePlayer* player);
void morse_player_toggle_pause(MorsePlayer* player);

MorsePlayerState morse_player_get_state(const MorsePlayer* player);
size_t morse_player_get_position(const MorsePlayer* player);
int8_t morse_player_get_symbol(const MorsePlayer* player);
