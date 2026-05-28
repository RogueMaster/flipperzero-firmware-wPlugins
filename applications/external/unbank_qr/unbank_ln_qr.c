/*
 * Unbank Lightning Address QR
 * Multi-wallet Lightning Address QR generator for the Flipper Zero.
 *
 * Top-level menu:
 *   - Choose Wallet  (Speed / Strike / Wallet of Satoshi → username editor + QR)
 *   - Download Unbank (QR code for unbank.com/open-app)
 *   - About Unbank   (Brief description of the app)
 *
 * Build:  ufbt
 * Deploy: ufbt launch  (or copy the built .fap into SD:/apps/Tools/)
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <input/input.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "qrcodegen.h"

/* ---- Config ---- */
#define USER_LEN          96
#define ADDR_LEN          128
#define DEFAULT_USERNAME  "unbank"
#define STORAGE_DIR       EXT_PATH("apps_data/unbank_ln_qr")
#define NFC_OUTPUT_DIR    EXT_PATH("nfc/unbank")
#define UNBANK_APP_URL    "unbank.com/open-app"

#define ABOUT_TEXT \
    "Unbank is a non-custodial exchange that allows users to " \
    "buy Bitcoin with cash or sell with ZERO fees on-chain or " \
    "via the lightning network. Learn more at unbank.com."

typedef enum {
    WalletSpeed = 0,
    WalletStrike,
    WalletWoS,
    WalletCount,
} Wallet;

typedef struct {
    const char* name;     /* display name */
    const char* suffix;   /* domain incl. leading @ */
    const char* filename; /* storage filename */
} WalletInfo;

static const WalletInfo WALLETS[WalletCount] = {
    [WalletSpeed]  = {"Speed",             "@speed.app",            "speed.txt"},
    [WalletStrike] = {"Strike",            "@strike.me",            "strike.txt"},
    [WalletWoS]    = {"Wallet of Satoshi", "@walletofsatoshi.com",  "wos.txt"},
};

typedef enum {
    ViewIdSplash = 0,
    ViewIdMainMenu,
    ViewIdWalletPicker,
    ViewIdWalletMenu,
    ViewIdQr,
    ViewIdTextInput,
    ViewIdAbout,
    ViewIdNfcInstructions,
} ViewId;

#define SPLASH_DURATION_MS 1500
#define CustomEventSplashDone 1

typedef enum {
    MainMenuChooseWallet = 0,
    MainMenuDownloadApp,
    MainMenuAboutUnbank,
} MainMenuItem;

typedef enum {
    WalletMenuShowQr = 0,
    WalletMenuEditUsername,
    WalletMenuWriteNfc,
} WalletMenuItem;

typedef struct App App;

typedef struct {
    App* app;
} QrViewModel;

typedef struct {
    App* app;
} NfcViewModel;

struct App {
    Gui*             gui;
    NotificationApp* notifications;
    ViewDispatcher*  view_dispatcher;
    Submenu*         main_menu;
    Submenu*         wallet_picker;
    Submenu*         wallet_menu;
    TextInput*       text_input;
    Widget*          about_widget;
    View*            nfc_view;
    View*            qr_view;
    View*            splash_view;
    FuriTimer*       splash_timer;

    /* State */
    ViewId current_view;
    ViewId qr_back_target;  /* where Back from QR returns */
    Wallet active_wallet;

    char usernames[WalletCount][USER_LEN];
    char address[ADDR_LEN];       /* QR content + label */

    /* NFC instructions state */
    char nfc_saved_path[160];
    bool nfc_save_ok;
    char edit_buffer[USER_LEN];
    char header_buffer[64];

    uint8_t qr_buffer[qrcodegen_BUFFER_LEN_FOR_VERSION(qrcodegen_VERSION_MAX)];
    uint8_t qr_temp[qrcodegen_BUFFER_LEN_FOR_VERSION(qrcodegen_VERSION_MAX)];
    bool    qr_ready;
};

