/**
 * @file miband_nfc_scene_uid_check.c
 * @brief UID checker with dynamic allocation and worker thread
 */

#include "miband_nfc_i.h"

#define TAG              "MiBandNfc"
#define INITIAL_CAPACITY 5
#define GROW_FACTOR      2
#define MAX_SCAN_TIME_MS 15000 // 15 secondi max

// ==================== PRIVATE STRUCTURES ====================

typedef struct {
    char filename[64];
    char path[128];
} FoundFile;

typedef struct {
    FoundFile* files;
    size_t count;
    size_t capacity;
} DynamicFileArray;

// Struttura PRIVATA - definita solo qui
typedef struct UidCheckContext {
    Storage* storage;
    uint8_t target_uid[4];
    DynamicFileArray* results;
    MfClassicBlock block0;

    // Status
    volatile bool scan_complete;
    volatile bool scan_aborted;
    volatile uint32_t files_checked;
    volatile uint32_t files_total;

    // Thread control
    FuriThread* thread;

    // Riferimento all'app (SOLO per logging, non chiamare funzioni!)
    struct MiBandNfcApp* app_ref;
} UidCheckContext;

// ==================== PRIVATE FUNCTIONS ====================

static DynamicFileArray* dynamic_array_alloc(void) {
    DynamicFileArray* arr = malloc(sizeof(DynamicFileArray));
    if(!arr) return NULL;

    arr->files = malloc(sizeof(FoundFile) * INITIAL_CAPACITY);
    if(!arr->files) {
        free(arr);
        return NULL;
    }

    arr->count = 0;
    arr->capacity = INITIAL_CAPACITY;

    FURI_LOG_I(TAG, "Array allocated: capacity=%zu", arr->capacity);
    return arr;
}

static void dynamic_array_free(DynamicFileArray* arr) {
    if(arr) {
        if(arr->files) free(arr->files);
        free(arr);
    }
}

static bool dynamic_array_grow(DynamicFileArray* arr) {
    size_t new_capacity = arr->capacity * GROW_FACTOR;

    // Limite di sicurezza: max 500 file
    if(new_capacity > 500) {
        FURI_LOG_W(TAG, "Array max capacity reached");
        return false;
    }

    FoundFile* new_files = realloc(arr->files, sizeof(FoundFile) * new_capacity);
    if(!new_files) {
        FURI_LOG_E(TAG, "Array grow failed");
        return false;
    }

    arr->files = new_files;
    arr->capacity = new_capacity;

    FURI_LOG_I(TAG, "Array grown: capacity=%zu", arr->capacity);
    return true;
}

static bool dynamic_array_add(DynamicFileArray* arr, const char* filename, const char* path) {
    if(arr->count >= arr->capacity) {
        if(!dynamic_array_grow(arr)) {
            return false;
        }
    }

    // Limita esplicitamente la lunghezza
    strncpy(
        arr->files[arr->count].filename, filename, sizeof(arr->files[arr->count].filename) - 1);
    arr->files[arr->count].filename[sizeof(arr->files[arr->count].filename) - 1] = '\0';

    strncpy(arr->files[arr->count].path, path, sizeof(arr->files[arr->count].path) - 1);
    arr->files[arr->count].path[sizeof(arr->files[arr->count].path) - 1] = '\0';

    arr->count++;
    return true;
}

// ==================== WORKER THREAD ====================

