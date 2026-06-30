#pragma once
#include <game/player.h>
#include <game/game.h>
#include <flip_world.h>
#include <flip_storage/storage.h>

bool save_uint32(const char *path_name, uint32_t value);
bool save_player_context(PlayerContext *player_context);
bool save_player_context_api(PlayerContext *player_context, FlipperHTTP *fhttp);
bool websocket_player_context(PlayerContext *player_context, FlipperHTTP *fhttp);
bool remove_player_from_lobby(FlipperHTTP *fhttp);
bool load_uint32(const char *path_name, uint32_t *value);
bool load_player_context(PlayerContext *player_context);
bool set_player_context();

// save the json_data and enemy_data to separate files
bool separate_world_data(char *id, FuriString *world_data);