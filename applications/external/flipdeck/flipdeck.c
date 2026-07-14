/*
 * FlipDeck v3 – Multi-Page USB HID Macro Pad
 *
 * Features:
 *   - Configurable pages (up to 8) with 6 buttons each (2×3 grid)
 *   - Page names displayed in header with indicator
 *   - Native media keys: Play/Pause, Vol+/-, Mute, Next/Prev
 *   - Custom keys: F13-F24 for host-side actions
 *   - Left/Right at grid edge = page switch
 *   - Short Back = previous page / Long Back at root = exit
 *   - Haptic + visual feedback on button press
 *   - Config from SD: /ext/apps_data/flipdeck/config.txt
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/file_stream.h>

/* ── Limits ───────────────────────────────────────────── */

#define TAG             "FlipDeck"
#define MAX_PAGES       8
#define BTNS_PER_PAGE   6
#define GRID_COLS       3
#define GRID_ROWS       2
#define MAX_LABEL       10
#define MAX_SYMBOL      4
#define MAX_PAGE_NAME   14
#define MAX_ACTION_NAME 8
#define CONFIG_PATH     APP_DATA_PATH("config.txt")
#define EXIT_HOLD_MS    600

/* ── Layout (128×64 OLED, full-screen, no footer) ───── */

#define HDR_H  11
#define GRID_Y (HDR_H + 1)
#define GRID_H (64 - HDR_H - 1)
#define CELL_W (128 / GRID_COLS)
#define CELL_H (GRID_H / GRID_ROWS)

/* ── HID codes ────────────────────────────────────────── */

/* Consumer (media) keys */
#define C_PLAY  0x00CD
#define C_NEXT  0x00B5
#define C_PREV  0x00B6
#define C_VOLUP 0x00E9
#define C_VOLDN 0x00EA
#define C_MUTE  0x00E2
#define C_BRTUP 0x006F
#define C_BRTDN 0x0070

/* Keyboard F-keys */
#define K_F13 0x68
#define K_F14 0x69
#define K_F15 0x6A
#define K_F16 0x6B
#define K_F17 0x6C
#define K_F18 0x6D
#define K_F19 0x6E
#define K_F20 0x6F
#define K_F21 0x70
#define K_F22 0x71
#define K_F23 0x72
#define K_F24 0x73

/* ── Types ────────────────────────────────────────────── */

typedef enum {
    ActConsumer,
    ActKeyboard
} ActType;

typedef struct {
    char label[MAX_LABEL];
    char symbol[MAX_SYMBOL];
    ActType type;
    uint16_t code;
} BtnDef;

typedef struct {
    char name[MAX_PAGE_NAME];
    BtnDef btns[BTNS_PER_PAGE];
    uint8_t count; /* how many buttons on this page (1-6) */
} Page;

typedef struct {
    Page pages[MAX_PAGES];
    uint8_t page_count;
    uint8_t cur_page;
    uint8_t cur_btn; /* 0..5 within page */
    bool running;
    bool hid_ok;
    bool flash;
    uint32_t flash_end;
} App;

/* ── Haptic ───────────────────────────────────────────── */

static const NotificationSequence seq_press = {
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    NULL,
};

/* ── Action registry ──────────────────────────────────── */

typedef struct {
    const char* name;
    ActType type;
    uint16_t code;
} ActEntry;

static const ActEntry act_table[] = {
    {"PLAY", ActConsumer, C_PLAY},   {"NEXT", ActConsumer, C_NEXT},
    {"PREV", ActConsumer, C_PREV},   {"VOLUP", ActConsumer, C_VOLUP},
    {"VOLDN", ActConsumer, C_VOLDN}, {"MUTE", ActConsumer, C_MUTE},
    {"BRTUP", ActConsumer, C_BRTUP}, {"BRTDN", ActConsumer, C_BRTDN},
    {"F13", ActKeyboard, K_F13},     {"F14", ActKeyboard, K_F14},
    {"F15", ActKeyboard, K_F15},     {"F16", ActKeyboard, K_F16},
    {"F17", ActKeyboard, K_F17},     {"F18", ActKeyboard, K_F18},
    {"F19", ActKeyboard, K_F19},     {"F20", ActKeyboard, K_F20},
    {"F21", ActKeyboard, K_F21},     {"F22", ActKeyboard, K_F22},
    {"F23", ActKeyboard, K_F23},     {"F24", ActKeyboard, K_F24},
};
#define ACT_COUNT (sizeof(act_table) / sizeof(act_table[0]))

