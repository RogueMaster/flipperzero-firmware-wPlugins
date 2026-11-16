/*
 * KeeLoq Keystore Decryptor - a KeeLoq keystore decryptor for Flipper Zero.
 * https://github.com/Lechnio/KeeLoq-Keystore-Decryptor
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <flipper_format/flipper_format.h>

#define KEYSTORE_PATH "/ext/subghz/assets/keeloq_mfcodes"
#define APP_FOLDER    EXT_PATH("apps_data/subghz_keystore_decryptor")
#define OUTPUT_FILE   "keystore_decrypted.txt"
#define OUTPUT_PATH   APP_FOLDER "/" OUTPUT_FILE

#define APP_NAME        "Sub-GHz Keystore Decryptor"
#define APP_VERSION     "1.1"
#define APP_LINK        "github.com/Lechnio"
#define TITLE_SCROLL_MS 400

/* The keystore ships with the firmware to every device, so it can only be encrypted
 * with a factory key: slots 1..10 hold the same keys on all Flippers. Slot 11 is
 * unique per device and 12..100 are user slots, so they are not offered. */
#define KEY_SLOT_MIN FURI_HAL_CRYPTO_ENCLAVE_FACTORY_KEY_SLOT_START
#define KEY_SLOT_MAX FURI_HAL_CRYPTO_ENCLAVE_FACTORY_KEY_SLOT_END

/* Slot the firmware encrypts its keystore with (SUBGHZ_KEYSTORE_FILE_ENCRYPTION_KEY_SLOT,
 * private to the firmware, so it is set here at build time). Stock firmware uses 1. */
#define KEY_SLOT_DEFAULT 1
_Static_assert(
    KEY_SLOT_DEFAULT >= KEY_SLOT_MIN && KEY_SLOT_DEFAULT <= KEY_SLOT_MAX,
    "KEY_SLOT_DEFAULT must be a factory key slot");

#define MENU_HEADER_H 16
#define MENU_ITEM_H   16

typedef enum {
    ViewMenu,
    ViewResult,
    ViewAbout,
} AppView;

typedef enum {
    MenuDecrypt,
    MenuKeySlot,
    MenuAbout,
    MenuCount,
} MenuIndex;

#define EVENT_DECRYPT 1

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    View* menu;
    Widget* result;
    View* about;
    FuriTimer* title_timer;
    Stream* output_stream;
    uint32_t keys_decrypted;
    uint8_t key_slot;
    char status[64];
} KeystoreDecryptApp;

typedef struct {
    KeystoreDecryptApp* app;
    uint8_t sel;
    uint32_t scroll;
} MenuModel;

typedef struct {
    uint32_t scroll;
} AboutModel;

/* ------------------------------------------------------------------ decryption */

static uint8_t hex_char_to_val(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;

    return 0;
}

/* A real entry is KEY:TYPE:NAME with a 64-bit key written as 16 hex digits. A wrong
 * key slot decrypts to random bytes, which the loose sscanf pattern alone can
 * occasionally accept (a ':' followed by a digit is enough). */
static bool is_hex_key(const char* key) {
    size_t len = 0;
    for(; key[len]; len++) {
        char c = key[len];
        if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
            return false;
    }
    return len == 16;
}

/* Reads the header (Filetype/Version/Encryption/IV) and leaves the stream at the
 * first data line. When the keystore is encrypted, returns the IV already permuted. */
static void keystore_read_header(Stream* stream, uint8_t iv[16], bool* encrypted) {
    char line[256];
    size_t line_pos = 0;
    memset(iv, 0, 16);
    *encrypted = false;

    while(true) {
        uint8_t c;
        if(stream_read(stream, &c, 1) != 1) break;

        if(c == '\n') {
            line[line_pos] = '\0';

            if(line_pos == 0 ||
               (line[0] != 'F' && line[0] != 'V' && line[0] != 'E' && line[0] != 'I')) {
                if(line_pos > 0) {
                    stream_seek(stream, -((int)line_pos + 1), StreamOffsetFromCurrent);
                }

                break;
            }

            if(strncmp(line, "Encryption:", 11) == 0) {
                if(strstr(line, "1")) *encrypted = true;
            } else if(strncmp(line, "IV:", 3) == 0) {
                char* iv_str = line + 3;
                while(*iv_str == ' ')
                    iv_str++;

                for(int i = 0; i < 16 && iv_str[i * 3] && iv_str[i * 3 + 1]; i++) {
                    iv[i] = (hex_char_to_val(iv_str[i * 3]) << 4) |
                            hex_char_to_val(iv_str[i * 3 + 1]);
                }
            }

            line_pos = 0;
        } else if(c != '\r' && line_pos < sizeof(line) - 1) {
            line[line_pos++] = c;
        }
    }

    if(*encrypted) {
        for(int i = 15; i >= 1; i--) {
            uint16_t sum = (uint16_t)iv[i] + (uint16_t)iv[i - 1];
            iv[i] = (sum > 255) ? 255 : (uint8_t)sum;
        }
    }
}

