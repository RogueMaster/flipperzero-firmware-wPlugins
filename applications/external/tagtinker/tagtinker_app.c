/*
 * TagTinker - ESL Flipper Zero application
 *
 * Transmit infrared commands to supported ESL displays
 * using the built-in IR LED at 1.255 MHz carrier.
 *
 * App by I12BP8 - github.com/i12bp8
 * Research by furrtek - github.com/furrtek
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tagtinker_app.h"

static bool navigation_cb(void* ctx) {
    TagTinkerApp* app = ctx;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void tick_cb(void* ctx) {
    TagTinkerApp* app = ctx;
    scene_manager_handle_tick_event(app->scene_manager);
}

static bool custom_event_cb(void* ctx, uint32_t event) {
    TagTinkerApp* app = ctx;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

extern const SceneManagerHandlers tagtinker_scene_handlers;

#define TAGTINKER_STREAM_PIXEL_BUDGET 24576U

static void tagtinker_clamp_region_to_target(
    const TagTinkerApp* app,
    uint16_t width,
    uint16_t height,
    uint16_t* pos_x,
    uint16_t* pos_y) {
    if(!app || !pos_x || !pos_y) return;
    if(app->selected_target < 0 || app->selected_target >= app->target_count) return;

    const TagTinkerTarget* target = &app->targets[app->selected_target];
    if(!target->profile.known || !target->profile.width || !target->profile.height) return;

    uint16_t max_x =
        (width < target->profile.width) ? (uint16_t)(target->profile.width - width) : 0U;
    uint16_t max_y =
        (height < target->profile.height) ? (uint16_t)(target->profile.height - height) : 0U;

    if(*pos_x > max_x) *pos_x = max_x;
    if(*pos_y > max_y) *pos_y = max_y;
}

void tagtinker_target_refresh_profile(TagTinkerTarget* target) {
    if(!target) return;

    memset(&target->profile, 0, sizeof(target->profile));
    tagtinker_barcode_to_profile(target->barcode, &target->profile);
}

void tagtinker_target_set_default_name(TagTinkerTarget* target) {
    if(!target) return;

    if(strlen(target->barcode) < 6U) {
        snprintf(target->name, sizeof(target->name), "Tag");
        return;
    }

    char suffix[7];
    memcpy(suffix, target->barcode + TAGTINKER_BC_LEN - 6, 6);
    suffix[6] = '\0';
    snprintf(target->name, sizeof(target->name), "Tag ...%s", suffix);
}

int8_t tagtinker_find_target_by_barcode(const TagTinkerApp* app, const char* barcode) {
    if(!app || !barcode || !*barcode) return -1;

    for(uint8_t i = 0; i < app->target_count; i++) {
        if(strcmp(app->targets[i].barcode, barcode) == 0) {
            return (int8_t)i;
        }
    }

    return -1;
}

int8_t tagtinker_ensure_target(TagTinkerApp* app, const char* barcode) {
    if(!app || !barcode) return -1;

    int8_t existing = tagtinker_find_target_by_barcode(app, barcode);
    if(existing >= 0) return existing;
    if(app->target_count >= TAGTINKER_MAX_TARGETS) return -1;

    TagTinkerTarget* target = &app->targets[app->target_count];
    memset(target, 0, sizeof(*target));
    strncpy(target->barcode, barcode, TAGTINKER_BC_LEN);
    target->barcode[TAGTINKER_BC_LEN] = '\0';

    if(!tagtinker_barcode_to_plid(target->barcode, target->plid)) {
        memset(target, 0, sizeof(*target));
        return -1;
    }

    tagtinker_target_set_default_name(target);
    tagtinker_target_refresh_profile(target);
    app->target_count++;
    tagtinker_targets_save(app);
    return (int8_t)(app->target_count - 1U);
}

bool tagtinker_find_latest_synced_image(
    const TagTinkerApp* app,
    const char* barcode,
    TagTinkerSyncedImage* image) {
    if(!app || !barcode || !*barcode || !image) return false;

    bool found = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("synced_images.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size > 0U && size < 8192U) {
            char* buf = malloc((size_t)size + 1U);
            if(buf) {
                uint16_t read = storage_file_read(file, buf, (uint16_t)size);
                buf[read] = '\0';

                char* line = buf;
                while(line && *line) {
                    char* nl = strchr(line, '\n');
                    if(nl) *nl = '\0';

                    if(*line) {
                        char* cursor = line;
                        char* job_id = cursor;
                        char* current_barcode = strchr(cursor, '|');
                        if(current_barcode) *current_barcode++ = '\0';
                        char* width = current_barcode ? strchr(current_barcode, '|') : NULL;
                        if(width) *width++ = '\0';
                        char* height = width ? strchr(width, '|') : NULL;
                        if(height) *height++ = '\0';
                        char* page = height ? strchr(height, '|') : NULL;
                        if(page) *page++ = '\0';
                        char* path = page ? strchr(page, '|') : NULL;
                        if(path) *path++ = '\0';

                        if(job_id && current_barcode && width && height && page && path &&
                           strcmp(current_barcode, barcode) == 0 && storage_common_exists(storage, path)) {
                            strncpy(image->job_id, job_id, TAGTINKER_SYNC_JOB_ID_LEN);
                            image->job_id[TAGTINKER_SYNC_JOB_ID_LEN] = '\0';
                            strncpy(image->barcode, current_barcode, TAGTINKER_BC_LEN);
                            image->barcode[TAGTINKER_BC_LEN] = '\0';
                            image->width = (uint16_t)atoi(width);
                            image->height = (uint16_t)atoi(height);
                            image->page = (uint8_t)atoi(page);
                            strncpy(image->image_path, path, TAGTINKER_IMAGE_PATH_LEN);
                            image->image_path[TAGTINKER_IMAGE_PATH_LEN] = '\0';
                            found = true;
                        }
                    }

                    line = nl ? (nl + 1) : NULL;
                }

                free(buf);
            }
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return found;
}

size_t tagtinker_delete_synced_images_for_barcode(TagTinkerApp* app, const char* barcode) {
    UNUSED(app);

    if(!barcode || !*barcode) return 0U;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    size_t removed_count = 0U;

    if(storage_file_open(file, APP_DATA_PATH("synced_images.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size > 0U && size < 8192U) {
            char* input = malloc((size_t)size + 1U);
            char* output = malloc((size_t)size + 1U);
            if(input && output) {
                uint16_t read = storage_file_read(file, input, (uint16_t)size);
                input[read] = '\0';
                output[0] = '\0';
                size_t output_len = 0U;

                char* line = input;
                while(line && *line) {
                    char* nl = strchr(line, '\n');
                    if(nl) *nl = '\0';

                    if(*line) {
                        char line_copy[384];
                        snprintf(line_copy, sizeof(line_copy), "%s", line);

                        char* cursor = line;
                        char* job_id = cursor;
                        char* current_barcode = strchr(cursor, '|');
                        if(current_barcode) *current_barcode++ = '\0';
                        char* width = current_barcode ? strchr(current_barcode, '|') : NULL;
                        if(width) *width++ = '\0';
                        char* height = width ? strchr(width, '|') : NULL;
                        if(height) *height++ = '\0';
                        char* page = height ? strchr(height, '|') : NULL;
                        if(page) *page++ = '\0';
                        char* path = page ? strchr(page, '|') : NULL;
                        if(path) *path++ = '\0';

                        bool matches =
                            job_id && current_barcode && width && height && page && path &&
                            strcmp(current_barcode, barcode) == 0;
                        if(matches) {
                            storage_common_remove(storage, path);
                            removed_count++;
                        } else {
                            size_t line_len = strlen(line_copy);
                            if((output_len + line_len + 1U) < ((size_t)size + 1U)) {
                                memcpy(output + output_len, line_copy, line_len);
                                output_len += line_len;
                                output[output_len++] = '\n';
                                output[output_len] = '\0';
                            }
                        }
                    }

                    line = nl ? (nl + 1) : NULL;
                }

                storage_file_close(file);
                if(output_len == 0U) {
                    storage_common_remove(storage, APP_DATA_PATH("synced_images.txt"));
                } else if(
                    storage_file_open(
                        file, APP_DATA_PATH("synced_images.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    storage_file_write(file, output, (uint16_t)output_len);
                    storage_file_close(file);
                }
            } else {
                storage_file_close(file);
            }

            free(output);
            free(input);
        } else {
            storage_file_close(file);
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return removed_count;
}

bool tagtinker_delete_target(TagTinkerApp* app, uint8_t index) {
    if(!app || index >= app->target_count) return false;

    tagtinker_delete_synced_images_for_barcode(app, app->targets[index].barcode);

    if(index + 1U < app->target_count) {
        memmove(
            &app->targets[index],
            &app->targets[index + 1U],
            sizeof(TagTinkerTarget) * (size_t)(app->target_count - index - 1U));
    }
    memset(&app->targets[app->target_count - 1U], 0, sizeof(TagTinkerTarget));
    app->target_count--;
    app->selected_target = -1;
    app->barcode[0] = '\0';
    memset(app->plid, 0, sizeof(app->plid));
    app->barcode_valid = false;

    return tagtinker_targets_save(app);
}

bool tagtinker_target_supports_graphics(const TagTinkerTarget* target) {
    if(!target) return false;

    return target->profile.kind != TagTinkerTagKindSegment;
}

bool tagtinker_target_supports_accent(const TagTinkerTarget* target) {
    if(!target) return false;

    return target->profile.color == TagTinkerTagColorRed ||
           target->profile.color == TagTinkerTagColorYellow;
}

const char* tagtinker_profile_kind_label(TagTinkerTagKind kind) {
    switch(kind) {
    case TagTinkerTagKindDotMatrix:
        return "Dot Matrix";
    case TagTinkerTagKindSegment:
        return "Segment";
    default:
        return "Unknown";
    }
}

const char* tagtinker_profile_color_label(TagTinkerTagColor color) {
    switch(color) {
    case TagTinkerTagColorRed:
        return "Red";
    case TagTinkerTagColorYellow:
        return "Yellow";
    default:
        return "Mono";
    }
}

void tagtinker_free_frame_sequence(TagTinkerApp* app) {
    if(!app || !app->frame_sequence) return;

    for(size_t i = 0; i < app->frame_seq_count; i++) {
        free(app->frame_sequence[i]);
    }

    free(app->frame_sequence);
    free(app->frame_lengths);
    free(app->frame_repeats);
    app->frame_sequence = NULL;
    app->frame_lengths = NULL;
    app->frame_repeats = NULL;
    app->frame_seq_count = 0;
}

uint16_t tagtinker_pick_chunk_height(uint16_t width, bool color_clear) {
    if(width == 0) return 1;

    size_t plane_budget = color_clear ? (TAGTINKER_STREAM_PIXEL_BUDGET / 2U) : TAGTINKER_STREAM_PIXEL_BUDGET;
    uint16_t chunk_h = (uint16_t)(plane_budget / width);
    if(chunk_h == 0) chunk_h = 1;
    return chunk_h;
}

void tagtinker_prepare_text_tx(TagTinkerApp* app, const uint8_t plid[4]) {
    if(!app) return;

    tagtinker_free_frame_sequence(app);
    memset(&app->image_tx_job, 0, sizeof(app->image_tx_job));
    app->image_tx_job.mode = TagTinkerTxModeTextImage;
    memcpy(app->image_tx_job.plid, plid, sizeof(app->image_tx_job.plid));
    app->image_tx_job.page = app->img_page;
    app->image_tx_job.width = app->esl_width;
    app->image_tx_job.height = app->esl_height;
    app->image_tx_job.pos_x = app->draw_x;
    app->image_tx_job.pos_y = app->draw_y;
    tagtinker_clamp_region_to_target(
        app,
        app->image_tx_job.width,
        app->image_tx_job.height,
        &app->image_tx_job.pos_x,
        &app->image_tx_job.pos_y);
}

void tagtinker_prepare_bmp_tx(
    TagTinkerApp* app,
    const uint8_t plid[4],
    const char* image_path,
    uint16_t width,
    uint16_t height,
    uint8_t page) {
    if(!app) return;

    tagtinker_free_frame_sequence(app);
    memset(&app->image_tx_job, 0, sizeof(app->image_tx_job));
    app->image_tx_job.mode = TagTinkerTxModeBmpImage;
    memcpy(app->image_tx_job.plid, plid, sizeof(app->image_tx_job.plid));
    app->image_tx_job.page = page;
    app->image_tx_job.width = width;
    app->image_tx_job.height = height;
    app->image_tx_job.pos_x = app->draw_x;
    app->image_tx_job.pos_y = app->draw_y;
    tagtinker_clamp_region_to_target(
        app,
        app->image_tx_job.width,
        app->image_tx_job.height,
        &app->image_tx_job.pos_x,
        &app->image_tx_job.pos_y);
    if(image_path) {
        strncpy(app->image_tx_job.image_path, image_path, TAGTINKER_IMAGE_PATH_LEN);
        app->image_tx_job.image_path[TAGTINKER_IMAGE_PATH_LEN] = '\0';
    }
}

void tagtinker_select_target(TagTinkerApp* app, uint8_t index) {
    if(!app || index >= app->target_count) return;

    TagTinkerTarget* target = &app->targets[index];
    app->selected_target = (int8_t)index;
    memcpy(app->barcode, target->barcode, TAGTINKER_BC_LEN + 1);
    memcpy(app->plid, target->plid, sizeof(target->plid));
    app->barcode_valid = true;

    if(target->profile.known && target->profile.kind == TagTinkerTagKindDotMatrix &&
       target->profile.width && target->profile.height) {
        app->esl_width = target->profile.width;
        app->esl_height = target->profile.height;
    }
}

void tagtinker_settings_load(TagTinkerApp* app) {
    app->show_startup_warning = true;
    app->signal_mode = TagTinkerSignalPP4;
    app->compression_mode = TagTinkerCompressionAuto;
    app->data_frame_repeats = 3;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("settings.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[64];
        uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[read] = '\0';

        if(strchr(buf, '=')) {
            char* line = buf;
            while(line && *line) {
                char* nl = strchr(line, '\n');
                if(nl) *nl = '\0';

                if(strncmp(line, "warning=", 8) == 0) {
                    app->show_startup_warning = (line[8] != '0');
                } else if(strncmp(line, "signal=", 7) == 0) {
                    int value = atoi(line + 7);
                    app->signal_mode = (value == 2) ? TagTinkerSignalPP16 : TagTinkerSignalPP4;
                } else if(strncmp(line, "compression=", 12) == 0) {
                    int value = atoi(line + 12);
                    if(value >= TagTinkerCompressionAuto && value <= TagTinkerCompressionRle) {
                        app->compression_mode = (TagTinkerCompressionMode)value;
                    }
                } else if(strncmp(line, "frame_repeat=", 13) == 0) {
                    int value = atoi(line + 13);
                    if(value >= 1 && value <= 5) {
                        app->data_frame_repeats = (uint8_t)value;
                    }
                } else if(strncmp(line, "draw_x=", 7) == 0) {
                    int value = atoi(line + 7);
                    if(value >= 0) app->draw_x = (uint16_t)value;
                } else if(strncmp(line, "draw_y=", 7) == 0) {
                    int value = atoi(line + 7);
                    if(value >= 0) app->draw_y = (uint16_t)value;
                }

                line = nl ? nl + 1 : NULL;
            }
        } else if(read >= 1) {
            app->show_startup_warning = (buf[0] != '0');
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool tagtinker_settings_save(const TagTinkerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, APP_DATA_PATH("settings.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[128];
        int len = snprintf(
            buf,
            sizeof(buf),
            "warning=%d\nsignal=%d\ncompression=%d\nframe_repeat=%u\ndraw_x=%u\ndraw_y=%u\n",
            app->show_startup_warning ? 1 : 0,
            (int)app->signal_mode,
            (int)app->compression_mode,
            app->data_frame_repeats,
            app->draw_x,
            app->draw_y);
        ok = (len > 0) && storage_file_write(file, buf, (uint16_t)len);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void tagtinker_targets_load(TagTinkerApp* app) {
    app->target_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("targets.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[768];
        uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[read] = '\0';
        storage_file_close(file);

        char* line = buf;
        while(line && *line && app->target_count < TAGTINKER_MAX_TARGETS) {
            char* nl = strchr(line, '\n');
            if(nl) *nl = '\0';

            if(*line) {
                char* sep = strchr(line, '|');
                if(sep) *sep = '\0';

                if(tagtinker_barcode_to_plid(line, app->targets[app->target_count].plid)) {
                    TagTinkerTarget* target = &app->targets[app->target_count];
                    strncpy(target->barcode, line, TAGTINKER_BC_LEN);
                    target->barcode[TAGTINKER_BC_LEN] = '\0';
                    memset(target->name, 0, sizeof(target->name));

                    if(sep && *(sep + 1)) {
                        strncpy(target->name, sep + 1, TAGTINKER_TARGET_NAME_LEN);
                        target->name[TAGTINKER_TARGET_NAME_LEN] = '\0';
                    } else {
                        tagtinker_target_set_default_name(target);
                    }

                    tagtinker_target_refresh_profile(target);
                    app->target_count++;
                }
            }

            line = nl ? nl + 1 : NULL;
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool tagtinker_targets_save(const TagTinkerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, APP_DATA_PATH("targets.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = true;
        for(uint8_t i = 0; i < app->target_count; i++) {
            char line[64];
            int len = snprintf(
                line,
                sizeof(line),
                "%s|%s\n",
                app->targets[i].barcode,
                app->targets[i].name);

            if(len <= 0 || !storage_file_write(file, line, (uint16_t)len)) {
                ok = false;
                break;
            }
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static TagTinkerApp* app_alloc(void) {
    TagTinkerApp* app = malloc(sizeof(TagTinkerApp));
    memset(app, 0, sizeof(TagTinkerApp));

    /* Defaults */
    app->page = 0;
    app->duration = 15;
    app->repeats = 200;
    app->draw_x = 0;
    app->draw_y = 0;
    app->img_page = 1;
    app->esl_width = 200;
    app->esl_height = 80;
    app->color_clear = false;
    app->invert_text = false;
    strcpy(app->text_input_buf, "TagTinker");
    app->selected_target = -1;
    app->ble_sync_ready_target = -1;
    tagtinker_settings_load(app);
    tagtinker_targets_load(app);

    /* Scene manager */
    app->scene_manager = scene_manager_alloc(&tagtinker_scene_handlers, app);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_cb);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tick_cb, 50);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_cb);

    /* GUI */
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Notifications */
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->bt = furi_record_open(RECORD_BT);

    /* Views */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewWidget, widget_get_view(app->widget));

    app->numlock = numlock_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewNumlock, numlock_input_get_view(app->numlock));

    app->warning_view = view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewWarning, app->warning_view);

    app->transmit_view = view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewTransmit, app->transmit_view);

    app->about_view = view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewAbout, app->about_view);

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    /* Allocate Thread and Timer for animations and TX */
    app->tx_thread = furi_thread_alloc();
    furi_thread_set_name(app->tx_thread, "TagTinkerTx");
    furi_thread_set_stack_size(app->tx_thread, 4096);
    furi_thread_set_priority(app->tx_thread, FuriThreadPriorityHighest);
    furi_thread_set_context(app->tx_thread, app);

    return app;
}

static void app_free(TagTinkerApp* app) {
    tagtinker_ir_deinit();

    tagtinker_free_frame_sequence(app);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewNumlock);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewWarning);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewTransmit);
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewAbout);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    text_input_free(app->text_input);
    popup_free(app->popup);
    widget_free(app->widget);
    numlock_input_free(app->numlock);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    if(app->bt) {
        furi_record_close(RECORD_BT);
    }

    view_free(app->warning_view);
    view_free(app->transmit_view);
    view_free(app->about_view);
    furi_thread_free(app->tx_thread);

    free(app);
}

int32_t tagtinker_app_main(void* p) {
    UNUSED(p);
    TagTinkerApp* app = app_alloc();
    scene_manager_next_scene(
        app->scene_manager,
        app->show_startup_warning ? TagTinkerSceneWarning : TagTinkerSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