static bool find_action(const char* name, ActType* t, uint16_t* c) {
    for(size_t i = 0; i < ACT_COUNT; i++) {
        if(strcasecmp(name, act_table[i].name) == 0) {
            *t = act_table[i].type;
            *c = act_table[i].code;
            return true;
        }
    }
    return false;
}

/* ── Config ───────────────────────────────────────────── */

/*
 * Format:
 *   # page:Media
 *   PLAY:Play:>
 *   NEXT:Next:>>
 *   # page:Volume
 *   VOLUP:Vol+:+
 *   VOLDN:Vol-:-
 *   MUTE:Mute:X
 *
 * Fields: ACTION:Label:Symbol
 * Page header: # page:PageName
 */

static void default_config(App* app) {
    app->page_count = 2;

    /* Page 0: Media */
    Page* p0 = &app->pages[0];
    strncpy(p0->name, "Media", MAX_PAGE_NAME - 1);
    p0->count = 6;
    const struct {
        const char* l;
        const char* s;
        ActType t;
        uint16_t c;
    } d0[] = {
        {"Play", ">", ActConsumer, C_PLAY},
        {"Next", ">>", ActConsumer, C_NEXT},
        {"Prev", "<<", ActConsumer, C_PREV},
        {"Vol+", "+", ActConsumer, C_VOLUP},
        {"Vol-", "-", ActConsumer, C_VOLDN},
        {"Mute", "X", ActConsumer, C_MUTE},
    };
    for(int i = 0; i < 6; i++) {
        strncpy(p0->btns[i].label, d0[i].l, MAX_LABEL - 1);
        strncpy(p0->btns[i].symbol, d0[i].s, MAX_SYMBOL - 1);
        p0->btns[i].type = d0[i].t;
        p0->btns[i].code = d0[i].c;
    }

    /* Page 1: Custom */
    Page* p1 = &app->pages[1];
    strncpy(p1->name, "Custom", MAX_PAGE_NAME - 1);
    p1->count = 6;
    for(int i = 0; i < 6; i++) {
        char lbl[MAX_LABEL];
        snprintf(lbl, sizeof(lbl), "F%d", 13 + i);
        strncpy(p1->btns[i].label, lbl, MAX_LABEL - 1);
        strncpy(p1->btns[i].symbol, "*", MAX_SYMBOL - 1);
        p1->btns[i].type = ActKeyboard;
        p1->btns[i].code = K_F13 + i;
    }
}

static size_t trim_end(const char* s, size_t len) {
    while(len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' '))
        len--;
    return len;
}

static bool parse_btn_line(const char* line, BtnDef* btn) {
    /* ACTION:Label:Symbol */
    const char* p1 = strchr(line, ':');
    if(!p1) return false;
    size_t a_len = p1 - line;
    if(a_len == 0 || a_len >= MAX_ACTION_NAME) return false;

    char aname[MAX_ACTION_NAME];
    memcpy(aname, line, a_len);
    aname[a_len] = '\0';

    ActType t;
    uint16_t c;
    if(!find_action(aname, &t, &c)) return false;

    const char* p2 = strchr(p1 + 1, ':');
    if(!p2) return false;
    size_t l_len = p2 - (p1 + 1);
    if(l_len == 0 || l_len >= MAX_LABEL) return false;

    const char* sym = p2 + 1;
    size_t s_len = trim_end(sym, strlen(sym));
    if(s_len == 0 || s_len >= MAX_SYMBOL) return false;

    memcpy(btn->label, p1 + 1, l_len);
    btn->label[l_len] = '\0';
    memcpy(btn->symbol, sym, s_len);
    btn->symbol[s_len] = '\0';
    btn->type = t;
    btn->code = c;
    return true;
}

