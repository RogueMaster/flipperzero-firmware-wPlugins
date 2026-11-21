#include "z5r.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view_stack.h>
#include <gui/elements.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_box.h>
#include <gui/modules/number_input.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification_messages.h>
#include <notification/notification_messages_notes.h>
#include <gui/icon_i.h>
#include <lfrfid/lfrfid_worker.h>
#include <ibutton/ibutton_worker.h>
#include <ibutton/ibutton_key.h>
#include <ibutton/ibutton_protocols.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <ctype.h>

#define DATA_DIR      "/ext/apps_data/z5flip"
#define EXT           ".bin" /* raw memory image, byte-identical to BaseZ5R "controller -> file" */
#define LOG_PATH      DATA_DIR "/log.txt"

typedef enum { VMain, VCtl, VKeys, VKey, VBytes, VText, VPopup, VInfo, VWire, VFilter, VAdd, VWarn, VNum, VTouch } ViewId;
typedef enum { ByteAdd, ByteEdit } ByteMode;
typedef enum { TickNone, TickTouch } TickMode;

enum { MCtl, MRead, MLoad, MErase, MInfo };
enum { CAdd, CTime, CKeys, CWrite, CSave };
#define ROWS_FIRST 1 /* row 0 of the key list is the search line */
enum { KAttr, KEdit, KExport, KDelete };
enum { AType, ATouch, ATouchRfid, AIbtn, ARfid, AManual }; /* AType: selector, Left / Right change the type of added keys */
enum { TmIbtn = 1, TmRfid = 2 }; /* touch reading source */
enum { FCode, FType, FReset };
/* same key seen again within this time = still on the reader; shorter = faster re-tap, but a flaky read may count twice */
#define TOUCH_GAP_MS      400 /* iButton polls every 100 ms */
#define TOUCH_GAP_RFID_MS 450 /* RFID worker re-reads a held card every few hundred ms */
#define KEYS_NONE   0xFFFD
/* stock 'Dolphin done' (thumbs up) bitmap (system icons are not exported to FAPs); data is firmware-compressed */
static const uint8_t dolphin_frame[] = {1,0,124,1,0,39,254,62,8,110,4,3,192,9,105,128,8,96,96,149,134,0,134,8,9,88,16,8,99,0,149,128,128,134,64,8,76,0,12,168,0,148,2,152,52,195,44,33,32,77,196,7,236,9,184,64,8,111,133,165,4,150,14,2,9,63,132,44,134,80,32,113,29,68,236,52,15,228,16,18,112,36,80,208,62,80,81,177,192,147,195,32,240,194,193,38,83,24,24,38,33,146,15,152,37,63,43,49,144,127,193,41,5,129,240,52,0,149,129,145,70,193,248,131,161,142,4,14,24,135,130,10,9,75,3,156,160,64,146,135,65,21,229,128,128,130,74,34,123,23,96,131,4,18,103,17,5,137,76,65,35,128,136,12,5,216,32,149,1,140,128,82,64,81,252,68,16,3,192,115,144,36,96,145,17,184,192,39,224,147,129,68,135,68,234,64,73,67,193,43,1,4,140,125,212,160,145,192,95,193,45,34,160,127,96,113,192,74,80,102,31,217,252,136,18,45,97,128,224,97,159,152,120,36,174,16,12,17,16,126,48,25,8,36,122,16,10,49,28,75,34,9,45,132,6,120,220,47,128,36,170,16,52,152,159,134,16,39,45,4,8,18,112,68,9,88,32,164,113,4,175,130,12,9,56,18,56,168,16,211,200,232,129,35,0,131,40,198,20,24,16,153,47,203,1,21,192,98,181,64,146,240,2,70,52,77,16,153,192,32,240,9,172,3,84,19,23,40,135,138,87,0,170,28,68,166,43,34,170,192,53,1,49,72,136,0,173,104,225,170,192,92,226,0,46,177,32,117,67,49,44,8,0,67,67,0,255,130,69,206,97,49,74,150,1,206,97,50,4,140,2,126,19,48};
static const uint8_t* const dolphin_frames[] = {dolphin_frame};
static const Icon dolphin_ok = {.width = 80, .height = 58, .frame_count = 1, .frame_rate = 0, .frames = dolphin_frames};
/* open time code = sec * 260/17 (~15.3 ticks/s; BaseZ5R: 255 s = 3900, factory 3 s = 46) */
#define SEC_TO_CODE(s) (((s) * 260 + 8) / 17)
#define CODE_TO_SEC(c) (((c) * 17 + 130) / 260)
#define KEYS_FILTER 0xFFFC
#define KROWS       (Z5R_KEYS_MAX + 2) /* search line + keys or a placeholder */

static uint32_t text_back = VMain; /* view the text input returns to on Back (save name / key search) */
static uint32_t bytes_back = VAdd; /* same for the key number input */

typedef struct {
    Z5rDb* db;
    Gui* gui;
    ViewDispatcher* vd;
    View* main; /* own view: header with key count and open time */
    uint32_t mq; /* last 250 ms step the menu was redrawn for (scrolling text) */
    bool mphase; /* pencil blink phase: true = hidden */
    bool led; /* yellow LED lit together with "unsaved" */
    View* ctl; /* same header as main */
    View* keys; /* own list view: needs a Right = menu hint at the bottom, which Submenu cannot draw */
    uint16_t rid[KROWS];
    uint16_t nrows, ksel;
    VariableItemList* key;
    ViewStack* keyv; /* key list + hint overlay for OK */
    View* keyhint;
    VariableItem* kitem; /* key Type item */
    VariableItem* kexp; /* key Export item: iButton / EM4100 */
    uint8_t kmap[4]; /* key list row -> KAttr.. action */
    uint8_t fbytes[6]; /* key search input, MSB first */
    VariableItemList* filter;
    VariableItem* fitem[3];
    View* add; /* same header as main; row 0 = key type selector */
    ByteInput* bytes;
    TextInput* text;
    Popup* popup;
    View* touch; /* continuous key reading: iButton or RFID */
    NumberInput* num;
    NotificationApp* notif;
    LFRFIDWorker* rfid;
    ProtocolDict* rdict;
    bool blink;
    uint8_t touch_mode; /* TmIbtn or TmRfid */
    iButtonProtocols* iprot; /* stock iButton reader: handles presence and timing itself */
    iButtonWorker* iwork;
    iButtonKey* ikey;
    volatile bool ihit; /* set by the worker thread, consumed in the tick */
    volatile bool rfid_hit; /* set by the worker thread, consumed in the tick */
    uint8_t rfid_data[8];
    uint8_t rfid_proto;
    uint8_t tattr; /* type given to the next key read */
    uint8_t src; /* where the base came from: 0 none / new, 1 controller, 2 file, 3 written to controller, not saved */
    bool bak; /* src == 1 and controller_last.bin holds what was read: Write needs no backup prompt */
    bool fdirty; /* base differs from the file it was loaded from / saved to */
    bool exit_ask; /* Back on the main screen is waiting for the exit warning */
    char srcname[32]; /* file name without .bin, when src == 2 */
    char label[40]; /* first main menu item */
    uint16_t base0; /* key count at the last read / load / write / save: header shows the change against it */
    uint8_t tstate; /* 0 nothing yet, 1 added, 2 already in base, 3 base full, 4 not a Dallas key, 5 type overwritten */
    uint16_t tadded, tdup;
    char tlast[13];
    Z5rKey tkey;
    uint32_t ttime;
    View* warn; /* message with button hints: Left = back, Right = go on */
    char hdr[24];
    TextBox* info;
    View* wire; /* wiring picture, first page of Info */
    DialogsApp* dialogs;
    Storage* storage;
    TickMode tick;
    uint32_t cur; /* last menu view shown: a message popup returns here on Back */
    uint32_t back; /* view Left returns to from the hint message */
    uint32_t ok; /* where OK on a message popup leads */
    uint8_t edit[6]; /* MSB first, as shown on screen */
    ByteMode byte_mode;
    int sel;
    bool power5v;
    bool modified;
    uint8_t fattr; /* key search: 0 = any type, else Z5rAttr */
    char fcode[13]; /* key search: part of the code, hex, upper case; empty = any */
    char flabel[48];
    char name[32];
    char msg[160];
    char klabel[24]; /* key screen first row: the list keeps the pointer, label is reused elsewhere */
} App;

static void go(App* app, uint32_t v) {
    if(v != VPopup && v != VBytes && v != VText && v != VWarn && v != VNum && v != VTouch)
        app->cur = v;
    view_dispatcher_switch_to_view(app->vd, v);
}

static void keys_fill(App* app);

static void open_keys(App* app) {
    keys_fill(app);
    app->ksel = app->nrows > ROWS_FIRST ? ROWS_FIRST : 0; /* first key, not the search line */
    go(app, VKeys);
}

static void popup_ok(void* ctx) {
    App* app = ctx;
    if(app->ok == VKeys)
        open_keys(app);
    else
        go(app, app->ok);
}

