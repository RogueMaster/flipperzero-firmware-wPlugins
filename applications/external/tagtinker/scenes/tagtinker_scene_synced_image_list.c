#include "../tagtinker_app.h"

#define EVT_SYNCED_IMAGE_BASE 300

static uint8_t synced_image_menu_map[TAGTINKER_MAX_SYNCED_IMAGES];
static char synced_image_labels[TAGTINKER_MAX_SYNCED_IMAGES][48];

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

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Uploads");

    if(app->synced_image_count == 0) {
        submenu_add_item(app->submenu, "No synced images", 0, synced_image_list_cb, app);
    } else {
        uint8_t menu_idx = 0;
        for(int16_t i = (int16_t)app->synced_image_count - 1; i >= 0; i--) {
            const TagTinkerSyncedImage* image = &app->synced_images[i];
            const char* suffix = image->job_id;
            size_t suffix_len = strlen(image->job_id);
            if(suffix_len > 6U) suffix += suffix_len - 6U;

            snprintf(
                synced_image_labels[menu_idx],
                sizeof(synced_image_labels[menu_idx]),
                "P%u %ux%u #%s",
                image->page,
                image->width,
                image->height,
                suffix);
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