/* Decrypts one hex-encoded line with the key loaded in the enclave.
 * Returns true when it holds a real KEY:TYPE:NAME entry. */
static bool
    decrypt_entry(const char* hex, size_t hex_len, char skey[17], uint16_t* type, char name[65]) {
    size_t bin_len = hex_len / 2;
    size_t decrypt_len = ((bin_len + 15) / 16) * 16;
    uint8_t decrypted[512 + 1] = {0};
    if(bin_len == 0 || decrypt_len > 512) return false;

    uint8_t* bin_data = malloc(bin_len + 16);
    memset(bin_data, 0, bin_len + 16);
    for(size_t j = 0; j + 1 < hex_len && j / 2 < bin_len; j += 2) {
        bin_data[j / 2] = (hex_char_to_val(hex[j]) << 4) | hex_char_to_val(hex[j + 1]);
    }

    bool valid = false;
    if(furi_hal_crypto_decrypt(bin_data, decrypted, decrypt_len)) {
        for(int j = decrypt_len - 1; j >= 0; j--) {
            if(decrypted[j] != 0) {
                decrypted[j + 1] = 0;
                break;
            }
        }

        skey[0] = '\0';
        name[0] = '\0';
        *type = 0;
        valid = sscanf((char*)decrypted, "%16[^:]:%hu:%64s", skey, type, name) >= 2 &&
                is_hex_key(skey);
    }

    free(bin_data);
    return valid;
}

static void close_output(KeystoreDecryptApp* app) {
    if(app->output_stream) {
        buffered_file_stream_close(app->output_stream);
        stream_free(app->output_stream);
        app->output_stream = NULL;
    }
}

