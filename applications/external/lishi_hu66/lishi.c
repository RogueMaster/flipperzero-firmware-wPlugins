#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#define MAX_KEY_TYPES       50
#define MAX_COLUMNS         10
#define SAVE_FILE_PATH      APP_DATA_PATH("lishi_values.txt")
#define VISIBLE_PROTOCOLS   5
#define VISIBLE_ABOUT_LINES 5
#define COLUMN_SPACING      10

typedef enum {
    MenuMain,
    MenuProtocols,
    MenuShow,
    MenuAbout,
} MenuState;

typedef struct {
    char name[16];
    int columns;
    int max_count;
    char car_models[128];
} KeyProtocol;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* mutex;
    NotificationApp* notifications;

    int counts[MAX_KEY_TYPES][MAX_COLUMNS];
    bool pressed[8];
    int boxtimer[8];
    int active_column;
    int selected_menu;
    MenuState menu_state;
    int selected_key;
    int selected_protocol;
    char current_key[32];

    KeyProtocol protocols[MAX_KEY_TYPES];
    int protocol_count;
    int scroll_offset;
    int about_scroll_offset;
} Lishi;

// Logo lishi bitmap - 50x50 pixels
static const uint8_t image_logo_0_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x1e, 0x01, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f, 0x01, 0x00, 0x00, 0x00, 0x00, 0xc0,
    0x7f, 0x02, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x02,
    0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x01, 0x00, 0x00,
    0x00, 0x00, 0xe0, 0xff, 0x03, 0x00, 0x00, 0x01, 0x01, 0xe0, 0xff, 0x03, 0x00, 0x40, 0x43, 0x01,
    0xf0, 0xff, 0x03, 0x00, 0xc0, 0xc7, 0x02, 0xf0, 0xff, 0x03, 0x00, 0xd0, 0xed, 0x82, 0xfa, 0xff,
    0x03, 0x00, 0x78, 0xcf, 0xab, 0xfd, 0xff, 0x01, 0x00, 0xf8, 0xdf, 0x5f, 0xf0, 0xa1, 0x01, 0x00,
    0x88, 0x97, 0x57, 0xd0, 0xc1, 0x01, 0x00, 0xc0, 0x17, 0x05, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x27,
    0x0b, 0xe0, 0x80, 0x00, 0x00, 0xa0, 0x01, 0x02, 0xf0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8,
    0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x46, 0x00, 0x00, 0x00, 0x80, 0x00, 0x7c, 0x26, 0x00,
    0x00, 0x00, 0x1b, 0x01, 0x74, 0x30, 0x00, 0x00, 0x80, 0x3f, 0x02, 0x74, 0x10, 0x00, 0x00, 0x80,
    0x1d, 0x02, 0x70, 0x08, 0x00, 0x00, 0x80, 0x15, 0x04, 0xe8, 0x0d, 0x00, 0x00, 0x80, 0x03, 0x04,
    0xfc, 0x0f, 0x00, 0x00, 0x00, 0xff, 0x00, 0xfc, 0x0f, 0x00, 0x00, 0x00, 0xff, 0x03, 0xfe, 0x0f,
    0x00, 0x00, 0x00, 0x17, 0x02, 0xfe, 0x1f, 0x00, 0x00, 0x00, 0x1f, 0x00, 0xff, 0x3f, 0x00, 0x00,
    0x00, 0x3f, 0x00, 0xff, 0x3e, 0x00, 0x00, 0x00, 0x08, 0x80, 0xbf, 0x7e, 0x00, 0x00, 0x00, 0x1a,
    0xd0, 0x3f, 0x7f, 0x00, 0x00, 0x00, 0x7c, 0xf4, 0x9f, 0xff, 0x00, 0x00, 0x00, 0xfc, 0xfd, 0xd7,
    0xff, 0x01, 0x00, 0x00, 0xf8, 0xff, 0xfb, 0xff, 0x03, 0x00, 0x00, 0xe0, 0x7f, 0xff, 0xff, 0x07,
    0x00, 0x00, 0x80, 0x5f, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x80, 0xe7, 0xff, 0xff, 0x3f, 0x00, 0x00,
    0xfa, 0xef, 0xff, 0xff, 0xff, 0x00, 0x80, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x01, 0x80, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x01, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0xe0, 0xff, 0xff, 0xff, 0xff,
    0x7f, 0x01, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x03, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x01,
    0xf8, 0xff, 0xff, 0xff, 0xff, 0xdf, 0x03, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xee, 0x01};

