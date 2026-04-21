/**
 * @file flippass_scene_send_confirm.h
 * @brief Helpers for executing pending credential typing actions.
 */
#pragma once

#include <furi.h>
struct App;

void flippass_entry_action_prepare_pending(struct App* app);
bool flippass_entry_action_execute_pending(struct App* app, FuriString* error);
