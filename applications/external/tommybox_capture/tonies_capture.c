#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/slix/slix.h>
#include <nfc/protocols/slix/slix_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>

#define TAG "ToniesCapture"

/*
 * SlixPassword is a uint32_t stored in little-endian byte order on STM32WB55:
 * 0x5B6EFD7F in memory is bytes { 0x7F, 0xFD, 0x6E, 0x5B } ("7F FD 6E 5B").
 */
#define TONIE_PRIVACY_PASSWORD (0x5B6EFD7FU)
#define TONIE_DESTROY_PASSWORD (0x0F0F0F0FU)
#define TONIE_EAS_PASSWORD     (0x00000000U)

#define TONIE_BLOCK_COUNT (8U)
#define TONIE_BLOCK_SIZE  (4U)
#define TONIE_DATA_SIZE   (TONIE_BLOCK_COUNT * TONIE_BLOCK_SIZE)
#define TONIE_UID_SIZE    (8U)

#define NFC_FOLDER   EXT_PATH("nfc")
#define MAX_NAME_LEN (64)

typedef enum {
    ToniesViewMenu,
    ToniesViewCapture,
    ToniesViewRename,
    ToniesViewAudit,
} ToniesView;

typedef enum {
    ToniesMenuScan = 0,
    ToniesMenuAudit = 1,
} ToniesMenuIndex;

typedef enum {
    ToniesEventMenuScan = 100,
    ToniesEventMenuAudit,
    ToniesEventScanningProgress,
    ToniesEventPartialRead,
    ToniesEventCompleteRead,
    ToniesEventDuplicateRead,
    ToniesEventActiveError,
    ToniesEventBtnRename,
    ToniesEventRenameDone,
} ToniesCustomEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget_capture;
    TextInput* text_input;
    TextBox* text_box_audit;
    NotificationApp* notifications;
    Storage* storage;

    ToniesView current_view;

    Nfc* nfc;
    NfcPoller* poller;
    NfcDevice* nfc_device;
    SlixData* working_slix_data;
    bool poller_running;

    // Mutex for data shared between NFC poller callback and GUI thread
    FuriMutex* mutex;

    // Candidate read data populated by poller worker
    SlixData* candidate_slix;
    uint8_t partial_blocks_read;
    uint8_t partial_uid[TONIE_UID_SIZE];

    // Last successfully captured Tonie
    bool has_last_capture;
    uint8_t last_uid[TONIE_UID_SIZE];
    FuriString* last_file_path;
    FuriString* last_file_name;
    uint32_t current_tonie_scans;
    uint32_t retry_count;

    // Buffer for renaming
    char rename_buffer[MAX_NAME_LEN];

    // Audit report buffer
    FuriString* audit_text;
} ToniesCaptureApp;

static const NotificationSequence sequence_partial_read = {
    &message_red_255,
    &message_green_255,
    &message_vibro_on,
    &message_delay_10,
    &message_vibro_off,
    NULL,
};

/**
 * Count how many 4-byte blocks are non-zero and check if the 32-byte payload
 * passes the Tonies completeness rules:
 *  1. Block count == 8 and block size == 4 (32 bytes total)
 *  2. Block 7 (bytes 28..31, the final block read sequentially) is not all zeros
 *  3. No 5 consecutive 0x00 bytes anywhere in Data Content (matches validate_files.sh)
 */
static bool tonies_verify_blocks(const uint8_t* data, size_t data_len, uint8_t* out_valid_blocks) {
    if(!data || data_len < TONIE_DATA_SIZE) {
        if(out_valid_blocks) *out_valid_blocks = 0;
        return false;
    }

    uint8_t non_zero_blocks = 0;
    for(size_t b = 0; b < TONIE_BLOCK_COUNT; b++) {
        const uint8_t* blk = &data[b * TONIE_BLOCK_SIZE];
        if(blk[0] != 0 || blk[1] != 0 || blk[2] != 0 || blk[3] != 0) {
            non_zero_blocks++;
        }
    }
    if(out_valid_blocks) {
        *out_valid_blocks = non_zero_blocks;
    }

    // Final block (block 7) must not be 00 00 00 00 (sequential read stops on error)
    const uint8_t* last_blk = &data[(TONIE_BLOCK_COUNT - 1) * TONIE_BLOCK_SIZE];
    if(last_blk[0] == 0 && last_blk[1] == 0 && last_blk[2] == 0 && last_blk[3] == 0) {
        return false;
    }

    // Check for 5 consecutive 0x00 bytes (same rule as scripts/validate_files.sh)
    size_t zero_run = 0;
    for(size_t i = 0; i < TONIE_DATA_SIZE; i++) {
        if(data[i] == 0x00) {
            zero_run++;
            if(zero_run >= 5) {
                return false;
            }
        } else {
            zero_run = 0;
        }
    }

    return true;
}

