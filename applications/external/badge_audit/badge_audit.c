// Badge Audit — a DEFENSIVE access-control auditor for the Flipper Zero.
//
// It does NOT clone or attack anything. It identifies an access badge, reads its
// UID, and for MIFARE Classic it tests whether sectors still use well-known
// FACTORY/DEFAULT keys (the #1 real-world access-control weakness). It then rates
// the badge and can save a posture scorecard to the SD card.
//
// Use only on credentials you own or are explicitly authorized to assess.

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SCORECARD_PATH APP_DATA_PATH("scorecard.txt")

// Most common MIFARE Classic factory/default keys. If a sector authenticates
// with any of these, the badge is effectively unprotected.
static const uint8_t DEFAULT_KEYS[][MF_CLASSIC_KEY_SIZE] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
};
#define DEFAULT_KEY_COUNT (sizeof(DEFAULT_KEYS) / sizeof(DEFAULT_KEYS[0]))

// ---- Static posture knowledge base (for non-Classic cards) ----------------

typedef struct {
    const char* name;
    const char* verdict;
    uint8_t score;
    const char* screen_note;
    const char* full_note;
} Posture;

static Posture posture_for(NfcProtocol p) {
    switch(p) {
    case NfcProtocolMfClassic:
        return (Posture){
            "MIFARE Classic", "WEAK", 25, "Crypto1 cloneable",
            "Crypto1 cipher is broken; keys are recoverable and the card is cloneable."};
    case NfcProtocolMfUltralight:
        return (Posture){
            "MIFARE Ultralight", "WEAK", 35, "Weak/no auth",
            "Typically little or no authentication; easily cloned. Not for access control."};
    case NfcProtocolIso14443_3a:
        return (Posture){
            "ISO14443-3A (UID)", "WEAK", 30, "UID-only = cloneable",
            "Only the low-level UID layer. If the reader trusts UID alone, trivially cloneable."};
    case NfcProtocolIso14443_3b:
        return (Posture){
            "ISO14443-3B (UID)", "WEAK", 35, "UID-only = cloneable",
            "Low-level UID layer. Cloneable if the reader trusts the UID."};
    case NfcProtocolIso14443_4a:
        return (Posture){
            "ISO14443-4A", "MEDIUM", 60, "APDU; depends on app",
            "Supports ISO14443-4 APDUs; security depends on the application layer."};
    case NfcProtocolIso14443_4b:
        return (Posture){
            "ISO14443-4B", "MEDIUM", 60, "APDU; depends on app",
            "Supports ISO14443-4 APDUs; security depends on the application layer."};
    case NfcProtocolIso15693_3:
        return (Posture){
            "ISO15693", "WEAK", 40, "Often UID-based",
            "Vicinity card; many access deployments authorize on UID, which is cloneable."};
    case NfcProtocolSlix:
        return (Posture){
            "NXP SLIX", "WEAK", 40, "ISO15693, often UID",
            "SLIX (ISO15693 family); frequently used in cloneable UID-based access."};
    case NfcProtocolFelica:
        return (Posture){
            "FeliCa", "MEDIUM", 65, "Has security features",
            "FeliCa has built-in security; posture depends on deployment and key management."};
    case NfcProtocolMfPlus:
        return (Posture){
            "MIFARE Plus", "STRONG", 85, "AES (SL3) = strong",
            "Supports AES; in Security Level 3 it is strong if keys are well managed."};
    case NfcProtocolMfDesfire:
        return (Posture){
            "MIFARE DESFire", "STRONG", 90, "AES/3DES = strong",
            "DESFire uses AES/3DES mutual auth; strong when keys are properly managed."};
    case NfcProtocolSt25tb:
        return (Posture){
            "ST25TB", "WEAK", 35, "Memory tag, cloneable",
            "Memory tag; commonly cloneable and weak for access control."};
    default:
        return (Posture){
            "Unknown card", "UNKNOWN", 50, "Unrecognized", "Unrecognized protocol."};
    }
}

static const char* classic_type_name(MfClassicType t) {
    switch(t) {
    case MfClassicTypeMini:
        return "MIFARE Classic Mini";
    case MfClassicType1k:
        return "MIFARE Classic 1K";
    case MfClassicType4k:
        return "MIFARE Classic 4K";
    default:
        return "MIFARE Classic";
    }
}