void state_free(Lishi* l) {
    if(l->view_port) gui_remove_view_port(l->gui, l->view_port);
    if(l->gui) furi_record_close(RECORD_GUI);
    if(l->view_port) view_port_free(l->view_port);
    if(l->input_queue) furi_message_queue_free(l->input_queue);
    if(l->mutex) furi_mutex_free(l->mutex);
    if(l->notifications) furi_record_close(RECORD_NOTIFICATION);
    free(l);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    Lishi* l = ctx;
    if(input_event->type == InputTypeShort || input_event->type == InputTypeLong) {
        furi_message_queue_put(l->input_queue, input_event, 0);
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    Lishi* l = ctx;
    furi_check(furi_mutex_acquire(l->mutex, FuriWaitForever) == FuriStatusOk);
    canvas_clear(canvas);

    if(l->menu_state == MenuMain) {
        canvas_draw_str(canvas, 5, 10, "LISHI Main Menu");
        const char* options[] = {"Key Type", "Show", "About"};
        for(int i = 0; i < 3; i++) {
            if(i == l->selected_menu) {
                canvas_draw_str(canvas, 5, 30 + i * 15, ">");
            }
            canvas_draw_str(canvas, 15, 30 + i * 15, options[i]);
        }
    } else if(l->menu_state == MenuProtocols) {
        canvas_draw_str(canvas, 5, 10, "Select Key Type");

        for(int i = 0; i < VISIBLE_PROTOCOLS; i++) {
            int protocol_index = i + l->scroll_offset;
            if(protocol_index >= l->protocol_count) break;

            int y_pos = 20 + i * 10;

            if(protocol_index == l->selected_protocol) {
                canvas_draw_str(canvas, 5, y_pos, ">");
            }

            canvas_draw_str(canvas, 15, y_pos, l->protocols[protocol_index].name);
        }

        char pos_info[16];
        snprintf(pos_info, sizeof(pos_info), "%d/%d", l->selected_protocol + 1, l->protocol_count);
        canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, pos_info);
    } else if(l->menu_state == MenuShow) {
        char lishi_key[64];
        if(*l->current_key) {
            snprintf(lishi_key, sizeof(lishi_key), "LISHI %s", l->current_key);
        } else {
            strcpy(lishi_key, "LISHI Key View");
        }
        canvas_draw_str(canvas, 5, 10, lishi_key);

        int current_columns = l->protocols[l->selected_key].columns;
        int* current_counts = l->counts[l->selected_key];

        int column_spacing;
        int start_x;
        if(current_columns <= 6) {
            column_spacing = 18;
            start_x = 8;
        } else if(current_columns <= 8) {
            column_spacing = 14;
            start_x = 6;
        } else if(current_columns <= 10) {
            column_spacing = 10;
            start_x = 6;
        } else {
            column_spacing = 8;
            start_x = 4;
        }

        canvas_set_font(canvas, FontSecondary);

        for(int i = 0; i < current_columns; i++) {
            char dynamic_number[16];
            snprintf(dynamic_number, sizeof(dynamic_number), "%d", i + 1);

            int x_pos = start_x + (i * column_spacing);
            if(x_pos < 124) {
                canvas_draw_str_aligned(
                    canvas,
                    x_pos + (column_spacing / 2),
                    28,
                    AlignCenter,
                    AlignCenter,
                    dynamic_number);
            }
        }

        canvas_set_font(canvas, FontSecondary);
        for(int i = 0; i < current_columns; i++) {
            char scount[16];
            snprintf(scount, sizeof(scount), "%d", current_counts[i]);

            int x_pos = start_x + (i * column_spacing);
            if(x_pos < 124) {
                canvas_draw_str_aligned(
                    canvas, x_pos + (column_spacing / 2), 42, AlignCenter, AlignCenter, scount);
            }
        }

        int active_x = start_x + l->active_column * column_spacing;
        if(active_x < 118) {
            canvas_draw_rframe(canvas, active_x, 35, column_spacing, 16, 2);
        }

        canvas_set_font(canvas, FontSecondary);
        const char* car_models = l->protocols[l->selected_key].car_models;

        int text_len = strlen(car_models);
        if(text_len > 25) {
            int split_pos = 25;
            while(split_pos > 0 && car_models[split_pos] != ' ') {
                split_pos--;
            }

            if(split_pos > 0) {
                char line1[64];
                strncpy(line1, car_models, split_pos);
                line1[split_pos] = '\0';
                canvas_draw_str(canvas, 5, 62, line1);

                char line2[64];
                strncpy(line2, car_models + split_pos + 1, sizeof(line2) - 1);
                line2[sizeof(line2) - 1] = '\0';
                canvas_draw_str(canvas, 5, 72, line2);
            } else {
                char truncated[64];
                strncpy(truncated, car_models, 30);
                truncated[30] = '\0';
                size_t len = strlen(truncated);
                snprintf(truncated + len, sizeof(truncated) - len, "...");
                canvas_draw_str(canvas, 5, 62, truncated);
            }
        } else {
            canvas_draw_str(canvas, 5, 62, car_models);

            char value_info[64];
            snprintf(
                value_info,
                sizeof(value_info),
                "Val:%d/%d",
                current_counts[l->active_column],
                l->protocols[l->selected_key].max_count);
            canvas_draw_str_aligned(canvas, 124, 72, AlignRight, AlignBottom, value_info);
        }
    } else if(l->menu_state == MenuAbout) {
        // New about made in lopaka.app
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 62, 26, "Version: 0.3");

        canvas_draw_xbm(canvas, 3, 7, 50, 50, image_logo_0_bits);
        canvas_draw_str(canvas, 44, 38, "github.com/evillero");

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 67, 10, "About");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 65, 50, "by evillero");
    }

    furi_mutex_release(l->mutex);
}