/**
 * Verify a saved .nfc file on the SD card by reading it back via FlipperFormat
 * and checking all required Tonie SLIX-L fields.
 */
static bool
    tonies_verify_saved_file(Storage* storage, const char* file_path, FuriString* out_reason) {
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    bool valid = false;

    do {
        if(!flipper_format_file_open_existing(ff, file_path)) {
            if(out_reason) furi_string_set(out_reason, "Cannot open file");
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, temp_str, &version) ||
           !furi_string_equal_str(temp_str, "Flipper NFC device") || version != 4) {
            if(out_reason) furi_string_set(out_reason, "Invalid NFC v4 header");
            break;
        }

        if(!flipper_format_read_string(ff, "Device type", temp_str) ||
           !furi_string_equal_str(temp_str, "SLIX")) {
            if(out_reason) furi_string_set(out_reason, "Not a SLIX device");
            break;
        }

        uint8_t uid[TONIE_UID_SIZE] = {0};
        if(!flipper_format_read_hex(ff, "UID", uid, TONIE_UID_SIZE)) {
            if(out_reason) furi_string_set(out_reason, "Missing 8-byte UID");
            break;
        }

        uint32_t block_count = 0;
        uint8_t block_size = 0;
        if(!flipper_format_read_uint32(ff, "Block Count", &block_count, 1) ||
           block_count != TONIE_BLOCK_COUNT) {
            if(out_reason) furi_string_set(out_reason, "Block Count != 8");
            break;
        }

        if(!flipper_format_read_hex(ff, "Block Size", &block_size, 1) ||
           block_size != TONIE_BLOCK_SIZE) {
            if(out_reason) furi_string_set(out_reason, "Block Size != 04");
            break;
        }

        uint8_t data_content[TONIE_DATA_SIZE] = {0};
        if(!flipper_format_read_hex(ff, "Data Content", data_content, TONIE_DATA_SIZE)) {
            if(out_reason) furi_string_set(out_reason, "Missing 32B Data Content");
            break;
        }

        uint8_t valid_blocks = 0;
        if(!tonies_verify_blocks(data_content, TONIE_DATA_SIZE, &valid_blocks)) {
            if(out_reason) {
                furi_string_printf(out_reason, "Incomplete (%u/8 blocks)", valid_blocks);
            }
            break;
        }

        uint8_t pwd_privacy[4] = {0};
        if(!flipper_format_read_hex(ff, "Password Privacy", pwd_privacy, 4)) {
            if(out_reason) furi_string_set(out_reason, "Missing Password Privacy");
            break;
        }

        if(pwd_privacy[0] != 0x7F || pwd_privacy[1] != 0xFD || pwd_privacy[2] != 0x6E ||
           pwd_privacy[3] != 0x5B) {
            if(out_reason) {
                furi_string_printf(
                    out_reason,
                    "Bad Pwd (%02X%02X%02X%02X)",
                    pwd_privacy[0],
                    pwd_privacy[1],
                    pwd_privacy[2],
                    pwd_privacy[3]);
            }
            break;
        }

        bool privacy_mode = true;
        if(!flipper_format_read_bool(ff, "Privacy Mode", &privacy_mode, 1) || privacy_mode) {
            if(out_reason) furi_string_set(out_reason, "Privacy Mode != false");
            break;
        }

        valid = true;
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(ff);
    return valid;
}

static void tonies_capture_button_callback(GuiButtonType result, InputType type, void* context) {
    ToniesCaptureApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight && app->has_last_capture) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventBtnRename);
    }
}