/* full-screen "working" notice: the dialog/menu that started a blocking operation is replaced first */
static const uint8_t z5_read_frame[] = {0,0,0,254,31,0,0,0,1,0,0,0,0,0,0,192,1,224,1,0,128,0,0,0,0,0,0,0,48,0,0,6,0,128,0,0,0,0,0,0,0,12,0,0,24,0,64,0,0,0,0,0,0,0,2,0,0,32,0,64,64,0,248,255,255,1,0,1,0,0,64,0,32,32,0,8,2,30,1,128,0,0,0,128,0,32,32,0,232,2,33,1,64,0,0,0,0,1,32,16,0,168,130,64,1,32,0,0,0,0,2,16,16,0,232,130,76,1,32,0,0,0,60,4,16,8,4,8,130,76,1,16,0,0,0,66,4,16,8,2,232,130,64,1,16,0,0,0,129,8,16,8,2,168,2,33,1,8,6,0,128,24,9,16,8,2,232,2,30,1,8,9,8,128,36,9,16,8,2,8,2,0,1,132,16,8,128,36,17,16,8,2,232,194,31,1,196,16,16,128,36,17,16,8,4,168,130,24,1,68,16,16,128,36,17,16,16,0,232,130,16,1,70,32,32,128,36,17,32,16,0,8,194,31,1,42,32,32,128,36,17,32,32,0,232,2,0,1,54,32,64,128,24,17,32,32,0,168,2,0,1,27,224,128,0,129,16,64,64,0,232,2,0,1,13,224,0,1,66,16,64,0,0,8,2,0,1,7,224,1,0,60,24,128,0,0,232,2,0,1,3,240,1,0,0,20,128,0,0,168,2,0,1,1,240,3,0,0,10,0,1,0,232,2,0,1,0,208,255,1,0,8,0,0,0,8,2,0,1,0,208,255,1,0,12,0,0,0,232,2,0,1,0,24,255,0,0,14,0,0,0,168,226,15,1,0,8,60,0,31,14,0,0,0,232,242,31,1,0,24,0,96,128,6,0,0,0,8,242,31,1,0,12,0,8,0,5,0,0,0,232,226,15,1,0,20,0,2,0,7,0,0,0,168,2,0,1,0,12,0,0,0,6,0,0,0,232,194,127,1,0,6,0,0,0,4,0,0,0,8,66,85,1,0,10,0,0,0,4,0,0,0,232,194,127,1,0,5,0,0,0,8,0,0,0,168,66,85,1,0,11,0,0,0,8,0,0,0,232,194,127,1,128,5,24,0,0,16,0,0,0,8,2,0,1,192,2,32,0,0,16,0,0,0,248,255,255,1,64,5,192,0,0,32,0,0,0,0,0,0,0,160,2,0,1,0,32,0,0,0,0,0,0,0,80,1,0,6,0,64,0,0,0,0,0,0,0,176,0,20,8,0,64,0,0,0,0,0,0,0,88,0,106,48,0,64,0,0,0,0,0,0,0,172,0,212,65,0,64,0,0,0,0,0,0,0,84,0,168,131,1,64,0,0,0,0,0,0,0,42,0,80,15,2,64,0,0,0,0,0,0,0,21,0,160,30,252,95,0,0,0,0,0,0,0,11,0,64,125,0,48,0,0,0,0,0,0,0,1,0,160,250,1,16,0,0,0,0,0,0,0,0,0,0,245,7,8,0,0,0,0,0,0,0,0,0,0,234,255,7,0,0,0,0,0,0,0,0,0,0,208,95,0,0,0,0,0,0,0,0,0,0,0,160,158,0,0,0,0,0,0,0,0,0,0,0,64,149,0,0,0,0,0,0,0,0,0,0,0,0,154,0,0,0,0,0,0,0,0,0,0,0,0,16,1,0,0,0,0,0,0,0,0,0,0,0,8,1,0,0,0,0,0,0,0,0,0,0,0,8,1,0,0,0,0,0,0,0,0,0,0,0,8,2,0,0,0,0,0,0,0,0,0,0,0,8,2,0,0,0,0,0,0,0};
static const uint8_t* const z5_read_frames[] = {z5_read_frame};
static const Icon z5_read_icon = {.width = 97, .height = 61, .frame_count = 1, .frame_rate = 0, .frames = z5_read_frames};
static const uint8_t z5_write_frame[] = {0,0,0,254,31,0,0,0,64,0,0,0,0,0,0,192,1,224,1,0,0,128,0,0,0,0,0,0,48,0,0,6,0,0,128,0,0,0,0,0,0,12,0,0,24,0,0,0,1,0,0,0,0,0,2,0,0,32,0,0,1,1,248,255,255,1,0,1,0,0,64,0,0,2,2,8,2,30,1,128,0,0,0,128,0,0,2,2,232,2,33,1,64,0,0,0,0,1,0,4,2,168,130,64,1,32,0,0,0,0,2,0,4,4,232,130,76,1,32,0,0,0,60,4,16,8,4,8,130,76,1,16,0,0,0,66,4,32,8,4,232,130,64,1,16,0,0,0,129,8,32,8,4,168,2,33,1,8,6,0,128,24,9,32,8,4,232,2,30,1,8,9,8,128,36,9,32,8,4,8,2,0,1,132,16,8,128,36,17,32,8,4,232,194,31,1,196,16,16,128,36,17,16,8,4,168,130,24,1,68,16,16,128,36,17,0,4,4,232,130,16,1,70,32,32,128,36,17,0,4,2,8,194,31,1,42,32,32,128,36,17,0,2,2,232,2,0,1,54,32,64,128,24,17,0,2,2,168,2,0,1,27,224,128,0,129,16,0,1,1,232,2,0,1,13,224,0,1,66,16,0,0,1,8,2,0,1,7,224,1,0,60,24,0,128,0,232,2,0,1,3,240,1,0,0,20,0,128,0,168,2,0,1,1,240,3,0,0,10,0,64,0,232,2,0,1,0,208,255,1,0,8,0,0,0,8,2,0,1,0,208,255,1,0,12,0,0,0,232,2,0,1,0,24,255,0,0,14,0,0,0,168,226,15,1,0,8,60,0,31,14,0,0,0,232,242,31,1,0,24,0,96,128,6,0,0,0,8,242,31,1,0,12,0,8,0,5,0,0,0,232,226,15,1,0,20,0,2,0,7,0,0,0,168,2,0,1,0,12,0,0,0,6,0,0,0,232,194,127,1,0,6,0,0,0,4,0,0,0,8,66,85,1,0,10,0,0,0,4,0,0,0,232,194,127,1,0,5,0,0,0,8,0,0,0,168,66,85,1,0,11,0,0,0,8,0,0,0,232,194,127,1,128,5,24,0,0,16,0,0,0,8,2,0,1,192,2,32,0,0,16,0,0,0,248,255,255,1,64,5,192,0,0,32,0,0,0,0,0,0,0,160,2,0,1,0,32,0,0,0,0,0,0,0,80,1,0,6,0,64,0,0,0,0,0,0,0,176,0,20,8,0,64,0,0,0,0,0,0,0,88,0,106,48,0,64,0,0,0,0,0,0,0,172,0,212,65,0,64,0,0,0,0,0,0,0,84,0,168,131,1,64,0,0,0,0,0,0,0,42,0,80,15,2,64,0,0,0,0,0,0,0,21,0,160,30,252,95,0,0,0,0,0,0,0,11,0,64,125,0,48,0,0,0,0,0,0,0,1,0,160,250,1,16,0,0,0,0,0,0,0,0,0,0,245,7,8,0,0,0,0,0,0,0,0,0,0,234,255,7,0,0,0,0,0,0,0,0,0,0,208,95,0,0,0,0,0,0,0,0,0,0,0,160,158,0,0,0,0,0,0,0,0,0,0,0,64,149,0,0,0,0,0,0,0,0,0,0,0,0,154,0,0,0,0,0,0,0,0,0,0,0,0,16,1,0,0,0,0,0,0,0,0,0,0,0,8,1,0,0,0,0,0,0,0,0,0,0,0,8,1,0,0,0,0,0,0,0,0,0,0,0,8,2,0,0,0,0,0,0,0,0,0,0,0,8,2,0,0,0,0,0,0,0};
static const uint8_t* const z5_write_frames[] = {z5_write_frame};
static const Icon z5_write_icon = {.width = 97, .height = 61, .frame_count = 1, .frame_rate = 0, .frames = z5_write_frames};

static void busy_icon(App* app, const Icon* icon, const char* text, const NotificationSequence* led) {
    notification_message(app->notif, led); /* LED blinks until the result is shown (log_msg) */
    app->blink = true;
    popup_reset(app->popup);
    popup_set_icon(app->popup, 0, 0, icon);
    popup_set_header(app->popup, text, 86, 44, AlignCenter, AlignTop); /* right of the dolphin */
    go(app, VPopup);
    furi_delay_ms(100);
}

static const uint8_t z5_warn_frame[] = {0,8,2,0,0,0,0,8,2,0,0,0,0,8,1,252,31,0,0,8,1,3,96,0,0,136,192,0,128,1,0,136,32,0,0,2,0,0,16,0,0,4,0,72,8,0,0,8,0,0,8,0,0,16,0,0,4,0,0,16,0,60,4,0,30,32,0,194,2,0,33,32,0,1,3,128,94,64,0,1,12,64,187,64,0,15,48,64,185,64,0,62,192,64,191,64,0,124,0,64,191,128,0,248,1,128,94,128,0,240,3,128,47,128,0,224,7,64,16,128,0,192,15,0,32,128,0,128,31,0,0,128,0,254,97,0,0,128,0,121,128,1,96,128,0,129,255,31,144,128,0,1,0,0,144,128,1,2,0,0,144,128,1,12,0,0,144,128,3,112,0,0,16,15,3,128,255,0,144,16,7,0,63,0,112,32,7,0,1,0,72,32,14,0,1,0,72,40,14,0,1,0,72,40,30,0,1,0,136,39,28,0,1,0,16,71,28,0,1,0,32,64,28,128,0,0,64,128,24,128,0,0,64,128,24,128,0,0,64,0,25,128,0,0,64,0,17,128,0,0,64,0,18};
static const uint8_t* const z5_warn_frames[] = {z5_warn_frame};
static const Icon z5_warn_icon = {.width = 45, .height = 42, .frame_count = 1, .frame_rate = 0, .frames = z5_warn_frames};

/* warning screen: header and word-wrapped text on the left, the warning dolphin on the right */
static void draw_wrapped(Canvas* c, const char* text, int x, int y, int w) {
    char t[160];
    snprintf(t, sizeof(t), "%s", text);
    for(char* k = t; *k; k++)
        if(*k == '\n') *k = ' '; /* line breaks in the message strings are fitted to another layout: re-wrap */
    size_t len = strlen(t), i = 0;
    for(int lines = 0; i < len && lines < 5; lines++, y += 10) {
        while(i < len && t[i] == ' ') i++;
        char ln[40];
        size_t j = i, sp = 0;
        for(; j < len && j - i < sizeof(ln) - 1; j++) {
            ln[j - i] = t[j];
            ln[j - i + 1] = 0;
            if(canvas_string_width(c, ln) > w) break;
            if(t[j] == ' ') sp = j;
        }
        size_t end = j;
        if(j < len && canvas_string_width(c, ln) > w) end = sp > i ? sp : (j > i ? j : i + 1);
        snprintf(ln, sizeof(ln), "%.*s", (int)(end - i), t + i);
        canvas_draw_str(c, x, y, ln);
        i = end;
    }
}

static void warn_draw(Canvas* c, void* model) {
    App* app = *(App**)model;
    canvas_clear(c);
    canvas_draw_icon(c, 83, 22, &z5_warn_icon);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 2, 1, AlignLeft, AlignTop, app->hdr);
    canvas_set_font(c, FontSecondary);
    draw_wrapped(c, app->msg, 2, 22, 78);
}

static bool warn_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->type == InputTypeShort && (e->key == InputKeyOk || e->key == InputKeyLeft || e->key == InputKeyRight)) {
        popup_ok(app);
        return true;
    }
    return false; /* Back must reach the navigation callback */
}

static const NotificationSequence seq_led_yellow = {&message_red_255, &message_green_255, &message_blue_0, NULL};