/* ---- Forwards ---- */
static void text_input_done_callback(void* context);
static void main_menu_callback(void* context, uint32_t index);
static void wallet_picker_callback(void* context, uint32_t index);
static void wallet_menu_callback(void* context, uint32_t index);
static void switch_to(App* app, ViewId id);
static void open_wallet_menu(App* app, Wallet w);
static void show_qr_text(App* app, const char* text, ViewId back_target);
static bool write_nfc_tag_file(App* app, Wallet w, char* out_path, size_t out_path_size);

/* ---- Util ---- */
static size_t bounded_strlen(const char* s, size_t maxlen) {
    size_t i = 0;
    while (i < maxlen && s[i] != '\0') i++;
    return i;
}

static void path_for_wallet(Wallet w, char* out, size_t out_size) {
    snprintf(out, out_size, "%s/%s", STORAGE_DIR, WALLETS[w].filename);
}

/* ---- Storage ---- */
static void save_username(App* app, Wallet w) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_DIR);

    char path[160];
    path_for_wallet(w, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    if (storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t len = bounded_strlen(app->usernames[w], USER_LEN);
        storage_file_write(file, app->usernames[w], len);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void load_username(App* app, Wallet w) {
    strncpy(app->usernames[w], DEFAULT_USERNAME, USER_LEN - 1);
    app->usernames[w][USER_LEN - 1] = '\0';

    Storage* storage = furi_record_open(RECORD_STORAGE);
    char path[160];
    path_for_wallet(w, path, sizeof(path));

    File* file = storage_file_alloc(storage);
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[USER_LEN];
        memset(buf, 0, USER_LEN);
        uint16_t bytes = storage_file_read(file, buf, USER_LEN - 1);
        if (bytes > 0) {
            buf[bytes] = '\0';
            int len = (int)bounded_strlen(buf, USER_LEN);
            while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' ||
                               buf[len - 1] == ' '  || buf[len - 1] == '\t')) {
                buf[--len] = '\0';
            }
            if (len > 0) {
                strncpy(app->usernames[w], buf, USER_LEN - 1);
                app->usernames[w][USER_LEN - 1] = '\0';
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void load_all_usernames(App* app) {
    for (int i = 0; i < WalletCount; i++) {
        load_username(app, (Wallet)i);
    }
}

/* ---- QR ---- */
static void compose_wallet_address(App* app) {
    Wallet w = app->active_wallet;
    snprintf(app->address, ADDR_LEN, "%s%s", app->usernames[w], WALLETS[w].suffix);
}

static void generate_qr_from_address(App* app) {
    app->qr_ready = qrcodegen_encodeText(
        app->address,
        app->qr_temp,
        app->qr_buffer,
        qrcodegen_Ecc_LOW,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true);
}

static void show_qr_text(App* app, const char* text, ViewId back_target) {
    strncpy(app->address, text, ADDR_LEN - 1);
    app->address[ADDR_LEN - 1] = '\0';
    app->qr_back_target = back_target;
    generate_qr_from_address(app);
    notification_message(app->notifications, &sequence_single_vibro);
    switch_to(app, ViewIdQr);
}

static void qr_view_draw(Canvas* canvas, void* _model) {
    QrViewModel* model = _model;
    App* app = model->app;

    canvas_clear(canvas);

    if (app->qr_ready) {
        int qr_size = qrcodegen_getSize(app->qr_buffer);
        /* Render every QR at the same on-screen size regardless of how many
         * modules it has, by distributing the target pixels across modules.
         * Some modules end up 3px wide, others 2px — fine for QR scanners.
         * 50px leaves a clean gap above the wallet label at the bottom. */
        const int target = 50;
        int offset_x = (128 - target) / 2;
        int offset_y = 1;

        for (int y = 0; y < qr_size; y++) {
            int py_start = (y * target) / qr_size;
            int py_end   = ((y + 1) * target) / qr_size;
            int py_h = py_end - py_start;
            for (int x = 0; x < qr_size; x++) {
                if (qrcodegen_getModule(app->qr_buffer, x, y)) {
                    int px_start = (x * target) / qr_size;
                    int px_end   = ((x + 1) * target) / qr_size;
                    int px_w = px_end - px_start;
                    canvas_draw_box(canvas,
                                    offset_x + px_start,
                                    offset_y + py_start,
                                    px_w, py_h);
                }
            }
        }
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "QR error");
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, app->address);
}

static bool qr_view_input(InputEvent* event, void* context) {
    App* app = context;
    if (event->type == InputTypeShort && event->key == InputKeyBack) {
        switch_to(app, app->qr_back_target);
        return true;
    }
    return false;
}

/* ---- Splash screen ---- */
/* Draw the Unbank bank silhouette ~26px wide, anchored at (cx, top_y) */
static void draw_bank_silhouette(Canvas* canvas, int cx, int top_y) {
    /* Cornice (24w x 3h) — slightly narrower than architrave-aligned, shifted down 1px */
    canvas_draw_box(canvas, cx - 12, top_y + 1, 24, 3);
    /* Architrave (22w x 3h) — slightly inset; 2px gap above */
    canvas_draw_box(canvas, cx - 11, top_y + 5, 22, 3);
    /* 4 pillars (2w x 10h) hanging from the architrave, evenly spaced */
    int pillar_y = top_y + 8;
    int pillar_h = 10;
    canvas_draw_box(canvas, cx - 10, pillar_y, 2, pillar_h);
    canvas_draw_box(canvas, cx - 4,  pillar_y, 2, pillar_h);
    canvas_draw_box(canvas, cx + 2,  pillar_y, 2, pillar_h);
    canvas_draw_box(canvas, cx + 8,  pillar_y, 2, pillar_h);
    /* Base bar (22w x 3h) — matches architrave width */
    canvas_draw_box(canvas, cx - 11, pillar_y + pillar_h, 22, 3);
    /* Downward arrow narrowing to a 4px tip; matches cornice width at the top */
    int tri_y = pillar_y + pillar_h + 4;
    canvas_draw_box(canvas, cx - 12, tri_y,     24, 1);
    canvas_draw_box(canvas, cx - 10, tri_y + 1, 20, 1);
    canvas_draw_box(canvas, cx - 8,  tri_y + 2, 16, 1);
    canvas_draw_box(canvas, cx - 6,  tri_y + 3, 12, 1);
    canvas_draw_box(canvas, cx - 4,  tri_y + 4,  8, 1);
    canvas_draw_box(canvas, cx - 2,  tri_y + 5,  4, 1);
}

static void splash_view_draw(Canvas* canvas, void* _model) {
    UNUSED(_model);
    canvas_clear(canvas);
    /* Bank silhouette centered horizontally at top */
    draw_bank_silhouette(canvas, 64, 3);
    /* App name and subtitle stacked underneath */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignBottom, "Unbank");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "BTCLN QR Generator");
}

