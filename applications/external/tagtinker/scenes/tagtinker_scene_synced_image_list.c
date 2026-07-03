#include "../tagtinker_app.h"

#define EVT_SYNCED_IMAGE_BASE 300
#define TAGTINKER_DROPPED_DIR APP_DATA_PATH("dropped")

/* Build a synced image entry from any .bmp file in the dropped/ folder. The
 * Flipper rescales BMPs at send time (see tagtinker_scene_transmit.c), so any
 * BMP can target any tag - the entry's width/height are the *target* dims, not
 * the source dims, and tx_stream_bmp_image samples the source pixels via
 * nearest-neighbour as it streams.
 *
 * Filenames produced by web-image-prep are "<W>x<H>[_<label>].bmp"; the W/H
 * prefix is optional and used only as a label hint. Legacy "_p<page>" suffix
 * sets the default page. */
static bool tagtinker_parse_dropped_filename(
    const char* name,
    const char* expected_barcode,
    uint16_t target_w,
    uint16_t target_h,
    TagTinkerSyncedImage* out) {
    if(!name || !out) return false;

    size_t name_len = strlen(name);
    if(name_len < 5U) return false; /* min: "a.bmp" */
    const char* ext = name + name_len - 4;
    if(!((ext[0] == '.') && (ext[1] == 'b' || ext[1] == 'B') && (ext[2] == 'm' || ext[2] == 'M') &&
         (ext[3] == 'p' || ext[3] == 'P')))
        return false;

    unsigned page = 1U;
    int consumed = 0;
    unsigned w_hint = 0U, h_hint = 0U;
    bool has_hint = false;
    /* Optional resolution prefix - we don't filter on it, the transmitter
     * rescales any BMP to the target's dimensions automatically. The "_p<N>"
     * suffix, when present, just sets the default page in image options. */
    if(sscanf(name, "%ux%u%n", &w_hint, &h_hint, &consumed) >= 2) {
        has_hint = true;
        if((size_t)consumed < name_len && name[consumed] == '_' && name[consumed + 1] == 'p') {
            unsigned parsed_page = 0U;
            if(sscanf(name + consumed + 2, "%u", &parsed_page) == 1 && parsed_page <= 7U) {
                page = parsed_page;
            }
        }
    }

    memset(out, 0, sizeof(*out));
    if(expected_barcode) {
        strncpy(out->barcode, expected_barcode, TAGTINKER_BC_LEN);
        out->barcode[TAGTINKER_BC_LEN] = '\0';
    }

    /* Use the filename (without .bmp) as a stable job_id for display. */
    size_t id_len = name_len - 4U;
    if(id_len > TAGTINKER_SYNC_JOB_ID_LEN) id_len = TAGTINKER_SYNC_JOB_ID_LEN;
    memcpy(out->job_id, name, id_len);
    out->job_id[id_len] = '\0';

    /* Stamp the target's resolution: the transmitter will rescale on the fly. */
    out->width = target_w;
    out->height = target_h;
    out->page = (uint8_t)page;
    /* Mark as resampled when the filename hints at a different source size,
     * or when there's no hint at all (we can't tell, so flag it as foreign
     * to be safe - it's a subtle "this might not be native" indicator). */
    if(has_hint) {
        out->resampled = (w_hint != target_w) || (h_hint != target_h);
    } else {
        out->resampled = true;
    }
    snprintf(out->image_path, sizeof(out->image_path), "%s/%s", TAGTINKER_DROPPED_DIR, name);
    return true;
}