// The most-derived ("leaf") protocol is the card's true type.
static NfcProtocol choose_leaf(const NfcProtocol* protocols, size_t num) {
    for(size_t i = 0; i < num; i++) {
        bool is_ancestor = false;
        for(size_t j = 0; j < num; j++) {
            if(i == j) continue;
            if(nfc_protocol_has_parent(protocols[j], protocols[i])) {
                is_ancestor = true;
                break;
            }
        }
        if(!is_ancestor) return protocols[i];
    }
    return protocols[0];
}

// ---- Application state ----------------------------------------------------

typedef enum {
    StateScanning,
    StateAnalyzing,
    StateResult,
} AppState;

typedef struct {
    NfcProtocol leaf;
    bool has_uid;
    uint8_t uid[ISO14443_3A_MAX_UID_SIZE];
    uint8_t uid_len;
    uint8_t sak;
    uint8_t atqa[2];
    bool is_classic;
    MfClassicType classic_type;
    uint8_t total_sectors;
    uint8_t default_key_sectors;
} AnalysisResult;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    NotificationApp* notifications;
    FuriMutex* mutex;

    Nfc* nfc;
    NfcScanner* scanner;
    bool scanner_running;

    AppState state;
    NfcProtocol pending_leaf;
    AnalysisResult result;
    bool has_result;
    bool saved;
    bool save_error;

    uint8_t prog_done; // sectors tested so far (during analysis)
    uint8_t prog_total;
} BadgeAudit;

typedef enum {
    AppEventInput,
    AppEventDetected,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

// ---- Verdict computation --------------------------------------------------

static void result_verdict(
    const AnalysisResult* r,
    const char** verdict,
    uint8_t* score,
    char* note,
    size_t note_sz) {
    if(r->is_classic) {
        if(r->default_key_sectors > 0) {
            *verdict = "CRITICAL";
            *score = (r->default_key_sectors >= r->total_sectors) ? 5 : 12;
            snprintf(note, note_sz, "DEFAULT keys: %u/%u", r->default_key_sectors, r->total_sectors);
        } else {
            *verdict = "WEAK";
            *score = 25;
            snprintf(note, note_sz, "Crypto1, no default keys");
        }
    } else {
        Posture p = posture_for(r->leaf);
        *verdict = p.verdict;
        *score = p.score;
        snprintf(note, note_sz, "%s", p.screen_note);
    }
}

static void uid_to_str(const uint8_t* uid, uint8_t len, char* out, size_t out_sz) {
    size_t pos = 0;
    out[0] = '\0';
    for(uint8_t i = 0; i < len; i++) {
        int n = snprintf(out + pos, out_sz - pos, "%02X ", uid[i]);
        if(n <= 0 || (size_t)n >= out_sz - pos) break;
        pos += n;
    }
}

// ---- Rendering ------------------------------------------------------------

static void draw_callback(Canvas* canvas, void* ctx) {
    BadgeAudit* app = ctx;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    AppState state = app->state;
    AnalysisResult r = app->result;
    bool saved = app->saved;
    bool save_error = app->save_error;
    uint8_t done = app->prog_done;
    uint8_t total = app->prog_total;
    furi_mutex_release(app->mutex);

    canvas_clear(canvas);

    if(state == StateScanning) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Badge Audit");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "Hold an NFC badge to");
        canvas_draw_str(canvas, 2, 42, "the BACK of Flipper.");
        canvas_draw_str(canvas, 2, 62, "Back = exit");
        return;
    }

    if(state == StateAnalyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Analyzing...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "Reading card & testing");
        canvas_draw_str(canvas, 2, 42, "factory keys.");
        if(total > 0) {
            char line[32];
            snprintf(line, sizeof(line), "Sector %u/%u", done, total);
            canvas_draw_str(canvas, 2, 56, line);
        }
        canvas_draw_str(canvas, 2, 64, "Hold card still");
        return;
    }

    // StateResult
    const char* name = r.is_classic ? classic_type_name(r.classic_type) : posture_for(r.leaf).name;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, name);

    canvas_set_font(canvas, FontSecondary);

    char line[40];
    if(r.has_uid) {
        char uid[32];
        uid_to_str(r.uid, r.uid_len, uid, sizeof(uid));
        snprintf(line, sizeof(line), "UID: %s", uid);
    } else {
        snprintf(line, sizeof(line), "UID: (not read)");
    }
    canvas_draw_str(canvas, 2, 22, line);

    const char* verdict;
    uint8_t score;
    char note[40];
    result_verdict(&r, &verdict, &score, note, sizeof(note));

    snprintf(line, sizeof(line), "Risk: %s  %u/100", verdict, (unsigned)score);
    canvas_draw_str(canvas, 2, 33, line);

    canvas_draw_frame(canvas, 2, 36, 124, 7);
    uint8_t w = (uint8_t)((uint16_t)score * 122 / 100);
    if(w > 0) canvas_draw_box(canvas, 3, 37, w, 5);

    canvas_draw_str(canvas, 2, 52, note);

    if(saved) {
        canvas_draw_str(canvas, 2, 63, "Saved to SD");
    } else if(save_error) {
        canvas_draw_str(canvas, 2, 63, "Save failed (no SD?)");
    } else {
        canvas_draw_str(canvas, 2, 63, "OK save  <>rescan  Back");
    }
}