static bool splash_view_input(InputEvent* event, void* context) {
    App* app = context;
    /* Any key press skips the splash */
    if (event->type == InputTypeShort || event->type == InputTypePress) {
        furi_timer_stop(app->splash_timer);
        view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventSplashDone);
        return true;
    }
    return false;
}

static void splash_timer_callback(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomEventSplashDone);
}

static bool custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    if (event == CustomEventSplashDone) {
        switch_to(app, ViewIdMainMenu);
        return true;
    }
    return false;
}

/* ---- Main menu ---- */
static void rebuild_main_menu(App* app) {
    submenu_reset(app->main_menu);
    submenu_set_header(app->main_menu, "BTCLN QR Generator");
    submenu_add_item(app->main_menu, "Choose Wallet",   MainMenuChooseWallet,
                     main_menu_callback, app);
    submenu_add_item(app->main_menu, "Download Unbank", MainMenuDownloadApp,
                     main_menu_callback, app);
    submenu_add_item(app->main_menu, "About Unbank",    MainMenuAboutUnbank,
                     main_menu_callback, app);
}

static void main_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch (index) {
        case MainMenuChooseWallet:
            switch_to(app, ViewIdWalletPicker);
            break;
        case MainMenuDownloadApp:
            show_qr_text(app, UNBANK_APP_URL, ViewIdMainMenu);
            break;
        case MainMenuAboutUnbank:
            switch_to(app, ViewIdAbout);
            break;
        default:
            break;
    }
}