static void tonies_show_waiting_ui(ToniesCaptureApp* app) {
    widget_reset(app->widget_capture);
    widget_add_rect_element(app->widget_capture, 4, 6, 120, 42, 4, false);
    widget_add_string_element(
        app->widget_capture, 64, 16, AlignCenter, AlignTop, FontPrimary, "Hold Tonie to Scan");
    widget_add_string_element(
        app->widget_capture, 64, 31, AlignCenter, AlignTop, FontSecondary, "0 / 8");
}

static void tonies_show_progress_ui(ToniesCaptureApp* app, uint8_t blocks, uint32_t attempt) {
    widget_reset(app->widget_capture);
    widget_add_rect_element(app->widget_capture, 4, 6, 120, 42, 4, false);

    FuriString* hdr = furi_string_alloc();
    furi_string_printf(hdr, "Scanning: %u / 8", blocks);
    widget_add_string_element(
        app->widget_capture, 64, 16, AlignCenter, AlignTop, FontPrimary, furi_string_get_cstr(hdr));
    furi_string_free(hdr);

    if(attempt > 0) {
        FuriString* sub = furi_string_alloc();
        furi_string_printf(sub, "Hold steady (#%lu)", (unsigned long)attempt);
        widget_add_string_element(
            app->widget_capture,
            64,
            31,
            AlignCenter,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(sub));
        furi_string_free(sub);
    }
}

static void tonies_show_saved_ui(ToniesCaptureApp* app) {
    widget_reset(app->widget_capture);

    // Bold double-framed banner to make SAVED unmistakable
    widget_add_rect_element(app->widget_capture, 2, 2, 124, 36, 4, false);
    widget_add_rect_element(app->widget_capture, 4, 4, 120, 32, 3, false);

    widget_add_string_element(
        app->widget_capture, 64, 9, AlignCenter, AlignTop, FontPrimary, "SAVED!   8 / 8");

    FuriString* scan_line = furi_string_alloc();
    furi_string_printf(scan_line, "Scan #%lu", (unsigned long)app->current_tonie_scans);
    widget_add_string_element(
        app->widget_capture,
        64,
        23,
        AlignCenter,
        AlignTop,
        FontSecondary,
        furi_string_get_cstr(scan_line));
    furi_string_free(scan_line);

    FuriString* fname = furi_string_alloc();
    furi_string_printf(fname, "%s.nfc", furi_string_get_cstr(app->last_file_name));
    widget_add_string_element(
        app->widget_capture,
        4,
        45,
        AlignLeft,
        AlignTop,
        FontSecondary,
        furi_string_get_cstr(fname));
    furi_string_free(fname);

    widget_add_button_element(
        app->widget_capture, GuiButtonTypeRight, "Rename", tonies_capture_button_callback, app);
}

