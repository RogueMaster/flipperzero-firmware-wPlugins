#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define NOTE_UP             587.33f
#define NOTE_LEFT           493.88f
#define NOTE_RIGHT          440.00f
#define NOTE_DOWN           349.23f
#define NOTE_OK             293.66f
#define SONGS_PER_PAGE      3
#define LONG_PRESS_DURATION 1000
#define INPUT_KEY_COUNT     5 // Define the total number of keys we care about

/*
  Fontname: open_iconic_arrow_1x
  Copyright: https://github.com/iconic/open-iconic, SIL OPEN FONT LICENSE
  Glyphs: 28/28
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_open_iconic_arrow_1x_t[] =
    "\34\0\3\3\4\4\3\3\5\10\10\0\0\7\0\7\0\0\12\0\0\1L@\12\205%V\60\247\312,"
    "\4A\14X\64VXz\210\310\1Q\0B\14X\64n\34 \71D\305!\0C\12\205%VlR"
    "\12\346\4D\17\210$\26M$\31\231\310C\211HF\2E\17\210$\26M\62\21\225\305#\212dF"
    "\2F\17\210$\26m\42!\215\305%\311DF\2G\17\210$\26M$\21\216I\246\211HF\2H"
    "\12\204&\216H\27\212,\2I\12H\64V\370p\10E\1J\12H\64\256\344p\10\207\0K\12\204"
    "&NhB\21\351\2L\10\66=\206\205&\2M\11c/V\344\20\21\5N\12c.FHr\210"
    "\204\0O\7\66\65\226\214bP\14X\64N\60\62\232\330\250\62\0Q\11d.\226DI\246\0R\12"
    "\205%Nl\323d-\6S\11F\65\226\214\42\32\6T\13\210$\236X\225*G=\4U\14\210$"
    "\306!\216*\245\212\325\0V\16x,v\344 \215\303\242\7I\30\0W\16h,\226T\26\212\331B"
    "\61\251\10\0X\21\210$\216)\30\7\204b\266P\34\20\14Y\0Y\20\210$vD\66\221\244\211\245"
    "\222\24\331\70\2Z\15h\64n\34 \263Xd\241\70\0[\17x,\306\64V\242\304\1q@\60d"
    "\2\0\0\0\4\377\377\0";
/*
  Fontname: open_iconic_gui_1x
  Copyright: https://github.com/iconic/open-iconic, SIL OPEN FONT LICENSE
  Glyphs: 30/30
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_open_iconic_play_1x_t[] =
    "\22\0\3\3\4\4\3\2\5\10\10\0\0\10\0\10\0\0\22\0\0\0\335@\22\207\24\63%\30\211H"
    "R$\231\42I\221p\10\0A\16w\34;&q\211PB\261P\20\0B\15x\24OJ\263\34\342"
    "\220\303A\0C\16x\34\213&\22\5C\301\210\220Q\0D\10f\35C\304'\1E\12e\35#\70"
    ":\230\202\0F\12f\35\207rx\230P\0G\15g\35+\26\21U,\23\221(\26H\16g\34#"
    "\26R\231XJ\222X\10\0I\12g\34C\70\243\270\21\3J\13g\34#H\263XhC\1K\7"
    "f\35\343\17\1L\16\206\25K(\212h\31I\210\61\12\0M\15\210\24\257d\12\206\202\241\220W\0"
    "N\15\210\24\213f\231\234\134(\66\22\0O\20\210\24/,\221\220,\221J\244\244\42\14\2P\13\206"
    "\25/(bi\224\206\0Q\12\204\26/t\70\220d\1\0\0\0\4\377\377\0";

typedef struct {
    const char* name;
    const char* sequence;
    const char* symbols; // unused
} OcarinaSong;

OcarinaSong songs[] = {
    {"Zelda's Lullaby", "Left Up Right Left Up Right", "◀ ▲ ▶ ◀ ▲ ▶"},
    {"Epona's Song", "Up Left Right Up Left Right", "▲ ◀ ▶ ▲ ◀ ▶"},
    {"Saria's Song", "Down Right Left Down Right Left", "▼ ▶ ◀ ▼ ▶ ◀"},
    {"Sun's Song", "Right Down Up Right Down Up", "▶ ▼ ▲ ▶ ▼ ▲"},
    {"Song of Time", "Right Ok Down Right Ok Down", "▶ ◉ ▼ ▶ ◉ ▼"},
    {"Song of Storms", "Ok Down Up Ok Down Up", "◉ ▼ ▲ ◉ ▼ ▲"},
    // Adding Warp Songs from Ocarina of Time
    {"Song of Soaring", "Down Right Up Down Right Up", "▼ ▶ ▲ ▼ ▶ ▲"},
    {"Prelude of Light", "Up Left Right Up Left Right", "▲ ◀ ▶ ▲ ◀ ▶"},
    {"Minuet of Forest", "Left Down Right Left Down Right", "◀ ▼ ▶ ◀ ▼ ▶"},
    {"Bolero of Fire", "Right Down Left Right Down Left", "▶ ▼ ◀ ▶ ▼ ◀"},
    {"Serenade of Water", "Down Left Right Down Left Right", "▼ ◀ ▶ ▼ ◀ ▶"},
    {"Requiem of Spirit", "Right Left Left Right Up Down", "▶ ◀ ◀ ▶ ▲ ▼"},
    {"Nocturne of Shadow", "Up Right Down Up Right Down", "▲ ▶ ▼ ▲ ▶ ▼"},
    // Adding some songs from Majora's Mask
    {"Song of Healing", "Down Right Up Down Right Up", "▼ ▶ ▲ ▼ ▶ ▲"},
    {"Elegy of Emptiness", "Left Down Right Left Down Right", "◀ ▼ ▶ ◀ ▼ ▶"},
    {"Oath to Order", "Right Up Left Right Up Left", "▶ ▲ ◀ ▶ ▲ ◀"},
    // Adding additional Majora's Mask Songs
    {"Sonata of Awakening", "Right Left Down Right Left Down", "▶ ◀ ▼ ▶ ◀ ▼ ▶"},
    {"Goron Lullaby", "Left Right Left Right Down Up", "◀ ▶ ◀ ▶ ▼ ▲"},
    {"New Wave Bossa Nova", "Right Left Up Down Left Right", "▶ ◀ ▲ ▼ ◀ ▶"}};

const int song_count = sizeof(songs) / sizeof(OcarinaSong);

typedef struct {
    FuriMutex* model_mutex;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    int start_index;
    uint32_t back_press_time; // Track back press time for long press detection
    bool key_down[INPUT_KEY_COUNT]; // Track the state of each key
} Ocarina;

// Helper function to split a string by delimiter
char* split_get_first(char* s, const char* del) {
    static char* last = NULL;
    if(s == NULL) s = last;
    if(s == NULL) return NULL;

    char* end = s + strcspn(s, del);
    if(*end) {
        *end++ = '\0';
        last = end;
    } else {
        last = NULL;
    }
    return s;
}

// Helper function to draw glyphs based on token
void draw_glyph(Canvas* canvas, const char* token, unsigned int x, unsigned int y) {
    if(strcmp(token, "Left") == 0) {
        canvas_draw_glyph(canvas, x, y, 0x004D); // Left arrow
    } else if(strcmp(token, "Right") == 0) {
        canvas_draw_glyph(canvas, x, y, 0x004E); // Right arrow
    } else if(strcmp(token, "Up") == 0) {
        canvas_draw_glyph(canvas, x, y, 0x004F); // Up arrow
    } else if(strcmp(token, "Down") == 0) {
        canvas_draw_glyph(canvas, x, y, 0x004C); // Down arrow
    } else if(strcmp(token, "Ok") == 0) {
        canvas_set_custom_u8g2_font(canvas, u8g2_font_open_iconic_play_1x_t);
        canvas_draw_glyph(canvas, x, y, 0x0046); // Ok symbol
        canvas_set_custom_u8g2_font(canvas, u8g2_font_open_iconic_arrow_1x_t); // Reset font
    }
}

void draw_callback(Canvas* canvas, void* ctx) {
    Ocarina* ocarina = ctx;
    furi_check(furi_mutex_acquire(ocarina->model_mutex, FuriWaitForever) == FuriStatusOk);

    // Draw title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_draw_str(canvas, 30, 8, "Ocarina Songs");

    // Draw songs titles and their sequences
    for(int i = 0; i < SONGS_PER_PAGE; i++) {
        canvas_set_font(canvas, FontSecondary);
        int song_index = ocarina->start_index + i;
        if(song_index < song_count) {
            // Draw song name
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 5, 20 + (i * 16), songs[song_index].name);

            // Draw sequence symbols
            char* sequence_copy = strdup(songs[song_index].sequence);
            char* token = split_get_first(sequence_copy, " ");
            unsigned int x_offset = 5;
            canvas_set_custom_u8g2_font(canvas, u8g2_font_open_iconic_arrow_1x_t);
            while(token != NULL) {
                canvas_set_custom_u8g2_font(canvas, u8g2_font_open_iconic_arrow_1x_t);
                draw_glyph(canvas, token, x_offset, 29 + (i * 16));
                x_offset += 8;
                token = split_get_first(NULL, " ");
            }
            free(sequence_copy);
        }
    }

    // Draw navigation arrows
    canvas_set_custom_u8g2_font(canvas, u8g2_font_open_iconic_arrow_1x_t);
    if(ocarina->start_index + SONGS_PER_PAGE < song_count)
        canvas_draw_glyph(canvas, 120, 64, 0x004C);
    if(ocarina->start_index > 0) canvas_draw_glyph(canvas, 120, 10, 0x004F);

    furi_mutex_release(ocarina->model_mutex);
}

void play_note(float frequency) {
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(frequency, 1.0f);
    }
}

void input_callback(InputEvent* input, void* ctx) {
    Ocarina* ocarina = ctx;
    furi_message_queue_put(ocarina->event_queue, input, FuriWaitForever);
}

Ocarina* ocarina_alloc() {
    Ocarina* instance = malloc(sizeof(Ocarina));
    instance->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    instance->view_port = view_port_alloc();
    view_port_draw_callback_set(instance->view_port, draw_callback, instance);
    view_port_input_callback_set(instance->view_port, input_callback, instance);
    instance->gui = furi_record_open("gui");
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);
    instance->start_index = 0;
    memset(
        instance->key_down, 0, sizeof(instance->key_down)); // Initialize the key states to false
    return instance;
}

void ocarina_free(Ocarina* instance) {
    view_port_enabled_set(instance->view_port, false);
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close("gui");
    view_port_free(instance->view_port);
    furi_message_queue_free(instance->event_queue);
    furi_mutex_free(instance->model_mutex);
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    free(instance);
}

int32_t ocarina_app(void* p) {
    UNUSED(p);
    Ocarina* ocarina = ocarina_alloc();
    InputEvent event;
    bool processing = true;
    while(processing) {
        FuriStatus status = furi_message_queue_get(ocarina->event_queue, &event, 100);
        furi_check(furi_mutex_acquire(ocarina->model_mutex, FuriWaitForever) == FuriStatusOk);

        if(status == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeLong ||
               event.type == InputTypeRepeat) {
                switch(event.key) {
                case InputKeyUp:
                    if(!ocarina->key_down[InputKeyUp]) {
                        ocarina->key_down[InputKeyUp] = true;
                        play_note(NOTE_UP);
                    }
                    break;
                case InputKeyDown:
                    if(!ocarina->key_down[InputKeyDown]) {
                        ocarina->key_down[InputKeyDown] = true;
                        play_note(NOTE_DOWN);
                    }
                    break;
                case InputKeyLeft:
                    if(!ocarina->key_down[InputKeyLeft]) {
                        ocarina->key_down[InputKeyLeft] = true;
                        play_note(NOTE_LEFT);
                    }
                    break;
                case InputKeyRight:
                    if(!ocarina->key_down[InputKeyRight]) {
                        ocarina->key_down[InputKeyRight] = true;
                        play_note(NOTE_RIGHT);
                    }
                    break;
                case InputKeyOk:
                    if(!ocarina->key_down[InputKeyOk]) {
                        ocarina->key_down[InputKeyOk] = true;
                        play_note(NOTE_OK);
                    }
                    break;
                case InputKeyBack:
                    if(event.type == InputTypeLong) {
                        processing = false; // Exit app on long press of back button
                    } else if(event.type == InputTypePress) {
                        // Back button scroll functionality
                        if(ocarina->start_index < song_count - SONGS_PER_PAGE) {
                            ocarina->start_index++;
                        } else {
                            ocarina->start_index = 0; // Cycle back to the top
                        }
                    }
                    break;
                default:
                    break;
                }
            } else if(event.type == InputTypeRelease) {
                switch(event.key) {
                case InputKeyUp:
                    ocarina->key_down[InputKeyUp] = false;
                    break;
                case InputKeyDown:
                    ocarina->key_down[InputKeyDown] = false;
                    break;
                case InputKeyLeft:
                    ocarina->key_down[InputKeyLeft] = false;
                    break;
                case InputKeyRight:
                    ocarina->key_down[InputKeyRight] = false;
                    break;
                case InputKeyOk:
                    ocarina->key_down[InputKeyOk] = false;
                    break;
                default:
                    break;
                }
                if(furi_hal_speaker_is_mine()) {
                    furi_hal_speaker_stop();
                    furi_hal_speaker_release();
                }
            }
        }

        furi_mutex_release(ocarina->model_mutex);
        view_port_update(ocarina->view_port);
    }

    ocarina_free(ocarina);
    return 0;
}
