/*
 * About and phone sync scene.
 */

#include "../tagtinker_app.h"
#include <gui/elements.h>

#define TAGTINKER_SYNC_DIR        APP_DATA_PATH("sync")
#define TAGTINKER_SYNC_INDEX_PATH APP_DATA_PATH("synced_images.txt")
#define TAGTINKER_BLE_FLOW_WINDOW 8192U
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
} AboutViewModel;

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

static void ble_set_rx_status(TagTinkerApp* app, const char* line) {
    if(!app || !line) return;
    snprintf(app->ble_status_text, sizeof(app->ble_status_text), "RX %.24s", line);
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

    if(app->ble_sync_temp_path[0] != '\0') {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        storage_common_remove(storage, app->ble_sync_temp_path);
        furi_record_close(RECORD_STORAGE);
    }

    sync_clear_active_job(app);
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

static bool sync_decode_base64(
    const char* input,
    uint8_t* output,
    size_t output_size,
    size_t* output_len) {
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
       (barcode && *barcode && !sync_safe_token(barcode, TAGTINKER_BC_LEN)) || width == 0U || height == 0U ||
       page > 7U || byte_count == 0U) {
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

    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, app->ble_sync_temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(ok) {
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!ok) return false;

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
    app->ble_status_text[0] = '\0';
    ble_set_status(app, "Upload started");
    ble_send_line(app, compact_protocol ? "AB" : "TT_ACK|BEGIN");
    return true;
}

static bool sync_set_job_barcode(TagTinkerApp* app, const char* barcode) {
    if(!app || !app->ble_sync_job_active || !barcode || !sync_safe_token(barcode, TAGTINKER_BC_LEN)) {
        return false;
    }

    if(app->ble_sync_barcode[0] != '\0' && strcmp(app->ble_sync_barcode, barcode) != 0) {
        return false;
    }

    strncpy(app->ble_sync_barcode, barcode, TAGTINKER_BC_LEN);
    app->ble_sync_barcode[TAGTINKER_BC_LEN] = '\0';
    ble_send_line(app, app->ble_sync_compact_protocol ? "AT" : "TT_ACK|TARGET");
    return true;
}

static bool sync_append_chunk(TagTinkerApp* app, uint16_t sequence, const char* payload) {
    if(!app || !app->ble_sync_job_active || !payload || sequence == 0U) return false;

    if(sequence == app->ble_sync_last_chunk) {
        char ack[32];
        if(app->ble_sync_compact_protocol) {
            snprintf(ack, sizeof(ack), "A%04X", sequence);
        } else {
            snprintf(ack, sizeof(ack), "TT_ACK|%u", sequence);
        }
        ble_send_line(app, ack);
        return true;
    }

    if(sequence != (uint16_t)(app->ble_sync_last_chunk + 1U)) return false;

    uint8_t decoded[TAGTINKER_SYNC_MAX_CHUNK_BYTES];
    size_t decoded_len = 0;
    if(!sync_decode_base64(payload, decoded, sizeof(decoded), &decoded_len)) return false;
    if((app->ble_sync_received_bytes + decoded_len) > app->ble_sync_expected_bytes) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, app->ble_sync_temp_path, FSAM_WRITE, FSOM_OPEN_APPEND);
    if(ok) {
        ok = storage_file_write(file, decoded, decoded_len) == decoded_len;
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    if(!ok) return false;

    app->ble_sync_received_bytes += decoded_len;
    app->ble_sync_last_chunk = sequence;
    app->ble_synced_lines = sequence;

    snprintf(app->ble_status_text, sizeof(app->ble_status_text), "RX %u chunks", sequence);

    char ack[32];
    if(app->ble_sync_compact_protocol) {
        snprintf(ack, sizeof(ack), "A%04X", sequence);
    } else {
        snprintf(ack, sizeof(ack), "TT_ACK|%u", sequence);
    }
    ble_send_line(app, ack);
    return true;
}

static bool sync_finish_job(TagTinkerApp* app, const char* job_id) {
    if(!app || !sync_safe_token(job_id, TAGTINKER_SYNC_JOB_ID_LEN)) return false;

    if(!app->ble_sync_job_active && strcmp(job_id, app->ble_sync_last_job_id) == 0) {
        char ack[32];
        if(app->ble_sync_last_compact_protocol) {
            snprintf(ack, sizeof(ack), "AE");
        } else {
            snprintf(ack, sizeof(ack), "TT_ACK|END|%u", app->ble_sync_last_completed_chunks);
        }
        ble_send_line(app, ack);
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

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_remove(storage, app->ble_sync_final_path);
    bool ok =
        storage_common_rename(storage, app->ble_sync_temp_path, app->ble_sync_final_path) ==
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

    char ack[32];
    if(app->ble_sync_compact_protocol) {
        snprintf(ack, sizeof(ack), "AE");
    } else {
        snprintf(ack, sizeof(ack), "TT_ACK|END|%u", app->ble_sync_last_chunk);
    }
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
    ble_send_line(app, ack);
    sync_clear_active_job(app);
    return true;
}

static void sync_apply_line(TagTinkerApp* app, const char* line) {
    if(!app || !line) return;

    /*
     * Compact upload protocol:
     * B<job><w><h><page><size>  begin upload
     * C<barcode>               bind upload to a target
     * D<seq><base64>           append one chunk
     * E<job>                   finish upload
     *
     * Acks are AB, AT, A<seq>, and AE.
     */
    if(strcmp(line, "TT_PING") == 0) {
        ble_set_status(app, "RX ping");
        ble_send_line(app, "TT_PONG");
        return;
    }

    if(strcmp(line, "TT_LIST_TARGETS") == 0) {
        sync_send_targets(app);
        return;
    }

    if(strncmp(line, "TT_BEGIN|", 9) == 0) {
        char temp[160];
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

        if(job_id && barcode && width && height && page && bytes &&
           sync_begin_job(
               app,
               job_id,
               barcode,
               (uint16_t)atoi(width),
               (uint16_t)atoi(height),
               (uint8_t)atoi(page),
               (uint32_t)strtoul(bytes, NULL, 10),
               false)) {
            return;
        }

        ble_set_status(app, "BEGIN failed");
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

    size_t compact_len = strlen(line);
    if(line[0] == 'B' && compact_len >= 18U && compact_len <= 20U) {
        char job_id[7];
        char width_hex[4];
        char height_hex[4];
        char page_hex[2];
        char size_hex[7];

        memcpy(job_id, line + 1, 6);
        job_id[6] = '\0';
        memcpy(width_hex, line + 7, 3);
        width_hex[3] = '\0';
        memcpy(height_hex, line + 10, 3);
        height_hex[3] = '\0';
        memcpy(page_hex, line + 13, 1);
        page_hex[1] = '\0';
        size_t size_hex_len = compact_len - 14U;
        memcpy(size_hex, line + 14, size_hex_len);
        size_hex[size_hex_len] = '\0';

        if(sync_begin_job(
               app,
               job_id,
               NULL,
               (uint16_t)strtoul(width_hex, NULL, 16),
               (uint16_t)strtoul(height_hex, NULL, 16),
               (uint8_t)strtoul(page_hex, NULL, 16),
               (uint32_t)strtoul(size_hex, NULL, 16),
               true)) {
            return;
        }

        ble_set_status(app, "BEGIN failed");
        return;
    }

    if(line[0] == 'C' && strlen(line) == 18U) {
        if(sync_set_job_barcode(app, line + 1)) {
            return;
        }

        ble_set_status(app, "TARGET failed");
        return;
    }

    if(line[0] == 'D' && strlen(line) > 5U) {
        char seq_hex[5];
        memcpy(seq_hex, line + 1, 4);
        seq_hex[4] = '\0';

        if(sync_append_chunk(app, (uint16_t)strtoul(seq_hex, NULL, 16), line + 5)) {
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

    if(line[0] == 'E' && strlen(line) == 7U) {
        if(sync_finish_job(app, line + 1)) {
            return;
        }

        ble_set_status(app, "END failed");
        return;
    }

    ble_set_rx_status(app, line);
}

static uint16_t ble_rx_callback(SerialServiceEvent event, void* context) {
    TagTinkerApp* app = context;
    if(event.event == SerialServiceEventTypeDataReceived) {
        for(uint16_t i = 0; i < event.data.size; i++) {
            char c = (char)event.data.buffer[i];
            if(c == '\n' || c == '\r') {
                if(app->ble_rx_len > 0U && !app->ble_rx_pending_ready) {
                    app->ble_rx_line[app->ble_rx_len] = '\0';
                    strncpy(
                        app->ble_rx_pending_line,
                        app->ble_rx_line,
                        sizeof(app->ble_rx_pending_line) - 1U);
                    app->ble_rx_pending_line[sizeof(app->ble_rx_pending_line) - 1U] = '\0';
                    app->ble_rx_pending_ready = true;
                    app->ble_rx_len = 0;
                }
            } else if(!app->ble_rx_pending_ready && app->ble_rx_len < (sizeof(app->ble_rx_line) - 1U)) {
                app->ble_rx_line[app->ble_rx_len++] = c;
            }
        }
    }

    if(app->ble_rx_pending_ready) return 0U;
    return (uint16_t)((sizeof(app->ble_rx_line) - 1U) - app->ble_rx_len);
}

static void bt_status_cb(BtStatus status, void* context) {
    TagTinkerApp* app = context;
    app->ble_status = status;

    switch(status) {
    case BtStatusConnected:
        if(app->ble_serial) {
            ble_profile_serial_set_event_callback(
                app->ble_serial, TAGTINKER_BLE_FLOW_WINDOW, ble_rx_callback, app);
            ble_profile_serial_set_rpc_active(app->ble_serial, false);
        }
        ble_set_status(app, "Connected");
        ble_send_line(app, "TT_HELLO");
        break;
    case BtStatusAdvertising:
        ble_set_status(app, "Waiting phone");
        break;
    case BtStatusOff:
        ble_set_status(app, "Bluetooth off");
        break;
    default:
        ble_set_status(app, "BLE idle");
        break;
    }
}

static void ble_sync_start(TagTinkerApp* app) {
    if(!app || !app->bt || app->ble_sync_active) return;

    bt_disconnect(app->bt);
    bt_set_status_changed_callback(app->bt, bt_status_cb, app);
    app->ble_serial = bt_profile_start(app->bt, ble_profile_serial, NULL);
    if(!app->ble_serial) {
        ble_set_status(app, "BLE start fail");
        return;
    }

    app->ble_synced_lines = 0;
    app->ble_rx_len = 0;
    app->ble_rx_line[0] = '\0';
    app->ble_rx_pending_line[0] = '\0';
    app->ble_rx_pending_ready = false;
    sync_clear_active_job(app);
    app->ble_sync_last_job_id[0] = '\0';
    app->ble_sync_last_completed_chunks = 0;
    app->ble_sync_last_compact_protocol = false;
    app->ble_sync_ready_target = -1;
    ble_profile_serial_set_event_callback(app->ble_serial, TAGTINKER_BLE_FLOW_WINDOW, ble_rx_callback, app);
    ble_profile_serial_set_rpc_active(app->ble_serial, false);
    ble_set_status(app, "Waiting phone");
    app->ble_sync_active = true;
}

static void ble_sync_stop(TagTinkerApp* app) {
    if(!app || !app->ble_sync_active) return;

    if(app->ble_sync_job_active) {
        sync_abort_active_job(app);
    }

    bt_profile_restore_default(app->bt);
    app->ble_serial = NULL;
    app->ble_sync_active = false;
    app->ble_sync_ready_target = -1;
}

static bool about_input_cb(InputEvent* event, void* context) {
    TagTinkerApp* app = context;
    if(event->type != InputTypeShort || event->key != InputKeyOk) {
        return false;
    }

    if(app->ble_sync_ready_target < 0 || app->ble_sync_ready_target >= app->target_count) {
        return false;
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, AboutEventSendLatestPhone);
    return true;
}

static void about_draw_cb(Canvas* canvas, void* _model) {
    AboutViewModel* model = _model;

    canvas_set_font(canvas, FontPrimary);
    if(model->mode == 1U) {
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Phone Sync");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            64,
            20,
            AlignCenter,
            AlignTop,
            model->can_send_latest ? model->target_name : "Pick target on phone");
        canvas_draw_str_aligned(
            canvas,
            64,
            30,
            AlignCenter,
            AlignTop,
            model->can_send_latest ? "Press OK to send latest" : "Upload image to Flipper");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignTop, "Status:");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, model->status_text);
        if(model->can_send_latest) {
            elements_button_center(canvas, "Send");
        }
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 10, AlignCenter, AlignTop, TAGTINKER_DISPLAY_NAME " v" TAGTINKER_VERSION);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Ported by I12BP8");
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "Research by furrtek");
    }
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
    if(app->ble_status_text[0] == '\0') {
        ble_set_status(app, mode == 1U ? "Waiting phone" : "Idle");
    }
    model->mode = mode;
    model->tick = 0;
    model->can_send_latest = false;
    model->target_name[0] = '\0';
    strncpy(model->status_text, app->ble_status_text, sizeof(model->status_text) - 1U);
    model->status_text[sizeof(model->status_text) - 1U] = '\0';
    view_commit_model(app->about_view, true);

    if(mode == 1U) {
        ble_sync_start(app);
    }

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
                    app,
                    target->plid,
                    image.image_path,
                    image.width,
                    image.height,
                    image.page);
                app->tx_spam = false;
                app->ble_sync_ready_target = -1;
                scene_manager_next_scene(app->scene_manager, TagTinkerSceneTransmit);
                return true;
            }
        }
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        AboutViewModel* model = view_get_model(app->about_view);
        model->tick++;
        if(app->ble_rx_pending_ready) {
            sync_apply_line(app, app->ble_rx_pending_line);
            app->ble_rx_pending_line[0] = '\0';
            app->ble_rx_pending_ready = false;
            if(app->ble_serial) {
                ble_profile_serial_notify_buffer_is_empty(app->ble_serial);
            }
        }
        model->can_send_latest =
            (app->ble_sync_ready_target >= 0 && app->ble_sync_ready_target < app->target_count);
        if(model->can_send_latest) {
            strncpy(
                model->target_name,
                app->targets[app->ble_sync_ready_target].name,
                sizeof(model->target_name) - 1U);
            model->target_name[sizeof(model->target_name) - 1U] = '\0';
        } else {
            model->target_name[0] = '\0';
        }
        strncpy(model->status_text, app->ble_status_text, sizeof(model->status_text) - 1U);
        model->status_text[sizeof(model->status_text) - 1U] = '\0';
        view_commit_model(app->about_view, true);
        return true;
    }

    return false;
}

void tagtinker_scene_about_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    ble_sync_stop(app);
}
