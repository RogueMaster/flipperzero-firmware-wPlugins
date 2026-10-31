#pragma once

#include <furi.h>
#include <flipper_application/flipper_application.h>
#include <gui/scene_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

struct App;
struct KDBXCustomField;
struct FlipPassEditorCustomFieldDraft;

#define FLIPPASS_EDITOR_PLUGIN_APP_ID      "flippass_editor"
#define FLIPPASS_EDITOR_PLUGIN_API_VERSION 1u

/** The entry, group, database and config editor scenes; they call host functions directly. */
typedef struct {
    uint32_t api_version;
    void (*on_enter)(void* context);
    bool (*on_event)(void* context, SceneManagerEvent event);
    void (*on_exit)(void* context);
    void (*text_input_on_enter)(void* context);
    bool (*text_input_on_event)(void* context, SceneManagerEvent event);
    void (*text_input_on_exit)(void* context);
    void (*prepare_new_custom_field)(struct App* app);
    void (*prepare_edit_custom_field)(
        struct App* app,
        struct KDBXCustomField* field,
        struct FlipPassEditorCustomFieldDraft* draft);
} FlipPassEditorPluginV1;

const FlipperAppPluginDescriptor* flippass_editor_plugin_ep(void);

#ifdef __cplusplus
}
#endif