/* duplicate while reading keys: two low beeps with a double buzz */
static const NotificationSequence seq_key_dup = {
    &message_vibro_on,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    NULL,
};
/* error: two low beeps, nothing else (no LED, no buzz) */
static const NotificationSequence seq_error_sound = {
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence* warn_seq; /* NULL = stock error + audiovisual alert; set for one message to override */

static void log_msg(App* app, const char* header) {
    if(app->blink && app->tick == TickNone) { /* end of a blocking operation; touch owns the LED itself */
        notification_message(app->notif, &sequence_blink_stop);
        app->blink = false;
    }
    File* f = storage_file_alloc(app->storage); /* session log, readable over CLI for remote tests */
    if(storage_file_open(f, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(f, header, strlen(header));
        storage_file_write(f, ": ", 2);
        storage_file_write(f, app->msg, strlen(app->msg));
        storage_file_write(f, "\n", 1);
    }
    storage_file_free(f);
}

/* success notice: short buzz, then the key base opens by itself */
/* 'Done!' screen with the thumbs-up dolphin; ok = view shown afterwards */
static void show_dolphin(App* app, const char* text, uint32_t ok) {
    if(text != app->msg) snprintf(app->msg, sizeof(app->msg), "%s", text); /* callers may pass msg itself */
    log_msg(app, "Done");
    notification_message(app->notif, &sequence_success); /* sound + vibro + green LED */
    app->ok = ok;
    popup_reset(app->popup);
    popup_set_icon(app->popup, 0, 6, &dolphin_ok);
    popup_set_header(app->popup, "Done!", 104, 24, AlignCenter, AlignBottom);
    popup_set_text(app->popup, app->msg, 104, 28, AlignCenter, AlignTop);
    popup_set_callback(app->popup, popup_ok);
    popup_set_context(app->popup, app);
    popup_set_timeout(app->popup, 1500);
    popup_enable_timeout(app->popup);
    go(app, VPopup);
}

static bool confirm(App* app, const char* header, const char* text) {
    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(m, header, 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, "No", NULL, "Yes");
    bool yes = dialog_message_show(app->dialogs, m) == DialogMessageButtonRight;
    dialog_message_free(m);
    return yes;
}

static void show_msg_ok(App* app, const char* header, const char* text, uint32_t ok) {
    app->back = app->cur;
    app->ok = ok == VIEW_NONE ? app->cur : ok;
    if(text != app->msg) snprintf(app->msg, sizeof(app->msg), "%s", text);
    snprintf(app->hdr, sizeof(app->hdr), "%s", header);
    log_msg(app, header);
    if(warn_seq) {
        notification_message(app->notif, warn_seq);
        warn_seq = NULL;
    } else {
        notification_message(app->notif, &seq_error_sound);
    }
    with_view_model(app->warn, App * *m, { UNUSED(m); }, true);
    go(app, VWarn);
}

static void show_msg(App* app, const char* header, const char* text) {
    show_msg_ok(app, header, text, VIEW_NONE);
}

/* menu screens: header (key count, open time, unsaved pencil) + list */
typedef struct {
    App* app;
    const char* const* items;
    int n, sel;
    void (*cb)(void*, uint32_t);
    uint32_t id;
} Menu;
static Menu menus[3];
static const uint8_t type_order[] = {Z5rAttrNormal, Z5rAttrMaster, Z5rAttrBlocking, Z5rAttrInactive};

/* position of a type in type_order (the order every list shows) */
static int type_pos(uint8_t attr) {
    int i = 0;
    while(i < 3 && type_order[i] != attr) i++;
    return i;
}

/* next (+1) / previous (-1) key type for keys added from now on */
static void step_type(App* app, int dir) {
    app->tattr = type_order[(type_pos(app->tattr) + (dir > 0 ? 1 : 3)) % 4];
}

static void main_labels(App* app) {
    const char* what = app->src == 1 ? "from controller" : app->src == 3 ? "to controller" : app->db->count ? "(new, unsaved)" : "(empty)";
    if(app->src == 2) what = app->srcname; /* cut to the screen width in main_draw */
    snprintf(app->label, sizeof(app->label), "Base: %s >", what);
    with_view_model(app->main, App * *m, { UNUSED(m); }, true);
    with_view_model(app->ctl, Menu * *m, { UNUSED(m); }, true);
    with_view_model(app->add, Menu * *m, { UNUSED(m); }, true);
}

/* 5V for the controller's EEPROM (header pin 5), switched on the first time it is needed; off on exit */
static void power_on(App* app) {
    if(app->power5v) return;
    z5r_link_power(true);
    app->power5v = true;
    furi_delay_ms(150);
}

/* ---------- controller operations ---------- */

static bool save_file(App* app, const char* path) {
    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(f, app->db->mem, app->db->mem_size) == app->db->mem_size;
    storage_file_free(f);
    return ok;
}

static bool backup_controller(App* app, uint16_t size, const char* path) {
    uint8_t* buf = malloc(size); /* transient: a static buffer would sit in the heap for the whole session */
    bool ok = true;
    for(uint16_t a = 0; ok && a < size; a += 256) ok = z5r_link_read(a, buf + a, 256);
    if(ok) {
        File* f = storage_file_alloc(app->storage);
        ok = storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
             storage_file_write(f, buf, size) == size;
        storage_file_free(f);
    }
    free(buf);
    return ok;
}

enum { OpRead, OpWrite, OpErase };

/* one place that turns a controller result into a message */
static void ctl_fail(App* app, int op, CtlResult r, const CtlInfo* ci) {
    char* m = app->msg;
    size_t n = sizeof(app->msg);
    switch(r) {
    case CtlNoLink:
        show_msg(app, "No controller", "No answer on I2C.\nCheck wiring / power\n(see Info)");
        return;
    case CtlBadCode:
        if(op == OpRead) {
            snprintf(m, n, "Unknown memory code 0x%02X\nTry Erase first", ci->code);
            show_msg(app, "Read failed", m);
        } else if(op == OpWrite) {
            show_msg(app, "Write refused", "Controller memory\nnot formatted.\nUse Erase first.");
        } else {
            snprintf(m, n, "Unknown memory code 0x%02X.\nNot a plain Z-5R?", ci->code);
            show_msg(app, "Erase refused", m);
        }
        return;
    case CtlIo:
        show_msg(app, "Read failed", "I2C error");
        return;
    case CtlNoFit: {
        unsigned cap = z5r_capacity(ci->size);
        snprintf(m, n, "%u keys, controller\nholds %u (%u KB).\nRemove %u keys.", app->db->count, cap, ci->size / 1024, app->db->count - cap);
        show_msg(app, "Does not fit", m);
        return;
    }
    case CtlPageFail:
        if(op == OpErase) {
            show_msg(app, "Erase FAILED", "I2C write error");
        } else {
            snprintf(m, n, "Page 0x%04X failed.\nController may hold a\nhalf-written base", ci->addr);
            show_msg(app, "Write FAILED", m);
        }
        return;
    case CtlLegacy:
        show_msg(app, "Write cancelled", "Old key layout kept");
        return;
    default:
        show_msg(app, "Write FAILED", "Commit page failed");
    }
}

static void do_read(App* app) {
    busy_icon(app, &z5_read_icon, "Reading", &sequence_blink_start_cyan);
    power_on(app);
    CtlInfo ci;
    CtlResult r = z5r_ctl_read(app->db, &ci);
    if(r) {
        ctl_fail(app, OpRead, r, &ci);
        return;
    }
    app->src = 1;
    app->bak = save_file(app, DATA_DIR "/controller_last" EXT); /* db->mem is still the raw image just read; replaced on every read */
    app->modified = false;
    app->base0 = app->db->count;
    main_labels(app);
    snprintf(app->msg, sizeof(app->msg), "Read\n%u keys", app->db->count);
    menus[1].sel = 0; /* the base screen opens on its first item */
    show_dolphin(app, app->msg, VCtl);
}

static void write_run(App* app) {
    busy_icon(app, &z5_write_icon, "Writing", &sequence_blink_start_magenta);
    power_on(app);
    CtlInfo ci;
    CtlResult r = z5r_ctl_write(app->db, &ci, false);
    if(r == CtlLegacy && confirm(app, "Old format!", "Controller uses the old\nkey layout. Write the\nnew one anyway?"))
        r = z5r_ctl_write(app->db, &ci, true);
    if(r) {
        ctl_fail(app, OpWrite, r, &ci);
        return;
    }
    app->modified = false;
    app->base0 = app->db->count;
    if(app->src != 2) app->src = 3;
    main_labels(app);
    snprintf(app->msg, sizeof(app->msg), "Written\n%u keys", app->db->count);
    show_dolphin(app, app->msg, app->cur);
}

static void erase_run(App* app) {
    busy_icon(app, &z5_write_icon, "Erasing", &sequence_blink_start_red);
    power_on(app);
    CtlInfo ci;
    CtlResult r = z5r_ctl_erase(&ci);
    if(r) {
        ctl_fail(app, OpErase, r, &ci);
        return;
    }
    show_dolphin(app, "Erased", app->cur);
}

enum { PendWrite = 1, PendErase };
static uint8_t pending; /* operation waiting for the backup file name */

static void backup_done(void* ctx) {
    App* app = ctx;
    for(char* c = app->name; *c; c++)
        if(*c == '/' || *c == '\\') *c = '_'; /* stay inside DATA_DIR */
    if(!app->name[0]) snprintf(app->name, sizeof(app->name), "controller");
    busy_icon(app, &z5_read_icon, "Backup", &sequence_blink_start_cyan);
    power_on(app);
    uint8_t p0[Z5R_PAGE];
    CtlInfo ci;
    CtlResult pr = z5r_ctl_probe(p0, &ci);
    if(pr) {
        ctl_fail(app, OpRead, pr, &ci);
        return;
    }
    if(ci.size) { /* unknown memory code: nothing to back up, the operation itself decides */
        FuriString* path = furi_string_alloc_printf("%s/%s%s", DATA_DIR, app->name, EXT);
        bool ok = backup_controller(app, ci.size, furi_string_get_cstr(path));
        furi_string_free(path);
        if(!ok) {
            show_msg(app, "Backup failed", "SD card? Nothing was\nchanged");
            return;
        }
    }
    if(pending == PendWrite) write_run(app); else erase_run(app);
}

/* before Write / Erase: offer a backup of the controller, named by the user */
static void ask_backup(App* app, uint8_t op) {
    pending = op;
    if((op == PendWrite && app->src == 1 && app->bak) || !confirm(app, "Backup?", "Save the controller\nmemory to a file first?")) {
        if(op == PendWrite) write_run(app); else erase_run(app);
        return;
    }
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt); /* a new name every time: the previous backup is never replaced */
    snprintf(app->name, sizeof(app->name), "controller_%04u%02u%02u_%02u%02u%02u", dt.year % 10000, dt.month % 100, dt.day % 100, dt.hour % 100, dt.minute % 100, dt.second % 100);
    text_input_set_header_text(app->text, "Backup file name");
    text_input_set_result_callback(app->text, backup_done, app, app->name, sizeof(app->name), false);
    text_back = VCtl;
    go(app, VText);
}

static void do_write(App* app) {
    snprintf(
        app->msg,
        sizeof(app->msg),
        "Write %u keys,\nopen time %us,\nto the controller?",
        app->db->count,
        (unsigned)CODE_TO_SEC((int32_t)z5r_open_time_code(app->db)));
    if(confirm(app, "Write?", app->msg)) ask_backup(app, PendWrite);
}

static void do_clear(App* app) {
    if(confirm(app, "Erase?", "Erase all keys in the\ncontroller?")) ask_backup(app, PendErase);
}

/* ---------- files ---------- */

static void do_open(App* app) {
    FuriString* path = furi_string_alloc_set(DATA_DIR);
    DialogsFileBrowserOptions opt;
    dialog_file_browser_set_basic_options(&opt, EXT, NULL);
    opt.base_path = DATA_DIR;
    if(dialog_file_browser_show(app->dialogs, path, path, &opt)) {
        File* f = storage_file_alloc(app->storage);
        uint8_t* tmp = malloc(Z5R_MEM_MAX); /* validate before touching the current db */
        size_t n = 0;
        if(storage_file_open(f, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING))
            n = storage_file_read(f, tmp, Z5R_MEM_MAX);
        storage_file_free(f);
        if((n == 4096 || n == 8192) && z5r_mem_size_from_code(tmp[5]) == n) {
            memcpy(app->db->mem, tmp, n);
            z5r_parse(app->db);
            app->src = 2;
            app->fdirty = false;
            const char* base = strrchr(furi_string_get_cstr(path), '/');
            snprintf(app->srcname, sizeof(app->srcname), "%s", base ? base + 1 : furi_string_get_cstr(path));
            char* dot = strrchr(app->srcname, '.');
            if(dot) *dot = 0;
            app->modified = false;
            app->base0 = app->db->count;
            keys_fill(app);
            snprintf(app->msg, sizeof(app->msg), "Loaded\n%u keys", app->db->count);
            menus[1].sel = 0; /* the base screen opens on its first item */
            show_dolphin(app, app->msg, VCtl);
        } else {
            show_msg(app, "Bad file", "Expect raw 4 or 8 KB\nZ-5R memory image (.bin)");
        }
        free(tmp);
        main_labels(app);
    }
    furi_string_free(path);
}

static void text_done(void* ctx) {
    App* app = ctx;
    for(char* c = app->name; *c; c++)
        if(*c == '/' || *c == '\\') *c = '_'; /* stay inside DATA_DIR */
    if(!app->name[0]) snprintf(app->name, sizeof(app->name), "base");
    FuriString* path = furi_string_alloc_printf("%s/%s%s", DATA_DIR, app->name, EXT);
    bool same = app->src == 2 && !strcmp(app->name, app->srcname);
    if(!same && storage_file_exists(app->storage, furi_string_get_cstr(path)) && !confirm(app, "Overwrite?", "A file with this name\nalready exists.")) {
        furi_string_free(path);
        return;
    }
    uint16_t cap = z5r_capacity(app->db->mem_size);
    if(app->db->count > cap) {
        snprintf(app->msg, sizeof(app->msg), "%u keys, image holds %u.\nRemove %u keys.", app->db->count, cap, app->db->count - cap);
        show_msg(app, "Does not fit", app->msg);
        furi_string_free(path);
        return;
    }
    z5r_build(app->db);
    app->db->mem[0] = 0x33;
    bool ok = save_file(app, furi_string_get_cstr(path));
    if(ok) {
        app->modified = false;
        app->src = 2;
        snprintf(app->srcname, sizeof(app->srcname), "%s", app->name);
        app->base0 = app->db->count;
    }
    main_labels(app);
    if(ok) {
        app->fdirty = false;
        show_dolphin(app, "Saved", VCtl);
    } else {
        show_msg(app, "Save failed", furi_string_get_cstr(path));
    }
    furi_string_free(path);
}

static void ask_name(App* app) {
    if(app->src == 2) /* base came from / went to a file: offer the same name, saving updates it */
        snprintf(app->name, sizeof(app->name), "%s", app->srcname);
    else
        snprintf(app->name, sizeof(app->name), "base_%u", app->db->count);
    text_input_set_header_text(app->text, "File name");
    text_input_set_result_callback(app->text, text_done, app, app->name, sizeof(app->name), false);
    text_back = VCtl;
    go(app, VText);
}

/* a base that exists only in RAM / controller, or has edits the file does not have */
static bool unsaved_base(const App* app) {
    return app->db->count && (app->src != 2 || app->fdirty);
}

/* lock open time: stored in the base header, goes to the controller with Write */
static void time_done(void* ctx, int32_t v) {
    App* app = ctx;
    if(CODE_TO_SEC((int32_t)z5r_open_time_code(app->db)) != v) { /* same value again: base stays unchanged */
        uint16_t code = SEC_TO_CODE(v);
        app->db->mem[3] = (uint8_t)(~code >> 8);
        app->db->mem[4] = (uint8_t)~code;
        app->modified = true;
        app->fdirty = true;
        main_labels(app);
    }
    go(app, VCtl);
}

/* ---------- keys ---------- */

static void keys_cb(void* ctx, uint32_t index);

static void krow(App* app, uint32_t id) {
    app->rid[app->nrows++] = id;
}

static bool key_match(const App* app, const Z5rKey* k) {
    if(app->fattr && k->attr != app->fattr) return false;
    if(!app->fcode[0]) return true;
    char s[13];
    z5r_key_str(k, s, sizeof(s));
    return strstr(s, app->fcode) != NULL; /* part of the code, anywhere in the 6/12 digits */
}

static void keys_fill(App* app) {
    app->nrows = 0;
    snprintf(
        app->flabel,
        sizeof(app->flabel),
        "Search: %s, %s",
        app->fcode[0] ? app->fcode : "any code",
        app->fattr ? z5r_attr_name(app->fattr) : "any type");
    krow(app, KEYS_FILTER);
    for(uint16_t i = 0; i < app->db->count; i++)
        if(key_match(app, &app->db->keys[i])) krow(app, i); /* drawn from the base in keys_draw, in columns */
    if(app->nrows == ROWS_FIRST) krow(app, KEYS_NONE);
    if(app->ksel >= app->nrows) app->ksel = app->nrows - 1;
    with_view_model(app->keys, App * *m, { UNUSED(m); }, true);
}

static void filter_sync(App* app) {
    variable_item_set_current_value_text(app->fitem[FCode], app->fcode[0] ? app->fcode : "any");
    variable_item_set_current_value_index(app->fitem[FType], app->fattr ? type_pos(app->fattr) + 1 : 0);
    variable_item_set_current_value_text(app->fitem[FType], app->fattr ? z5r_attr_name(app->fattr) : "any");
}

static void fcode_done(void* ctx) {
    App* app = ctx;
    char s[13];
    snprintf(s, sizeof(s), "%02X%02X%02X%02X%02X%02X", app->fbytes[0], app->fbytes[1], app->fbytes[2], app->fbytes[3], app->fbytes[4], app->fbytes[5]);
    const char* p = s;
    while(*p == '0') p++; /* leading zeros are padding: search is by part of the code */
    snprintf(app->fcode, sizeof(app->fcode), "%s", p);
    keys_fill(app);
    filter_sync(app);
    go(app, VFilter);
}

static void filter_type_cb(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t fi = variable_item_get_current_value_index(item); /* 0 any, then type_order */
    app->fattr = fi ? type_order[fi - 1] : 0;
    keys_fill(app);
    filter_sync(app);
}

static void filter_enter_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == FCode) {
        byte_input_set_header_text(app->bytes, "Code part (leading 0 ignored)");
        byte_input_set_result_callback(app->bytes, fcode_done, NULL, app, app->fbytes, 6);
        bytes_back = VFilter;
        go(app, VBytes);
    } else if(index == FReset) {
        memset(app->fbytes, 0, sizeof(app->fbytes));
        app->fattr = 0;
        app->fcode[0] = 0;
        keys_fill(app);
        filter_sync(app);
    }
}

