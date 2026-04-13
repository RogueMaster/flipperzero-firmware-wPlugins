/**
 * @file flippass_scene.h
 * @brief Scene management for the FlipPass application.
 *
 * This file defines the scenes used in the application and declares the
 * scene manager handlers.
 */
#pragma once

#include <gui/scene_manager.h>

/**
 * @enum FlipPassScene
 * @brief Enumeration of the different scenes in the application.
 *
 * This enum is used to identify and manage the different scenes.
 */
typedef enum {
    FlipPassScene_FileBrowser, /**< The file browser scene. */
    FlipPassScene_PasswordEntry, /**< The password entry scene. */
    FlipPassScene_DbEntries, /**< The database browser scene. */
    FlipPassScene_VaultFallback, /**< Retry unlock using an encrypted session file. */
    FlipPassScene_OtherFields, /**< Select an alternate entry field to inspect or type. */
    FlipPassScene_OtherFieldActions, /**< Choose how to use the selected alternate field. */
    FlipPassScene_KeyboardLayout, /**< Select the typing layout before continuing a send. */
    FlipPassScene_TextView, /**< Shared long-text viewer for notes and fields. */
    FlipPassScene_Status, /**< The status and error scene. */
} FlipPassScene;

extern const SceneManagerHandlers flippass_scene_handlers; /**< Scene manager handlers. */