static NfcCommand tonies_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolSlix);
    ToniesCaptureApp* app = context;
    SlixPollerEvent* slix_event = event.event_data;

    if(slix_event->type == SlixPollerEventTypePrivacyUnlockRequest) {
        // Mirror Extra Actions -> Unlock SLIX-L -> Auth As TommyBox (0x5B6EFD7F -> 7F FD 6E 5B)
        slix_event->data->privacy_password.password = TONIE_PRIVACY_PASSWORD;
        slix_event->data->privacy_password.password_set = true;

        const SlixData* slix_data = nfc_poller_get_data(app->poller);
        if(slix_data) {
            size_t uid_len = 0;
            const uint8_t* uid = slix_get_uid(slix_data, &uid_len);
            const Iso15693_3Data* iso_data = slix_get_base_data(slix_data);
            if(uid && uid_len == TONIE_UID_SIZE && uid[0] == 0xE0) {
                uint8_t valid_blocks = 0;
                if(iso_data && iso_data->block_data != NULL &&
                   iso15693_3_get_block_count(iso_data) == TONIE_BLOCK_COUNT) {
                    const uint8_t* raw_blocks = simple_array_cget_data(iso_data->block_data);
                    tonies_verify_blocks(raw_blocks, TONIE_DATA_SIZE, &valid_blocks);
                }
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->partial_blocks_read = valid_blocks;
                memcpy(app->partial_uid, uid, TONIE_UID_SIZE);
                furi_mutex_release(app->mutex);
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ToniesEventScanningProgress);
            }
        }

        return NfcCommandContinue;
    } else if(slix_event->type == SlixPollerEventTypeError) {
        const SlixData* slix_data = nfc_poller_get_data(app->poller);
        if(slix_data) {
            size_t uid_len = 0;
            const uint8_t* uid = slix_get_uid(slix_data, &uid_len);
            if(uid && uid_len == TONIE_UID_SIZE && uid[0] == 0xE0) {
                view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventActiveError);
                return NfcCommandStop;
            }
        }
        return NfcCommandContinue;
    } else if(slix_event->type == SlixPollerEventTypeReady) {
        const SlixData* slix_data = nfc_poller_get_data(app->poller);
        const Iso15693_3Data* iso_data = slix_get_base_data(slix_data);

        size_t uid_len = 0;
        const uint8_t* uid = slix_get_uid(slix_data, &uid_len);
        uint16_t block_count = iso15693_3_get_block_count(iso_data);
        uint8_t block_size = iso15693_3_get_block_size(iso_data);

        uint8_t valid_blocks = 0;
        bool blocks_ok = false;
        if(block_count == TONIE_BLOCK_COUNT && block_size == TONIE_BLOCK_SIZE &&
           iso_data->block_data != NULL) {
            const uint8_t* raw_blocks = simple_array_cget_data(iso_data->block_data);
            blocks_ok = tonies_verify_blocks(raw_blocks, TONIE_DATA_SIZE, &valid_blocks);
        }

        if(!blocks_ok || uid_len != TONIE_UID_SIZE) {
            // Partial / incomplete read! Do NOT accept or save it.
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->partial_blocks_read = valid_blocks;
            if(uid && uid_len == TONIE_UID_SIZE) {
                memcpy(app->partial_uid, uid, TONIE_UID_SIZE);
            }
            app->retry_count++;
            furi_mutex_release(app->mutex);

            view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventPartialRead);
            return NfcCommandStop;
        }

        // All 8 blocks are verified non-zero and complete!
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool is_duplicate = app->has_last_capture &&
                            (memcmp(app->last_uid, uid, TONIE_UID_SIZE) == 0);
        if(!is_duplicate) {
            slix_copy(app->candidate_slix, slix_data);
            app->candidate_slix->passwords[SlixPasswordTypePrivacy] = TONIE_PRIVACY_PASSWORD;
            app->candidate_slix->passwords[SlixPasswordTypeDestroy] = TONIE_DESTROY_PASSWORD;
            app->candidate_slix->passwords[SlixPasswordTypeEasAfi] = TONIE_EAS_PASSWORD;
            app->candidate_slix->privacy = false;
            app->candidate_slix->capabilities = SlixCapabilitiesDefault;
        }
        furi_mutex_release(app->mutex);

        if(is_duplicate) {
            view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventDuplicateRead);
            return NfcCommandStop;
        }

        view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventCompleteRead);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void tonies_start_poller(ToniesCaptureApp* app) {
    if(!app->poller_running) {
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolSlix);
        nfc_poller_start(app->poller, tonies_poller_callback, app);
        app->poller_running = true;
        notification_message(app->notifications, &sequence_blink_start_cyan);
    }
}

static void tonies_stop_poller(ToniesCaptureApp* app) {
    if(app->poller_running) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
        app->poller_running = false;
        notification_message(app->notifications, &sequence_blink_stop);
    }
}

static void tonies_restart_poller(ToniesCaptureApp* app) {
    tonies_stop_poller(app);
    tonies_start_poller(app);
}

/**
 * Search /ext/nfc for an existing .nfc file with the same 8-byte UID.
 * - If a valid Tonie file already exists with this UID, returns true and sets out_existing_name.
 * - If a broken/incomplete file exists with this UID (like SLIX-L_*.nfc), deletes the broken file
 *   so it gets cleanly replaced!
 */
