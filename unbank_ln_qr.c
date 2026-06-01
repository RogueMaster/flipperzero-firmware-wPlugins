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
#define LAST_WALLET_FILE  EXT_PATH("apps_data/unbank_ln_qr/last_wallet.txt")
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
    Ntag213 = 0,
    Ntag215,
    Ntag216,
    NtagTypeCount,
} NtagType;

typedef struct {
    const char* name;        /* display + file-header type label */
    int         total_pages; /* total pages in user memory layout */
    uint8_t     cc_size;     /* CC[2] byte (NDEF area size) */
    uint8_t     mifare_ver;  /* byte 6 of Mifare version: storage size code */
} NtagTypeInfo;

/* Values from NXP NTAG datasheets:
 *  NTAG213: CC=0x12 (144B), Mifare ver byte 6 = 0x0F
 *  NTAG215: CC=0x3E (504B), Mifare ver byte 6 = 0x11
 *  NTAG216: CC=0x6D (888B), Mifare ver byte 6 = 0x13
 */
static const NtagTypeInfo NTAG_TYPES[NtagTypeCount] = {
    [Ntag213] = {"NTAG213", 45,  0x12, 0x0F},
    [Ntag215] = {"NTAG215", 135, 0x3E, 0x11},
    [Ntag216] = {"NTAG216", 231, 0x6D, 0x13},
};

typedef enum {
    ViewIdSplash = 0,
    ViewIdMainMenu,
    ViewIdWalletPicker,
    ViewIdWalletMenu,
    ViewIdQr,
    ViewIdTextInput,
    ViewIdAbout,
    ViewIdNfcTypePicker,
    ViewIdNfcInstructions,
} ViewId;

#define SPLASH_DURATION_MS 1500
#define CustomEventSplashDone 1

