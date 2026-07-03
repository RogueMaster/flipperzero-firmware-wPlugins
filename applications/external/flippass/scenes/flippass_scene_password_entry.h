/**
 * @file flippass_scene_password_entry.h
 * @brief Password entry scene for the FlipPass application.
 *
 * This file defines the handlers for the password entry scene, which allows
 * the user to input the database password.
 */
#pragma once

#include <gui/scene_manager.h>

// Forward declaration
struct App;

/**
 * @brief Handler for the on_enter event of the password entry scene.
 *
 * This function is called when the scene is entered. It configures and
 * displays the text input view for password entry.
 *
 * @param context The application context.
 */
void flippass_scene_password_entry_on_enter(void* context);

/**
 * @brief Handler for the on_event event of the password entry scene.
 *
 * This function is called when an event is triggered in the scene.
 *
 * @param context The application context.
 * @param event The event that was triggered.
 * @return True if the event was handled, false otherwise.
 */
bool flippass_scene_password_entry_on_event(void* context, SceneManagerEvent event);

/**
 * @brief Handler for the on_exit event of the password entry scene.
 *
 * This function is called when the scene is exited. It resets the text
 * input view.
 *
 * @param context The application context.
 */
void flippass_scene_password_entry_on_exit(void* context);
