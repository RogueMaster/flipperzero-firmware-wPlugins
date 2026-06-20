#include "../tagtinker_app.h"

enum {
    ImageOptionPage,
    ImageOptionTransmit,
};

static void image_option_page_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->image_tx_job.page = variable_item_get_current_value_index(item);
    app->img_page = app->image_tx_job.page;

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->image_tx_job.page);
    variable_item_set_current_value_text(item, buf);
}

static void image_option_enter_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;

    if(index != ImageOptionTransmit) return;

    app->tx_spam = false;
    scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
}

void tagtinker_scene_image_options_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    VariableItemList* list = app->var_item_list;
    TagTinkerImageTxJob* job = &app->image_tx_job;

    if(job->mode != TagTinkerTxModeBmpImage) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    variable_item_list_reset(list);

    VariableItem* item = variable_item_list_add(list, "Page", 8, image_option_page_changed, app);
    variable_item_set_current_value_index(item, job->page);
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", job->page);
        variable_item_set_current_value_text(item, buf);
    }

    /* Position, compression and frame-repeat are intentionally fixed here:
     * pages are the only knob that meaningfully changes per-image, the rest
     * are best left at their defaults (top-left, auto RLE, x2 frame repeat).
     * Power users can still tune them in Settings. */
    job->pos_x = 0;
    job->pos_y = 0;
    app->draw_x = 0;
    app->draw_y = 0;

    variable_item_list_add(list, ">> Send BMP <<", 0, NULL, app);
    variable_item_list_set_enter_callback(list, image_option_enter_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_image_options_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void tagtinker_scene_image_options_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    variable_item_list_reset(app->var_item_list);
}