static int32_t uid_check_worker_thread(void* context) {
    UidCheckContext* ctx = (UidCheckContext*)context;

    FURI_LOG_I(TAG, "Worker started");

    // Usa variabili locali solo quando necessario
    File* dir = storage_file_alloc(ctx->storage);
    if(!dir) {
        FURI_LOG_E(TAG, "Worker: Failed to alloc dir");
        ctx->scan_complete = true;
        return -1;
    }

    if(!storage_dir_open(dir, NFC_APP_FOLDER)) {
        FURI_LOG_E(TAG, "Worker: Failed to open dir");
        storage_file_free(dir);
        ctx->scan_complete = true;
        return -1;
    }

    FileInfo file_info;
    char name[128]; // Ridotto
    uint32_t start_time = furi_get_tick();

    // Prima passata: conta i file totali
    ctx->files_total = 0;
    while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
        if(ctx->scan_aborted) break;
        if(name[0] == '.') continue;
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t name_len = strlen(name);
        if(name_len < 4) continue;
        if(strcasecmp(&name[name_len - 4], ".nfc") != 0) continue;
        ctx->files_total++;

        // Yield ogni 10 file
        if(ctx->files_total % 10 == 0) {
            furi_delay_ms(10);
        }
    }

    FURI_LOG_I(TAG, "Found %lu .nfc files", ctx->files_total);

    // Torna all'inizio
    storage_dir_close(dir);
    if(!storage_dir_open(dir, NFC_APP_FOLDER)) {
        FURI_LOG_E(TAG, "Worker: Failed to reopen dir");
        storage_file_free(dir);
        ctx->scan_complete = true;
        return -1;
    }

    // Seconda passata: scansiona
    ctx->files_checked = 0;
    FuriString* full_path = furi_string_alloc_set_str(NFC_APP_FOLDER);
    size_t base_path_len = furi_string_size(full_path);

    while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
        // Check abort
        if(ctx->scan_aborted) {
            FURI_LOG_W(TAG, "Worker aborted by user");
            break;
        }

        // Check timeout
        if(furi_get_tick() - start_time > MAX_SCAN_TIME_MS) {
            FURI_LOG_W(TAG, "Worker timeout");
            break;
        }

        if(name[0] == '.') continue;
        if(file_info.flags & FSF_DIRECTORY) continue;

        size_t name_len = strlen(name);
        if(name_len < 4) continue;
        if(strcasecmp(&name[name_len - 4], ".nfc") != 0) continue;

        ctx->files_checked++;

        // FIX: Use furi_string_left instead of self-referencing furi_string_set_strn
        furi_string_left(full_path, base_path_len);
        furi_string_cat_str(full_path, "/");
        furi_string_cat_str(full_path, name);

        // Yield ogni 2 file invece di 3
        if(ctx->files_checked % 2 == 0) {
            furi_delay_ms(30);
        }

        NfcDevice* temp_device = nfc_device_alloc();
        if(!temp_device) {
            FURI_LOG_E(TAG, "Worker: Failed to alloc device");
            continue;
        }

        bool loaded = nfc_device_load(temp_device, furi_string_get_cstr(full_path));

        if(loaded && nfc_device_get_protocol(temp_device) == NfcProtocolMfClassic) {
            const MfClassicData* data = nfc_device_get_data(temp_device, NfcProtocolMfClassic);

            if(data && memcmp(data->block[0].data, ctx->target_uid, 4) == 0) {
                FURI_LOG_I(TAG, "*** MATCH: %s ***", name);

                if(!dynamic_array_add(ctx->results, name, furi_string_get_cstr(full_path))) {
                    FURI_LOG_W(TAG, "Cannot add more files");
                }
            }
        }

        nfc_device_free(temp_device);
        furi_delay_ms(10); // Ridotto da 5 a 10ms
    }

    FURI_LOG_I(
        TAG,
        "Worker done: %lu/%lu files, %zu matches",
        ctx->files_checked,
        ctx->files_total,
        ctx->results->count);

    furi_string_free(full_path);
    storage_dir_close(dir);
    storage_file_free(dir);

    ctx->scan_complete = true;
    return 0;
}

// ==================== REPORT GENERATION ====================

static FuriString* generate_report(UidCheckContext* ctx) {
    FuriString* report = furi_string_alloc();
    if(!report) return NULL;

    furi_string_printf(
        report,
        "Card UID Check\n"
        "==============\n\n"
        "UID: %02X %02X %02X %02X\n"
        "BCC: %02X\n"
        "SAK: %02X\n"
        "ATQA: %02X %02X\n\n"
        "Scan Results:\n"
        "-------------\n"
        "Files checked: %lu\n"
        "Matches: %zu\n\n",
        ctx->block0.data[0],
        ctx->block0.data[1],
        ctx->block0.data[2],
        ctx->block0.data[3],
        ctx->block0.data[4],
        ctx->block0.data[5],
        ctx->block0.data[6],
        ctx->block0.data[7],
        ctx->files_checked,
        ctx->results->count);

    if(ctx->results->count == 0) {
        furi_string_cat_str(report, "No matching files\n\n");
    } else {
        furi_string_cat_str(report, "Matching files:\n");
        furi_string_cat_printf(report, "%zu file(s):\n\n", ctx->results->count);
        for(size_t i = 0; i < ctx->results->count; i++) {
            furi_string_cat_printf(report, "%zu. %s\n", i + 1, ctx->results->files[i].path);
        }
    }

    furi_string_cat_str(report, "\nPress Back");
    return report;
}