typedef enum {
    MainMenuChooseWallet = 0,
    MainMenuQuickQr,
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
    Submenu*         nfc_type_picker;
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
static void nfc_type_picker_callback(void* context, uint32_t index);
static void switch_to(App* app, ViewId id);
static void open_wallet_menu(App* app, Wallet w);
static void show_qr_text(App* app, const char* text, ViewId back_target);
static bool write_nfc_tag_file(App* app, Wallet w, NtagType type,
                               char* out_path, size_t out_path_size);

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

static void save_last_wallet(Wallet w) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_DIR);
    File* file = storage_file_alloc(storage);
    if (storage_file_open(file, LAST_WALLET_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[4];
        int n = snprintf(buf, sizeof(buf), "%d", (int)w);
        storage_file_write(file, buf, (uint16_t)n);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static Wallet load_last_wallet(void) {
    Wallet result = WalletSpeed;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if (storage_file_open(file, LAST_WALLET_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[8];
        memset(buf, 0, sizeof(buf));
        uint16_t bytes = storage_file_read(file, buf, sizeof(buf) - 1);
        if (bytes > 0) {
            buf[bytes] = '\0';
            int v = atoi(buf);
            if (v >= 0 && v < WalletCount) {
                result = (Wallet)v;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
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
    submenu_add_item(app->main_menu, "Quick QR",        MainMenuQuickQr,
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
        case MainMenuQuickQr:
            app->active_wallet = load_last_wallet();
            compose_wallet_address(app);
            app->qr_back_target = ViewIdMainMenu;
            generate_qr_from_address(app);
            notification_message(app->notifications, &sequence_single_vibro);
            switch_to(app, ViewIdQr);
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
    save_last_wallet(w);
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

        case WalletMenuWriteNfc:
            switch_to(app, ViewIdNfcTypePicker);
            break;

        default:
            break;
    }
}

/* ---- NFC tag type picker ---- */
static void rebuild_nfc_type_picker(App* app) {
    submenu_reset(app->nfc_type_picker);
    submenu_set_header(app->nfc_type_picker, "Select Tag Type");
    for (int i = 0; i < NtagTypeCount; i++) {
        submenu_add_item(app->nfc_type_picker, NTAG_TYPES[i].name,
                         (uint32_t)i, nfc_type_picker_callback, app);
    }
}

static void nfc_type_picker_callback(void* context, uint32_t index) {
    App* app = context;
    if (index >= (uint32_t)NtagTypeCount) return;
    app->nfc_save_ok = write_nfc_tag_file(
        app, app->active_wallet, (NtagType)index,
        app->nfc_saved_path, sizeof(app->nfc_saved_path));
    switch_to(app, ViewIdNfcInstructions);
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
 * Writes a Flipper .nfc file for an NTAG213/215/216 with an NDEF URL
 * record containing "lightning:<username><suffix>". The file is saved
 * into /ext/nfc/unbank/<wallet>_<ntagtype>.nfc.
 * Returns true if the file was written successfully.
 */
static bool write_nfc_tag_file(App* app, Wallet w, NtagType type,
                               char* out_path, size_t out_path_size) {
    const NtagTypeInfo* info = &NTAG_TYPES[type];
    const int total_pages = info->total_pages;
    const size_t mem_size = (size_t)(total_pages * 4);

    /* Compose the URL */
    char url[200];
    snprintf(url, sizeof(url), "lightning:%s%s", app->usernames[w], WALLETS[w].suffix);
    size_t url_len = bounded_strlen(url, sizeof(url));

    /* Cap URL to NDEF area size (CC[2] * 8) minus TLV overhead */
    size_t max_url = (size_t)(info->cc_size) * 8 - 7;
    if (url_len > max_url) url_len = max_url;

    /* Pick filename: lowercase wallet name + ntag type with underscores */
    char wname[32];
    int fi = 0;
    const char* nm = WALLETS[w].name;
    for (int i = 0; nm[i] && fi < (int)sizeof(wname) - 1; i++) {
        char c = nm[i];
        if (c >= 'A' && c <= 'Z') c = c + 32;
        if (c == ' ') c = '_';
        wname[fi++] = c;
    }
    wname[fi] = '\0';

    char tname[16];
    int ti = 0;
    for (int i = 0; info->name[i] && ti < (int)sizeof(tname) - 1; i++) {
        char c = info->name[i];
        if (c >= 'A' && c <= 'Z') c = c + 32;
        tname[ti++] = c;
    }
    tname[ti] = '\0';

    snprintf(out_path, out_path_size, "%s/%s_%s.nfc", NFC_OUTPUT_DIR, wname, tname);

    /* Open storage and ensure dir exists */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("nfc"));
    storage_simply_mkdir(storage, NFC_OUTPUT_DIR);

    File* file = storage_file_alloc(storage);
    bool success = false;

    uint8_t* mem = malloc(mem_size);
    if (mem == NULL) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    memset(mem, 0, mem_size);

    if (storage_file_open(file, out_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        /* Placeholder UID — the actual tag's UID is preserved on write.
         * BCC bytes are computed below from the UID. */
        const uint8_t uid[7] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        const uint8_t bcc0 = 0x88 ^ uid[0] ^ uid[1] ^ uid[2];
        const uint8_t bcc1 = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];

        /* Page 0: UID0..UID2 + BCC0 */
        mem[0]  = uid[0]; mem[1]  = uid[1]; mem[2]  = uid[2]; mem[3]  = bcc0;
        /* Page 1: UID3..UID6 */
        mem[4]  = uid[3]; mem[5]  = uid[4]; mem[6]  = uid[5]; mem[7]  = uid[6];
        /* Page 2: BCC1 + internal byte + static lock bytes (unlocked) */
        mem[8]  = bcc1;   mem[9]  = 0x48;   mem[10] = 0x00;   mem[11] = 0x00;
        /* Page 3: Capability Container for the selected NTAG type with NDEF */
        mem[12] = 0xE1;   mem[13] = 0x10;   mem[14] = info->cc_size; mem[15] = 0x00;

        /* Page 4+: NDEF data */
        int p = 16;
        mem[p++] = 0x03;
        mem[p++] = (uint8_t)(url_len + 5);
        mem[p++] = 0xD1;
        mem[p++] = 0x01;
        mem[p++] = (uint8_t)(url_len + 1);
        mem[p++] = 0x55;
        mem[p++] = 0x00;
        for (size_t i = 0; i < url_len && p < (int)mem_size - 1; i++) {
            mem[p++] = (uint8_t)url[i];
        }
        if (p < (int)mem_size) {
            mem[p++] = 0xFE;
        }

        /* Config pages at end of memory.
         * Layout (relative to lock_page):
         *   lock_page + 0 : Dynamic lock bytes (3 bytes) + RFUI (0xBD)
         *   lock_page + 1 : CFG_0 (MIRROR/AUTH0 — 0xFF = no protection)
         *   lock_page + 2 : CFG_1 (default access settings)
         *   lock_page + 3 : PWD  (default 0xFFFFFFFF)
         *   lock_page + 4 : PACK + RFUI
         */
        const int lock_page = total_pages - 5;
        if (lock_page >= 4) {
            int lp = lock_page * 4;
            mem[lp + 0] = 0x00; mem[lp + 1] = 0x00; mem[lp + 2] = 0x00; mem[lp + 3] = 0xBD;
            mem[lp + 4] = 0x04; mem[lp + 5] = 0x00; mem[lp + 6] = 0x00; mem[lp + 7] = 0xFF;
            mem[lp + 8] = 0x00; mem[lp + 9] = 0x05; mem[lp +10] = 0x00; mem[lp +11] = 0x00;
            mem[lp +12] = 0xFF; mem[lp +13] = 0xFF; mem[lp +14] = 0xFF; mem[lp +15] = 0xFF;
            mem[lp +16] = 0x00; mem[lp +17] = 0x00; mem[lp +18] = 0x00; mem[lp +19] = 0x00;
        }

        char header[512];
        int hlen = snprintf(header, sizeof(header),
            "Filetype: Flipper NFC device\n"
            "Version: 4\n"
            "Device type: NTAG/Ultralight\n"
            "UID: %02X %02X %02X %02X %02X %02X %02X\n"
            "ATQA: 00 44\n"
            "SAK: 00\n"
            "Data format version: 2\n"
            "NTAG/Ultralight type: %s\n"
            "Signature: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00"
            " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
            "Mifare version: 00 04 04 02 01 00 %02X 03\n"
            "Counter 0: 0\n"
            "Tearing 0: 00\n"
            "Counter 1: 0\n"
            "Tearing 1: 00\n"
            "Counter 2: 0\n"
            "Tearing 2: 00\n"
            "Pages total: %d\n"
            "Pages read: %d\n",
            uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6],
            info->name, info->mifare_ver, total_pages, total_pages);
        storage_file_write(file, header, (uint16_t)hlen);

        char line[40];
        bool write_ok = true;
        for (int page = 0; page < total_pages; page++) {
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
        const char* footer = "Failed authentication attempts: 0\n";
        if (write_ok) {
            size_t flen = strlen(footer);
            if (storage_file_write(file, footer, flen) != (uint16_t)flen) {
                write_ok = false;
            }
        }
        success = write_ok;
    }

    free(mem);
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
        case ViewIdNfcTypePicker:
            switch_to(app, ViewIdWalletMenu);
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

    /* NFC tag type picker */
    app->nfc_type_picker = submenu_alloc();
    rebuild_nfc_type_picker(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdNfcTypePicker, submenu_get_view(app->nfc_type_picker));

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
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdNfcTypePicker);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdNfcInstructions);

    submenu_free(app->main_menu);
    submenu_free(app->wallet_picker);
    submenu_free(app->wallet_menu);
    submenu_free(app->nfc_type_picker);
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