static bool tonies_find_existing_uid_on_sd(
    ToniesCaptureApp* app,
    const uint8_t* target_uid,
    FuriString* out_existing_name,
    FuriString* out_existing_path) {
    File* dir = storage_file_alloc(app->storage);
    FileInfo file_info;
    char name_buf[128];
    bool found_valid = false;

    FuriString* full_path = furi_string_alloc();
    FuriString* broken_to_delete = furi_string_alloc();

    if(storage_dir_open(dir, NFC_FOLDER)) {
        while(storage_dir_read(dir, &file_info, name_buf, sizeof(name_buf))) {
            if(file_info_is_dir(&file_info)) continue;
            if(name_buf[0] == '.') continue; // Skip macOS ._ resource fork files
            size_t len = strlen(name_buf);
            if(len < 5 || strcmp(&name_buf[len - 4], ".nfc") != 0) continue;

            furi_string_printf(full_path, "%s/%s", NFC_FOLDER, name_buf);

            FlipperFormat* ff = flipper_format_file_alloc(app->storage);
            uint8_t file_uid[TONIE_UID_SIZE] = {0};
            bool uid_read = false;
            if(flipper_format_file_open_existing(ff, furi_string_get_cstr(full_path))) {
                uid_read = flipper_format_read_hex(ff, "UID", file_uid, TONIE_UID_SIZE);
            }
            flipper_format_free(ff);

            if(uid_read && memcmp(file_uid, target_uid, TONIE_UID_SIZE) == 0) {
                if(tonies_verify_saved_file(app->storage, furi_string_get_cstr(full_path), NULL)) {
                    if(out_existing_name) {
                        furi_string_set_strn(out_existing_name, name_buf, len - 4);
                    }
                    if(out_existing_path) {
                        furi_string_set(out_existing_path, full_path);
                    }
                    found_valid = true;
                    break;
                } else {
                    furi_string_set(broken_to_delete, full_path);
                }
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    if(!furi_string_empty(broken_to_delete)) {
        storage_simply_remove(app->storage, furi_string_get_cstr(broken_to_delete));
    }

    furi_string_free(broken_to_delete);
    furi_string_free(full_path);
    return found_valid;
}

static void tonies_handle_complete_read(ToniesCaptureApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    slix_copy(app->working_slix_data, app->candidate_slix);
    furi_mutex_release(app->mutex);

    size_t uid_len = 0;
    const uint8_t* uid = slix_get_uid(app->working_slix_data, &uid_len);
    if(!uid || uid_len != TONIE_UID_SIZE) {
        return;
    }

    storage_simply_mkdir(app->storage, NFC_FOLDER);

    if(tonies_find_existing_uid_on_sd(app, uid, app->last_file_name, app->last_file_path)) {
        // Already exists and verified on SD card!
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        memcpy(app->last_uid, uid, TONIE_UID_SIZE);
        app->has_last_capture = true;
        app->current_tonie_scans = 1;
        app->retry_count = 0;
        furi_mutex_release(app->mutex);

        notification_message(app->notifications, &sequence_success);
        tonies_show_saved_ui(app);
        return;
    }

    furi_string_printf(
        app->last_file_name, "Tonie_%02X%02X%02X%02X", uid[4], uid[5], uid[6], uid[7]);
    furi_string_printf(
        app->last_file_path, "%s/%s.nfc", NFC_FOLDER, furi_string_get_cstr(app->last_file_name));

    nfc_device_set_data(app->nfc_device, NfcProtocolSlix, app->working_slix_data);
    bool saved = nfc_device_save(app->nfc_device, furi_string_get_cstr(app->last_file_path));

    FuriString* verify_err = furi_string_alloc();
    bool verified_on_disk = false;
    if(saved) {
        verified_on_disk = tonies_verify_saved_file(
            app->storage, furi_string_get_cstr(app->last_file_path), verify_err);
    }

    if(saved && verified_on_disk) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        memcpy(app->last_uid, uid, TONIE_UID_SIZE);
        app->has_last_capture = true;
        app->current_tonie_scans = 1;
        app->retry_count = 0;
        furi_mutex_release(app->mutex);

        notification_message(app->notifications, &sequence_success);
        tonies_show_saved_ui(app);
    } else {
        notification_message(app->notifications, &sequence_error);
        widget_reset(app->widget_capture);
        widget_add_string_element(
            app->widget_capture, 64, 14, AlignCenter, AlignTop, FontPrimary, "Save Failed!");
        widget_add_string_element(
            app->widget_capture,
            64,
            30,
            AlignCenter,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(verify_err));
    }

    furi_string_free(verify_err);
}

static void tonies_rename_callback(void* context) {
    ToniesCaptureApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventRenameDone);
}

static void tonies_submenu_callback(void* context, uint32_t index) {
    ToniesCaptureApp* app = context;
    if(index == ToniesMenuScan) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventMenuScan);
    } else if(index == ToniesMenuAudit) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ToniesEventMenuAudit);
    }
}

