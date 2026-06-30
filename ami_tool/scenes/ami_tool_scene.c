#include "ami_tool_i.h"

/* Forward declarations of scene functions */
void ami_tool_scene_main_menu_on_enter(void* context);
bool ami_tool_scene_main_menu_on_event(void* context, SceneManagerEvent event);
void ami_tool_scene_main_menu_on_exit(void* context);

void ami_tool_scene_read_on_enter(void* context);
bool ami_tool_scene_read_on_event(void* context, SceneManagerEvent event);
void ami_tool_scene_read_on_exit(void* context);

void ami_tool_scene_generate_on_enter(void* context);
bool ami_tool_scene_generate_on_event(void* context, SceneManagerEvent event);
void ami_tool_scene_generate_on_exit(void* context);

void ami_tool_scene_saved_on_enter(void* context);
bool ami_tool_scene_saved_on_event(void* context, SceneManagerEvent event);
void ami_tool_scene_saved_on_exit(void* context);

void ami_tool_scene_amiibo_link_on_enter(void* context);
bool ami_tool_scene_amiibo_link_on_event(void* context, SceneManagerEvent event);
void ami_tool_scene_amiibo_link_on_exit(void* context);

void ami_tool_scene_bluetooth_on_enter(void* context);
bool ami_tool_scene_bluetooth_on_event(void* context, SceneManagerEvent event);
void ami_tool_scene_bluetooth_on_exit(void* context);

/* Arrays of handlers: index == AmiToolScene enum value */
static const AppSceneOnEnterCallback ami_tool_on_enter_handlers[AmiToolSceneCount] = {
    [AmiToolSceneMainMenu] = ami_tool_scene_main_menu_on_enter,
    [AmiToolSceneRead] = ami_tool_scene_read_on_enter,
    [AmiToolSceneGenerate] = ami_tool_scene_generate_on_enter,
    [AmiToolSceneSaved] = ami_tool_scene_saved_on_enter,
    [AmiToolSceneAmiiboLink] = ami_tool_scene_amiibo_link_on_enter,
    [AmiToolSceneBluetooth] = ami_tool_scene_bluetooth_on_enter,
};

static const AppSceneOnEventCallback ami_tool_on_event_handlers[AmiToolSceneCount] = {
    [AmiToolSceneMainMenu] = ami_tool_scene_main_menu_on_event,
    [AmiToolSceneRead] = ami_tool_scene_read_on_event,
    [AmiToolSceneGenerate] = ami_tool_scene_generate_on_event,
    [AmiToolSceneSaved] = ami_tool_scene_saved_on_event,
    [AmiToolSceneAmiiboLink] = ami_tool_scene_amiibo_link_on_event,
    [AmiToolSceneBluetooth] = ami_tool_scene_bluetooth_on_event,
};

static const AppSceneOnExitCallback ami_tool_on_exit_handlers[AmiToolSceneCount] = {
    [AmiToolSceneMainMenu] = ami_tool_scene_main_menu_on_exit,
    [AmiToolSceneRead] = ami_tool_scene_read_on_exit,
    [AmiToolSceneGenerate] = ami_tool_scene_generate_on_exit,
    [AmiToolSceneSaved] = ami_tool_scene_saved_on_exit,
    [AmiToolSceneAmiiboLink] = ami_tool_scene_amiibo_link_on_exit,
    [AmiToolSceneBluetooth] = ami_tool_scene_bluetooth_on_exit,
};

const SceneManagerHandlers ami_tool_scene_handlers = {
    .on_enter_handlers = ami_tool_on_enter_handlers,
    .on_event_handlers = ami_tool_on_event_handlers,
    .on_exit_handlers = ami_tool_on_exit_handlers,
    .scene_num = AmiToolSceneCount,
};