static void key_cb(void* ctx, uint32_t index);

static void key_type_cb(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t a = type_order[variable_item_get_current_value_index(item) % 4];
    app->db->keys[app->sel].attr = a;
    variable_item_set_current_value_text(item, z5r_attr_name(a));
    app->modified = true;
    app->fdirty = true;
    keys_fill(app);
    main_labels(app);
}

static void keyhint_draw(Canvas* c, void* model) {
    App* app = *(App**)model;
    uint8_t i = variable_item_list_get_selected_item_index(app->key);
    static const char* const h[] = {NULL, "Edit", "Export", NULL}; /* by KAttr, KEdit, KExport, KDelete */
    const char* s = i < sizeof(app->kmap) ? h[app->kmap[i]] : NULL;
    canvas_set_font(c, FontSecondary);
    if(s) elements_button_center(c, s);
}

static void key_exp_cb(VariableItem* item) {
    bool em = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, em ? "EM4100" : "iButton");
}

static void key_fill(App* app) {
    Z5rKey* k = &app->db->keys[app->sel];
    char s[13];
    z5r_key_str(k, s, sizeof(s));
    variable_item_list_reset(app->key);
    uint8_t n = 0;
    /* one line, whole text in the label: no value column, so nothing scrolls */
    snprintf(app->klabel, sizeof(app->klabel), "#%u  %s", app->sel + 1, s);
    variable_item_list_add(app->key, app->klabel, 0, NULL, app);
    app->kmap[n++] = KEdit; /* OK on the number = edit it */
    app->kitem = variable_item_list_add(app->key, "Type", 4, key_type_cb, app);
    variable_item_set_current_value_index(app->kitem, type_pos(k->attr));
    variable_item_set_current_value_text(app->kitem, z5r_attr_name(k->attr));
    app->kmap[n++] = KAttr;
    bool em = k->len == 6 && !k->b[5]; /* such a key may also be an EM4100 */
    app->kexp = variable_item_list_add(app->key, "Export", em ? 2 : 1, key_exp_cb, app);
    variable_item_set_current_value_text(app->kexp, k->len == 3 ? "RFID" : "iButton");
    app->kmap[n++] = KExport;
    variable_item_list_add(app->key, "Delete", 0, NULL, app);
    app->kmap[n++] = KDelete;
}

static void key_enter_cb(void* ctx, uint32_t row) {
    App* app = ctx;
    key_cb(app, app->kmap[row]);
}

static void keys_draw(Canvas* c, void* model) {
    App* app = *(App**)model;
    canvas_clear(c);
    canvas_set_font(c, FontSecondary);
    int top = app->ksel > 1 ? app->ksel - 1 : 0;
    if(top > app->nrows - 4) top = app->nrows > 4 ? app->nrows - 4 : 0;
    for(int i = 0; i < 4 && top + i < app->nrows; i++) {
        int y = i * 12;
        bool sel = top + i == app->ksel;
        canvas_set_color(c, ColorBlack);
        if(sel) canvas_draw_box(c, 0, y, 123, 12);
        canvas_set_color(c, sel ? ColorWhite : ColorBlack);
        uint32_t id = app->rid[top + i];
        if(id < app->db->count) { /* key row: fixed columns number | code | type */
            const Z5rKey* k = &app->db->keys[id];
            char n[6], s[13];
            snprintf(n, sizeof(n), "%04u", (unsigned)id + 1);
            z5r_key_str(k, s, sizeof(s));
            static const char* const abbr[] = {"?", "N", "B", "M", "I"};
            canvas_draw_str(c, 1, y + 9, n);
            canvas_draw_str(c, 27, y + 9, s);
            canvas_draw_str(c, 111, y + 9, abbr[k->attr <= 4 ? k->attr : 0]);
        } else {
            canvas_draw_str(c, 2, y + 9, id == KEYS_FILTER ? app->flabel : app->db->count ? "(no matches)" : "(empty)");
        }
    }
    canvas_set_color(c, ColorBlack);
    elements_scrollbar_pos(c, 127, 0, 48, app->ksel, app->nrows);
    if(app->rid[app->ksel] < app->db->count) elements_button_center(c, "Edit");
}


static bool keys_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if((e->type == InputTypeShort || e->type == InputTypeRepeat) &&
       (e->key == InputKeyUp || e->key == InputKeyDown)) {
        app->ksel = e->key == InputKeyUp ? (app->ksel ? app->ksel - 1 : app->nrows - 1) :
                                           (app->ksel + 1 < app->nrows ? app->ksel + 1 : 0);
        with_view_model(app->keys, App * *m, { UNUSED(m); }, true);
        return true;
    }
    if((e->type == InputTypeShort || e->type == InputTypeRepeat) &&
       (e->key == InputKeyLeft || e->key == InputKeyRight)) { /* page-sized jump through a long list */
        int n = app->ksel + (e->key == InputKeyRight ? 12 : -12);
        app->ksel = n < 0 ? 0 : n >= app->nrows ? app->nrows - 1 : n;
        with_view_model(app->keys, App * *m, { UNUSED(m); }, true);
        return true;
    }
    if(e->type == InputTypeShort && e->key == InputKeyOk) {
        keys_cb(app, app->rid[app->ksel]);
        return true;
    }
    return false;
}

static void keys_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == KEYS_NONE) return;
    if(index == KEYS_FILTER) {
        filter_sync(app);
        go(app, VFilter);
        return;
    }
    app->sel = index;
    key_fill(app);
    go(app, VKey);
}

static void open_bytes(App* app, ByteMode mode, const Z5rKey* k) {
    app->byte_mode = mode;
    memset(app->edit, 0, sizeof(app->edit));
    uint8_t len = k ? k->len : 6;
    for(int i = 0; k && i < len; i++) app->edit[i] = k->b[len - 1 - i];
    bytes_back = mode == ByteAdd ? VAdd : VKey;
    byte_input_set_header_text(app->bytes, len == 3 ? "Short key, MSB first" : "Key MSB first");
    go(app, VBytes);
}

