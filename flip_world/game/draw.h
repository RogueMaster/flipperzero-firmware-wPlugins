#pragma once
#include "game.h"
#include "game/icon.h"
#include <game/player.h>
extern int draw_camera_x;
extern int draw_camera_y;
void draw_user_stats(Canvas *canvas, Vector pos, GameManager *manager);
void draw_username(Canvas *canvas, Vector pos, char *username);
void draw_background_render(Canvas *canvas, GameManager *manager);