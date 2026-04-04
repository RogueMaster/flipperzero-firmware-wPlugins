/**
 * @file flippass_scene.c
 * @brief Scene management implementation for the FlipPass application.
 *
 * This file defines the scene handlers for the application, mapping scene
 * enums to their corresponding on_enter, on_event, and on_exit handlers.
 */
#include "flippass_scene.h"
#include "../flippass.h"
#include "flippass_scene_db_entries.h"
#include "flippass_scene_file_browser.h"
#include "flippass_scene_other_field_actions.h"
#include "flippass_scene_other_fields.h"
#include "flippass_scene_password_entry.h"
#include "flippass_scene_status.h"
#include "flippass_scene_text_view.h"
#include "flippass_scene_vault_fallback.h"

/** @brief Array of on_enter handlers for each scene. */
void (*const flippass_scene_on_enter_handlers[])(void*) = {
    flippass_scene_file_browser_on_enter,
    flippass_scene_password_entry_on_enter,
    flippass_scene_db_entries_on_enter,
    flippass_scene_vault_fallback_on_enter,
    flippass_scene_other_fields_on_enter,
    flippass_scene_other_field_actions_on_enter,
    flippass_scene_text_view_on_enter,
    flippass_scene_status_on_enter,
};

/** @brief Array of on_event handlers for each scene. */
bool (*const flippass_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    flippass_scene_file_browser_on_event,
    flippass_scene_password_entry_on_event,
    flippass_scene_db_entries_on_event,
    flippass_scene_vault_fallback_on_event,
    flippass_scene_other_fields_on_event,
    flippass_scene_other_field_actions_on_event,
    flippass_scene_text_view_on_event,
    flippass_scene_status_on_event,
};

/** @brief Array of on_exit handlers for each scene. */
void (*const flippass_scene_on_exit_handlers[])(void*) = {
    flippass_scene_file_browser_on_exit,
    flippass_scene_password_entry_on_exit,
    flippass_scene_db_entries_on_exit,
    flippass_scene_vault_fallback_on_exit,
    flippass_scene_other_fields_on_exit,
    flippass_scene_other_field_actions_on_exit,
    flippass_scene_text_view_on_exit,
    flippass_scene_status_on_exit,
};

/** @brief Scene manager handlers structure. */
const SceneManagerHandlers flippass_scene_handlers = {
    .on_enter_handlers = flippass_scene_on_enter_handlers,
    .on_event_handlers = flippass_scene_on_event_handlers,
    .on_exit_handlers  = flippass_scene_on_exit_handlers,
    .scene_num         = FlipPassScene_Status + 1,
};
