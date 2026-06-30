#pragma once
#include <game/draw.h>
// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// World size (6x6)
#define WORLD_WIDTH 768
#define WORLD_HEIGHT 384

// Maximum number of world objects
#define MAX_WORLD_OBJECTS 25

const LevelBehaviour *world_pvp();
bool world_json_draw(GameManager *manager, Level *level, const FuriString *json_data);
FuriString *world_fetch(FlipperHTTP *fhttp, const char *name);