static void dropped_images_load(TagTinkerApp* app) {
    if(app->selected_target < 0 || app->selected_target >= app->target_count) return;
    const TagTinkerTarget* target = &app->targets[app->selected_target];
    if(!tagtinker_target_supports_graphics(target)) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, TAGTINKER_DROPPED_DIR);

    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, TAGTINKER_DROPPED_DIR)) {
        FileInfo info;
        char name[128];
        while(app->synced_image_count < TAGTINKER_MAX_SYNCED_IMAGES &&
              storage_dir_read(dir, &info, name, (uint16_t)sizeof(name))) {
            if(file_info_is_dir(&info)) continue;

            TagTinkerSyncedImage entry;
            if(!tagtinker_parse_dropped_filename(
                   name, target->barcode, target->profile.width, target->profile.height, &entry)) {
                continue;
            }

            app->synced_images[app->synced_image_count++] = entry;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

static uint8_t synced_image_menu_map[TAGTINKER_MAX_SYNCED_IMAGES];
/* Wide enough for "~ P9 " plus a 32-char job_id - the submenu module
 * auto-marquees the selected row when it overflows the screen, so the full
 * filename stays visible by selecting the entry. */
static char synced_image_labels[TAGTINKER_MAX_SYNCED_IMAGES][64];

static void synced_image_list_cb(void* ctx, uint32_t index) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static char* synced_image_next_token(char** cursor) {
    if(!cursor || !*cursor) return NULL;

    char* token = *cursor;
    char* sep = strchr(token, '|');
    if(sep) {
        *sep = '\0';
        *cursor = sep + 1;
    } else {
        *cursor = NULL;
    }

    return token;
}

static void synced_images_load(TagTinkerApp* app) {
    app->synced_image_count = 0;

    if(app->selected_target < 0 || app->selected_target >= app->target_count) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, APP_DATA_PATH("synced_images.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        if(size > 0 && size < 8192U) {
            char* buf = malloc((size_t)size + 1U);
            if(buf) {
                uint16_t read = storage_file_read(file, buf, (uint16_t)size);
                buf[read] = '\0';

                char* line = buf;
                while(line && *line && app->synced_image_count < TAGTINKER_MAX_SYNCED_IMAGES) {
                    char* nl = strchr(line, '\n');
                    if(nl) *nl = '\0';

                    if(*line) {
                        char* cursor = line;
                        char* job_id = synced_image_next_token(&cursor);
                        char* barcode = synced_image_next_token(&cursor);
                        char* width = synced_image_next_token(&cursor);
                        char* height = synced_image_next_token(&cursor);
                        char* page = synced_image_next_token(&cursor);
                        char* path = synced_image_next_token(&cursor);

                        if(job_id && barcode && width && height && page && path &&
                           strcmp(barcode, app->targets[app->selected_target].barcode) == 0 &&
                           storage_common_exists(storage, path)) {
                            TagTinkerSyncedImage* image =
                                &app->synced_images[app->synced_image_count++];
                            strncpy(image->job_id, job_id, TAGTINKER_SYNC_JOB_ID_LEN);
                            image->job_id[TAGTINKER_SYNC_JOB_ID_LEN] = '\0';
                            strncpy(image->barcode, barcode, TAGTINKER_BC_LEN);
                            image->barcode[TAGTINKER_BC_LEN] = '\0';
                            image->width = (uint16_t)atoi(width);
                            image->height = (uint16_t)atoi(height);
                            image->page = (uint8_t)atoi(page);
                            strncpy(image->image_path, path, TAGTINKER_IMAGE_PATH_LEN);
                            image->image_path[TAGTINKER_IMAGE_PATH_LEN] = '\0';
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
}

void tagtinker_scene_synced_image_list_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    synced_images_load(app);
    dropped_images_load(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Set Image");

    if(app->synced_image_count == 0) {
        submenu_add_item(app->submenu, "No matching BMPs", 0, synced_image_list_cb, app);
        submenu_add_item(app->submenu, "Drop into apps_data/", 0, synced_image_list_cb, app);
        submenu_add_item(app->submenu, "  tagtinker/dropped/", 0, synced_image_list_cb, app);
        submenu_add_item(app->submenu, "Use Image Prep page", 0, synced_image_list_cb, app);
    } else {
        uint8_t menu_idx = 0;
        for(int16_t i = (int16_t)app->synced_image_count - 1; i >= 0; i--) {
            const TagTinkerSyncedImage* image = &app->synced_images[i];

            /* "~" prefix subtly marks BMPs that aren't native to this tag's
             * resolution (the FAP rescales them at TX time). The full job_id
             * is appended so the submenu's auto-marquee can scroll long
             * filenames sideways when the row is selected. */
            snprintf(
                synced_image_labels[menu_idx],
                sizeof(synced_image_labels[menu_idx]),
                "%sP%u %s",
                image->resampled ? "~ " : "",
                image->page,
                image->job_id);
            synced_image_menu_map[menu_idx] = (uint8_t)i;
            submenu_add_item(
                app->submenu,
                synced_image_labels[menu_idx],
                EVT_SYNCED_IMAGE_BASE + menu_idx,
                synced_image_list_cb,
                app);
            menu_idx++;
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewSubmenu);
}

bool tagtinker_scene_synced_image_list_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event < EVT_SYNCED_IMAGE_BASE) return true;

    uint32_t menu_idx = event.event - EVT_SYNCED_IMAGE_BASE;
    if(menu_idx >= app->synced_image_count) return true;
    if(app->selected_target < 0 || app->selected_target >= app->target_count) return true;

    TagTinkerSyncedImage* image = &app->synced_images[synced_image_menu_map[menu_idx]];
    TagTinkerTarget* target = &app->targets[app->selected_target];

    app->img_page = image->page;
    app->draw_x = 0;
    app->draw_y = 0;
    app->color_clear = false;
    tagtinker_prepare_bmp_tx(
        app, target->plid, image->image_path, image->width, image->height, image->page);
    scene_manager_next_scene(app->scene_manager, TagTinkerSceneImageOptions);
    return true;
}

void tagtinker_scene_synced_image_list_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    submenu_reset(app->submenu);
}
