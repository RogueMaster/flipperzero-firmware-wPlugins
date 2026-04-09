#include "../tagtinker_app.h"

enum {
    ImageOptionPage,
    ImageOptionOffsetX,
    ImageOptionOffsetY,
    ImageOptionCompression,
    ImageOptionFrameRepeat,
    ImageOptionTransmit,
};

static const char* image_option_compression_labels[] = {"Auto", "Raw", "RLE"};
static const uint16_t image_option_coord_values[] = {
    0,   8,   16,  24,  32,  40,  48,  56,  64,  72,  80,  88,  96,  104, 112, 120, 128,
    136, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 232, 240, 248, 256, 264,
    272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384, 392, 400,
    408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512, 520, 528, 536,
    544, 552, 560, 568, 576, 584, 592, 600, 608, 616, 624, 632, 640, 648, 656, 664, 672,
    680, 688, 696, 704, 712, 720, 728, 736, 744, 752, 760, 768, 776, 784, 792, 800,
};

#define IMAGE_OPTION_COORD_COUNT COUNT_OF(image_option_coord_values)

static uint8_t image_option_nearest_coord_index(uint16_t value) {
    uint8_t best_idx = 0;
    uint16_t best_diff = UINT16_MAX;

    for(uint8_t i = 0; i < IMAGE_OPTION_COORD_COUNT; i++) {
        uint16_t current = image_option_coord_values[i];
        uint16_t diff = (current > value) ? (current - value) : (value - current);
        if(diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    return best_idx;
}

static void image_option_clamp_position(TagTinkerApp* app) {
    if(!app) return;
    if(app->selected_target < 0 || app->selected_target >= app->target_count) return;

    TagTinkerImageTxJob* job = &app->image_tx_job;
    const TagTinkerTarget* target = &app->targets[app->selected_target];

    if(!target->profile.known || !target->profile.width || !target->profile.height) return;

    uint16_t max_x = (job->width < target->profile.width) ?
                         (uint16_t)(target->profile.width - job->width) :
                         0U;
    uint16_t max_y = (job->height < target->profile.height) ?
                         (uint16_t)(target->profile.height - job->height) :
                         0U;

    if(job->pos_x > max_x) job->pos_x = max_x;
    if(job->pos_y > max_y) job->pos_y = max_y;
}

static void image_option_page_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    app->image_tx_job.page = variable_item_get_current_value_index(item);
    app->img_page = app->image_tx_job.page;

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->image_tx_job.page);
    variable_item_set_current_value_text(item, buf);
}

static void image_option_offset_x_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= IMAGE_OPTION_COORD_COUNT) index = IMAGE_OPTION_COORD_COUNT - 1U;

    app->image_tx_job.pos_x = image_option_coord_values[index];
    image_option_clamp_position(app);
    app->draw_x = app->image_tx_job.pos_x;

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->image_tx_job.pos_x);
    variable_item_set_current_value_text(item, buf);
}

static void image_option_offset_y_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= IMAGE_OPTION_COORD_COUNT) index = IMAGE_OPTION_COORD_COUNT - 1U;

    app->image_tx_job.pos_y = image_option_coord_values[index];
    image_option_clamp_position(app);
    app->draw_y = app->image_tx_job.pos_y;

    char buf[8];
    snprintf(buf, sizeof(buf), "%u", app->image_tx_job.pos_y);
    variable_item_set_current_value_text(item, buf);
}

static void image_option_compression_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index > TagTinkerCompressionRle) index = TagTinkerCompressionRle;

    app->compression_mode = (TagTinkerCompressionMode)index;
    variable_item_set_current_value_text(item, image_option_compression_labels[index]);
}

static void image_option_frame_repeat_changed(VariableItem* item) {
    TagTinkerApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    app->data_frame_repeats = index + 1U;

    char buf[4];
    snprintf(buf, sizeof(buf), "%u", app->data_frame_repeats);
    variable_item_set_current_value_text(item, buf);
}

static void image_option_enter_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;

    if(index != ImageOptionTransmit) return;

    image_option_clamp_position(app);
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

    image_option_clamp_position(app);
    variable_item_list_reset(list);

    VariableItem* item =
        variable_item_list_add(list, "Page", 8, image_option_page_changed, app);
    variable_item_set_current_value_index(item, job->page);
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", job->page);
        variable_item_set_current_value_text(item, buf);
    }

    item = variable_item_list_add(
        list, "Offset X", IMAGE_OPTION_COORD_COUNT, image_option_offset_x_changed, app);
    variable_item_set_current_value_index(item, image_option_nearest_coord_index(job->pos_x));
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", job->pos_x);
        variable_item_set_current_value_text(item, buf);
    }

    item = variable_item_list_add(
        list, "Offset Y", IMAGE_OPTION_COORD_COUNT, image_option_offset_y_changed, app);
    variable_item_set_current_value_index(item, image_option_nearest_coord_index(job->pos_y));
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", job->pos_y);
        variable_item_set_current_value_text(item, buf);
    }

    item = variable_item_list_add(
        list, "Compression", 3, image_option_compression_changed, app);
    variable_item_set_current_value_index(item, app->compression_mode);
    variable_item_set_current_value_text(item, image_option_compression_labels[app->compression_mode]);

    item = variable_item_list_add(
        list, "Frame Repeat", 5, image_option_frame_repeat_changed, app);
    variable_item_set_current_value_index(item, app->data_frame_repeats - 1U);
    {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", app->data_frame_repeats);
        variable_item_set_current_value_text(item, buf);
    }

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