static void load_config(App* app) {
    default_config(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();

    if(!file_stream_open(stream, CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_I(TAG, "No config, defaults loaded");
        furi_string_free(line);
        stream_free(stream);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    /* Reset for parsing */
    app->page_count = 0;
    int cur_page = -1;

    while(stream_read_line(stream, line)) {
        const char* s = furi_string_get_cstr(line);

        /* Skip empty lines */
        if(s[0] == '\n' || s[0] == '\r' || s[0] == '\0') continue;

        /* Page header: "# page:Name" */
        if(s[0] == '#') {
            const char* marker = strstr(s, "page:");
            if(marker && app->page_count < MAX_PAGES) {
                cur_page = app->page_count;
                app->page_count++;
                Page* p = &app->pages[cur_page];
                memset(p, 0, sizeof(Page));

                const char* name = marker + 5;
                size_t nlen = trim_end(name, strlen(name));
                if(nlen >= MAX_PAGE_NAME) nlen = MAX_PAGE_NAME - 1;
                memcpy(p->name, name, nlen);
                p->name[nlen] = '\0';
            }
            continue;
        }

        /* Button line */
        if(cur_page < 0) {
            /* No page header yet, create implicit first page */
            cur_page = 0;
            app->page_count = 1;
            memset(&app->pages[0], 0, sizeof(Page));
            strncpy(app->pages[0].name, "Main", MAX_PAGE_NAME - 1);
        }

        Page* p = &app->pages[cur_page];
        if(p->count < BTNS_PER_PAGE) {
            BtnDef tmp = {0};
            if(parse_btn_line(s, &tmp)) {
                p->btns[p->count] = tmp;
                p->count++;
            }
        }
    }

    if(app->page_count == 0) {
        FURI_LOG_W(TAG, "Config empty, using defaults");
        default_config(app);
    } else {
        int total = 0;
        for(uint8_t i = 0; i < app->page_count; i++)
            total += app->pages[i].count;
        FURI_LOG_I(TAG, "Loaded %d pages, %d buttons", app->page_count, total);
    }

    furi_string_free(line);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
}

/* ── USB HID ──────────────────────────────────────────── */

static FuriHalUsbInterface* usb_prev = NULL;

static bool hid_init(void) {
    usb_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_hid, NULL)) {
        FURI_LOG_E(TAG, "HID init fail");
        usb_prev = NULL;
        return false;
    }
    furi_delay_ms(500);
    return true;
}

static void hid_deinit(void) {
    if(usb_prev) {
        furi_hal_usb_set_config(usb_prev, NULL);
        furi_delay_ms(100);
        usb_prev = NULL;
    }
}

static void hid_press(const BtnDef* b) {
    if(b->type == ActConsumer) {
        furi_hal_hid_consumer_key_press(b->code);
        furi_delay_ms(50);
        furi_hal_hid_consumer_key_release(b->code);
    } else {
        furi_hal_hid_kb_press(b->code);
        furi_delay_ms(50);
        furi_hal_hid_kb_release(b->code);
    }
}

/* ── Drawing ──────────────────────────────────────────── */

static void draw_cb(Canvas* c, void* ctx) {
    App* a = ctx;
    Page* pg = &a->pages[a->cur_page];
    canvas_clear(c);

    /* ── Header: page name + page dots ── */
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 9, pg->name);

    /* Page indicator dots (right side of header) */
    if(a->page_count > 1) {
        int dot_w = a->page_count * 6 - 2;
        int dot_x = 126 - dot_w;
        for(uint8_t i = 0; i < a->page_count; i++) {
            int dx = dot_x + i * 6;
            if(i == a->cur_page) {
                canvas_draw_disc(c, dx, 5, 2);
            } else {
                canvas_draw_circle(c, dx, 5, 2);
            }
        }
    }
    canvas_draw_line(c, 0, HDR_H, 127, HDR_H);

    /* ── Button grid ── */
    uint32_t now = furi_get_tick();
    bool show_flash = a->flash && (now < a->flash_end);

    for(uint8_t i = 0; i < pg->count; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int x = col * CELL_W;
        int y = GRID_Y + row * CELL_H;
        bool sel = (i == a->cur_btn);

        if(sel) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_rbox(c, x + 1, y + 1, CELL_W - 2, CELL_H - 2, 3);
            canvas_set_color(c, ColorWhite);
        } else {
            canvas_set_color(c, ColorBlack);
            canvas_draw_rframe(c, x + 1, y + 1, CELL_W - 2, CELL_H - 2, 3);
        }

        if(sel && show_flash) canvas_set_color(c, ColorXOR);

        /* Symbol */
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(
            c, x + CELL_W / 2, y + CELL_H / 2 - 3, AlignCenter, AlignCenter, pg->btns[i].symbol);

        /* Label */
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(
            c, x + CELL_W / 2, y + CELL_H - 2, AlignCenter, AlignBottom, pg->btns[i].label);

        canvas_set_color(c, ColorBlack);
    }

    /* ── Page arrows (if multiple pages) ── */
    if(a->page_count > 1) {
        canvas_set_font(c, FontSecondary);
        if(a->cur_page > 0) {
            canvas_draw_str(c, 0, GRID_Y + GRID_H / 2 + 3, "<");
        }
        if(a->cur_page < a->page_count - 1) {
            canvas_draw_str_aligned(c, 127, GRID_Y + GRID_H / 2 + 3, AlignRight, AlignBottom, ">");
        }
    }
}