static bool decrypt_keystore(KeystoreDecryptApp* app) {
    /* Load and unload must use the same slot. */
    const uint8_t key_slot = app->key_slot;
    app->keys_decrypted = 0;

    storage_simply_mkdir(app->storage, APP_FOLDER);
    app->output_stream = buffered_file_stream_alloc(app->storage);
    if(!buffered_file_stream_open(
           app->output_stream, OUTPUT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        strncpy(app->status, "Output error", sizeof(app->status));
        return false;
    }

    const char* header = "# Decrypted Keystore\n# Format: KEY:TYPE:NAME\n\n";
    stream_write(app->output_stream, (uint8_t*)header, strlen(header));

    Stream* keystore_stream = buffered_file_stream_alloc(app->storage);
    if(!buffered_file_stream_open(keystore_stream, KEYSTORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        strncpy(app->status, "Keystore not found", sizeof(app->status));
        stream_free(keystore_stream);
        return false;
    }

    uint8_t iv[16];
    bool encrypted = false;
    keystore_read_header(keystore_stream, iv, &encrypted);

    if(encrypted && !furi_hal_crypto_enclave_load_key(key_slot, iv)) {
        strncpy(app->status, "Crypto init failed", sizeof(app->status));
        buffered_file_stream_close(keystore_stream);
        stream_free(keystore_stream);
        return false;
    }

    char enc_line[1024];
    size_t enc_pos = 0;
    uint8_t buffer[64];

    while(true) {
        size_t ret = stream_read(keystore_stream, buffer, sizeof(buffer));
        if(ret == 0) break;

        for(size_t i = 0; i < ret; i++) {
            if(buffer[i] == '\n') {
                if(enc_pos > 0) {
                    enc_line[enc_pos] = '\0';

                    if(encrypted) {
                        char skey[17];
                        char name[65];
                        uint16_t type;
                        if(decrypt_entry(enc_line, enc_pos, skey, &type, name)) {
                            FuriString* out_line =
                                furi_string_alloc_printf("%s:%u:%s\n", skey, type, name);
                            stream_write(
                                app->output_stream,
                                (uint8_t*)furi_string_get_cstr(out_line),
                                furi_string_size(out_line));
                            furi_string_free(out_line);
                            app->keys_decrypted++;
                        }
                    } else {
                        stream_write(app->output_stream, (uint8_t*)enc_line, enc_pos);
                        stream_write(app->output_stream, (uint8_t*)"\n", 1);
                    }

                    enc_pos = 0;
                }
            } else if(buffer[i] != '\r' && enc_pos < sizeof(enc_line) - 1) {
                enc_line[enc_pos++] = buffer[i];
            }
        }
    }

    if(encrypted) {
        furi_hal_crypto_enclave_unload_key(key_slot);
    }

    buffered_file_stream_close(keystore_stream);
    stream_free(keystore_stream);

    return true;
}

/* ------------------------------------------------------------------ shared drawing */

/* App name in the current font: centered or left-aligned when it fits,
 * otherwise scrolled sideways with the stock scrolling element. */
static void
    draw_app_name(Canvas* canvas, int x, int y, int width, bool centered, uint32_t scroll) {
    if(canvas_string_width(canvas, APP_NAME) <= width) {
        if(centered)
            canvas_draw_str_aligned(canvas, x + width / 2, y, AlignCenter, AlignBottom, APP_NAME);
        else
            canvas_draw_str(canvas, x, y, APP_NAME);
        return;
    }

    FuriString* name = furi_string_alloc_set(APP_NAME);
    elements_scrollable_text_line(canvas, x, y, width, name, scroll, false);
    furi_string_free(name);
}

static void fill_tri(Canvas* canvas, int bx, int by, int base, int height, bool left) {
    int half = base / 2;
    for(int k = 0; k <= height; k++) {
        int ext = half - (half * k) / height;
        int x = left ? bx - k : bx + k;
        canvas_draw_line(canvas, x, by - ext, x, by + ext);
    }
}

static void draw_adjust_arrows(Canvas* canvas, int rx, int cy) {
    fill_tri(canvas, rx - 7, cy, 5, 4, true);
    fill_tri(canvas, rx - 4, cy, 5, 4, false);
}

/* ------------------------------------------------------------------ menu */

static void menu_draw_callback(Canvas* canvas, void* model) {
    MenuModel* m = model;
    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    draw_app_name(canvas, 4, 12, 116, false, m->scroll);

    char slot_label[32];
    snprintf(
        slot_label,
        sizeof(slot_label),
        "Key slot (%u-%u): %u",
        (unsigned)KEY_SLOT_MIN,
        (unsigned)KEY_SLOT_MAX,
        m->app->key_slot);
    const char* labels[MenuCount] = {
        [MenuDecrypt] = "Decrypt",
        [MenuKeySlot] = slot_label,
        [MenuAbout] = "About",
    };

    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < MenuCount; i++) {
        int y = MENU_HEADER_H + i * MENU_ITEM_H;
        if(i == m->sel) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 0, y, 122, MENU_ITEM_H - 1);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(canvas, 6, y + 11, labels[i]);
        if(i == MenuKeySlot) draw_adjust_arrows(canvas, 112, y + MENU_ITEM_H / 2);
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, m->sel, MenuCount);
}

static bool menu_input_callback(InputEvent* event, void* context) {
    KeystoreDecryptApp* app = context;
    if(event->key == InputKeyBack) return false; /* previous callback exits the app */
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    uint8_t sel = 0;
    with_view_model(
        app->menu,
        MenuModel * m,
        {
            if(event->key == InputKeyUp)
                m->sel = (m->sel + MenuCount - 1) % MenuCount;
            else if(event->key == InputKeyDown)
                m->sel = (m->sel + 1) % MenuCount;
            else if(m->sel == MenuKeySlot && event->key == InputKeyLeft && app->key_slot > KEY_SLOT_MIN)
                app->key_slot--;
            else if(m->sel == MenuKeySlot && event->key == InputKeyRight && app->key_slot < KEY_SLOT_MAX)
                app->key_slot++;
            sel = m->sel;
        },
        true);

    if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(sel == MenuDecrypt)
            view_dispatcher_send_custom_event(app->view_dispatcher, EVENT_DECRYPT);
        else if(sel == MenuAbout)
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewAbout);
    }
    return true;
}

