#pragma once

#include "ui_internal.h"

typedef struct {
    uint8_t count;
    uint8_t xy[12];
} FrTileSprite;

bool fr_draw_terrain_sprite(
    Canvas* canvas,
    const FrGame* game,
    uint8_t map_x,
    uint8_t map_y,
    uint8_t screen_x,
    uint8_t screen_y,
    uint8_t anim_tick);

bool fr_draw_item_sprite(Canvas* canvas, const FrItem* item, uint8_t screen_x, uint8_t screen_y);

bool fr_draw_fire_overlay(
    Canvas* canvas,
    const FrGame* game,
    uint8_t map_x,
    uint8_t map_y,
    uint8_t screen_x,
    uint8_t screen_y,
    uint8_t anim_tick);
