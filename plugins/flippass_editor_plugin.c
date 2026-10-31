#include "flippass_editor_plugin.h"

#include "../scenes/flippass_scene_editor.h"

static const FlipPassEditorPluginV1 flippass_editor_plugin = {
    .api_version = FLIPPASS_EDITOR_PLUGIN_API_VERSION,
    .on_enter = flippass_scene_editor_on_enter,
    .on_event = flippass_scene_editor_on_event,
    .on_exit = flippass_scene_editor_on_exit,
    .text_input_on_enter = flippass_scene_editor_text_input_on_enter,
    .text_input_on_event = flippass_scene_editor_text_input_on_event,
    .text_input_on_exit = flippass_scene_editor_text_input_on_exit,
    .prepare_new_custom_field = flippass_editor_prepare_new_custom_field,
    .prepare_edit_custom_field = flippass_editor_prepare_edit_custom_field,
};

static const FlipperAppPluginDescriptor flippass_editor_descriptor = {
    .appid = FLIPPASS_EDITOR_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_EDITOR_PLUGIN_API_VERSION,
    .entry_point = &flippass_editor_plugin,
};

const FlipperAppPluginDescriptor* flippass_editor_plugin_ep(void) {
    return &flippass_editor_descriptor;
}
