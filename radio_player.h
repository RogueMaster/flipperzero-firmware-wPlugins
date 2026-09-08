#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RadioPlayer RadioPlayer;

RadioPlayer* radio_player_alloc(void);
void radio_player_free(RadioPlayer* player);
bool radio_player_start(RadioPlayer* player);
void radio_player_request_stop(RadioPlayer* player);
void radio_player_stop(RadioPlayer* player);
bool radio_player_push(RadioPlayer* player, const uint8_t* data, size_t length);
bool radio_player_is_running(const RadioPlayer* player);
uint32_t radio_player_decoded_frames(const RadioPlayer* player);
size_t radio_player_buffered_bytes(RadioPlayer* player);
uint32_t radio_player_underflows(const RadioPlayer* player);