/* ---- Wallet picker ---- */
static void rebuild_wallet_picker(App* app) {
    submenu_reset(app->wallet_picker);
    submenu_set_header(app->wallet_picker, "Choose Wallet");
    for (int i = 0; i < WalletCount; i++) {
        submenu_add_item(app->wallet_picker, WALLETS[i].name, (uint32_t)i,
                         wallet_picker_callback, app);
    }
}

static void wallet_picker_callback(void* context, uint32_t index) {
    App* app = context;
    if (index >= (uint32_t)WalletCount) return;
    open_wallet_menu(app, (Wallet)index);
}

/* ---- Per-wallet menu ---- */
static void rebuild_wallet_menu(App* app) {
    submenu_reset(app->wallet_menu);
    submenu_set_header(app->wallet_menu, WALLETS[app->active_wallet].name);
    submenu_add_item(app->wallet_menu, "Show QR",        WalletMenuShowQr,
                     wallet_menu_callback, app);
    submenu_add_item(app->wallet_menu, "Edit Username",  WalletMenuEditUsername,
                     wallet_menu_callback, app);
    submenu_add_item(app->wallet_menu, "Write NFC Tag",  WalletMenuWriteNfc,
                     wallet_menu_callback, app);
}

static void open_wallet_menu(App* app, Wallet w) {
    app->active_wallet = w;
    rebuild_wallet_menu(app);
    switch_to(app, ViewIdWalletMenu);
}

static void wallet_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch (index) {
        case WalletMenuShowQr:
            compose_wallet_address(app);
            app->qr_back_target = ViewIdWalletMenu;
            generate_qr_from_address(app);
            notification_message(app->notifications, &sequence_single_vibro);
            switch_to(app, ViewIdQr);
            break;

        case WalletMenuEditUsername: {
            strncpy(app->edit_buffer, app->usernames[app->active_wallet], USER_LEN - 1);
            app->edit_buffer[USER_LEN - 1] = '\0';

            snprintf(app->header_buffer, sizeof(app->header_buffer),
                     "Username (+%s)", WALLETS[app->active_wallet].suffix);

            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, app->header_buffer);
            text_input_set_result_callback(
                app->text_input,
                text_input_done_callback,
                app,
                app->edit_buffer,
                USER_LEN,
                false);
            switch_to(app, ViewIdTextInput);
            break;
        }

        case WalletMenuWriteNfc: {
            app->nfc_save_ok = write_nfc_tag_file(
                app, app->active_wallet,
                app->nfc_saved_path, sizeof(app->nfc_saved_path));
            switch_to(app, ViewIdNfcInstructions);
            break;
        }

        default:
            break;
    }
}

/* ---- Text input ---- */
static void text_input_done_callback(void* context) {
    App* app = context;
    Wallet w = app->active_wallet;

    size_t user_len = bounded_strlen(app->edit_buffer, USER_LEN);
    if (user_len == 0) {
        strncpy(app->usernames[w], DEFAULT_USERNAME, USER_LEN - 1);
        app->usernames[w][USER_LEN - 1] = '\0';
    } else {
        strncpy(app->usernames[w], app->edit_buffer, USER_LEN - 1);
        app->usernames[w][USER_LEN - 1] = '\0';
    }
    save_username(app, w);
    switch_to(app, ViewIdWalletMenu);
}

