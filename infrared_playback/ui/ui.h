#pragma once

#include <gui/modules/file_browser.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <storage/storage.h>
#include <notification/notification.h>

#define MAX_NUM_REMOTES 1024

typedef enum {
    RemoteSelectDisplay,
    RemotePlaybackDisplay,
    ui_count
} UIScene;

typedef enum {
    View_RemoteSelectDisplay,
    View_RemotePlaybackDisplay,
    view_count
} UIView;

typedef struct {
    FuriString* file_path;
    FileBrowser* file_browser;
} RemoteSelectScene;

typedef struct {
    View* view;
    
    uint16_t num_ir_payloads;
    uint16_t current_ir_payload;
    
    bool is_loading;
    bool has_error;
    FuriString* error_string;
    
    size_t remote_offsets[MAX_NUM_REMOTES];
} RemotePlaybackScene;

typedef struct {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    
    Storage* storage;
    NotificationApp* notifications;
    
    RemoteSelectScene* remote_select_scene;
    RemotePlaybackScene* remote_playback_scene;
} UI;

UI* ui_alloc();
void ui_start(UI* ui);
void ui_free(UI* ui);

RemoteSelectScene* remote_select_scene_alloc(UI* ui);
void remote_select_scene_on_enter(void* context);
bool remote_select_scene_on_event(void* context, SceneManagerEvent event);
void remote_select_scene_on_exit(void* context);
void remote_select_scene_free(RemoteSelectScene* scene);

RemotePlaybackScene* remote_playback_scene_alloc(UI* ui);
void remote_playback_scene_on_enter(void* context);
bool remote_playback_scene_on_event(void* context, SceneManagerEvent event);
void remote_playback_scene_on_exit(void* context);
void remote_playback_scene_free(RemotePlaybackScene* scene);