static void bytes_done(void* ctx);

static void key_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    Z5rKey* k = &app->db->keys[app->sel];
    switch(index) {
    case KAttr: /* changed in place by key_type_cb */
        break;
    case KEdit:
        open_bytes(app, ByteEdit, k);
        byte_input_set_result_callback(app->bytes, bytes_done, NULL, app, app->edit, k->len);
        break;
    case KExport: {
        bool em = variable_item_get_current_value_index(app->kexp) == 1;
        bool dallas = k->len == 6 && !em;
        char s[13];
        z5r_key_str(k, s, sizeof(s));
        snprintf(app->msg, sizeof(app->msg), "Z5R_%s.%s\nto /ext/%s", s, dallas ? "ibtn" : "rfid", dallas ? "ibutton" : "lfrfid");
        if(!confirm(app, "Export key?", app->msg)) break;
        char path[64];
        bool ok = z5r_key_export(app->storage, k, em, path, sizeof(path));
        if(ok) {
            log_msg(app, path);
            show_dolphin(app, "Exported", app->cur);
        } else {
            show_msg(app, "Export failed", "SD card?");
        }
        break;
    }
    case KDelete:
        snprintf(app->msg, sizeof(app->msg), "#%u will be removed\nfrom the base", app->sel + 1);
        if(!confirm(app, "Delete key?", app->msg)) break;
        memmove(k, k + 1, (app->db->count - app->sel - 1) * sizeof(Z5rKey));
        app->db->count--;
        app->modified = true;
        app->fdirty = true;
        keys_fill(app);
        main_labels(app);
        go(app, VKeys);
        break;
    }
}

/* 0 = added, 1 = already in base (*dup = its index), 2 = base full, 3 = in base with another type, type overwritten */
static int try_add(App* app, const Z5rKey* k, int* dup) {
    *dup = z5r_find(app->db, k);
    if(*dup >= 0) {
        Z5rKey* e = &app->db->keys[*dup];
        if(e->attr == k->attr) return 1;
        e->attr = k->attr;
        app->modified = true;
        app->fdirty = true;
        keys_fill(app);
        return 3;
    }
    if(app->db->count >= z5r_capacity(app->db->mem_size)) return 2; /* the controller could not hold more */
    app->db->keys[app->db->count++] = *k;
    app->modified = true;
    app->fdirty = true;
    keys_fill(app);
    main_labels(app);
    return 0;
}

static void add_key(App* app, const Z5rKey* k) {
    int dup;
    int r = try_add(app, k, &dup);
    if(r == 1) {
        snprintf(app->msg, sizeof(app->msg), "Already in base as #%d", dup + 1);
        warn_seq = &seq_key_dup; /* duplicate: double buzz everywhere */
        show_msg(app, "Duplicate", app->msg);
    } else if(r == 3) {
        snprintf(app->msg, sizeof(app->msg), "#%d set to %s", dup + 1, z5r_attr_name(k->attr));
        show_msg(app, "Type updated", app->msg);
    } else if(r == 2) {
        show_msg(app, "Base full", "No free slots");
    } else {
        snprintf(app->msg, sizeof(app->msg), "Added\n#%u", app->db->count);
        show_dolphin(app, app->msg, app->cur);
    }
}

static void bytes_done(void* ctx) {
    App* app = ctx;
    Z5rKey k = {.len = 6, .attr = app->tattr};
    if(app->byte_mode == ByteEdit) k = app->db->keys[app->sel];
    for(int i = 0; i < k.len; i++) k.b[i] = app->edit[k.len - 1 - i];
    if(k.len == 6) k.b[5] &= 0x0F; /* top nibble of byte 6 holds flags in controller memory: the controller drops it too */
    if(app->byte_mode == ByteAdd) {
        add_key(app, &k);
        return;
    }
    int dup = z5r_find(app->db, &k);
    if(dup >= 0 && dup != app->sel) {
        warn_seq = &seq_key_dup;
        show_msg(app, "Duplicate", "Number already in base");
        return;
    }
    app->db->keys[app->sel] = k;
    app->modified = true;
    app->fdirty = true;
    keys_fill(app);
    key_fill(app);
    go(app, VKey);
}

/* ---------- key reading ticker ---------- */

static void blink(App* app, const NotificationSequence* seq) {
    if(app->led) notification_message(app->notif, &sequence_reset_rgb); /* before the blink starts, not on top of it */
    app->led = false;
    notification_message(app->notif, seq);
    app->blink = true;
}

/* GUI-thread tick: a FuriTimer callback can block on the event queue while Back waits in furi_timer_stop */
static void start_tick(App* app, TickMode m) {
    app->tick = m;
}

static void stop_tick(App* app) {
    if(app->tick == TickTouch) notification_message(app->notif, &sequence_display_backlight_enforce_auto);
    if(app->iwork) {
        ibutton_worker_stop(app->iwork);
        ibutton_worker_stop_thread(app->iwork);
        ibutton_worker_free(app->iwork);
        ibutton_key_free(app->ikey);
        ibutton_protocols_free(app->iprot);
        app->iwork = NULL;
    }
    if(app->rfid) {
        lfrfid_worker_stop(app->rfid);
        lfrfid_worker_stop_thread(app->rfid);
        lfrfid_worker_free(app->rfid);
        protocol_dict_free(app->rdict);
        app->rfid = NULL;
    }
    if(app->blink) {
        notification_message(app->notif, &sequence_blink_stop); /* LED follows the read state */
        app->blink = false;
    }
    app->tick = TickNone;
    main_labels(app);
}

/* ---- touch reading: stays on screen, adds every new key, buzzes on success ---- */

static void touch_redraw(App* app) {
    with_view_model(app->touch, App * *m, { UNUSED(m); }, true);
}

static const uint8_t ico_key[] = {0x0E,0x00,0xF1,0x01,0x51,0x01,0x0E,0x00};
static const uint8_t ico_lock_open[] = {0x1C,0x22,0x02,0x7F,0x7F,0x77,0x7F,0x3E};
static const uint8_t ico_ibtn[] = {0x00,0x00,0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00,0x00,0xC0,0xC0,0x00,0x00,0x00,0x00,0x00,0x30,0x80,0x00,0x00,0x00,0x00,0x00,0x0C,0x00,0x01,0x00,0x00,0x00,0x00,0x02,0x00,0x01,0x00,0x00,0x00,0x80,0x01,0x00,0x01,0x00,0x00,0x00,0x60,0x00,0x00,0x01,0x00,0x00,0x00,0x10,0x00,0x80,0x00,0x00,0x00,0x00,0x0C,0x00,0x40,0x00,0x00,0x00,0x00,0x02,0x00,0x20,0x00,0x00,0x00,0x00,0x01,0x00,0x10,0x00,0x00,0x00,0xC0,0x00,0x00,0x08,0x00,0x00,0x00,0x30,0x00,0x00,0x04,0x00,0x00,0x00,0x08,0x00,0x00,0x02,0x00,0x00,0x00,0x06,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0xC0,0x00,0x00,0x00,0xF0,0x0F,0x00,0x20,0x00,0x00,0x00,0x0C,0x30,0x00,0x10,0x00,0x00,0x00,0x03,0x40,0x00,0x0C,0x00,0x00,0x80,0x00,0x80,0x00,0x02,0x00,0x00,0x40,0x00,0x80,0x01,0x01,0x00,0x00,0x20,0x00,0x80,0x83,0x00,0x00,0x00,0x10,0x00,0x80,0x47,0x00,0x00,0x00,0x08,0x00,0x80,0x4F,0x00,0x00,0x00,0x04,0x00,0xC0,0x4F,0x00,0x00,0x00,0x04,0x00,0x40,0x4F,0x00,0x00,0x00,0x02,0x00,0xC0,0x4E,0x00,0x00,0x00,0x02,0x00,0x60,0x45,0x00,0x00,0x00,0x01,0x00,0xA0,0x46,0x00,0x00,0x00,0x01,0x00,0x30,0x45,0x00,0x00,0x00,0x01,0x00,0x98,0x42,0x00,0x00,0x00,0x01,0x00,0x0C,0x22,0x00,0x00,0x00,0x01,0x00,0x02,0x21,0x00,0x00,0x00,0x01,0x00,0x81,0x11,0x00,0x00,0x00,0x03,0xC0,0xC0,0x10,0x00,0x00,0x00,0x06,0x78,0x40,0x08,0x00,0x00,0x00,0xFC,0x0F,0x20,0x04,0x00,0x00,0x00,0x78,0x55,0x11,0x02,0x00,0x00,0x00,0xF0,0x0A,0x0E,0x01,0x00,0x00,0x00,0xE0,0xD5,0x83,0x00,0x00,0x00,0x00,0xC0,0x7F,0x40,0x00,0x00,0x00,0x00,0x80,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x0F,0x0F,0x00,0x00,0x00,0x00,0x00,0xF0,0x00,0x00,0x00,0x00,0x00};
static const uint8_t ico_nfc[] = {0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0xC0,0xFF,0x03,0x00,0x00,0x00,0xE0,0x00,0x07,0x00,0x00,0x00,0x30,0x00,0x0E,0x00,0x00,0x00,0x38,0x00,0x1C,0x00,0x00,0x00,0x18,0x00,0x18,0x00,0x00,0x00,0x0C,0x00,0x30,0x00,0x00,0x00,0x0C,0x00,0x30,0x00,0x00,0xE0,0x0F,0x00,0x30,0x00,0x00,0xFE,0x7F,0x00,0x30,0x00,0xC0,0xFF,0xFF,0x00,0x30,0x00,0xF0,0xFF,0xFF,0x01,0x30,0x00,0xFE,0xFF,0xFF,0x03,0x30,0x80,0xFF,0xFF,0xEF,0x07,0x18,0xC0,0xFF,0xFF,0x97,0x07,0x18,0xE0,0xFF,0xFF,0xB3,0x0F,0x0C,0xF0,0xFF,0xFF,0x77,0x0F,0x06,0xF8,0xFF,0xFF,0xEF,0x8D,0x03,0xFC,0x07,0xF8,0xFF,0xFF,0x01,0xFE,0x01,0xE0,0xFF,0x7F,0x00,0xFE,0x00,0xC0,0xFF,0x0F,0x00,0x7E,0x00,0x80,0xFF,0x0F,0x00,0x3F,0x00,0x80,0xFF,0x07,0x00,0x3F,0x00,0x00,0xFF,0x07,0x00,0x3F,0x00,0x00,0xFF,0x07,0x00,0x1F,0x00,0x00,0xFF,0x03,0x00,0x1F,0x00,0x00,0xFE,0x03,0x00,0x1F,0x00,0x00,0xFE,0x01,0x00,0x1F,0x00,0x00,0xFE,0x01,0x00,0x1F,0x00,0x00,0xFF,0x00,0x00,0x3F,0x00,0x00,0xFF,0x00,0x00,0x3F,0x00,0x00,0x7F,0x00,0x00,0x7E,0x00,0x80,0x3F,0x00,0x00,0x7E,0x00,0xC0,0x3F,0x00,0x00,0xFC,0x00,0xE0,0x1F,0x00,0x00,0xF8,0x03,0xF0,0x0F,0x00,0x00,0xF8,0x0F,0xFC,0x0F,0x00,0x00,0xF0,0xFF,0xFF,0x07,0x00,0x00,0xE0,0xFF,0xFF,0x03,0x00,0x00,0xC0,0xFF,0xFF,0x01,0x00,0x00,0x00,0xFF,0x7F,0x00,0x00,0x00,0x00,0xFC,0x1F,0x00,0x00,0x00};