/* ---- About view ---- */
static void build_about_widget(App* app) {
    widget_reset(app->about_widget);
    /* text_scroll wraps on word boundaries and scrolls with Up/Down */
    widget_add_text_scroll_element(
        app->about_widget,
        0, 0, 128, 64,
        "\e#About Unbank\n"
        ABOUT_TEXT);
}

/* ---- NFC tag file writer ----
 * Writes a Flipper .nfc file for an NTAG213 with an NDEF URL record
 * containing "lightning:<username><suffix>". The file is saved into
 * /ext/nfc/unbank/<wallet>.nfc so the Flipper NFC app can find it.
 * Returns true if the file was written successfully.
 */
static bool write_nfc_tag_file(App* app, Wallet w, char* out_path, size_t out_path_size) {
    /* Compose the URL */
    char url[100];
    snprintf(url, sizeof(url), "lightning:%s%s", app->usernames[w], WALLETS[w].suffix);
    size_t url_len = bounded_strlen(url, sizeof(url));

    /* Limit URL length to fit comfortably in NTAG213 user memory (144B) */
    if (url_len > 130) url_len = 130;

    /* Pick filename: lowercase wallet name with underscores */
    char fname[64];
    int fi = 0;
    const char* nm = WALLETS[w].name;
    for (int i = 0; nm[i] && fi < (int)sizeof(fname) - 6; i++) {
        char c = nm[i];
        if (c >= 'A' && c <= 'Z') c = c + 32;
        if (c == ' ') c = '_';
        fname[fi++] = c;
    }
    fname[fi] = '\0';

    snprintf(out_path, out_path_size, "%s/%s.nfc", NFC_OUTPUT_DIR, fname);

    /* Open storage and ensure dir exists */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("nfc"));
    storage_simply_mkdir(storage, NFC_OUTPUT_DIR);

    File* file = storage_file_alloc(storage);
    bool success = false;

    if (storage_file_open(file, out_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        /* Build the 45 pages (180 bytes) of NTAG213 memory */
        uint8_t mem[45 * 4];
        memset(mem, 0, sizeof(mem));

        /* Page 0-1: placeholder UID (the actual tag's UID is used on write) */
        mem[0]  = 0x04; mem[1]  = 0x11; mem[2]  = 0x22; mem[3]  = 0x9D; /* UID0-2 + BCC0 */
        mem[4]  = 0x33; mem[5]  = 0x44; mem[6]  = 0x55; mem[7]  = 0x66; /* UID3-6 */
        /* Page 2: BCC1 + internal + lock bytes */
        mem[8]  = 0xDD; mem[9]  = 0x48; mem[10] = 0x00; mem[11] = 0x00;
        /* Page 3: Capability Container for NTAG213 with NDEF */
        mem[12] = 0xE1; mem[13] = 0x10; mem[14] = 0x12; mem[15] = 0x00;

        /* Page 4+: NDEF data */
        int p = 16; /* start of page 4 in mem[] */
        mem[p++] = 0x03;                         /* NDEF TLV type */
        mem[p++] = (uint8_t)(url_len + 5);       /* TLV length */
        mem[p++] = 0xD1;                         /* NDEF: MB+ME+SR, TNF=well-known */
        mem[p++] = 0x01;                         /* Type length */
        mem[p++] = (uint8_t)(url_len + 1);       /* Payload length (prefix + URL) */
        mem[p++] = 0x55;                         /* Type 'U' (URI) */
        mem[p++] = 0x00;                         /* URI prefix code: none */
        for (size_t i = 0; i < url_len && p < (int)sizeof(mem) - 1; i++) {
            mem[p++] = (uint8_t)url[i];
        }
        if (p < (int)sizeof(mem)) {
            mem[p++] = 0xFE;                     /* TLV terminator */
        }

        /* Write Flipper .nfc file header.
         * Matches the current firmware format:
         *   - "Device type: NTAG/Ultralight" (family), with a separate
         *     "NTAG/Ultralight type: NTAG213" line for the subtype.
         *   - "Data format version: 2" is required.
         *   - Counter values are decimal uint32 (just "0"), Tearing is 1 hex byte.
         */
        const char* header =
            "Filetype: Flipper NFC device\n"
            "Version: 4\n"
            "Device type: NTAG/Ultralight\n"
            "UID: 04 11 22 33 44 55 66\n"
            "ATQA: 00 44\n"
            "SAK: 00\n"
            "Data format version: 2\n"
            "NTAG/Ultralight type: NTAG213\n"
            "Signature: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
            " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
            "Mifare version: 00 04 04 02 01 00 0F 03\n"
            "Counter 0: 0\n"
            "Tearing 0: 00\n"
            "Counter 1: 0\n"
            "Tearing 1: 00\n"
            "Counter 2: 0\n"
            "Tearing 2: 00\n"
            "Pages total: 45\n"
            "Pages read: 45\n";
        storage_file_write(file, header, strlen(header));

        /* Write each page */
        char line[40];
        bool write_ok = true;
        for (int page = 0; page < 45; page++) {
            int n = snprintf(line, sizeof(line), "Page %d: %02X %02X %02X %02X\n",
                             page,
                             mem[page * 4 + 0],
                             mem[page * 4 + 1],
                             mem[page * 4 + 2],
                             mem[page * 4 + 3]);
            if (storage_file_write(file, line, n) != (uint16_t)n) {
                write_ok = false;
                break;
            }
        }
        /* Trailing field required by current Flipper firmware */
        const char* footer = "Failed authentication attempts: 0\n";
        if (write_ok) {
            size_t flen = strlen(footer);
            if (storage_file_write(file, footer, flen) != (uint16_t)flen) {
                write_ok = false;
            }
        }
        success = write_ok;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

/* ---- NFC instructions view ----
 * Custom View so we can fully control input. Only Back dismisses;
 * all other keys are swallowed so stray events (e.g. an OK release
 * arriving after a menu selection) can't auto-dismiss the screen.
 */
static void nfc_view_draw(Canvas* canvas, void* _model) {
    NfcViewModel* model = _model;
    App* app = model->app;
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop,
                            app->nfc_save_ok ? "NFC File Saved" : "NFC Save Failed");

    canvas_set_font(canvas, FontSecondary);
    if (app->nfc_save_ok) {
        canvas_draw_str(canvas, 2, 18, "1. Open NFC app");
        canvas_draw_str(canvas, 2, 27, "2. Saved -> unbank");
        canvas_draw_str(canvas, 2, 36, "3. Pick file & Write");
        canvas_draw_str(canvas, 2, 45, "4. Hold blank NTAG");
    } else {
        canvas_draw_str(canvas, 2, 22, "Could not save file.");
        canvas_draw_str(canvas, 2, 32, "Check SD card and");
        canvas_draw_str(canvas, 2, 42, "try again.");
    }

    /* Footer hint */
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom,
                            "Press Back to return");
}