/* ------------------------------------------------------------------ result */

static void show_result(KeystoreDecryptApp* app, bool ok) {
    char text[96];
    if(!ok)
        snprintf(text, sizeof(text), "%s\nKey slot: %u", app->status, app->key_slot);
    else if(app->keys_decrypted == 0)
        snprintf(text, sizeof(text), "No keys decrypted.\nWrong key slot (%u)?", app->key_slot);
    else
        snprintf(
            text,
            sizeof(text),
            "Keys: %lu (slot %u)\nSaved to app folder:\n" OUTPUT_FILE,
            app->keys_decrypted,
            app->key_slot);

    widget_reset(app->result);
    widget_add_string_element(
        app->result, 64, 4, AlignCenter, AlignTop, FontPrimary, ok ? "Done!" : "Failed!");
    widget_add_string_multiline_element(
        app->result, 64, 40, AlignCenter, AlignCenter, FontSecondary, text);
}

static bool custom_event_callback(void* context, uint32_t event) {
    KeystoreDecryptApp* app = context;
    if(event != EVENT_DECRYPT) return false;

    widget_reset(app->result);
    widget_add_string_element(
        app->result, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Decrypting...");
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewResult);

    bool ok = decrypt_keystore(app);
    /* Flush the output now, so the file is complete even before the app exits. */
    close_output(app);
    show_result(app, ok);
    return true;
}

/* ------------------------------------------------------------------ about */

static void about_draw_callback(Canvas* canvas, void* model) {
    AboutModel* m = model;
    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 14);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    draw_app_name(canvas, 2, 11, 124, true, m->scroll);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 0, 26, "Version " APP_VERSION);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 37, APP_LINK);
}

/* ------------------------------------------------------------------ app */

static void title_timer_callback(void* context) {
    KeystoreDecryptApp* app = context;
    with_view_model(app->menu, MenuModel * m, { m->scroll++; }, true);
    with_view_model(app->about, AboutModel * m, { m->scroll++; }, true);
}

static uint32_t exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t back_to_menu_callback(void* context) {
    UNUSED(context);
    return ViewMenu;
}

static View* make_view(
    KeystoreDecryptApp* app,
    ViewDrawCallback draw,
    ViewInputCallback input,
    size_t model_size,
    ViewNavigationCallback previous) {
    View* view = view_alloc();
    view_set_context(view, app);
    view_allocate_model(view, ViewModelTypeLocking, model_size);
    view_set_draw_callback(view, draw);
    if(input) view_set_input_callback(view, input);
    view_set_previous_callback(view, previous);
    return view;
}

int32_t keystore_decrypt_app(void* p) {
    UNUSED(p);

    KeystoreDecryptApp* app = malloc(sizeof(KeystoreDecryptApp));
    memset(app, 0, sizeof(KeystoreDecryptApp));

    app->key_slot = KEY_SLOT_DEFAULT;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);

    app->menu =
        make_view(app, menu_draw_callback, menu_input_callback, sizeof(MenuModel), exit_callback);
    with_view_model(app->menu, MenuModel * m, { m->app = app; }, false);

    app->result = widget_alloc();
    view_set_previous_callback(widget_get_view(app->result), back_to_menu_callback);

    app->about =
        make_view(app, about_draw_callback, NULL, sizeof(AboutModel), back_to_menu_callback);

    app->title_timer = furi_timer_alloc(title_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->title_timer, furi_ms_to_ticks(TITLE_SCROLL_MS));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, ViewMenu, app->menu);
    view_dispatcher_add_view(app->view_dispatcher, ViewResult, widget_get_view(app->result));
    view_dispatcher_add_view(app->view_dispatcher, ViewAbout, app->about);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_run(app->view_dispatcher);

    furi_timer_stop(app->title_timer);
    furi_timer_free(app->title_timer);

    view_dispatcher_remove_view(app->view_dispatcher, ViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewResult);
    view_dispatcher_remove_view(app->view_dispatcher, ViewAbout);
    view_dispatcher_free(app->view_dispatcher);

    view_free(app->menu);
    widget_free(app->result);
    view_free(app->about);
    close_output(app);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
