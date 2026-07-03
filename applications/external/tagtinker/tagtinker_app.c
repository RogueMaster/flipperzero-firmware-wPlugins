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

#define TAGTINKER_WEB_JOB_PATH APP_DATA_PATH("web_job.txt")

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

#define TAGTINKER_STREAM_PIXEL_BUDGET 131072U

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

    uint16_t max_x = (width < target->profile.width) ? (uint16_t)(target->profile.width - width) :
                                                       0U;
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

void tagtinker_target_set_default_name(TagTinkerApp* app, TagTinkerTarget* target) {
    if(!target || !app) return;
    snprintf(target->name, sizeof(target->name), "tag%d", app->target_count + 1);
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

    tagtinker_target_set_default_name(app, target);
    tagtinker_target_refresh_profile(target);
    app->target_count++;
    tagtinker_targets_save(app);
    return (int8_t)(app->target_count - 1U);
}

bool tagtinker_find_latest_synced_image(
    const TagTinkerApp* app,
    const char* barcode,
    TagTinkerSyncedImage* image) {
    if(!app || !barcode || !image) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool found = false;

    if(storage_file_open(file, APP_DATA_PATH("synced_images.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        /*
         * Index file can grow well beyond 512 bytes with many synced images.
         * Previously the 512-byte buffer truncated file paths mid-name
         * (e.g. "D72B7A.bm" instead of "D72B7A.bmp"), causing bmp_open
         * to fail and the entire IR transmission to abort in ~1ms.
         */
        uint64_t file_size = storage_file_size(file);
        if(file_size > 16384U) file_size = 16384U;
        size_t alloc_size = (size_t)file_size + 1U;
        char* buf = malloc(alloc_size);
        if(!buf) {
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }
        uint16_t read = storage_file_read(file, buf, (uint16_t)file_size);
        buf[read] = '\0';
        storage_file_close(file);

        char* line = buf;
        while(line && *line) {
            char* nl = strchr(line, '\n');
            if(nl) *nl = '\0';

            if(*line) {
                char* cursor = line;
                char* current_barcode = strchr(cursor, '|');
                if(current_barcode) {
                    *current_barcode++ = '\0';
                    char* width = strchr(current_barcode, '|');
                    if(width) {
                        *width++ = '\0';
                        char* height = strchr(width, '|');
                        if(height) {
                            *height++ = '\0';
                            char* page = strchr(height, '|');
                            if(page) {
                                *page++ = '\0';
                                char* path = strchr(page, '|');
                                if(path) {
                                    *path++ = '\0';
                                    if(strcmp(current_barcode, barcode) == 0) {
                                        strncpy(image->barcode, current_barcode, TAGTINKER_BC_LEN);
                                        image->width = (uint16_t)atoi(width);
                                        image->height = (uint16_t)atoi(height);
                                        image->page = (uint8_t)atoi(page);
                                        strncpy(image->image_path, path, TAGTINKER_IMAGE_PATH_LEN);
                                        found = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            line = nl ? (nl + 1) : NULL;
        }
        free(buf);
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
        if(size > 0U && size < 16384U) {
            char* input = malloc((size_t)size + 1U);
            char* output = malloc((size_t)size + 1U);
            if(input && output) {
                uint16_t read = storage_file_read(file, input, (uint16_t)size);
                input[read] = '\0';
                size_t output_len = 0U;

                char* line = input;
                while(line && *line) {
                    char* nl = strchr(line, '\n');
                    if(nl) *nl = '\0';

                    if(*line) {
                        char line_copy[256];
                        strncpy(line_copy, line, sizeof(line_copy) - 1U);
                        line_copy[sizeof(line_copy) - 1U] = '\0';

                        char* cursor = line;
                        char* current_barcode = strchr(cursor, '|');
                        if(current_barcode) {
                            *current_barcode++ = '\0';
                            char* width = strchr(current_barcode, '|');
                            if(width) {
                                *width++ = '\0';
                                char* height = strchr(width, '|');
                                if(height) {
                                    *height++ = '\0';
                                    char* page = strchr(height, '|');
                                    if(page) {
                                        *page++ = '\0';
                                        char* path = strchr(page, '|');
                                        if(path) {
                                            *path++ = '\0';
                                            if(strcmp(current_barcode, barcode) == 0) {
                                                storage_common_remove(storage, path);
                                                removed_count++;
                                            } else {
                                                size_t line_len = strlen(line_copy);
                                                if(output_len + line_len + 2U <=
                                                   (size_t)size + 1U) {
                                                    strcpy(output + output_len, line_copy);
                                                    output_len += line_len;
                                                    output[output_len++] = '\n';
                                                    output[output_len] = '\0';
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    line = nl ? (nl + 1) : NULL;
                }

                storage_file_close(file);
                if(removed_count > 0U) {
                    if(output_len == 0U) {
                        storage_common_remove(storage, APP_DATA_PATH("synced_images.txt"));
                    } else if(storage_file_open(
                                  file,
                                  APP_DATA_PATH("synced_images.txt"),
                                  FSAM_WRITE,
                                  FSOM_CREATE_ALWAYS)) {
                        storage_file_write(file, output, (uint16_t)output_len);
                        storage_file_close(file);
                    }
                }
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
        return "Graphic";
    case TagTinkerTagKindSegment:
        return "Segment";
    default:
        return "Unknown";
    }
}

const char* tagtinker_profile_color_label(TagTinkerTagColor color) {
    switch(color) {
    case TagTinkerTagColorMono:
        return "Mono";
    case TagTinkerTagColorRed:
        return "Red";
    case TagTinkerTagColorYellow:
        return "Yellow";
    default:
        return "Unknown";
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

    size_t plane_budget = color_clear ? (TAGTINKER_STREAM_PIXEL_BUDGET / 2U) :
                                        TAGTINKER_STREAM_PIXEL_BUDGET;
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
    app->image_tx_job.page = (app->img_page > 0U) ? (uint8_t)(app->img_page - 1U) : 0U;
    if(app->image_tx_job.page > 7U) app->image_tx_job.page = 7U;
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

    app->selected_target = (int8_t)index;
    strncpy(app->barcode, app->targets[index].barcode, TAGTINKER_BC_LEN);
    app->barcode[TAGTINKER_BC_LEN] = '\0';
    memcpy(app->plid, app->targets[index].plid, sizeof(app->plid));
    app->barcode_valid = true;

    app->esl_width = app->targets[index].profile.width;
    app->esl_height = app->targets[index].profile.height;

    if(app->esl_width == 0 || app->esl_height == 0) {
        app->esl_width = 200;
        app->esl_height = 80;
    }
}

void tagtinker_settings_load(TagTinkerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    app->show_startup_warning = true;
    app->data_frame_repeats = 2;
    app->signal_mode = TagTinkerSignalPP4;

    if(storage_file_open(file, APP_DATA_PATH("settings.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[32];
        uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[read] = '\0';
        storage_file_close(file);

        int warn, rep, sig;
        if(sscanf(buf, "%d|%d|%d", &warn, &rep, &sig) == 3) {
            app->show_startup_warning = (warn != 0);
            app->data_frame_repeats = (uint16_t)rep;
        } else if(sscanf(buf, "%d|%d", &warn, &rep) == 2) {
            app->show_startup_warning = (warn != 0);
            app->data_frame_repeats = (uint16_t)rep;
        }
    }

    if(app->data_frame_repeats < 1U) app->data_frame_repeats = 1U;
    if(app->data_frame_repeats > 10U) app->data_frame_repeats = 10U;
    app->signal_mode = TagTinkerSignalPP4;

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool tagtinker_settings_save(const TagTinkerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, APP_DATA_PATH("settings.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[32];
        int len = snprintf(
            buf,
            sizeof(buf),
            "%d|%u|%d",
            app->show_startup_warning ? 1 : 0,
            app->data_frame_repeats,
            (int)TagTinkerSignalPP4);
        if(len > 0 && storage_file_write(file, buf, (uint16_t)len)) {
            ok = true;
        }
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
        char buf[1024];
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
                        tagtinker_target_set_default_name(app, target);
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
                line, sizeof(line), "%s|%s\n", app->targets[i].barcode, app->targets[i].name);

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

void tagtinker_recents_load(TagTinkerApp* app) {
    app->recent_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("recents.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[1024];
        uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[read] = '\0';
        storage_file_close(file);

        char* line = buf;
        while(line && *line && app->recent_count < TAGTINKER_MAX_PRESETS) {
            char* nl = strchr(line, '\n');
            if(nl) *nl = '\0';

            unsigned w, h, pg, inv, clr, pad, sig;
            /* Parse current format: w|h|pg|inv|clr|pad|sig|text.
               Older saved entries omitted sig and are kept on the current mode. */
            int parsed =
                sscanf(line, "%u|%u|%u|%u|%u|%u|%u|", &w, &h, &pg, &inv, &clr, &pad, &sig);
            if(parsed >= 6) {
                char* p = line;
                int pipes = 0;
                int wanted_pipes = (parsed >= 7) ? 7 : 6;
                while(*p && pipes < wanted_pipes) {
                    if(*p == '|') pipes++;
                    p++;
                }

                if(pipes == wanted_pipes) {
                    uint8_t idx = app->recent_count++;
                    app->recents[idx].width = (uint16_t)w;
                    app->recents[idx].height = (uint16_t)h;
                    app->recents[idx].page = (uint8_t)pg;
                    app->recents[idx].invert = (inv != 0);
                    app->recents[idx].color_clear = (clr != 0);
                    app->recents[idx].padding = (uint8_t)pad;
                    app->recents[idx].signal_mode = (uint8_t)TagTinkerSignalPP4;
                    strncpy(app->recents[idx].text, p, TAGTINKER_PRESET_TEXT_LEN - 1);
                    app->recents[idx].text[TAGTINKER_PRESET_TEXT_LEN - 1] = '\0';
                }
            }
            line = nl ? nl + 1 : NULL;
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool tagtinker_recents_save(const TagTinkerApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, APP_DATA_PATH("recents.txt"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = true;
        for(uint8_t i = 0; i < app->recent_count; i++) {
            char line[128];
            int len = snprintf(
                line,
                sizeof(line),
                "%u|%u|%u|%d|%d|%u|%u|%s\n",
                app->recents[i].width,
                app->recents[i].height,
                app->recents[i].page,
                app->recents[i].invert ? 1 : 0,
                app->recents[i].color_clear ? 1 : 0,
                app->recents[i].padding,
                (uint8_t)TagTinkerSignalPP4,
                app->recents[i].text);

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

void tagtinker_recents_add(TagTinkerApp* app, const char* text) {
    if(!app || !text || !*text) return;

    /* Check if already in recents (move to top if so) */
    int8_t existing_idx = -1;
    for(uint8_t i = 0; i < app->recent_count; i++) {
        if(strcmp(app->recents[i].text, text) == 0 && app->recents[i].width == app->esl_width &&
           app->recents[i].height == app->esl_height) {
            existing_idx = (int8_t)i;
            break;
        }
    }

    if(existing_idx >= 0) {
        /* Move to front */
        if(existing_idx > 0) {
            uint16_t width = app->recents[existing_idx].width;
            uint16_t height = app->recents[existing_idx].height;
            uint8_t page = app->recents[existing_idx].page;
            bool invert = app->recents[existing_idx].invert;
            bool color_clear = app->recents[existing_idx].color_clear;
            uint8_t padding = app->recents[existing_idx].padding;
            char text_copy[TAGTINKER_PRESET_TEXT_LEN];
            strncpy(text_copy, app->recents[existing_idx].text, TAGTINKER_PRESET_TEXT_LEN);

            memmove(
                &app->recents[1],
                &app->recents[0],
                sizeof(app->recents[0]) * (size_t)existing_idx);

            app->recents[0].width = width;
            app->recents[0].height = height;
            app->recents[0].page = page;
            app->recents[0].invert = invert;
            app->recents[0].color_clear = color_clear;
            app->recents[0].padding = padding;
            app->recents[0].signal_mode = (uint8_t)TagTinkerSignalPP4;
            strncpy(app->recents[0].text, text_copy, TAGTINKER_PRESET_TEXT_LEN);
        }
    } else {
        /* New entry, shift others */
        if(app->recent_count < TAGTINKER_MAX_PRESETS) {
            app->recent_count++;
        }
        memmove(
            &app->recents[1],
            &app->recents[0],
            sizeof(app->recents[0]) * (size_t)(app->recent_count - 1));
        app->recents[0].width = app->esl_width;
        app->recents[0].height = app->esl_height;
        app->recents[0].page = app->img_page;
        app->recents[0].invert = app->invert_text;
        app->recents[0].color_clear = app->color_clear;
        app->recents[0].padding = app->text_padding_pct;
        app->recents[0].signal_mode = (uint8_t)TagTinkerSignalPP4;
        strncpy(app->recents[0].text, text, TAGTINKER_PRESET_TEXT_LEN - 1);
        app->recents[0].text[TAGTINKER_PRESET_TEXT_LEN - 1] = '\0';
    }

    tagtinker_recents_save(app);
}

static void app_free(TagTinkerApp* app) {
    furi_assert(app);

    tagtinker_free_frame_sequence(app);

    /* Tear down WiFi link if it was lazily allocated. */
    if(app->wifi) {
        extern void tagtinker_wifi_free(void* w);
        tagtinker_wifi_free(app->wifi);
        app->wifi = NULL;
    }
    free(app->wifi_plugins);
    app->wifi_plugins = NULL;

    /* Views */
    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewVarItemList);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewNumlock);
    numlock_input_free(app->numlock);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewTextBox);
    text_box_free(app->text_box);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewWarning);
    view_free(app->warning_view);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewTransmit);
    view_free(app->transmit_view);

    view_dispatcher_remove_view(app->view_dispatcher, TagTinkerViewAbout);
    view_free(app->about_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_thread_free(app->tx_thread);

    /* NFC cleanup */
    if(app->nfc) {
        app->nfc_scanning = false;
        furi_thread_join(app->nfc_thread);
        nfc_free(app->nfc);
    }
    furi_thread_free(app->nfc_thread);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    if(app->bt) {
        furi_record_close(RECORD_BT);
    }

    free(app);
}

static bool tagtinker_try_consume_web_job(TagTinkerApp* app) {
    if(!app) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, TAGTINKER_WEB_JOB_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[512];
        uint16_t read = storage_file_read(file, buf, sizeof(buf) - 1U);
        buf[read] = '\0';
        storage_file_close(file);
        storage_common_remove(storage, TAGTINKER_WEB_JOB_PATH);

        char barcode[TAGTINKER_BC_LEN + 1] = {0};
        char image_path[TAGTINKER_IMAGE_PATH_LEN + 1] = {0};
        unsigned width = 0U;
        unsigned height = 0U;
        unsigned page = 0U;

        if(sscanf(
               buf, "%17[^|]|%u|%u|%u|%255[^\r\n]", barcode, &width, &height, &page, image_path) ==
           5) {
            int8_t target_index = tagtinker_ensure_target(app, barcode);
            if(target_index >= 0) {
                tagtinker_select_target(app, (uint8_t)target_index);
                app->img_page = (uint8_t)page;
                app->draw_x = 0U;
                app->draw_y = 0U;
                app->color_clear = false;
                tagtinker_prepare_bmp_tx(
                    app,
                    app->targets[target_index].plid,
                    image_path,
                    (uint16_t)width,
                    (uint16_t)height,
                    (uint8_t)page);
                app->tx_spam = false;
                ok = true;
            }
        }
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
    app->text_padding_pct = 0;
    strcpy(app->text_input_buf, "TagTinker");
    app->selected_target = -1;

    /* Ensure the dropped-image folder exists so the user can pre-stage BMPs
     * prepared with the web image preparer (web-image-prep/index.html). */
    {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_mkdir(storage, APP_DATA_PATH(""));
        storage_common_mkdir(storage, APP_DATA_PATH("dropped"));
        furi_record_close(RECORD_STORAGE);
    }

    tagtinker_settings_load(app);
    tagtinker_targets_load(app);
    tagtinker_recents_load(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->bt = furi_record_open(RECORD_BT);

    /* Momentum safety: Ensure radio is idle on startup */
    bt_disconnect(app->bt);
    bt_profile_restore_default(app->bt);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&tagtinker_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_cb);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, tick_cb, 50);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Views */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TagTinkerViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TagTinkerViewPopup, popup_get_view(app->popup));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewWidget, widget_get_view(app->widget));

    app->numlock = numlock_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewNumlock, numlock_input_get_view(app->numlock));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TagTinkerViewTextBox, text_box_get_view(app->text_box));

    app->warning_view = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TagTinkerViewWarning, app->warning_view);

    app->transmit_view = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TagTinkerViewTransmit, app->transmit_view);

    app->about_view = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TagTinkerViewAbout, app->about_view);

    /* TX Thread */
    app->tx_thread = furi_thread_alloc();
    furi_thread_set_name(app->tx_thread, "TagTinkerTx");
    furi_thread_set_stack_size(app->tx_thread, 4096);
    furi_thread_set_priority(app->tx_thread, FuriThreadPriorityHighest);
    furi_thread_set_context(app->tx_thread, app);

    /* NFC scan thread */
    app->nfc_thread = furi_thread_alloc();
    furi_thread_set_name(app->nfc_thread, "TagTinkerNfc");
    furi_thread_set_stack_size(app->nfc_thread, 2048);
    app->nfc = NULL;
    app->nfc_scanning = false;

    return app;
}

int32_t tagtinker_app_main(void* p) {
    UNUSED(p);
    TagTinkerApp* app = app_alloc();
    bool web_job_ready = tagtinker_try_consume_web_job(app);

    if(web_job_ready) {
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
    } else if(app->show_startup_warning) {
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneWarning);
    } else {
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneMainMenu);
    }

    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
