#pragma once

#include <furi.h>
#include <notification/notification.h>

// void notify_init();
// void notify_free();

void notify_ball_released(void* ctx);
void notify_table_bump(void* ctx);
void notify_table_tilted(void* ctx);

void notify_error_message(void* ctx);
void notify_game_over(void* ctx);

void notify_bumper_hit(void* ctx);
void notify_rail_hit(void* ctx);

void notify_portal(void* ctx);
void notify_lost_life(void* ctx);

void notify_flipper(void* ctx);
