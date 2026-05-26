#include "../ble_spam.h"

typedef enum {
    SettingsFastMode = 0,
    SettingsPrewarm,
    SettingsLED,
    SettingsLock,
} SettingsItem;

static void on_fast_mode(VariableItem* item) {
    Ctx* ctx = variable_item_get_context(item);
    ctx->fast_mode = (bool)variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, ctx->fast_mode ? "ON" : "OFF");
}

static void on_prewarm(VariableItem* item) {
    Ctx* ctx = variable_item_get_context(item);
    ctx->fast_mode_prewarm_idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, mb_prewarm_names[ctx->fast_mode_prewarm_idx]);
}

static void on_led(VariableItem* item) {
    Ctx* ctx = variable_item_get_context(item);
    ctx->led_indicator = (bool)variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, ctx->led_indicator ? "ON" : "OFF");
}

static void settings_enter_cb(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    if(index == SettingsLock) {
        ctx->lock_keyboard = true;
        view_dispatcher_send_custom_event(ctx->view_dispatcher, 0);
        notification_message_block(ctx->notification, &sequence_display_backlight_off);
    }
}

void scene_settings_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    VariableItemList* list = ctx->variable_item_list;
    VariableItem* item;
    variable_item_list_reset(list);
    // Fast Mode
    item = variable_item_list_add(list, "Fast Mode", 2, on_fast_mode, ctx);
    variable_item_set_current_value_index(item, ctx->fast_mode ? 1 : 0);
    variable_item_set_current_value_text(item, ctx->fast_mode ? "ON" : "OFF");

    // Pre-warm time (only relevant when Fast Mode is ON)
    item = variable_item_list_add(list, "Fast Prewarm", MB_PREWARM_OPT_COUNT, on_prewarm, ctx);
    variable_item_set_current_value_index(item, ctx->fast_mode_prewarm_idx);
    variable_item_set_current_value_text(item, mb_prewarm_names[ctx->fast_mode_prewarm_idx]);

    // LED Indicator
    item = variable_item_list_add(list, "LED Indicator", 2, on_led, ctx);
    variable_item_set_current_value_index(item, ctx->led_indicator ? 1 : 0);
    variable_item_set_current_value_text(item, ctx->led_indicator ? "ON" : "OFF");

    // Lock Keyboard (enter to activate)
    variable_item_list_add(list, "Lock Keyboard", 0, NULL, NULL);

    variable_item_list_set_enter_callback(list, settings_enter_cb, ctx);
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewVariableItemList);
}

bool scene_settings_on_event(void* _ctx, SceneManagerEvent event) {
    Ctx* ctx = _ctx;
    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_previous_scene(ctx->scene_manager);
        return true;
    }
    return false;
}

void scene_settings_on_exit(void* _ctx) {
    Ctx* ctx = _ctx;
    variable_item_list_reset(ctx->variable_item_list);
}
