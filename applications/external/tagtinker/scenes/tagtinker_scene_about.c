/*
 * About and phone sync scene.
 */

#include "../tagtinker_app.h"
#include <gui/elements.h>

#define TAGTINKER_SYNC_DIR             APP_DATA_PATH("sync")
#define TAGTINKER_SYNC_INDEX_PATH      APP_DATA_PATH("synced_images.txt")
#define TAGTINKER_BLE_FLOW_WINDOW      8192U
#define TAGTINKER_SYNC_MAX_CHUNK_BYTES 384U

enum {
    AboutEventSendLatestPhone = 1,
};

typedef struct {
    uint32_t mode;
    uint32_t tick;
    char status_text[32];
    bool can_send_latest;
    char target_name[TAGTINKER_TARGET_NAME_LEN + 1];
    uint32_t total_rx;
    uint8_t last_bytes[3];
} AboutViewModel;

/* Prototypes */
static uint16_t ble_rx_callback(SerialServiceEvent event, void* context);
static void bt_status_cb(BtStatus status, void* context);

static void ble_set_status(TagTinkerApp* app, const char* text) {
    if(!app || !text) return;
    snprintf(app->ble_status_text, sizeof(app->ble_status_text), "%s", text);
}

static void ble_send_line(TagTinkerApp* app, const char* line) {
    if(!app || !app->ble_serial || !line) return;
    uint8_t buf[256];
    size_t n = strlen(line);
    if(n > sizeof(buf) - 2U) n = sizeof(buf) - 2U;
    memcpy(buf, line, n);
    buf[n++] = '\n';
    ble_profile_serial_tx(app->ble_serial, buf, (uint16_t)n);
}

static void ble_configure_serial(TagTinkerApp* app) {
    if(!app || !app->ble_serial || app->ble_serial_configured) return;
    ble_profile_serial_set_event_callback(
        app->ble_serial, TAGTINKER_BLE_FLOW_WINDOW, ble_rx_callback, app);
    ble_profile_serial_set_rpc_active(app->ble_serial, false);
    app->ble_serial_configured = true;
}

static void ble_sync_start(TagTinkerApp* app) {
    if(!app || !app->bt || app->ble_sync_active) return;

    app->ble_total_rx = 0;
    memset(app->ble_last_bytes, 0, 3);
    app->ble_rx_len = 0;
    app->ble_rx_pending_ready = false;
    app->ble_serial_configured = false;
    app->ble_sync_active = true;

    bt_disconnect(app->bt);
    bt_set_status_changed_callback(app->bt, bt_status_cb, app);
    app->ble_serial = bt_profile_start(app->bt, ble_profile_serial, NULL);

    if(!app->ble_serial) {
        ble_set_status(app, "Serial Start Fail");
    } else {
        ble_set_status(app, "Waiting phone");
    }
}

static void sync_clear_active_job(TagTinkerApp* app);

static void ble_sync_stop(TagTinkerApp* app) {
    if(!app || !app->ble_sync_active) return;
    sync_clear_active_job(app);
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    bt_disconnect(app->bt);
    bt_profile_restore_default(app->bt);
    app->ble_serial = NULL;
    app->ble_serial_configured = false;
    app->ble_sync_active = false;
}

static uint16_t ble_rx_callback(SerialServiceEvent event, void* context) {
    TagTinkerApp* app = context;
    if(event.event == SerialServiceEventTypeDataReceived) {
        app->ble_total_rx += event.data.size;
        for(size_t i = 0; i < 3 && i < event.data.size; i++) {
            app->ble_last_bytes[i] = event.data.buffer[i];
        }
        for(uint16_t i = 0; i < event.data.size; i++) {
            char c = (char)event.data.buffer[i];
            if(c == '\n' || c == '\r') {
                if(app->ble_rx_len > 0U && !app->ble_rx_pending_ready) {
                    app->ble_rx_line[app->ble_rx_len] = '\0';
                    strncpy(app->ble_rx_pending_line, app->ble_rx_line, 1023);
                    app->ble_rx_pending_ready = true;
                    app->ble_rx_len = 0;
                }
            } else if(!app->ble_rx_pending_ready && app->ble_rx_len < 1023U) {
                app->ble_rx_line[app->ble_rx_len++] = c;
            }
        }
    }
    if(app->ble_rx_pending_ready) return 0U;
    return (uint16_t)(1023U - app->ble_rx_len);
}