static bool nfc_view_input(InputEvent* event, void* context) {
    App* app = context;
    if (event->type == InputTypeShort && event->key == InputKeyBack) {
        switch_to(app, ViewIdWalletMenu);
        return true;
    }
    /* Swallow everything else so the screen can't be dismissed by stray events */
    return true;
}

/* ---- View switching ---- */
static void switch_to(App* app, ViewId id) {
    app->current_view = id;
    view_dispatcher_switch_to_view(app->view_dispatcher, (uint32_t)id);
}

/* ---- Back navigation ---- */
static bool back_event_callback(void* context) {
    App* app = context;
    switch (app->current_view) {
        case ViewIdSplash:
            /* Skip splash, go straight to main menu */
            furi_timer_stop(app->splash_timer);
            switch_to(app, ViewIdMainMenu);
            return true;
        case ViewIdMainMenu:
            return false; /* exit app */
        case ViewIdWalletPicker:
            switch_to(app, ViewIdMainMenu);
            return true;
        case ViewIdWalletMenu:
            switch_to(app, ViewIdWalletPicker);
            return true;
        case ViewIdQr:
            switch_to(app, app->qr_back_target);
            return true;
        case ViewIdTextInput:
            switch_to(app, ViewIdWalletMenu);
            return true;
        case ViewIdAbout:
            switch_to(app, ViewIdMainMenu);
            return true;
        case ViewIdNfcInstructions:
            switch_to(app, ViewIdWalletMenu);
            return true;
        default:
            return false;
    }
}

