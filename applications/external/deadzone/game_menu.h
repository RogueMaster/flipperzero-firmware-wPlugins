#pragma once
#include <engine/engine.h>
#include <engine/game_engine.h>
#include <furi_hal.h>

#include <notification/notification_messages.h>

#include <gui/view_holder.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <api_lock.h>

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool game_menu_game_selected;
extern bool game_menu_tutorial_selected;
extern bool game_menu_quit_selected;
extern FuriApiLock game_menu_exit_lock;
extern char username[100];
void game_settings_menu_button_callback(void* game_manager, uint32_t index);
void game_menu_button_callback(void* game_manager, uint32_t index);
void game_menu_open(GameManager* gameManager, bool reopen);

#ifdef __cplusplus
}
#endif