static void bt_status_cb(BtStatus status, void* context) {
    TagTinkerApp* app = context;
    app->ble_status = status;
    if(status == BtStatusConnected) {
        /* Configure the serial callback immediately on connection */
        ble_configure_serial(app);
        ble_set_status(app, "Connected");
        ble_send_line(app, "TT_HELLO");
    } else if(status == BtStatusAdvertising) {
        ble_set_status(app, "Waiting phone");
    }
}

static char* sync_next_token(char** cursor) {
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

static bool sync_safe_token(const char* value, size_t max_len) {
    if(!value || !*value) return false;
    size_t len = strlen(value);
    if(len == 0U || len > max_len) return false;
    for(size_t i = 0; i < len; i++) {
        char c = value[i];
        if((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '_' || c == '-') {
            continue;
        }
        return false;
    }
    return true;
}

static void sync_send_targets(TagTinkerApp* app) {
    if(!app) return;
    char line[96];
    snprintf(line, sizeof(line), "TT_TARGETS_BEGIN|%u", app->target_count);
    ble_send_line(app, line);
    for(uint8_t i = 0; i < app->target_count; i++) {
        const TagTinkerTarget* target = &app->targets[i];
        snprintf(
            line,
            sizeof(line),
            "TT_TARGET|%s|%s|%u|%u",
            target->barcode,
            target->name,
            target->profile.width,
            target->profile.height);
        ble_send_line(app, line);
    }
    ble_send_line(app, "TT_TARGETS_END");
}

static void sync_clear_active_job(TagTinkerApp* app) {
    if(!app) return;
    if(app->ble_sync_file) {
        storage_file_close(app->ble_sync_file);
        storage_file_free(app->ble_sync_file);
        app->ble_sync_file = NULL;
    }
    app->ble_sync_job_active = false;
    app->ble_sync_compact_protocol = false;
    app->ble_sync_job_id[0] = '\0';
    app->ble_sync_barcode[0] = '\0';
    app->ble_sync_temp_path[0] = '\0';
    app->ble_sync_final_path[0] = '\0';
    app->ble_sync_expected_bytes = 0;
    app->ble_sync_received_bytes = 0;
    app->ble_sync_last_chunk = 0;
}

static void sync_abort_active_job(TagTinkerApp* app) {
    if(!app) return;
    char temp_path[TAGTINKER_IMAGE_PATH_LEN + 1];
    strncpy(temp_path, app->ble_sync_temp_path, TAGTINKER_IMAGE_PATH_LEN);
    temp_path[TAGTINKER_IMAGE_PATH_LEN] = '\0';
    sync_clear_active_job(app);
    if(temp_path[0] != '\0') {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_remove(storage, temp_path);
        furi_record_close(RECORD_STORAGE);
    }
}

static bool sync_append_index_record(
    const char* job_id,
    const char* barcode,
    uint16_t width,
    uint16_t height,
    uint8_t page,
    const char* image_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, TAGTINKER_SYNC_INDEX_PATH, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(!ok) {
        ok = storage_file_open(file, TAGTINKER_SYNC_INDEX_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    }
    if(ok) {
        char line[384];
        int len = snprintf(
            line,
            sizeof(line),
            "%s|%s|%u|%u|%u|%s\n",
            job_id,
            barcode,
            width,
            height,
            page,
            image_path);
        ok = (len > 0) && ((size_t)len < sizeof(line)) &&
             (storage_file_write(file, line, (uint16_t)len) == (uint16_t)len);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static int8_t sync_base64_value(char c) {
    if(c >= 'A' && c <= 'Z') return (int8_t)(c - 'A');
    if(c >= 'a' && c <= 'z') return (int8_t)(c - 'a' + 26);
    if(c >= '0' && c <= '9') return (int8_t)(c - '0' + 52);
    if(c == '+' || c == '-') return 62;
    if(c == '/' || c == '_') return 63;
    if(c == '=') return -2;
    return -1;
}

static bool
    sync_decode_base64(const char* input, uint8_t* output, size_t output_size, size_t* output_len) {
    if(!input || !output || !output_len) return false;
    size_t out_len = 0;
    uint8_t quartet[4];
    uint8_t quartet_len = 0;
    uint8_t padding = 0;
    for(const char* p = input; *p; p++) {
        int8_t value = sync_base64_value(*p);
        if(value == -1) return false;
        if(value == -2) {
            value = 0;
            padding++;
        }
        quartet[quartet_len++] = (uint8_t)value;
        if(quartet_len != 4U) continue;
        if(out_len + 3U > output_size) return false;
        output[out_len++] = (uint8_t)((quartet[0] << 2U) | (quartet[1] >> 4U));
        if(padding < 2U) output[out_len++] = (uint8_t)((quartet[1] << 4U) | (quartet[2] >> 2U));
        if(padding == 0U) output[out_len++] = (uint8_t)((quartet[2] << 6U) | quartet[3]);
        quartet_len = 0;
        padding = 0;
    }
    if(quartet_len != 0U) return false;
    *output_len = out_len;
    return true;
}

static bool sync_begin_job(
    TagTinkerApp* app,
    const char* job_id,
    const char* barcode,
    uint16_t width,
    uint16_t height,
    uint8_t page,
    uint32_t byte_count,
    bool compact_protocol) {
    if(!app || !sync_safe_token(job_id, TAGTINKER_SYNC_JOB_ID_LEN) ||
       (barcode && *barcode && !sync_safe_token(barcode, TAGTINKER_BC_LEN)) || width == 0U ||
       height == 0U || page > 7U || byte_count == 0U) {
        return false;
    }
    sync_abort_active_job(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, APP_DATA_PATH(""));
    storage_common_mkdir(storage, TAGTINKER_SYNC_DIR);
    snprintf(
        app->ble_sync_temp_path,
        sizeof(app->ble_sync_temp_path),
        "%s/%s.part",
        TAGTINKER_SYNC_DIR,
        job_id);
    snprintf(
        app->ble_sync_final_path,
        sizeof(app->ble_sync_final_path),
        "%s/%s.bmp",
        TAGTINKER_SYNC_DIR,
        job_id);
    storage_common_remove(storage, app->ble_sync_temp_path);
    storage_common_remove(storage, app->ble_sync_final_path);

    app->ble_sync_file = storage_file_alloc(storage);
    bool ok = storage_file_open(
        app->ble_sync_file, app->ble_sync_temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    furi_record_close(RECORD_STORAGE);
    if(!ok) {
        storage_file_free(app->ble_sync_file);
        app->ble_sync_file = NULL;
        return false;
    }

    strncpy(app->ble_sync_job_id, job_id, TAGTINKER_SYNC_JOB_ID_LEN);
    app->ble_sync_job_id[TAGTINKER_SYNC_JOB_ID_LEN] = '\0';
    if(barcode && *barcode) {
        strncpy(app->ble_sync_barcode, barcode, TAGTINKER_BC_LEN);
        app->ble_sync_barcode[TAGTINKER_BC_LEN] = '\0';
    } else {
        app->ble_sync_barcode[0] = '\0';
    }
    app->ble_sync_expected_bytes = byte_count;
    app->ble_sync_received_bytes = 0;
    app->ble_sync_last_chunk = 0;
    app->ble_synced_lines = 0;
    app->img_page = page;
    app->esl_width = width;
    app->esl_height = height;
    app->ble_sync_job_active = true;
    app->ble_sync_compact_protocol = compact_protocol;
    app->ble_sync_ready_target = -1;
    ble_set_status(app, "Upload started");
    ble_send_line(app, compact_protocol ? "AB" : "TT_ACK|BEGIN");
    return true;
}

static bool sync_append_chunk(TagTinkerApp* app, uint16_t sequence, const char* payload) {
    if(!app || !app->ble_sync_job_active || !payload || sequence == 0U || !app->ble_sync_file)
        return false;
    if(sequence == app->ble_sync_last_chunk) {
        char ack[32];
        snprintf(ack, sizeof(ack), "TT_ACK|%u", sequence);
        ble_send_line(app, ack);
        return true;
    }
    if(sequence != (uint16_t)(app->ble_sync_last_chunk + 1U)) return false;
    uint8_t decoded[TAGTINKER_SYNC_MAX_CHUNK_BYTES];
    size_t decoded_len = 0;
    if(!sync_decode_base64(payload, decoded, sizeof(decoded), &decoded_len)) return false;
    if((app->ble_sync_received_bytes + decoded_len) > app->ble_sync_expected_bytes) return false;

    if(storage_file_write(app->ble_sync_file, decoded, decoded_len) != decoded_len) return false;

    app->ble_sync_received_bytes += decoded_len;
    app->ble_sync_last_chunk = sequence;
    app->ble_synced_lines = sequence;
    snprintf(app->ble_status_text, sizeof(app->ble_status_text), "RX %u chunks", sequence);
    char ack[32];
    snprintf(ack, sizeof(ack), "TT_ACK|%u", sequence);
    ble_send_line(app, ack);
    return true;
}

static bool sync_finish_job(TagTinkerApp* app, const char* job_id) {
    if(!app || !sync_safe_token(job_id, TAGTINKER_SYNC_JOB_ID_LEN)) return false;
    if(!app->ble_sync_job_active && strcmp(job_id, app->ble_sync_last_job_id) == 0) {
        ble_send_line(app, "TT_ACK|END");
        return true;
    }
    if(!app->ble_sync_job_active || strcmp(job_id, app->ble_sync_job_id) != 0) return false;
    if(app->ble_sync_barcode[0] == '\0') {
        ble_set_status(app, "No target");
        return false;
    }
    if(app->ble_sync_received_bytes != app->ble_sync_expected_bytes) {
        ble_set_status(app, "Size mismatch");
        return false;
    }

    if(app->ble_sync_file) {
        storage_file_close(app->ble_sync_file);
        storage_file_free(app->ble_sync_file);
        app->ble_sync_file = NULL;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, app->ble_sync_final_path);
    bool ok = storage_common_rename(storage, app->ble_sync_temp_path, app->ble_sync_final_path) ==
              FSE_OK;
    furi_record_close(RECORD_STORAGE);
    if(!ok) {
        ble_set_status(app, "Save failed");
        return false;
    }
    ok = sync_append_index_record(
        app->ble_sync_job_id,
        app->ble_sync_barcode,
        app->esl_width,
        app->esl_height,
        app->img_page,
        app->ble_sync_final_path);
    if(!ok) {
        ble_set_status(app, "Index failed");
        return false;
    }
    strncpy(app->ble_sync_last_job_id, app->ble_sync_job_id, TAGTINKER_SYNC_JOB_ID_LEN);
    app->ble_sync_last_job_id[TAGTINKER_SYNC_JOB_ID_LEN] = '\0';
    app->ble_sync_last_completed_chunks = app->ble_sync_last_chunk;
    app->ble_sync_last_compact_protocol = app->ble_sync_compact_protocol;

    ble_send_line(app, "TT_ACK|END");

    int8_t target_index = tagtinker_ensure_target(app, app->ble_sync_barcode);
    if(target_index >= 0) {
        tagtinker_select_target(app, (uint8_t)target_index);
        app->ble_sync_ready_target = target_index;
        snprintf(
            app->ble_status_text,
            sizeof(app->ble_status_text),
            "Saved for %.20s",
            app->targets[target_index].name);
    } else {
        app->ble_sync_ready_target = -1;
        ble_set_status(app, "Saved on Flipper");
    }
    app->ble_sync_job_active = false;
    return true;
}

static void sync_apply_line(TagTinkerApp* app, const char* line) {
    if(!app || !line) return;
    if(strcmp(line, "TT_HELLO") == 0) {
        ble_send_line(app, "TT_PONG");
        return;
    }
    if(strcmp(line, "TT_PING") == 0) {
        ble_set_status(app, "Handshake OK");
        ble_send_line(app, "TT_PONG");
        return;
    }
    if(strcmp(line, "TT_LIST_TARGETS") == 0) {
        ble_set_status(app, "Sending targets");
        sync_send_targets(app);
        return;
    }
    if(strncmp(line, "TT_BEGIN|", 9) == 0) {
        ble_set_status(app, "Got BEGIN");
        char temp[256];
        strncpy(temp, line, sizeof(temp) - 1U);
        temp[sizeof(temp) - 1U] = '\0';
        char* cursor = temp;
        sync_next_token(&cursor);
        char* job_id = sync_next_token(&cursor);
        char* barcode = sync_next_token(&cursor);
        char* width = sync_next_token(&cursor);
        char* height = sync_next_token(&cursor);
        char* page = sync_next_token(&cursor);
        char* bytes = sync_next_token(&cursor);
        if(job_id && barcode && width && height && page && bytes) {
            if(sync_begin_job(
                   app,
                   job_id,
                   barcode,
                   (uint16_t)atoi(width),
                   (uint16_t)atoi(height),
                   (uint8_t)atoi(page),
                   (uint32_t)strtoul(bytes, NULL, 10),
                   false)) {
                return;
            } else {
                ble_set_status(app, "BEGIN JOB FAIL");
                return;
            }
        }
        ble_set_status(app, "BEGIN PARSE FAIL");
        return;
    }
    if(strncmp(line, "TT_DATA|", 8) == 0) {
        char temp[1024];
        strncpy(temp, line, sizeof(temp) - 1U);
        temp[sizeof(temp) - 1U] = '\0';
        char* cursor = temp;
        sync_next_token(&cursor);
        char* seq = sync_next_token(&cursor);
        char* payload = sync_next_token(&cursor);
        if(seq && payload && sync_append_chunk(app, (uint16_t)atoi(seq), payload)) {
            return;
        }
        ble_set_status(app, "DATA failed");
        return;
    }
    if(strncmp(line, "TT_END|", 7) == 0) {
        const char* job_id = line + 7;
        if(sync_finish_job(app, job_id)) {
            return;
        }
        ble_set_status(app, "END failed");
        return;
    }
    snprintf(app->ble_status_text, sizeof(app->ble_status_text), "Err: %.20s", line);
}

static void about_draw_cb(Canvas* canvas, void* _model) {
    AboutViewModel* model = _model;
    canvas_set_font(canvas, FontPrimary);
    if(model->mode == 1U) {
        char header[32];
        snprintf(header, sizeof(header), "Custom Image %c", (model->tick % 2) ? '*' : ' ');
        canvas_draw_str_aligned(canvas, 64, 7, AlignCenter, AlignTop, header);
        canvas_set_font(canvas, FontSecondary);
        if(model->can_send_latest) {
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Image received!");
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, model->target_name);
            canvas_draw_str_aligned(
                canvas, 64, 46, AlignCenter, AlignTop, "Point at tag & press OK");
        } else {
            canvas_draw_str_aligned(
                canvas, 64, 19, AlignCenter, AlignTop, "1. Open TagTinker app");
            canvas_draw_str_aligned(
                canvas, 64, 29, AlignCenter, AlignTop, "2. Connect to Flipper");
            canvas_draw_str_aligned(
                canvas, 64, 39, AlignCenter, AlignTop, "3. Select & send image");
            canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, model->status_text);
        }
        if(model->can_send_latest) elements_button_center(canvas, "Send");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 10, AlignCenter, AlignTop, TAGTINKER_DISPLAY_NAME " v" TAGTINKER_VERSION);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Ported by I12BP8");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Research by furrtek");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "NFC by 7h30th3r0n3");
    }
}

static bool about_input_cb(InputEvent* event, void* context) {
    TagTinkerApp* app = context;
    if(event->type != InputTypeShort || event->key != InputKeyOk) return false;
    if(app->ble_sync_ready_target < 0) return false;
    view_dispatcher_send_custom_event(app->view_dispatcher, AboutEventSendLatestPhone);
    return true;
}

void tagtinker_scene_about_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    uint32_t mode = scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneAbout);

    if(!app->about_view_allocated) {
        view_allocate_model(app->about_view, ViewModelTypeLockFree, sizeof(AboutViewModel));
        view_set_context(app->about_view, app);
        view_set_draw_callback(app->about_view, about_draw_cb);
        view_set_input_callback(app->about_view, about_input_cb);
        app->about_view_allocated = true;
    }

    AboutViewModel* model = view_get_model(app->about_view);
    model->mode = mode;
    model->tick = 0;
    model->can_send_latest = false;
    model->total_rx = 0;
    memset(model->last_bytes, 0, 3);
    strncpy(model->status_text, "Init...", 31);

    app->ble_sync_ready_target = -1; // RESET STATE

    view_commit_model(app->about_view, true);

    /* Delay BLE start until GUI settles */
    app->ble_sync_start_pending = (mode == 1U);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewAbout);
}

