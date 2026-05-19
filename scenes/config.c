#include "../ble_spam.h"
#include "protocols/_protocols.h"

// Per-attack config: ONLY protocol-specific options (no global settings here;
// those live in Settings).  Hold OK on any preset to open this.

void scene_config_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;
    VariableItemList* list = ctx->variable_item_list;
    variable_item_list_reset(list);
    ctx->item_pp_color = NULL;

    if(ctx->attack && ctx->attack->protocol && ctx->attack->protocol->extra_config
       && ctx->attack->protocol->config_count
       && ctx->attack->protocol->config_count(&ctx->attack->payload) > 0) {

        ctx->fallback_config_enter = NULL;
        ctx->attack->protocol->extra_config(ctx);

    } else {
        variable_item_list_add(list, "No options for this preset", 0, NULL, NULL);
    }

    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(ctx->scene_manager, SceneConfig));
    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewVariableItemList);
}

bool scene_config_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx); UNUSED(event);
    return false;
}

void scene_config_on_exit(void* _ctx) {
    Ctx* ctx = _ctx;
    variable_item_list_reset(ctx->variable_item_list);
}