/* ---- Entry point ---- */
int32_t unbank_ln_qr_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    load_all_usernames(app);
    app->active_wallet  = WalletSpeed;
    app->current_view   = ViewIdSplash;
    app->qr_back_target = ViewIdMainMenu;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, back_event_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    /* Splash view */
    app->splash_view = view_alloc();
    view_set_context(app->splash_view, app);
    view_set_draw_callback(app->splash_view, splash_view_draw);
    view_set_input_callback(app->splash_view, splash_view_input);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdSplash, app->splash_view);

    /* Splash auto-dismiss timer */
    app->splash_timer = furi_timer_alloc(splash_timer_callback, FuriTimerTypeOnce, app);

    /* Main menu */
    app->main_menu = submenu_alloc();
    rebuild_main_menu(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMainMenu, submenu_get_view(app->main_menu));

    /* Wallet picker */
    app->wallet_picker = submenu_alloc();
    rebuild_wallet_picker(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdWalletPicker, submenu_get_view(app->wallet_picker));

    /* Wallet menu (rebuilt on entry) */
    app->wallet_menu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdWalletMenu, submenu_get_view(app->wallet_menu));

    /* Text input */
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    /* About widget */
    app->about_widget = widget_alloc();
    build_about_widget(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdAbout, widget_get_view(app->about_widget));

    /* NFC instructions view (custom — only Back dismisses) */
    app->nfc_view = view_alloc();
    view_allocate_model(app->nfc_view, ViewModelTypeLocking, sizeof(NfcViewModel));
    with_view_model(app->nfc_view, NfcViewModel* m, { m->app = app; }, false);
    view_set_context(app->nfc_view, app);
    view_set_draw_callback(app->nfc_view, nfc_view_draw);
    view_set_input_callback(app->nfc_view, nfc_view_input);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdNfcInstructions, app->nfc_view);

    /* QR view (custom) */
    app->qr_view = view_alloc();
    view_allocate_model(app->qr_view, ViewModelTypeLocking, sizeof(QrViewModel));
    with_view_model(app->qr_view, QrViewModel* m, { m->app = app; }, false);
    view_set_context(app->qr_view, app);
    view_set_draw_callback(app->qr_view, qr_view_draw);
    view_set_input_callback(app->qr_view, qr_view_input);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdQr, app->qr_view);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    /* Start with the splash screen and arm the auto-dismiss timer */
    switch_to(app, ViewIdSplash);
    furi_timer_start(app->splash_timer, furi_ms_to_ticks(SPLASH_DURATION_MS));

    view_dispatcher_run(app->view_dispatcher);

    /* Cleanup */
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSplash);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdWalletPicker);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdWalletMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdQr);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdAbout);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdNfcInstructions);

    submenu_free(app->main_menu);
    submenu_free(app->wallet_picker);
    submenu_free(app->wallet_menu);
    text_input_free(app->text_input);
    widget_free(app->about_widget);
    view_free(app->nfc_view);
    view_free(app->qr_view);
    view_free(app->splash_view);
    furi_timer_stop(app->splash_timer);
    furi_timer_free(app->splash_timer);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
    return 0;
}