/* top bar: key count, then open time if show_time; blinking "unsaved" */
static void draw_header(Canvas* c, App* app, bool show_time) {
    char t[16];
    canvas_draw_xbm(c, 2, 4, 9, 4, ico_key);
    canvas_set_font(c, FontPrimary);
    snprintf(t, sizeof(t), "%u", app->db->count);
    canvas_draw_str(c, 15, 10, t);
    int x = 15 + canvas_string_width(c, t) + 8;
    int d = (int)app->db->count - app->base0;
    if(d) {
        snprintf(t, sizeof(t), "%+d", d); /* keys added (+) or removed (-) against the base as loaded */
        canvas_draw_str(c, x, 10, t);
        x += canvas_string_width(c, t) + 4;
    }
    if(app->modified && !app->mphase) { /* blinking, right after the change */
        canvas_set_font(c, FontSecondary);
        canvas_draw_str(c, x, 10, "unsaved");
        canvas_set_font(c, FontPrimary);
    }
    if(show_time) { /* right edge */
        uint32_t sec = CODE_TO_SEC((int32_t)z5r_open_time_code(app->db)); /* blank base: factory 3 s */
        snprintf(t, sizeof(t), "%lus", (unsigned long)sec);
        canvas_draw_str_aligned(c, 127, 10, AlignRight, AlignBottom, t);
        canvas_draw_xbm(c, 127 - canvas_string_width(c, t) - 11, 2, 7, 8, ico_lock_open);
    }
    canvas_draw_line(c, 0, 14, 127, 14);
}

static void touch_draw(Canvas* c, void* model) {
    App* app = *(App**)model;
    char t[32];
    canvas_clear(c);
    draw_header(c, app, false);
    if(app->touch_mode == TmRfid)
        canvas_draw_xbm(c, 82, 20, 46, 42, ico_nfc);
    else
        canvas_draw_xbm(c, 78, 18, 49, 44, ico_ibtn);
    canvas_set_font(c, FontSecondary);
    if(app->tstate == 2)
        snprintf(t, sizeof(t), "Duplicate #%u", app->tdup);
    else if(app->tstate == 5)
        snprintf(t, sizeof(t), "Type set #%u", app->tdup);
    else
        snprintf(t, sizeof(t), app->tstate == 3 ? "Base full" : app->tstate == 4 ? "Not a Dallas key" : app->tstate == 1 ? "Added #%u" : "Waiting...", app->db->count);
    canvas_draw_str(c, 2, 24, t); /* 9 px rows: five lines fit under the header */
    canvas_draw_str(c, 1, 33, app->tlast[0] ? app->tlast : "-");
    snprintf(t, sizeof(t), "Type: <%s>", z5r_attr_name(app->tattr));
    canvas_draw_str(c, 2, 42, t);
    canvas_draw_str(c, 2, 51, "Hold </>: type");
    canvas_draw_str(c, 2, 60, "Hold Back: exit");
}

static bool touch_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->key == InputKeyBack) {
        if(e->type == InputTypeLong) {
            stop_tick(app);
            go(app, VAdd);
        }
        return true; /* short Back does nothing: a stray press must not drop the reading */
    }
    if(e->type == InputTypeLong && (e->key == InputKeyLeft || e->key == InputKeyRight)) { /* long: a stray press must not switch to Master */
        step_type(app, e->key == InputKeyRight ? 1 : -1);
        touch_redraw(app);
    }
    return true;
}

static void rfid_cb(LFRFIDWorkerReadResult result, ProtocolId proto, void* ctx) {
    App* app = ctx;
    if(result != LFRFIDWorkerReadDone) return;
    size_t n = protocol_dict_get_data_size(app->rdict, proto);
    if(n > sizeof(app->rfid_data)) n = sizeof(app->rfid_data);
    protocol_dict_get_data(app->rdict, proto, app->rfid_data, n);
    app->rfid_proto = proto;
    app->rfid_hit = true;
}

static void ibtn_cb(void* ctx) {
    ((App*)ctx)->ihit = true;
}

static void start_touch(App* app, uint8_t mode) {
    app->touch_mode = mode;
    app->tadded = 0;
    app->tstate = 0;
    app->tlast[0] = 0;
    app->ttime = 0;
    app->rfid_hit = false;
    app->ihit = false;
    memset(&app->tkey, 0, sizeof(app->tkey));
    if(mode & TmIbtn) {
        app->iprot = ibutton_protocols_alloc();
        app->iwork = ibutton_worker_alloc(app->iprot);
        app->ikey = ibutton_key_alloc(ibutton_protocols_get_max_data_size(app->iprot));
        ibutton_worker_start_thread(app->iwork);
        ibutton_worker_read_set_callback(app->iwork, ibtn_cb, app);
        ibutton_worker_read_start(app->iwork, app->ikey);
    }
    if(mode & TmRfid) {
        app->rdict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        app->rfid = lfrfid_worker_alloc(app->rdict);
        lfrfid_worker_start_thread(app->rfid);
        lfrfid_worker_read_start(app->rfid, LFRFIDWorkerReadTypeAuto, rfid_cb, app);
        blink(app, &sequence_blink_start_cyan); /* as in the stock apps: iButton yellow, RFID cyan */
    } else {
        blink(app, &sequence_blink_start_yellow);
    }
    notification_message(app->notif, &sequence_display_backlight_enforce_on); /* screen stays lit while reading */
    start_tick(app, TickTouch);
    go(app, VTouch);
}

static void touch_got(App* app, const Z5rKey* key) {
    uint32_t now = furi_get_tick();
    bool held = app->ttime && !memcmp(&app->tkey, key, sizeof(*key)) &&
                now - app->ttime < furi_ms_to_ticks(app->touch_mode & TmRfid ? TOUCH_GAP_RFID_MS : TOUCH_GAP_MS);
    app->ttime = now;
    app->tkey = *key;
    if(held) return; /* still on the reader */
    Z5rKey k = *key;
    k.attr = app->tattr;
    int dup;
    int r = try_add(app, &k, &dup);
    z5r_key_str(&k, app->tlast, sizeof(app->tlast));
    app->tstate = r == 0 ? 1 : r == 1 ? 2 : r == 3 ? 5 : 3;
    app->tdup = dup + 1;
    if(r == 3) {
        notification_message(app->notif, &sequence_success); /* type overwritten */
    } else if(r == 0) {
        app->tadded++;
        notification_message(app->notif, &sequence_success); /* same as the Done screen */
        blink(app, app->touch_mode == TmRfid ? &sequence_blink_start_cyan : &sequence_blink_start_yellow);
    } else if(r == 1) {
        notification_message(app->notif, &seq_key_dup); /* already in the base */
    } else {
        notification_message(app->notif, &sequence_error); /* base full */
    }
    touch_redraw(app);
}

static void tick_cb(void* ctx) {
    App* app = ctx;
    uint32_t mq = furi_get_tick() / 250;
    if(app->src == 2 && mq != app->mq) { /* long file name scrolls in the selected menu item */
        app->mq = mq;
        with_view_model(app->main, App * *m, { UNUSED(m); }, true);
    }
    bool ph = (furi_get_tick() / 1200) & 1; /* blink the unsaved-changes text */
    if(app->modified && ph != app->mphase) {
        app->mphase = ph;
        main_labels(app);
        if(app->tick == TickTouch) touch_redraw(app);
    }
    bool led = app->modified && !app->mphase && !app->blink; /* the read-in-progress blink owns the LED */
    if(led != app->led) {
        app->led = led;
        notification_message(app->notif, led ? &seq_led_yellow : &sequence_reset_rgb);
    }
    if(app->tick == TickTouch) {
        Z5rKey k = {.len = 6};
        bool got = false;
        if(app->touch_mode & TmRfid) {
            if(app->rfid_hit) {
                app->rfid_hit = false;
                uint8_t* d = app->rfid_data;
                if(app->rfid_proto == LFRFIDProtocolEM4100) { /* 5 bytes, stored as a 6-byte key */
                    for(int i = 0; i < 5; i++) k.b[i] = d[4 - i];
                    got = true;
                } else if(app->rfid_proto == LFRFIDProtocolH10301) { /* 3 bytes: FC, number hi, lo */
                    k.len = 3;
                    k.b[0] = d[2];
                    k.b[1] = d[1];
                    k.b[2] = d[0];
                    got = true;
                }
                lfrfid_worker_stop(app->rfid); /* read again for the next card */
                lfrfid_worker_read_start(app->rfid, LFRFIDWorkerReadTypeAuto, rfid_cb, app);
            }
            if(got) touch_got(app, &k);
        }
        if((app->touch_mode & TmIbtn) && app->ihit) {
            app->ihit = false;
            iButtonEditableData ed;
            ibutton_protocols_get_editable_data(app->iprot, app->ikey, &ed);
            if(ed.size) { /* any key the stock reader understands */
                if(ed.size >= 8) { /* Dallas family */
                    Z5rKey ik = {.len = 6};
                    memcpy(ik.b, ed.ptr + 1, 6); /* ROM[1..6] = serial, LSB first, as BaseZ5R stores it */
                    ik.b[5] &= 0x0F;
                    touch_got(app, &ik);
                } else { /* Cyfral / Metakom: the controller cannot match them as Dallas keys */
                    uint32_t now = furi_get_tick();
                    if(!app->ttime || now - app->ttime > furi_ms_to_ticks(TOUCH_GAP_MS)) {
                        app->tstate = 4;
                        app->tlast[0] = 0;
                        notification_message(app->notif, &sequence_error);
                        touch_redraw(app);
                    }
                    app->ttime = now;
                }
            }
            ibutton_worker_stop(app->iwork); /* wait for the next key */
            ibutton_worker_read_start(app->iwork, app->ikey);
        }
    }
}

/* ---------- add key sub-menu ---------- */

static void add_from_file(App* app, bool rfid) {
    const char* dir = rfid ? "/ext/lfrfid" : "/ext/ibutton";
    FuriString* path = furi_string_alloc_set(dir);
    DialogsFileBrowserOptions opt;
    dialog_file_browser_set_basic_options(&opt, rfid ? ".rfid" : ".ibtn", NULL);
    opt.base_path = dir;
    if(dialog_file_browser_show(app->dialogs, path, path, &opt)) {
        Z5rKey k;
        if(z5r_key_import(app->storage, furi_string_get_cstr(path), &k)) {
            k.attr = app->tattr;
            add_key(app, &k);
        }
        else
            show_msg(app, "Bad file", rfid ? "Need H10301 or EM4100\nRFID key" : "Need a DS1990\niButton key");
    }
    furi_string_free(path);
}

static void add_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    switch(index) {
    case AType:
        break;
    case AManual:
        open_bytes(app, ByteAdd, NULL);
        byte_input_set_result_callback(app->bytes, bytes_done, NULL, app, app->edit, 6);
        break;
    case ATouch:
    case ATouchRfid:
        start_touch(app, index == ATouchRfid ? TmRfid : TmIbtn);
        break;
    case AIbtn:
    case ARfid:
        add_from_file(app, index == ARfid);
        break;
    }
}

/* ---------- main menu ---------- */

static const char* main_items[] = {"", /* [0] = base state, set in main_labels */ "Read controller", "Load base from file", "Erase controller", "Info / connection"};
static const char* const add_items[] = {"", /* [0] = type selector, drawn specially */ "Read iButton", "Read RFID", "File: iButton (.ibtn)", "File: RFID (.rfid)", "Manual entry"};
static const char* const ctl_items[] = {"Add key", "Set open time", "Edit keys list", "Write base to controller", "Save base to file"};