bool tagtinker_scene_about_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type == SceneManagerEventTypeCustom && event.event == AboutEventSendLatestPhone) {
        if(app->ble_sync_ready_target >= 0 && app->ble_sync_ready_target < app->target_count) {
            TagTinkerTarget* target = &app->targets[app->ble_sync_ready_target];
            TagTinkerSyncedImage image;
            if(tagtinker_find_latest_synced_image(app, target->barcode, &image)) {
                tagtinker_select_target(app, (uint8_t)app->ble_sync_ready_target);
                app->img_page = image.page;
                app->draw_x = 0;
                app->draw_y = 0;
                app->color_clear = false;
                tagtinker_prepare_bmp_tx(
                    app, target->plid, image.image_path, image.width, image.height, image.page);
                app->tx_spam = false;
                app->ble_sync_ready_target = -1;
                /* Tear down BLE before IR transmission to prevent timing interference */
                app->ble_sync_start_pending = false;
                ble_sync_stop(app);
                scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
                return true;
            }
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeTick) {
        AboutViewModel* model = view_get_model(app->about_view);
        model->tick++;

        /* Delayed start: Wait until 5th tick (250ms) */
        if(app->ble_sync_start_pending && model->tick >= 5) {
            app->ble_sync_start_pending = false;
            ble_sync_start(app);
        }

        if(app->ble_sync_active) {
            if(app->ble_status == BtStatusConnected) {
                /* Serial is configured in bt_status_cb, but ensure it on tick too
                 * in case connection fired before bt_status_cb was set up */
                ble_configure_serial(app);
            }

            if(app->ble_rx_pending_ready) {
                char safe_line[1024];
                strncpy(safe_line, app->ble_rx_pending_line, 1023);
                safe_line[1023] = '\0';
                app->ble_rx_pending_line[0] = '\0';
                app->ble_rx_pending_ready = false;

                /* Process the line FIRST (SD card write + send ACK) */
                sync_apply_line(app, safe_line);

                /* THEN tell BLE stack we're ready for next packet.
                 * Order matters: phone waits for ACK before sending next chunk,
                 * so notify_buffer_is_empty here is just belt-and-suspenders. */
                if(app->ble_serial) ble_profile_serial_notify_buffer_is_empty(app->ble_serial);
            }

            model->total_rx = app->ble_total_rx;
            memcpy(model->last_bytes, app->ble_last_bytes, 3);
            model->can_send_latest = (app->ble_sync_ready_target >= 0);
            strncpy(model->status_text, app->ble_status_text, 31);
        }

        view_commit_model(app->about_view, true);
        return true;
    }
    return false;
}

void tagtinker_scene_about_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    app->ble_sync_start_pending = false;
    ble_sync_stop(app);
}