// ---- NFC scanner callback (NFC service thread) ----------------------------

static void scanner_callback(NfcScannerEvent event, void* context) {
    BadgeAudit* app = context;
    if(event.type != NfcScannerEventTypeDetected) return;
    if(event.data.protocol_num == 0) return;

    NfcProtocol leaf = choose_leaf(event.data.protocols, event.data.protocol_num);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool accept = (app->state == StateScanning);
    if(accept) app->pending_leaf = leaf;
    furi_mutex_release(app->mutex);

    if(accept) {
        AppEvent ev = {.type = AppEventDetected};
        furi_message_queue_put(app->queue, &ev, 0);
    }
}

// ---- Deep analysis (main thread, blocking) --------------------------------

static void run_deep_analysis(BadgeAudit* app, NfcProtocol leaf) {
    AnalysisResult res;
    memset(&res, 0, sizeof(res));
    res.leaf = leaf;

    // 1) Read the ISO14443-3A base layer (UID / SAK / ATQA) for A-family cards.
    if(leaf == NfcProtocolIso14443_3a || nfc_protocol_has_parent(leaf, NfcProtocolIso14443_3a)) {
        Iso14443_3aData* d = iso14443_3a_alloc();
        if(iso14443_3a_poller_sync_read(app->nfc, d) == Iso14443_3aErrorNone) {
            res.has_uid = true;
            res.uid_len = d->uid_len > ISO14443_3A_MAX_UID_SIZE ? ISO14443_3A_MAX_UID_SIZE :
                                                                  d->uid_len;
            memcpy(res.uid, d->uid, res.uid_len);
            res.sak = d->sak;
            res.atqa[0] = d->atqa[0];
            res.atqa[1] = d->atqa[1];
        }
        iso14443_3a_free(d);
    }

    // 2) For MIFARE Classic, test every sector against the default-key list.
    if(leaf == NfcProtocolMfClassic) {
        MfClassicType type;
        if(mf_classic_poller_sync_detect_type(app->nfc, &type) == MfClassicErrorNone) {
            res.is_classic = true;
            res.classic_type = type;
            uint8_t sectors = mf_classic_get_total_sectors_num(type);
            res.total_sectors = sectors;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->prog_total = sectors;
            app->prog_done = 0;
            furi_mutex_release(app->mutex);

            uint8_t cracked = 0;
            for(uint8_t s = 0; s < sectors; s++) {
                uint8_t block = mf_classic_get_sector_trailer_num_by_sector(s);
                bool sector_default = false;
                for(size_t k = 0; k < DEFAULT_KEY_COUNT && !sector_default; k++) {
                    MfClassicKey key;
                    memcpy(key.data, DEFAULT_KEYS[k], MF_CLASSIC_KEY_SIZE);
                    MfClassicAuthContext auth_ctx;
                    if(mf_classic_poller_sync_auth(
                           app->nfc, block, &key, MfClassicKeyTypeA, &auth_ctx) ==
                       MfClassicErrorNone) {
                        sector_default = true;
                    } else if(
                        mf_classic_poller_sync_auth(
                            app->nfc, block, &key, MfClassicKeyTypeB, &auth_ctx) ==
                        MfClassicErrorNone) {
                        sector_default = true;
                    }
                }
                if(sector_default) cracked++;

                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->prog_done = s + 1;
                furi_mutex_release(app->mutex);
                view_port_update(app->view_port);
            }
            res.default_key_sectors = cracked;
        }
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->result = res;
    app->has_result = true;
    app->state = StateResult;
    app->saved = false;
    app->save_error = false;
    furi_mutex_release(app->mutex);

    notification_message(app->notifications, &sequence_success);
}

// ---- Save scorecard -------------------------------------------------------

static void save_scorecard(BadgeAudit* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool has = app->has_result;
    AnalysisResult r = app->result;
    furi_mutex_release(app->mutex);
    if(!has) return;

    const char* verdict;
    uint8_t score;
    char note[40];
    result_verdict(&r, &verdict, &score, note, sizeof(note));

    char uid[32];
    if(r.has_uid)
        uid_to_str(r.uid, r.uid_len, uid, sizeof(uid));
    else
        snprintf(uid, sizeof(uid), "(not read)");

    const char* name = r.is_classic ? classic_type_name(r.classic_type) : posture_for(r.leaf).name;

    char buf[400];
    int detail = 0;
    if(r.is_classic) {
        detail = snprintf(
            buf,
            sizeof(buf),
            "=== Badge Audit ===\n"
            "Card: %s\n"
            "UID: %s\n"
            "SAK: %02X  ATQA: %02X %02X\n"
            "Risk: %s (%u/100)\n"
            "Default-key sectors: %u/%u\n"
            "Note: Sectors authenticating with factory keys are unprotected; rekey or "
            "migrate to DESFire.\n"
            "-------------------\n",
            name,
            uid,
            r.sak,
            r.atqa[0],
            r.atqa[1],
            verdict,
            (unsigned)score,
            r.default_key_sectors,
            r.total_sectors);
    } else {
        Posture p = posture_for(r.leaf);
        detail = snprintf(
            buf,
            sizeof(buf),
            "=== Badge Audit ===\n"
            "Card: %s\n"
            "UID: %s\n"
            "Risk: %s (%u/100)\n"
            "Note: %s\n"
            "-------------------\n",
            name,
            uid,
            verdict,
            (unsigned)score,
            p.full_note);
    }
    if(detail < 0) detail = 0;
    size_t len = strlen(buf);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, SCORECARD_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        ok = (storage_file_write(file, buf, len) == len);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->saved = ok;
    app->save_error = !ok;
    furi_mutex_release(app->mutex);

    notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    view_port_update(app->view_port);
}

// ---- Rescan ---------------------------------------------------------------

static void restart_scan(BadgeAudit* app) {
    if(app->scanner_running) {
        nfc_scanner_stop(app->scanner);
        app->scanner_running = false;
    }
    nfc_scanner_free(app->scanner);
    app->scanner = nfc_scanner_alloc(app->nfc);

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = StateScanning;
    app->has_result = false;
    app->saved = false;
    app->save_error = false;
    app->prog_done = 0;
    app->prog_total = 0;
    furi_mutex_release(app->mutex);

    nfc_scanner_start(app->scanner, scanner_callback, app);
    app->scanner_running = true;
    view_port_update(app->view_port);
}

// ---- Entry point ----------------------------------------------------------

static void input_callback(InputEvent* input_event, void* ctx) {
    BadgeAudit* app = ctx;
    AppEvent ev = {.type = AppEventInput, .input = *input_event};
    furi_message_queue_put(app->queue, &ev, FuriWaitForever);
}

int32_t badge_audit_app(void* p) {
    UNUSED(p);

    BadgeAudit* app = malloc(sizeof(BadgeAudit));
    memset(app, 0, sizeof(BadgeAudit));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->state = StateScanning;
    app->pending_leaf = NfcProtocolInvalid;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->nfc = nfc_alloc();
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, scanner_callback, app);
    app->scanner_running = true;

    AppEvent ev;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;

        if(ev.type == AppEventInput) {
            if(ev.input.type != InputTypePress) continue;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            AppState st = app->state;
            furi_mutex_release(app->mutex);

            if(ev.input.key == InputKeyBack) {
                running = false;
            } else if(ev.input.key == InputKeyOk) {
                if(st == StateResult) save_scorecard(app);
            } else { // arrows
                if(st == StateResult) restart_scan(app);
            }
        } else if(ev.type == AppEventDetected) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            AppState st = app->state;
            NfcProtocol leaf = app->pending_leaf;
            furi_mutex_release(app->mutex);

            if(st == StateScanning) {
                if(app->scanner_running) {
                    nfc_scanner_stop(app->scanner);
                    app->scanner_running = false;
                }
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->state = StateAnalyzing;
                app->prog_done = 0;
                app->prog_total = 0;
                furi_mutex_release(app->mutex);
                view_port_update(app->view_port);

                run_deep_analysis(app, leaf);
                view_port_update(app->view_port);
            }
        }
    }

    if(app->scanner_running) nfc_scanner_stop(app->scanner);
    nfc_scanner_free(app->scanner);
    nfc_free(app->nfc);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