static void tonies_run_sd_audit(ToniesCaptureApp* app) {
    furi_string_reset(app->audit_text);
    furi_string_cat_str(app->audit_text, "=== /ext/nfc Audit ===\n");

    File* dir = storage_file_alloc(app->storage);
    FileInfo file_info;
    char name_buf[128];

    uint32_t total_nfc = 0;
    uint32_t valid_count = 0;
    uint32_t broken_count = 0;

    FuriString* full_path = furi_string_alloc();
    FuriString* reason = furi_string_alloc();

    if(storage_dir_open(dir, NFC_FOLDER)) {
        while(storage_dir_read(dir, &file_info, name_buf, sizeof(name_buf))) {
            if(file_info_is_dir(&file_info)) continue;
            if(name_buf[0] == '.') continue;
            size_t len = strlen(name_buf);
            if(len < 5 || strcmp(&name_buf[len - 4], ".nfc") != 0) continue;

            total_nfc++;
            furi_string_printf(full_path, "%s/%s", NFC_FOLDER, name_buf);

            if(tonies_verify_saved_file(app->storage, furi_string_get_cstr(full_path), reason)) {
                valid_count++;
            } else {
                broken_count++;
                furi_string_cat_printf(
                    app->audit_text, "[BAD] %s\n  -> %s\n", name_buf, furi_string_get_cstr(reason));
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    FuriString* summary = furi_string_alloc();
    furi_string_printf(
        summary,
        "Scanned: %lu .nfc files\nValid Tonies: %lu\nIncomplete/Non-Tonie: %lu\n\n%s",
        (unsigned long)total_nfc,
        (unsigned long)valid_count,
        (unsigned long)broken_count,
        broken_count == 0 ? "All SLIX Tonie files in /ext/nfc are complete!" :
                            furi_string_get_cstr(app->audit_text));

    furi_string_move(app->audit_text, summary);
    furi_string_free(full_path);
    furi_string_free(reason);

    text_box_reset(app->text_box_audit);
    text_box_set_font(app->text_box_audit, TextBoxFontText);
    text_box_set_text(app->text_box_audit, furi_string_get_cstr(app->audit_text));
}

static bool tonies_custom_event_callback(void* context, uint32_t event) {
    ToniesCaptureApp* app = context;

    if(event == ToniesEventMenuScan) {
        app->current_view = ToniesViewCapture;
        if(app->has_last_capture) {
            tonies_show_saved_ui(app);
        } else {
            tonies_show_waiting_ui(app);
        }
        view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewCapture);
        tonies_start_poller(app);
        return true;
    } else if(event == ToniesEventMenuAudit) {
        tonies_stop_poller(app);
        app->current_view = ToniesViewAudit;
        tonies_run_sd_audit(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewAudit);
        return true;
    } else if(event == ToniesEventScanningProgress) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        uint8_t blocks = app->partial_blocks_read;
        bool same_as_last = app->has_last_capture &&
                            (memcmp(app->last_uid, app->partial_uid, TONIE_UID_SIZE) == 0);
        furi_mutex_release(app->mutex);

        if(!same_as_last) {
            tonies_show_progress_ui(app, blocks, 0);
        }
        return true;
    } else if(event == ToniesEventPartialRead) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        uint8_t blocks = app->partial_blocks_read;
        uint32_t retries = app->retry_count;
        furi_mutex_release(app->mutex);

        tonies_show_progress_ui(app, blocks, retries);
        tonies_restart_poller(app);
        notification_message(app->notifications, &sequence_partial_read);
        notification_message(app->notifications, &sequence_blink_start_yellow);
        return true;
    } else if(event == ToniesEventActiveError) {
        tonies_restart_poller(app);
        return true;
    } else if(event == ToniesEventCompleteRead) {
        tonies_stop_poller(app);
        tonies_handle_complete_read(app);
        tonies_start_poller(app);
        return true;
    } else if(event == ToniesEventDuplicateRead) {
        tonies_stop_poller(app);
        app->current_tonie_scans++;
        notification_message(app->notifications, &sequence_success);
        tonies_show_saved_ui(app);
        furi_delay_ms(400);
        tonies_start_poller(app);
        return true;
    } else if(event == ToniesEventBtnRename) {
        tonies_stop_poller(app);
        app->current_view = ToniesViewRename;
        strncpy(app->rename_buffer, furi_string_get_cstr(app->last_file_name), MAX_NAME_LEN - 1);
        app->rename_buffer[MAX_NAME_LEN - 1] = '\0';

        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Name Tonie (.nfc):");
        text_input_set_result_callback(
            app->text_input, tonies_rename_callback, app, app->rename_buffer, MAX_NAME_LEN, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewRename);
        return true;
    } else if(event == ToniesEventRenameDone) {
        if(strlen(app->rename_buffer) > 0) {
            FuriString* new_path = furi_string_alloc();
            furi_string_printf(new_path, "%s/%s.nfc", NFC_FOLDER, app->rename_buffer);

            if(!furi_string_equal(app->last_file_path, new_path)) {
                if(storage_common_rename(
                       app->storage,
                       furi_string_get_cstr(app->last_file_path),
                       furi_string_get_cstr(new_path)) == FSE_OK) {
                    furi_string_set(app->last_file_path, new_path);
                    furi_string_set_str(app->last_file_name, app->rename_buffer);
                }
            }
            furi_string_free(new_path);
        }

        app->current_view = ToniesViewCapture;
        tonies_show_saved_ui(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewCapture);
        tonies_start_poller(app);
        return true;
    }

    return false;
}