/* text cut with ".." to the pixel width; a trailing " >" arrow is kept */
static void fit_text(Canvas* c, const char* in, char* out, size_t n, int w) {
    size_t full = strlen(in);
    const char* tail = "";
    if(full > 2 && !strcmp(in + full - 2, " >")) {
        full -= 2;
        tail = " >";
    }
    for(size_t len = full;; len--) {
        snprintf(out, n, "%.*s%s%s", (int)len, in, len < full ? ".." : "", tail);
        if(len == 0 || canvas_string_width(c, out) <= w) break;
    }
}

/* selected item: if the text is wider than the row, its body scrolls; "Base: " and the " >" arrow stay put */
static void scroll_text(Canvas* c, const char* in, char* out, size_t n, int w) {
    size_t full = strlen(in), pre = strncmp(in, "Base: ", 6) ? 0 : 6;
    size_t tl = (full > 2 && !strcmp(in + full - 2, " >")) ? 2 : 0;
    const char* body = in + pre;
    size_t blen = full - pre - tl, fit = blen;
    char pf[8];
    snprintf(pf, sizeof(pf), "%.*s", (int)pre, in);
    for(;; fit--) {
        snprintf(out, n, "%s%.*s%s", pf, (int)fit, body, in + full - tl);
        if(fit == 0 || canvas_string_width(c, out) <= w) break;
    }
    if(fit >= blen) return; /* fits as it is */
    size_t steps = blen - fit, phase = (furi_get_tick() / 250) % (steps + 8); /* 4 steps pause at each end */
    size_t off = phase < 4 ? 0 : phase - 4 > steps ? steps : phase - 4;
    snprintf(out, n, "%s%.*s%s", pf, (int)fit, body + off, in + full - tl);
}

static void main_draw(Canvas* c, void* model) {
    Menu* mn = *(Menu**)model;
    App* app = mn->app;
    canvas_clear(c);
    draw_header(c, app, true);
    canvas_set_font(c, FontSecondary);
    int top = mn->sel > 1 ? mn->sel - 1 : 0;
    if(top > mn->n - 4) top = mn->n - 4;
    for(int i = 0; i < 4; i++) {
        int y = 16 + i * 12;
        bool sel = top + i == mn->sel;
        canvas_set_color(c, ColorBlack);
        if(sel) canvas_draw_box(c, 0, y, 123, 12);
        canvas_set_color(c, sel ? ColorWhite : ColorBlack);
        char line[48];
        if(mn->id == VAdd && top + i == AType) {
            snprintf(line, sizeof(line), "Type: < %s >", z5r_attr_name(app->tattr));
            canvas_draw_str(c, 2, y + 9, line);
            continue;
        }
        if(sel)
            scroll_text(c, mn->items[top + i], line, sizeof(line), 118);
        else
            fit_text(c, mn->items[top + i], line, sizeof(line), 118);
        canvas_draw_str(c, 2, y + 9, line);
    }
    canvas_set_color(c, ColorBlack);
    elements_scrollbar_pos(c, 127, 16, 48, mn->sel, mn->n);
}

static bool main_input(InputEvent* e, void* ctx) {
    Menu* mn = ctx;
    App* app = mn->app;
    if(mn->id == VAdd && mn->sel == AType && e->type == InputTypeShort && (e->key == InputKeyLeft || e->key == InputKeyRight)) {
        step_type(app, e->key == InputKeyRight ? 1 : -1);
        main_labels(app);
        return true;
    }
    if((e->type == InputTypeShort || e->type == InputTypeRepeat) && (e->key == InputKeyUp || e->key == InputKeyDown)) {
        mn->sel = e->key == InputKeyUp ? (mn->sel ? mn->sel - 1 : mn->n - 1) : (mn->sel + 1 < mn->n ? mn->sel + 1 : 0);
        main_labels(app);
        return true;
    }
    if(e->type == InputTypeShort && e->key == InputKeyOk) {
        app->cur = mn->id; /* Back from a sub-menu does not go through go(): messages must return here */
        mn->cb(app, mn->sel);
        return true;
    }
    return false;
}

static void main_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    switch(index) {
    case MCtl:
        menus[1].sel = 0; /* entering the base screen: first item active */
        go(app, VCtl);
        break;
    case MLoad:
        if(app->modified && !confirm(app, "Discard base?", "Unsaved keys will\nbe replaced. Go on?")) break;
        do_open(app);
        break;
    case MRead:
        if(app->modified && !confirm(app, "Discard base?", "Unsaved keys will\nbe replaced. Go on?")) break;
        do_read(app);
        break;
    case MErase:
        do_clear(app);
        break;
    case MInfo:
        go(app, VWire);
        break;
    }
}

static void ctl_cb(void* ctx, uint32_t index) {
    App* app = ctx;
    switch(index) {
    case CKeys:
        open_keys(app);
        break;
    case CWrite:
        do_write(app);
        break;
    case CAdd:
        menus[2].sel = ATouch; /* first action item, not the type selector */
        go(app, VAdd);
        break;
    case CSave:
        ask_name(app);
        break;
    case CTime:
        number_input_set_header_text(app->num, "Open time, sec (0-255)");
        {
            int32_t sec = CODE_TO_SEC((int32_t)z5r_open_time_code(app->db));
            number_input_set_result_callback(app->num, time_done, app, sec > 255 ? 255 : sec, 0, 255);
        }
        go(app, VNum);
        break;
    }
}

static uint32_t to_main(void* ctx) {
    UNUSED(ctx);
    return VMain;
}
static uint32_t to_keys(void* ctx) {
    UNUSED(ctx);
    return VKeys;
}
static uint32_t to_ctl(void* ctx) {
    UNUSED(ctx);
    return VCtl;
}
static uint32_t to_bytes_back(void* ctx) {
    UNUSED(ctx);
    return bytes_back;
}
static uint32_t to_text_back(void* ctx) {
    UNUSED(ctx);
    return text_back;
}
static uint32_t to_ignore(void* ctx) {
    UNUSED(ctx);
    return VIEW_IGNORE; /* routes Back through nav_cb */
}
static uint32_t to_exit_ask(void* ctx) {
    App* app = ((Menu*)ctx)->app;
    if(!unsaved_base(app)) return VIEW_NONE;
    app->exit_ask = true; /* nav_cb shows the warning */
    return VIEW_IGNORE;
}
static bool nav_cb(void* ctx) {
    App* app = ctx;
    if(app->exit_ask) {
        app->exit_ask = false;
        if(confirm(app, "Unsaved base!", "The base is not saved\nto a file. Exit anyway?")) view_dispatcher_stop(app->vd);
        return true;
    }
    if(app->tick != TickNone) stop_tick(app); /* leaving popup stops the iButton wait */
    go(app, app->cur);
    return true;
}

static const uint8_t wiring_img[] = {0x00,0x00,0x00,0x00,0x00,0x80,0xE0,0x41,0xE0,0xFF,0x3F,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x10,0x42,0x10,0x00,0x40,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x18,0x44,0x08,0x00,0x80,0x00,0x04,0x00,0x00,0x00,0xF0,0xFF,0xFF,0x03,0x00,0x80,0x24,0x48,0x04,0x00,0x00,0x01,0x04,0x00,0x00,0x00,0x10,0x04,0x3C,0x02,0x00,0x80,0x44,0x48,0x04,0x00,0x00,0x01,0x04,0x00,0x00,0x00,0xD0,0x05,0x42,0x02,0x00,0x80,0x84,0x48,0x04,0x00,0x00,0x01,0x04,0x00,0x00,0x00,0x50,0x05,0x81,0x02,0x00,0x80,0x04,0x49,0x04,0x00,0x00,0x01,0x04,0x00,0x00,0x00,0xD0,0x05,0x99,0x02,0x00,0x80,0x08,0x46,0x08,0x00,0x80,0x00,0x04,0x00,0x00,0x00,0x10,0x04,0x99,0x02,0x00,0x80,0x10,0x42,0x10,0x00,0x40,0x00,0x04,0x00,0xFC,0x1F,0xD0,0x05,0x81,0x02,0x00,0x80,0xE0,0x41,0xE0,0xFF,0x3F,0x00,0xC4,0x8D,0xDC,0x18,0x50,0x05,0x42,0x02,0x00,0x80,0x00,0x40,0x00,0x00,0x00,0x00,0x44,0x54,0xCD,0x1E,0xD0,0x05,0x3C,0x02,0x00,0x80,0x00,0x40,0x00,0x00,0xE0,0xFF,0xDF,0xD5,0xDD,0x18,0x10,0x04,0x00,0x02,0x00,0x80,0x00,0x40,0x00,0x00,0x20,0x00,0x04,0x55,0xDD,0x1B,0xD0,0x85,0x3F,0x02,0x00,0x80,0x00,0x40,0xC0,0xE3,0xF1,0x78,0xC4,0x4D,0x8D,0x18,0x50,0x05,0x31,0x02,0x00,0x80,0x00,0x40,0x40,0x22,0xF1,0x48,0x04,0x00,0xFC,0x1F,0xD0,0x05,0x21,0x02,0x00,0x80,0x00,0x40,0x40,0x22,0xF1,0x48,0x04,0x00,0x00,0x00,0x10,0x84,0x3F,0x02,0x00,0xA0,0xE0,0x41,0xC0,0xE3,0xF1,0x78,0x04,0x00,0x00,0x00,0xD0,0x05,0x00,0x02,0x00,0x90,0x10,0x42,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x50,0x75,0x77,0x02,0x00,0x88,0x18,0x44,0x00,0x00,0x00,0x00,0x04,0x00,0xFC,0x01,0xD0,0x45,0x51,0x02,0x00,0x84,0x24,0x48,0x00,0x00,0x00,0x00,0x04,0x5C,0xDD,0x01,0x10,0x24,0x37,0x02,0x00,0x82,0x44,0x48,0xC0,0xE3,0xF1,0x78,0x84,0x44,0xCD,0x01,0xD0,0x15,0x54,0x02,0x00,0x81,0x84,0x48,0xC0,0xE3,0xF1,0xF8,0xDF,0x5D,0xDD,0x01,0x50,0x75,0x57,0x02,0x80,0x80,0x04,0x49,0xC0,0xE3,0xF1,0x78,0x84,0x50,0xDD,0x01,0xD0,0x05,0x00,0x02,0x40,0x80,0x08,0x46,0xC0,0xE3,0xF1,0x78,0x04,0x9C,0x8C,0x01,0x10,0x04,0x00,0x02,0x20,0x80,0x10,0x42,0x80,0x80,0x20,0x00,0x04,0x00,0xFC,0x01,0xD0,0x05,0x7C,0x02,0x10,0x80,0xE0,0x41,0x80,0x80,0x20,0x00,0x04,0x00,0x00,0x00,0x50,0xC5,0x83,0x03,0x08,0x80,0x00,0x40,0x80,0x80,0x20,0x00,0x04,0x00,0x00,0x00,0xD0,0xE5,0x00,0x02,0x04,0x80,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x07,0x00,0x00,0x00,0x10,0x64,0x00,0x04,0x02,0x00,0x00,0x00,0x80,0x80,0x20,0x00,0x00,0x00,0xFC,0x1F,0xD0,0xE5,0xCC,0x0C,0x01,0x00,0x00,0x00,0x80,0x80,0x20,0x00,0xC0,0x5D,0xDC,0x18,0x50,0xE5,0xCC,0x8C,0x00,0x00,0x00,0x00,0x80,0x80,0x20,0x00,0x40,0x44,0xCC,0x1E,0xD0,0xD5,0xCC,0x5C,0x00,0x00,0x00,0x00,0x80,0xB6,0xE0,0xFF,0xDF,0x45,0xDC,0x18,0x10,0x14,0x00,0x10,0x00,0x00,0x00,0x00,0x80,0x8A,0x00,0x00,0x00,0x45,0xDC,0x1A,0xD0,0x15,0x00,0x10,0x00,0x00,0x00,0x00,0x90,0xBE,0x04,0x00,0xC0,0xDD,0x8D,0x18,0x50,0xD5,0xCC,0x1C,0x00,0x00,0x00,0x00,0xA0,0x80,0x02,0x00,0x00,0x00,0xFC,0x1F,0xD0,0xD5,0xCC,0x1C,0x00,0x00,0x00,0x00,0x90,0xBE,0x04,0x00,0x00,0x00,0x00,0x00,0x10,0xE4,0xCC,0x0C,0x00,0x00,0x00,0x00,0x80,0xA2,0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,0x00,0x08,0x00,0x00,0x00,0x00,0x80,0xBE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x04,0x00,0x00,0x00,0x00,0x80,0x80,0x00,0x00,0xFF,0xFF,0xFF,0x1F,0x00,0x80,0x00,0x0A,0x00,0x00,0x00,0x00,0x80,0x80,0x70,0x33,0xE3,0xBB,0xDF,0x18,0x00,0x00,0x83,0x1D,0x00,0x00,0x00,0x00,0x80,0x80,0x10,0x55,0xEB,0x99,0xCF,0x1A,0x00,0x00,0x7C,0x38,0x00,0x00,0x00,0x00,0x80,0xFF,0x57,0x55,0xE3,0xBB,0xDF,0x18,0x00,0x00,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x50,0x55,0xEB,0xBB,0xDF,0x1A,0x00,0x00,0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x35,0xE3,0x11,0x8F,0x18,0x00,0x00,0x00,0xC0,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x1F,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x06,0xFC,0xFF,0xFF,0xFF,0xFF,0x1F,0x00,0xFC,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,0xC0,0x01,0x04,0x00,0x00,0x00,0x00,0x10,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0xF4,0xDE,0x7B,0xEF,0xBD,0x17,0x00,0xF4,0xDE,0x7B,0xEF,0xBD,0xF7,0x5E,0x00,0x00,0xF4,0x52,0x4A,0x29,0xA5,0x17,0x00,0x94,0xD2,0x4B,0x29,0xBD,0x97,0x5E,0x00,0x00,0xF4,0x52,0x4A,0x29,0xA5,0x17,0x00,0x94,0xD2,0x4B,0x29,0xBD,0x97,0x5E,0x00,0x00,0xF4,0xDE,0x7B,0xEF,0xBD,0x17,0x00,0xF4,0xDE,0x7B,0xEF,0xBD,0xF7,0x5E,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x10,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0xFC,0xFF,0xFF,0xFF,0xFF,0x1F,0x00,0xFC,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x80,0x03,0x00,0x00,0x40,0x04,0x40,0x0E,0x39,0xE4,0x00,0x00,0x30,0x00,0x00,0x00,0x80,0x02,0x00,0x00,0x60,0x06,0x60,0x82,0x09,0xA6,0x00,0x00,0x20,0x00,0x00,0x00,0x80,0x03,0x00,0x00,0x40,0x04,0x40,0x0E,0x39,0xE4,0x00,0x00,0x20,0x00,0x00,0x00,0x80,0x02,0x00,0x00,0x40,0x04,0x40,0x08,0x29,0xA4,0x00,0x00,0x70,0x00,0x00,0x00,0x80,0x03,0x00,0x00,0xE0,0x0E,0xE0,0x8E,0x3B,0xEE,0x00};