Lishi* state_init() {
    Lishi* l = malloc(sizeof(Lishi));
    l->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    l->view_port = view_port_alloc();
    l->gui = furi_record_open(RECORD_GUI);
    l->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    l->notifications = furi_record_open(RECORD_NOTIFICATION);

    l->protocol_count = 0;

    // PROTOKOLI
    // TOY43AT
    strcpy(l->protocols[l->protocol_count].name, "TOY43AT");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Toyota");
    l->protocol_count++;

    // HU87
    strcpy(l->protocols[l->protocol_count].name, "HU87");
    l->protocols[l->protocol_count].columns = 9;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Nissan, Fiat, Subaru");
    l->protocol_count++;

    // HY14
    strcpy(l->protocols[l->protocol_count].name, "HY14");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Hyundai, KIA");
    l->protocol_count++;

    // HY12
    strcpy(l->protocols[l->protocol_count].name, "HY12");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Hyundai, KIA");
    l->protocol_count++;

    // HY15
    strcpy(l->protocols[l->protocol_count].name, "HY15");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Hyundai, KIA");
    l->protocol_count++;

    // MIT8
    strcpy(l->protocols[l->protocol_count].name, "MIT8");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Mitsubishi");
    l->protocol_count++;

    // MIT11
    strcpy(l->protocols[l->protocol_count].name, "MIT11");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Mitsubishi, Citroen");
    l->protocol_count++;

    // HY16
    strcpy(l->protocols[l->protocol_count].name, "HY16");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Hyundai and KIA");
    l->protocol_count++;

    // NSN14
    strcpy(l->protocols[l->protocol_count].name, "NSN14");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Nissan, Ford, Subaru");
    l->protocol_count++;

    // MAZ24
    strcpy(l->protocols[l->protocol_count].name, "MAZ24");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Mazda, Ford");
    l->protocol_count++;

    // CY24
    strcpy(l->protocols[l->protocol_count].name, "CY24");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Chrysler");
    l->protocol_count++;

    // FO38
    strcpy(l->protocols[l->protocol_count].name, "FO38");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Ford, Lincoln");
    l->protocol_count++;

    // DWO4R
    strcpy(l->protocols[l->protocol_count].name, "DWO4R");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Chevrolet-Daewoo");
    l->protocol_count++;

    // MZ24
    strcpy(l->protocols[l->protocol_count].name, "MZ24");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Mazda");
    l->protocol_count++;

    // HU64
    strcpy(l->protocols[l->protocol_count].name, "HU64");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Mercedes-Benz");
    l->protocol_count++;

    // HU66
    strcpy(l->protocols[l->protocol_count].name, "HU66");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "VAG group");
    l->protocol_count++;

    // HU83
    strcpy(l->protocols[l->protocol_count].name, "HU83");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Peugeot, Citroen");
    l->protocol_count++;

    // HU92
    strcpy(l->protocols[l->protocol_count].name, "HU92");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "BMW, Mini, Rover");
    l->protocol_count++;

    // HU100
    strcpy(l->protocols[l->protocol_count].name, "HU100");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Buick, Cadillac, Chevy");
    l->protocol_count++;

    // HU101
    strcpy(l->protocols[l->protocol_count].name, "HU101");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Ford, Volvo, Range Rover");
    l->protocol_count++;

    // HU100R
    strcpy(l->protocols[l->protocol_count].name, "HU100R");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "BMW");
    l->protocol_count++;

    // HY22
    strcpy(l->protocols[l->protocol_count].name, "HY22");
    l->protocols[l->protocol_count].columns = 6;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Hyundai, KIA");
    l->protocol_count++;

    // TOY48
    strcpy(l->protocols[l->protocol_count].name, "TOY48");
    l->protocols[l->protocol_count].columns = 5;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "Toyota/Lexus");
    l->protocol_count++;

    // K5
    strcpy(l->protocols[l->protocol_count].name, "K5");
    l->protocols[l->protocol_count].columns = 8;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "KIA");
    l->protocol_count++;

    // GM39
    strcpy(l->protocols[l->protocol_count].name, "GM39");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "GM, Chevrolet");
    l->protocol_count++;

    // B106
    strcpy(l->protocols[l->protocol_count].name, "B106");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 5;
    strcpy(l->protocols[l->protocol_count].car_models, "GM, Chevrolet");
    l->protocol_count++;

    // HU100(10)
    strcpy(l->protocols[l->protocol_count].name, "HU100(10)");
    l->protocols[l->protocol_count].columns = 10;
    l->protocols[l->protocol_count].max_count = 4;
    strcpy(l->protocols[l->protocol_count].car_models, "Buick, Cadillac, Chevy");
    l->protocol_count++;

    for(int i = 0; i < MAX_KEY_TYPES; i++) {
        for(int j = 0; j < MAX_COLUMNS; j++) {
            l->counts[i][j] = 0;
        }
    }

    l->active_column = 0;
    l->menu_state = MenuMain;
    l->selected_menu = 0;
    l->selected_key = 0;
    l->selected_protocol = 0;
    l->scroll_offset = 0;
    l->about_scroll_offset = 0;
    memset(l->current_key, 0, sizeof(l->current_key));

    view_port_input_callback_set(l->view_port, input_callback, l);
    view_port_draw_callback_set(l->view_port, render_callback, l);
    gui_add_view_port(l->gui, l->view_port, GuiLayerFullscreen);

    return l;
}