static bool tonies_navigation_callback(void* context) {
    ToniesCaptureApp* app = context;
    if(app->current_view == ToniesViewRename) {
        app->current_view = ToniesViewCapture;
        tonies_show_saved_ui(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewCapture);
        tonies_start_poller(app);
        return true;
    } else if(app->current_view == ToniesViewCapture || app->current_view == ToniesViewAudit) {
        tonies_stop_poller(app);
        app->current_view = ToniesViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewMenu);
        return true;
    }
    return false;
}

int32_t tonies_capture_app(void* p) {
    UNUSED(p);

    ToniesCaptureApp* app = malloc(sizeof(ToniesCaptureApp));
    memset(app, 0, sizeof(ToniesCaptureApp));

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    app->working_slix_data = slix_alloc();
    app->candidate_slix = slix_alloc();

    app->last_file_path = furi_string_alloc();
    app->last_file_name = furi_string_alloc();
    app->audit_text = furi_string_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, tonies_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, tonies_navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Tonies Capture");
    submenu_add_item(app->submenu, "Scan Tonies", ToniesMenuScan, tonies_submenu_callback, app);
    submenu_add_item(app->submenu, "Check SD Card", ToniesMenuAudit, tonies_submenu_callback, app);
    view_dispatcher_add_view(app->view_dispatcher, ToniesViewMenu, submenu_get_view(app->submenu));

    app->widget_capture = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ToniesViewCapture, widget_get_view(app->widget_capture));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ToniesViewRename, text_input_get_view(app->text_input));

    app->text_box_audit = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ToniesViewAudit, text_box_get_view(app->text_box_audit));

    app->current_view = ToniesViewMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, ToniesViewMenu);

    view_dispatcher_run(app->view_dispatcher);

    tonies_stop_poller(app);

    view_dispatcher_remove_view(app->view_dispatcher, ToniesViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ToniesViewCapture);
    view_dispatcher_remove_view(app->view_dispatcher, ToniesViewRename);
    view_dispatcher_remove_view(app->view_dispatcher, ToniesViewAudit);

    submenu_free(app->submenu);
    widget_free(app->widget_capture);
    text_input_free(app->text_input);
    text_box_free(app->text_box_audit);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->last_file_path);
    furi_string_free(app->last_file_name);
    furi_string_free(app->audit_text);

    slix_free(app->candidate_slix);
    slix_free(app->working_slix_data);
    nfc_device_free(app->nfc_device);
    nfc_free(app->nfc);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->mutex);

    free(app);
    return 0;
}
