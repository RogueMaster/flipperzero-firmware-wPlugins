#pragma once

typedef struct SubGhzPlaylistCreator SubGhzPlaylistCreator;

// Callback type for file selection
typedef void (*SceneFileBrowserSelectCallback)(SubGhzPlaylistCreator* app, const char* path);

// Launch file browser scene, call on_select when a file is selected
void scene_file_browser_select(
    SubGhzPlaylistCreator* app,
    const char* start_dir,
    const char* extension,
    SceneFileBrowserSelectCallback on_select
);

void scene_file_browser_show(SubGhzPlaylistCreator* app);

// Add missing init view function declaration
void scene_file_browser_init_view(SubGhzPlaylistCreator* app); 