void save_lishi_values(Lishi* l) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, SAVE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(int i = 0; i < l->protocol_count; i++) {
            storage_file_write(file, l->counts[i], sizeof(int) * MAX_COLUMNS);
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void load_lishi_values(Lishi* l) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, SAVE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        for(int i = 0; i < l->protocol_count; i++) {
            if(storage_file_read(file, l->counts[i], sizeof(int) * MAX_COLUMNS) !=
               sizeof(int) * MAX_COLUMNS) {
                for(int j = 0; j < MAX_COLUMNS; j++) {
                    l->counts[i][j] = 0;
                }
                break;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

int32_t lishiapp(void) {
    Lishi* l = state_init();
    load_lishi_values(l);

    InputEvent input;
    while(true) {
        while(furi_message_queue_get(l->input_queue, &input, FuriWaitForever) == FuriStatusOk) {
            furi_check(furi_mutex_acquire(l->mutex, FuriWaitForever) == FuriStatusOk);

            if(input.type == InputTypeShort) {
                switch(input.key) {
                case InputKeyBack:
                    if(l->menu_state == MenuMain) {
                        save_lishi_values(l);
                        furi_mutex_release(l->mutex);
                        state_free(l);
                        return 0;
                    } else {
                        l->menu_state = MenuMain;
                    }
                    break;

                case InputKeyDown:
                    if(l->menu_state == MenuMain) {
                        l->selected_menu = (l->selected_menu + 1) % 3;
                    } else if(l->menu_state == MenuProtocols) {
                        if(l->selected_protocol < l->protocol_count - 1) {
                            l->selected_protocol++;

                            if(l->selected_protocol - l->scroll_offset >= VISIBLE_PROTOCOLS) {
                                l->scroll_offset = l->selected_protocol - VISIBLE_PROTOCOLS + 1;
                            }
                        }
                    } else if(l->menu_state == MenuShow) {
                        int max_count = l->protocols[l->selected_key].max_count;
                        if(l->counts[l->selected_key][l->active_column] < max_count) {
                            l->counts[l->selected_key][l->active_column]++;
                        }
                    } else if(l->menu_state == MenuAbout) {
                    }
                    break;

                case InputKeyUp:
                    if(l->menu_state == MenuMain) {
                        l->selected_menu = (l->selected_menu - 1 + 3) % 3;
                    } else if(l->menu_state == MenuProtocols) {
                        if(l->selected_protocol > 0) {
                            l->selected_protocol--;

                            if(l->selected_protocol < l->scroll_offset) {
                                l->scroll_offset = l->selected_protocol;
                            }
                        }
                    } else if(l->menu_state == MenuShow) {
                        if(l->counts[l->selected_key][l->active_column] > 0) {
                            l->counts[l->selected_key][l->active_column]--;
                        }
                    } else if(l->menu_state == MenuAbout) {
                    }
                    break;

                case InputKeyLeft:
                    if(l->menu_state == MenuShow) {
                        int columns = l->protocols[l->selected_key].columns;
                        l->active_column = (l->active_column - 1 + columns) % columns;
                    }
                    break;

                case InputKeyRight:
                    if(l->menu_state == MenuShow) {
                        int columns = l->protocols[l->selected_key].columns;
                        l->active_column = (l->active_column + 1) % columns;
                    }
                    break;

                case InputKeyOk:
                    if(l->menu_state == MenuMain) {
                        if(l->selected_menu == 0) {
                            l->menu_state = MenuProtocols;
                            l->selected_protocol = 0;
                            l->scroll_offset = 0;
                        } else if(l->selected_menu == 1) {
                            l->menu_state = MenuShow;
                        } else if(l->selected_menu == 2) {
                            l->menu_state = MenuAbout;
                            l->about_scroll_offset = 0;
                        }
                    } else if(l->menu_state == MenuProtocols) {
                        l->selected_key = l->selected_protocol;
                        strncpy(
                            l->current_key,
                            l->protocols[l->selected_protocol].name,
                            sizeof(l->current_key) - 1);
                        l->current_key[sizeof(l->current_key) - 1] = '\0';
                        l->menu_state = MenuShow;
                        l->active_column = 0;
                    }
                    break;

                default:
                    break;
                }
            }

            furi_mutex_release(l->mutex);
            view_port_update(l->view_port);
        }
    }
}
