#include "../magicband_plus_lights.h"
#include "protocols/_protocols.h"

static void config_enter_cb(void* _ctx, uint32_t index) {
    Ctx* ctx = _ctx;
    uint8_t cnt = 0;
    if(ctx->attack && ctx->attack->protocol && ctx->attack->protocol->config_count) {
        cnt = ctx->attack->protocol->config_count(&ctx->attack->payload);
    }
    if(index == cnt) {
        ble_spam_toggle_adv(ctx);
        bool adv = ble_spam_is_advertising(ctx);
        if(adv && ctx->fast_mode) {
            ctx->config_in_prewarm = true;
            view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewMain);
        } else if(ctx->item_send) {
            variable_item_set_current_value_text(ctx->item_send, adv ? "LIVE" : ">");
        }
    }
}

void scene_config_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    VariableItemList* list = ctx->variable_item_list;
    variable_item_list_reset(list);
    ctx->item_pp_color = NULL;
    ctx->item_send = NULL;
    ctx->config_in_prewarm = false;

    if(ctx->attack && ctx->attack->protocol && ctx->attack->protocol->extra_config &&
       ctx->attack->protocol->config_count &&
       ctx->attack->protocol->config_count(&ctx->attack->payload) > 0) {
        ctx->fallback_config_enter = NULL;
        ctx->attack->protocol->extra_config(ctx);
    }

    bool adv = ble_spam_is_advertising(ctx);
    ctx->item_send = variable_item_list_add(list, "Send / Stop", 0, NULL, NULL);
    variable_item_set_current_value_text(ctx->item_send, adv ? "LIVE" : ">");

    variable_item_list_set_enter_callback(list, config_enter_cb, ctx);
    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(ctx->scene_manager, SceneConfig));
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewVariableItemList);
}

bool scene_config_on_event(void* _ctx, SceneManagerEvent event) {
    Ctx* ctx = _ctx;
    if(event.type == SceneManagerEventTypeTick) {
        if(ctx->config_in_prewarm && !ble_spam_is_warming(ctx)) {
            ctx->config_in_prewarm = false;
            view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewVariableItemList);
            if(ctx->item_send) {
                bool adv = ble_spam_is_advertising(ctx);
                variable_item_set_current_value_text(ctx->item_send, adv ? "LIVE" : ">");
            }
        }
        return true;
    }
    return false;
}

void scene_config_on_exit(void* _ctx) {
    Ctx* ctx = _ctx;
    ble_spam_stop_adv(ctx);
    ctx->config_in_prewarm = false;
    ctx->item_send = NULL;
    variable_item_list_reset(ctx->variable_item_list);
}