static void wire_draw(Canvas* c, void* model) {
    UNUSED(model);
    canvas_clear(c);
    canvas_draw_xbm(c, 0, 0, 128, 64, wiring_img);
}

static bool wire_input(InputEvent* e, void* ctx) {
    App* app = ctx;
    if(e->type == InputTypeShort && (e->key == InputKeyOk || e->key == InputKeyDown)) {
        go(app, VInfo); /* details as text */
        return true;
    }
    return false;
}

static uint32_t to_wire(void* ctx) {
    UNUSED(ctx);
    return VWire;
}

static const char* info_text =
    "Z5Flip: Z-5R key base\n"
    "Link: I2C EEPROM 24C64 @0xA0\n"
    "Z-5R header (1-4 top, 5-8 bot):\n"
    " 1 VCC MCU - do NOT use\n"
    " 2 SDA, 6 SCL, 7/8 GND\n"
    " 5 VCC EEPROM only\n"
    " 3,4 mode jumpers\n"
    "Flipper -> header:\n"
    " 1 (5V)  -> 5\n"
    " 15 SDA  -> 2\n"
    " 16 SCL  -> 6\n"
    " 18 GND  -> 7\n"
    "5V is switched on by the app\n"
    " when needed (USB gives it\n"
    " anyway).\n"
    "Memory: 8KB=1363 keys,\n"
    " 4KB=680 keys.\n"
    "Files: apps_data/z5flip\n"
    "*.bin = raw memory image\n"
    " (same as BaseZ5R).\n"
    "Read saves the controller to\n"
    " controller_last.bin; Write\n"
    " of that base skips backup.\n"
    "Otherwise Write/Erase offer\n"
    " a backup file first.";

int32_t z5r_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->db = malloc(sizeof(Z5rDb));
    app->tattr = Z5rAttrNormal;
    z5r_init_blank(app->db, 8192);
    app->storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(app->storage, DATA_DIR);
    storage_simply_remove(app->storage, LOG_PATH);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->gui = furi_record_open(RECORD_GUI);
    app->notif = furi_record_open(RECORD_NOTIFICATION);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_tick_event_callback(app->vd, tick_cb, furi_ms_to_ticks(100)); /* key polling rate: 250 ms felt sluggish */
    view_dispatcher_set_navigation_event_callback(app->vd, nav_cb);

    app->main = view_alloc();
    app->ctl = view_alloc();
    main_items[0] = app->label;
    menus[0] = (Menu){app, main_items, COUNT_OF(main_items), 0, main_cb, VMain};
    menus[1] = (Menu){app, ctl_items, COUNT_OF(ctl_items), 0, ctl_cb, VCtl};
    app->add = view_alloc();
    menus[2] = (Menu){app, add_items, COUNT_OF(add_items), ATouch, add_cb, VAdd};
    View* mv[3] = {app->main, app->ctl, app->add};
    for(int i = 0; i < 3; i++) {
        view_allocate_model(mv[i], ViewModelTypeLockFree, sizeof(Menu*));
        with_view_model(mv[i], Menu * *m, { *m = &menus[i]; }, false);
        view_set_context(mv[i], &menus[i]);
        view_set_draw_callback(mv[i], main_draw);
        view_set_input_callback(mv[i], main_input);
    }
    app->keys = view_alloc();
    view_allocate_model(app->keys, ViewModelTypeLockFree, sizeof(App*));
    with_view_model(app->keys, App * *m, { *m = app; }, false);
    view_set_context(app->keys, app);
    view_set_draw_callback(app->keys, keys_draw);
    view_set_input_callback(app->keys, keys_input);
    app->key = variable_item_list_alloc();
    variable_item_list_set_enter_callback(app->key, key_enter_cb, app);
    app->keyhint = view_alloc();
    view_allocate_model(app->keyhint, ViewModelTypeLockFree, sizeof(App*));
    with_view_model(app->keyhint, App * *m, { *m = app; }, false);
    view_set_draw_callback(app->keyhint, keyhint_draw); /* no input callback: keys go to the list */
    app->keyv = view_stack_alloc();
    view_stack_add_view(app->keyv, variable_item_list_get_view(app->key));
    view_stack_add_view(app->keyv, app->keyhint);
    app->filter = variable_item_list_alloc();
    app->fitem[FCode] = variable_item_list_add(app->filter, "Code", 1, NULL, app);
    app->fitem[FType] = variable_item_list_add(app->filter, "Type", 5, filter_type_cb, app);
    app->fitem[FReset] = variable_item_list_add(app->filter, "Reset", 1, NULL, app);
    variable_item_list_set_enter_callback(app->filter, filter_enter_cb, app);
    filter_sync(app);
    app->bytes = byte_input_alloc();
    app->text = text_input_alloc();
    app->popup = popup_alloc();
    app->warn = view_alloc();
    view_allocate_model(app->warn, ViewModelTypeLockFree, sizeof(App*));
    with_view_model(app->warn, App * *m, { *m = app; }, false);
    view_set_context(app->warn, app);
    view_set_draw_callback(app->warn, warn_draw);
    view_set_input_callback(app->warn, warn_input);
    app->num = number_input_alloc();
    app->touch = view_alloc();
    view_allocate_model(app->touch, ViewModelTypeLockFree, sizeof(App*));
    with_view_model(app->touch, App * *m, { *m = app; }, false);
    view_set_context(app->touch, app);
    view_set_draw_callback(app->touch, touch_draw);
    view_set_input_callback(app->touch, touch_input);
    app->wire = view_alloc();
    view_set_context(app->wire, app);
    view_set_draw_callback(app->wire, wire_draw);
    view_set_input_callback(app->wire, wire_input);
    app->info = text_box_alloc();
    text_box_set_text(app->info, info_text);
    main_labels(app);

    struct {
        ViewId id;
        View* v;
        uint32_t (*prev)(void*);
    } views[] = {
        {VMain, app->main, to_exit_ask},
        {VCtl, app->ctl, to_main},
        {VKeys, app->keys, to_ctl},
        {VKey, view_stack_get_view(app->keyv), to_keys},
        {VBytes, byte_input_get_view(app->bytes), to_bytes_back},
        {VText, text_input_get_view(app->text), to_text_back},
        {VPopup, popup_get_view(app->popup), to_ignore},
        {VWarn, app->warn, to_ignore},
        {VNum, number_input_get_view(app->num), to_ctl},
        {VTouch, app->touch, to_ignore},
        {VWire, app->wire, to_main},
        {VInfo, text_box_get_view(app->info), to_wire},
        {VFilter, variable_item_list_get_view(app->filter), to_keys},
        {VAdd, app->add, to_ctl},
    };
    for(size_t i = 0; i < COUNT_OF(views); i++) {
        view_set_previous_callback(views[i].v, views[i].prev);
        view_dispatcher_add_view(app->vd, views[i].id, views[i].v);
    }

    go(app, VMain);
    view_dispatcher_run(app->vd);

    stop_tick(app);
    if(app->power5v) z5r_link_power(false);
    for(size_t i = 0; i < COUNT_OF(views); i++) view_dispatcher_remove_view(app->vd, views[i].id);
    view_free(app->main);
    view_free(app->ctl);
    view_free(app->keys);
    view_stack_remove_view(app->keyv, variable_item_list_get_view(app->key));
    view_stack_remove_view(app->keyv, app->keyhint);
    view_stack_free(app->keyv);
    view_free(app->keyhint);
    variable_item_list_free(app->key);
    variable_item_list_free(app->filter);
    view_free(app->add);
    byte_input_free(app->bytes);
    text_input_free(app->text);
    popup_free(app->popup);
    view_free(app->warn);
    view_free(app->touch);
    number_input_free(app->num);
    text_box_free(app->info);
    view_free(app->wire);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    if(app->led) notification_message(app->notif, &sequence_reset_rgb);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    free(app->db);
    free(app);
    return 0;
}
