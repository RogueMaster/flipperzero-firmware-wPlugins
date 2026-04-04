/**
 * @file flippass_scene_db_entries.h
 * @brief Database browser scene for the FlipPass application.
 *
 * This scene hosts the submenu-driven browser that lists KeePass groups and
 * entries after the database has been unlocked.
 */
#pragma once

#include <furi.h>
#include <gui/scene_manager.h>
// Forward declaration
struct App;

enum {
    FlipPassSceneDbEntriesEventLoadDatabase = 1,
    FlipPassSceneDbEntriesEventEnterSelected,
    FlipPassSceneDbEntriesEventLeaveCurrentGroup,
    FlipPassSceneDbEntriesEventOpenActionMenu,
    FlipPassSceneDbEntriesEventShowSelectedAction,
    FlipPassSceneDbEntriesEventExecuteUsbAction,
    FlipPassSceneDbEntriesEventExecuteBluetoothAction,
    FlipPassSceneDbEntriesEventOpenOtherFields,
    FlipPassSceneDbEntriesEventRunPendingAction,
    FlipPassSceneDbEntriesEventConfirmCloseDatabase = 0x100U,
};

/**
 * @brief Handler for the on_enter event of the database entries scene.
 *
 * This function is called when the scene is entered. It loads or refreshes the
 * unlocked database and displays the browser submenu.
 *
 * @param context The application context.
 */
void flippass_scene_db_entries_on_enter(void* context);

/**
 * @brief Handler for the on_event event of the database entries scene.
 *
 * This function is called when an event is triggered in the scene.
 *
 * @param context The application context.
 * @param event The event that was triggered.
 * @return True if the event was handled, false otherwise.
 */
bool flippass_scene_db_entries_on_event(void* context, SceneManagerEvent event);

/**
 * @brief Handler for the on_exit event of the database entries scene.
 *
 * This function is called when the scene is exited. It clears the shared
 * submenu so the next scene can rebuild it deterministically.
 *
 * @param context The application context.
 */
void flippass_scene_db_entries_on_exit(void* context);
