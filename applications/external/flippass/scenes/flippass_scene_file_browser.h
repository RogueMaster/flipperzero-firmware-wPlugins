/**
 * @file flippass_scene_file_browser.h
 * @brief File browser scene for the FlipPass application.
 *
 * This file defines the handlers for the file browser scene, which allows
 * the user to select a database file.
 */
#pragma once

#include <gui/scene_manager.h>

// Forward declaration
struct App;

/**
 * @brief Handler for the on_enter event of the file browser scene.
 *
 * This function is called when the scene is entered. It configures and
 * starts the file browser.
 *
 * @param context The application context.
 */
void flippass_scene_file_browser_on_enter(void* context);

/**
 * @brief Handler for the on_event event of the file browser scene.
 *
 * This function is called when an event is triggered in the scene. It handles
 * custom events to transition to the next scene.
 *
 * @param context The application context.
 * @param event The event that was triggered.
 * @return True if the event was handled, false otherwise.
 */
bool flippass_scene_file_browser_on_event(void* context, SceneManagerEvent event);

/**
 * @brief Handler for the on_exit event of the file browser scene.
 *
 * This function is called when the scene is exited. It stops the file browser.
 *
 * @param context The application context.
 */
void flippass_scene_file_browser_on_exit(void* context);
