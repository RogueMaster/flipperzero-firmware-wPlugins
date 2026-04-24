/*
 * Size picker scene - streamlined for "Set Text" flow.
 * Only keeps Page, Polarity, Padding, and Transmit.
 */

#include "../tagtinker_app.h"
#include "../views/tagtinker_font.h"
#include "../protocol/tagtinker_proto.h"

enum {
    SettingPage,
    SettingPolarity,
    SettingPadding,
    SettingTransmit,
};

static uint8_t text_page_to_index(uint8_t page) {
    if(page == 0U) return 0U;
    if(page > 8U) return 7U;
    return (uint8_t)(page - 1U);
}

static void page_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->img_page = (uint8_t)(variable_item_get_current_value_index(item) + 1U);

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->img_page);
    variable_item_set_current_value_text(item, buf);
}

static void polarity_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->invert_text = (variable_item_get_current_value_index(item) == 1);

    variable_item_set_current_value_text(item, app->invert_text ? "W on B" : "B on W");
}

static void padding_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->text_padding_pct = index * 5; /* 0, 5, 10, 15, 20, 25, 30, 35, 40 % */

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", app->text_padding_pct);
    variable_item_set_current_value_text(item, buf);
}

static void setting_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;

    if(index != SettingTransmit) return;

    TagTinkerTarget* target = &app->targets[app->selected_target];

    /* Auto-set best settings based on profile */
    app->esl_width = target->profile.width;
    app->esl_height = target->profile.height;
    /* Default dot matrix to RLE if supported? PricehaxBT does this automatically. */
    app->compression_mode = TagTinkerCompressionAuto;
    app->signal_mode = TagTinkerSignalPP4;

    FURI_LOG_I(
        TAGTINKER_TAG,
        "TX: %ux%u pg=%u inv=%d pad=%u%% reps=%u",
        app->esl_width,
        app->esl_height,
        app->img_page,
        app->invert_text,
        app->text_padding_pct,
        app->data_frame_repeats);

    /* Auto-save to recents */
    tagtinker_recents_add(app, app->text_input_buf);

    tagtinker_prepare_text_tx(app, target->plid);
    app->tx_spam = false;
    scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
}

void tagtinker_scene_size_picker_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    app->signal_mode = TagTinkerSignalPP4;
    if(app->img_page == 0U) app->img_page = 1U;
    if(app->img_page > 8U) app->img_page = 8U;

    variable_item_list_reset(app->var_item_list);

    /* Page */
    VariableItem* item_pg =
        variable_item_list_add(app->var_item_list, "Page", 8, page_changed, app);
    variable_item_set_current_value_index(item_pg, text_page_to_index(app->img_page));
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", app->img_page);
        variable_item_set_current_value_text(item_pg, buf);
    }

    /* Polarity */
    VariableItem* item_col =
        variable_item_list_add(app->var_item_list, "Polarity", 2, polarity_changed, app);
    variable_item_set_current_value_index(item_col, app->invert_text ? 1 : 0);
    variable_item_set_current_value_text(item_col, app->invert_text ? "W on B" : "B on W");

    /* Padding */
    VariableItem* item_pad =
        variable_item_list_add(app->var_item_list, "Padding", 9, padding_changed, app);
    variable_item_set_current_value_index(item_pad, app->text_padding_pct / 5);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", app->text_padding_pct);
        variable_item_set_current_value_text(item_pad, buf);
    }

    /* Transmit button */
    variable_item_list_add(app->var_item_list, ">> Send to Tag <<", 0, NULL, app);

    variable_item_list_set_enter_callback(app->var_item_list, setting_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewVarItemList);
}

bool tagtinker_scene_size_picker_on_event(void* ctx, SceneManagerEvent event) {
    UNUSED(ctx);
    UNUSED(event);
    return false;
}

void tagtinker_scene_size_picker_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    variable_item_list_reset(app->var_item_list);
}