static void input_cb(InputEvent* ev, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, ev, FuriWaitForever);
}

/* ── Main ─────────────────────────────────────────────── */

int32_t flipdeck_main(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->running = true;

    load_config(app);
    app->hid_ok = hid_init();

    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(queue, &ev, 100) == FuriStatusOk) {
            Page* pg = &app->pages[app->cur_page];
            uint8_t col = app->cur_btn % GRID_COLS;
            uint8_t row = app->cur_btn / GRID_COLS;

            /* ── Long press Back = exit ── */
            if(ev.key == InputKeyBack && ev.type == InputTypeLong) {
                app->running = false;
                continue;
            }

            /* ── Short press Back = previous page (or nothing at page 0) ── */
            if(ev.key == InputKeyBack && ev.type == InputTypeShort) {
                if(app->cur_page > 0) {
                    app->cur_page--;
                    if(app->cur_btn >= app->pages[app->cur_page].count) {
                        app->cur_btn = 0;
                    }
                }
                continue;
            }

            /* ── Navigation ── */
            if(ev.type == InputTypePress || ev.type == InputTypeRepeat) {
                switch(ev.key) {
                case InputKeyLeft:
                    if(col == 0) {
                        /* At left edge → go to previous page */
                        if(app->cur_page > 0) {
                            app->cur_page--;
                            pg = &app->pages[app->cur_page];
                            col = GRID_COLS - 1;
                            /* Keep same row, clamp to page button count */
                            uint8_t new_btn = row * GRID_COLS + col;
                            if(new_btn >= pg->count) new_btn = pg->count - 1;
                            app->cur_btn = new_btn;
                        }
                    } else {
                        col--;
                        uint8_t nb = row * GRID_COLS + col;
                        if(nb < pg->count) app->cur_btn = nb;
                    }
                    break;

                case InputKeyRight:
                    if(col == GRID_COLS - 1) {
                        /* At right edge → go to next page */
                        if(app->cur_page < app->page_count - 1) {
                            app->cur_page++;
                            pg = &app->pages[app->cur_page];
                            col = 0;
                            uint8_t new_btn = row * GRID_COLS + col;
                            if(new_btn >= pg->count) new_btn = 0;
                            app->cur_btn = new_btn;
                        }
                    } else {
                        col++;
                        uint8_t nb = row * GRID_COLS + col;
                        if(nb < pg->count) app->cur_btn = nb;
                    }
                    break;

                case InputKeyUp:
                    if(row > 0) {
                        row--;
                        uint8_t nb = row * GRID_COLS + col;
                        if(nb < pg->count) app->cur_btn = nb;
                    }
                    break;

                case InputKeyDown:
                    if(row < GRID_ROWS - 1) {
                        uint8_t nb = (row + 1) * GRID_COLS + col;
                        if(nb < pg->count) app->cur_btn = nb;
                    }
                    break;

                default:
                    break;
                }
            }

            /* ── OK = send HID action ── */
            if(ev.key == InputKeyOk && ev.type == InputTypeShort) {
                if(app->hid_ok && app->cur_btn < pg->count) {
                    hid_press(&pg->btns[app->cur_btn]);
                }
                notification_message(notif, &seq_press);
                app->flash = true;
                app->flash_end = furi_get_tick() + furi_ms_to_ticks(120);
            }
        }

        if(app->flash && furi_get_tick() >= app->flash_end) {
            app->flash = false;
        }

        view_port_update(vp);
    }

    /* Cleanup */
    view_port_enabled_set(vp, false);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    hid_deinit();
    free(app);
    return 0;
}