// ==================== SCENE HANDLERS ====================

void miband_nfc_scene_uid_check_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    FURI_LOG_I(TAG, "=== UID CHECK START ===");

    // LOG: Inizio scena
    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "UID Check scene started");
    }

    furi_string_reset(app->temp_text_buffer);
    text_box_reset(app->text_box_report);
    popup_reset(app->popup);

    popup_set_header(app->popup, "UID Check", 64, 4, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Place card", 64, 22, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    // STEP 1: Read UID
    Iso14443_3aData iso_data = {0};
    bool read_ok = false;

    for(int i = 0; i < 12 && !read_ok; i++) {
        furi_delay_ms(250);
        if(iso14443_3a_poller_sync_read(app->nfc, &iso_data) == Iso14443_3aErrorNone &&
           iso_data.uid_len >= 4) {
            read_ok = true;
            FURI_LOG_I(
                TAG,
                "UID: %02X %02X %02X %02X",
                iso_data.uid[0],
                iso_data.uid[1],
                iso_data.uid[2],
                iso_data.uid[3]);
        }
    }

    notification_message(app->notifications, &sequence_blink_stop);

    if(!read_ok) {
        // LOG: Lettura fallita
        if(app->logger) {
            miband_logger_log(
                app->logger, LogLevelError, "Failed to read card UID after 12 attempts");
        }
        popup_set_text(app->popup, "Card not found", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(1500);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    // LOG: UID letto
    if(app->logger) {
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "UID read: %02X %02X %02X %02X",
            iso_data.uid[0],
            iso_data.uid[1],
            iso_data.uid[2],
            iso_data.uid[3]);
    }

    notification_message(app->notifications, &sequence_success);

    // STEP 2: Setup context
    UidCheckContext* uid_ctx = malloc(sizeof(UidCheckContext));
    if(!uid_ctx) {
        // LOG: Alloc fallita
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Failed to allocate UID check context");
        }
        popup_set_text(app->popup, "Memory error", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(1500);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    // Inizializza tutti i campi a 0
    memset(uid_ctx, 0, sizeof(UidCheckContext));

    uid_ctx->app_ref = app; // Solo per riferimento, NON per chiamare funzioni!
    app->uid_check_context = uid_ctx; // Salva nell'app

    uid_ctx->results = dynamic_array_alloc();
    if(!uid_ctx->results) {
        // LOG: Alloc array fallita
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Failed to allocate dynamic array");
        }
        free(uid_ctx);
        app->uid_check_context = NULL; // IMPORTANTE: resetta il puntatore
        popup_set_text(app->popup, "Memory error", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(1500);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    uid_ctx->storage = app->storage;
    memcpy(uid_ctx->target_uid, iso_data.uid, 4);
    uid_ctx->scan_complete = false;
    uid_ctx->scan_aborted = false;
    uid_ctx->files_checked = 0;
    uid_ctx->files_total = 0;

    memcpy(uid_ctx->block0.data, iso_data.uid, 4);
    uid_ctx->block0.data[4] = iso_data.uid[0] ^ iso_data.uid[1] ^ iso_data.uid[2] ^
                              iso_data.uid[3];
    uid_ctx->block0.data[5] = iso_data.sak;
    uid_ctx->block0.data[6] = iso_data.atqa[0];
    uid_ctx->block0.data[7] = iso_data.atqa[1];

    // STEP 3: Launch worker
    popup_set_text(app->popup, "Scanning...\n\nInitializing", 64, 22, AlignCenter, AlignTop);

    uid_ctx->thread =
        furi_thread_alloc_ex("UidCheckWorker", 2048, uid_check_worker_thread, uid_ctx);

    furi_thread_start(uid_ctx->thread);

    // STEP 4: Poll worker with UI updates
    uint32_t last_update = furi_get_tick();
    uint32_t last_count = 0;

    while(!uid_ctx->scan_complete) {
        uint32_t now = furi_get_tick();

        // Forza update se counter cambia O ogni 300ms
        if((uid_ctx->files_checked != last_count) || (now - last_update > 300)) {
            // FIX: Use app->temp_text_buffer - popup_set_text stores pointer, not copy.
            // Alloc/free per iteration was use-after-free (popup rendered freed memory).
            furi_string_printf(
                app->temp_text_buffer,
                "Scanning...\n\n%lu / %lu files\n%zu matches",
                uid_ctx->files_checked,
                uid_ctx->files_total > 0 ? uid_ctx->files_total : uid_ctx->files_checked,
                uid_ctx->results->count);

            popup_reset(app->popup);
            popup_set_header(app->popup, "UID Check", 64, 4, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->temp_text_buffer),
                64,
                18,
                AlignCenter,
                AlignTop);

            last_update = now;
            last_count = uid_ctx->files_checked;
        }

        furi_delay_ms(50); // Ridotto da 100 a 50ms per UI più responsive
    }

    furi_thread_join(uid_ctx->thread);
    furi_thread_free(uid_ctx->thread);

    // LOG: Scan completato
    if(app->logger) {
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "UID Check complete: %lu files scanned, %zu matches found",
            uid_ctx->files_checked,
            uid_ctx->results->count);
    }

    // STEP 5: Generate report
    FuriString* report = generate_report(uid_ctx);
    if(!report) {
        // LOG: Report fallito
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Failed to generate report");
        }
        dynamic_array_free(uid_ctx->results);
        free(uid_ctx);
        app->uid_check_context = NULL; // IMPORTANTE: resetta
        popup_set_text(app->popup, "Error", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(1500);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return;
    }

    furi_string_set(app->temp_text_buffer, report);
    furi_string_free(report);

    dynamic_array_free(uid_ctx->results);
    free(uid_ctx);
    app->uid_check_context = NULL; // IMPORTANTE: resetta dopo free

    popup_reset(app->popup);

    text_box_reset(app->text_box_report);
    text_box_set_text(app->text_box_report, furi_string_get_cstr(app->temp_text_buffer));
    text_box_set_font(app->text_box_report, TextBoxFontText);
    text_box_set_focus(app->text_box_report, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdUidReport);

    FURI_LOG_I(TAG, "=== UID CHECK DONE ===");
}

bool miband_nfc_scene_uid_check_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // Abort scan if running - usa il puntatore salvato nell'app
        if(app->uid_check_context) {
            UidCheckContext* uid_ctx = (UidCheckContext*)app->uid_check_context;
            if(!uid_ctx->scan_complete) {
                uid_ctx->scan_aborted = true;
                FURI_LOG_W(TAG, "Scan aborted by user");
            }
        }

        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        return true;
    }
    return false;
}

void miband_nfc_scene_uid_check_on_exit(void* context) {
    MiBandNfcApp* app = context;

    // Cleanup usando il puntatore salvato nell'app
    if(app->uid_check_context) {
        UidCheckContext* uid_ctx = (UidCheckContext*)app->uid_check_context;

        if(!uid_ctx->scan_complete) {
            uid_ctx->scan_aborted = true;

            // Aspetta che il thread termini
            if(uid_ctx->thread) {
                furi_thread_join(uid_ctx->thread);
                furi_thread_free(uid_ctx->thread);
                uid_ctx->thread = NULL;
            }
        }

        // Libera memoria
        if(uid_ctx->results) {
            dynamic_array_free(uid_ctx->results);
            uid_ctx->results = NULL;
        }

        free(uid_ctx);
        app->uid_check_context = NULL; // IMPORTANTE: resetta
    }

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    text_box_reset(app->text_box_report);
}
