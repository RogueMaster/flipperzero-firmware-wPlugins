#include <furi.h>
#include <gui/gui.h>
#include <stdlib.h>
#include <input/input.h>
#include <gui/elements.h>
#include <string.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#undef CHAR_WIDTH
#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8
#define MAX_COLUMNS (SCREEN_WIDTH / CHAR_WIDTH)
#define MAX_ROWS (SCREEN_HEIGHT / CHAR_HEIGHT)
#define MENU_VISIBLE_ITEMS 4
#define MENU_ITEM_HEIGHT 10
#define MENU_START_Y 25

typedef struct {
    int x;
    int y;
    int length;
    int speed;
} MatrixColumn;

typedef struct {
    int value;
    int min;
    int max;
    char label[32];
} IntSetting;

typedef struct {
    bool use_uppercase;
    bool use_lowercase;
    bool use_numbers;
    bool use_symbols;
    bool inverted_mode;
    IntSetting max_speed;
    IntSetting min_speed;
    IntSetting spawn_rate;
    IntSetting max_length;
    IntSetting min_length;
} Settings;

typedef struct {
    MatrixColumn columns[MAX_COLUMNS];
    int num_columns;
    char* char_pool;
    int char_pool_size;
    Settings settings;
} MatrixContext;

typedef enum {
    AppStateMainMenu,
    AppStateOptionsMenu,
    AppStateMatrix
} AppState;

typedef struct {
    Gui* gui;
    ViewPort* viewport;
    FuriTimer* timer;
    bool exit;
    MatrixContext matrix_ctx;
    AppState state;
    int selected_menu_item;
    int menu_scroll_position;
} MatrixApp;

// Settings initialization
static void init_settings(Settings* settings) {
    settings->use_uppercase = true;
    settings->use_lowercase = false;
    settings->use_numbers = false;
    settings->use_symbols = false;
    settings->inverted_mode = false;

    settings->max_speed = (IntSetting){.value = 3, .min = 1, .max = 20, .label = "Max Fall Speed"};
    settings->min_speed = (IntSetting){.value = 1, .min = 1, .max = 20, .label = "Min Fall Speed"};
    settings->spawn_rate = (IntSetting){.value = 5, .min = 1, .max = 10, .label = "Spawn Rate"};
    settings->max_length = (IntSetting){.value = 10, .min = 1, .max = 10, .label = "Max Length"};
    settings->min_length = (IntSetting){.value = 3, .min = 1, .max = 10, .label = "Min Length"};
}

// Function to update character pool based on selected options
static void update_char_pool(MatrixContext* ctx) {
    if (ctx->char_pool != NULL) {
        free(ctx->char_pool);
    }
    
    Settings* settings = &ctx->settings;
    int size = 0;
    if (settings->use_uppercase) size += 26;
    if (settings->use_lowercase) size += 26;
    if (settings->use_numbers) size += 10;
    if (settings->use_symbols) size += 32;
    
    if (size == 0) {
        settings->use_uppercase = true;
        size = 26;
    }
    
    ctx->char_pool = malloc(size);
    ctx->char_pool_size = 0;
    
    if (settings->use_uppercase) {
        for (char c = 'A'; c <= 'Z'; c++) {
            ctx->char_pool[ctx->char_pool_size++] = c;
        }
    }
    if (settings->use_lowercase) {
        for (char c = 'a'; c <= 'z'; c++) {
            ctx->char_pool[ctx->char_pool_size++] = c;
        }
    }
    if (settings->use_numbers) {
        for (char c = '0'; c <= '9'; c++) {
            ctx->char_pool[ctx->char_pool_size++] = c;
        }
    }
    if (settings->use_symbols) {
        const char symbols[] = "!@#$%^&*()_+-=[]{}|;:,.<>?/~`";
        for (int i = 0; symbols[i] != '\0'; i++) {
            ctx->char_pool[ctx->char_pool_size++] = symbols[i];
        }
    }
}

static void init_matrix_column(MatrixColumn* column, int x_pos, Settings* settings) {
    column->x = x_pos * CHAR_WIDTH;
    column->y = -(rand() % SCREEN_HEIGHT);
    column->length = settings->min_length.value + 
                    (rand() % (settings->max_length.value - settings->min_length.value + 1));
    column->speed = settings->min_speed.value + 
                   (rand() % (settings->max_speed.value - settings->min_speed.value + 1));
}

static void init_matrix_context(MatrixContext* ctx) {
    init_settings(&ctx->settings);
    ctx->num_columns = MAX_COLUMNS;
    ctx->char_pool = NULL;
    
    update_char_pool(ctx);
    
    for(int i = 0; i < ctx->num_columns; i++) {
        init_matrix_column(&ctx->columns[i], i, &ctx->settings);
        ctx->columns[i].y -= (rand() % SCREEN_HEIGHT);
    }
}

static void render_main_menu(Canvas* canvas, MatrixApp* app) {
    const char* menu_items[] = {
        "Make it rain!",
        "Configure fmatrix",
        "Exit"
    };
    int num_items = sizeof(menu_items) / sizeof(menu_items[0]);
    
    canvas_draw_str(canvas, 2, 10, "fmatrix v0.3.5");
    
    for(int i = 0; i < num_items; i++) {
        if (i == app->selected_menu_item) {
            canvas_draw_str(canvas, 2, MENU_START_Y + (i * MENU_ITEM_HEIGHT), ">");
        }
        canvas_draw_str(canvas, 12, MENU_START_Y + (i * MENU_ITEM_HEIGHT), menu_items[i]);
    }
}

static void render_options_menu(Canvas* canvas, MatrixApp* app) {
    canvas_draw_str(canvas, 2, 10, "Configure fmatrix");
    
    Settings* settings = &app->matrix_ctx.settings;
    
    const int visible_start = app->menu_scroll_position;
    const int total_items = 10;  // Total number of options
    
    // Draw scroll indicators
    if (app->menu_scroll_position > 0) {
        canvas_draw_str(canvas, 120, MENU_START_Y - 8, "^");
    }
    if (app->menu_scroll_position + MENU_VISIBLE_ITEMS < total_items) {
        canvas_draw_str(canvas, 120, MENU_START_Y + (MENU_VISIBLE_ITEMS * MENU_ITEM_HEIGHT), "v");
    }
    
    for(int i = 0; i < MENU_VISIBLE_ITEMS && (i + visible_start) < total_items; i++) {
        int current_item = i + visible_start;
        int y_pos = MENU_START_Y + (i * MENU_ITEM_HEIGHT);
        
        if (current_item == app->selected_menu_item) {
            canvas_draw_str(canvas, 2, y_pos, ">");
        }
        
        char value_str[32];
        
        switch(current_item) {
            case 0:
                snprintf(value_str, sizeof(value_str), "UPPERCASE: [%c]", 
                        settings->use_uppercase ? 'X' : ' ');
                break;
            case 1:
                snprintf(value_str, sizeof(value_str), "lowercase: [%c]", 
                        settings->use_lowercase ? 'X' : ' ');
                break;
            case 2:
                snprintf(value_str, sizeof(value_str), "Numbers: [%c]", 
                        settings->use_numbers ? 'X' : ' ');
                break;
            case 3:
                snprintf(value_str, sizeof(value_str), "Symbols: [%c]", 
                        settings->use_symbols ? 'X' : ' ');
                break;
            case 4:
                snprintf(value_str, sizeof(value_str), "Inverted mode: [%c]", 
                        settings->inverted_mode ? 'X' : ' ');
                break;
            case 5:
                snprintf(value_str, sizeof(value_str), "Max Speed: < %d >", 
                        settings->max_speed.value);
                break;
            case 6:
                snprintf(value_str, sizeof(value_str), "Min Speed: < %d >", 
                        settings->min_speed.value);
                break;
            case 7:
                snprintf(value_str, sizeof(value_str), "Spawn Rate: < %d >", 
                        settings->spawn_rate.value);
                break;
            case 8:
                snprintf(value_str, sizeof(value_str), "Max Length: < %d >", 
                        settings->max_length.value);
                break;
            case 9:
                snprintf(value_str, sizeof(value_str), "Min Length: < %d >", 
                        settings->min_length.value);
                break;
        }
        
        canvas_draw_str(canvas, 12, y_pos, value_str);
    }
}

static void matrix_render(Canvas* canvas, void* context) {
    MatrixApp* app = (MatrixApp*)context;

    MatrixContext* ctx = &app->matrix_ctx;

    if(ctx->settings.inverted_mode == true){
        canvas_set_color(canvas, 0x01);
        canvas_clear(canvas);
        canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        canvas_set_color(canvas, 0x00);
    } else {
        canvas_clear(canvas);
    }

    switch(app->state) {
        case AppStateMainMenu:
            render_main_menu(canvas, app);
            break;
        case AppStateOptionsMenu:
            render_options_menu(canvas, app);
            break;
        case AppStateMatrix:
            {
                MatrixContext* ctx = &app->matrix_ctx;

                for(int i = 0; i < ctx->num_columns; i++) {
                    for(int j = 0; j < ctx->columns[i].length; j++) {
                        int char_y = ctx->columns[i].y - (j * CHAR_HEIGHT);
                        
                        if(char_y >= 0 && char_y < SCREEN_HEIGHT) {
                            char random_char = ctx->char_pool[rand() % ctx->char_pool_size];
                            canvas_draw_str(canvas, ctx->columns[i].x, char_y, &random_char);
                        }
                    }
                    
                    ctx->columns[i].y += ctx->columns[i].speed;
                    
                    if(ctx->columns[i].y - (ctx->columns[i].length * CHAR_HEIGHT) > SCREEN_HEIGHT) {
                        init_matrix_column(&ctx->columns[i], ctx->columns[i].x / CHAR_WIDTH, &ctx->settings);
                    }
                }
            }
            break;
    }
}


static void adjust_int_setting(IntSetting* setting, bool increment) {
    if (increment) {
        if (setting->value < setting->max) setting->value++;
    } else {
        if (setting->value > setting->min) setting->value--;
    }
}

static void handle_options_menu_input(MatrixApp* app, InputKey key) {
    Settings* settings = &app->matrix_ctx.settings;
    const int total_items = 10;
    
    switch(key) {
        case InputKeyUp:
            if (app->selected_menu_item > 0) {
                app->selected_menu_item--;
                if (app->selected_menu_item < app->menu_scroll_position) {
                    app->menu_scroll_position = app->selected_menu_item;
                }
            }
            break;
            
        case InputKeyDown:
            if (app->selected_menu_item < total_items - 1) {
                app->selected_menu_item++;
                if (app->selected_menu_item >= app->menu_scroll_position + MENU_VISIBLE_ITEMS) {
                    app->menu_scroll_position = app->selected_menu_item - MENU_VISIBLE_ITEMS + 1;
                }
            }
            break;
            
        case InputKeyLeft:
        case InputKeyRight:
            {
                bool increment = (key == InputKeyRight);
                switch(app->selected_menu_item) {
                    case 5: adjust_int_setting(&settings->max_speed, increment); break;
                    case 6: adjust_int_setting(&settings->min_speed, increment); break;
                    case 7: adjust_int_setting(&settings->spawn_rate, increment); break;
                    case 8: adjust_int_setting(&settings->max_length, increment); break;
                    case 9: adjust_int_setting(&settings->min_length, increment); break;
                }
            }
            break;
            
        case InputKeyOk:
            switch(app->selected_menu_item) {
                case 0: settings->use_uppercase = !settings->use_uppercase; break;
                case 1: settings->use_lowercase = !settings->use_lowercase; break;
                case 2: settings->use_numbers = !settings->use_numbers; break;
                case 3: settings->use_symbols = !settings->use_symbols; break;
                case 4: settings->inverted_mode = !settings->inverted_mode; break;

            }
            update_char_pool(&app->matrix_ctx);
            break;
            
        case InputKeyBack:
            app->state = AppStateMainMenu;
            app->selected_menu_item = 0;
            app->menu_scroll_position = 0;
            break;
        case InputKeyMAX:
            // i don't even know what or where this key is lmfao
            break;
    }
}

static void handle_main_menu_input(MatrixApp* app, InputKey key) {
    switch(key) {
        case InputKeyUp:
            if (app->selected_menu_item > 0) app->selected_menu_item--;
            break;
            
        case InputKeyDown:
            if (app->selected_menu_item < 2) app->selected_menu_item++;
            break;
            
        case InputKeyOk:
            switch(app->selected_menu_item) {
                case 0:  // Start Effect
                    app->state = AppStateMatrix;
                    break;
                case 1:  // Options
                    app->state = AppStateOptionsMenu;
                    app->selected_menu_item = 0;
                    app->menu_scroll_position = 0;
                    break;
                case 2:  // Exit
                    app->exit = true;
                    break;
            }
            break;
            
        case InputKeyBack:
            app->exit = true;
            break;
        default: break; // don't go over every single key, just put this here for your unused buttons
    }
}

static void matrix_input_callback(InputEvent* input_event, void* context) {
    MatrixApp* app = context;
    furi_assert(app != NULL);
    
    if (input_event->type != InputTypePress && 
        input_event->type != InputTypeRepeat) return;
    
    switch(app->state) {
        case AppStateMainMenu:
            handle_main_menu_input(app, input_event->key);
            break;
        case AppStateOptionsMenu:
            handle_options_menu_input(app, input_event->key);
            break;
        case AppStateMatrix:
            if (input_event->key == InputKeyBack) {
                app->state = AppStateMainMenu;
                app->selected_menu_item = 0;
                app->menu_scroll_position = 0;
            }
            break;
    }
}

static void matrix_timer_callback(void* context) {
    MatrixApp* app = context;
    furi_assert(app != NULL);
    view_port_update(app->viewport);
}

static void matrix_app_free(MatrixApp* app) {
    furi_assert(app != NULL);
    
    if(app->timer != NULL) {
        furi_timer_stop(app->timer);
        furi_timer_free(app->timer);
    }
    
    if(app->viewport != NULL) {
        view_port_enabled_set(app->viewport, false);
        gui_remove_view_port(app->gui, app->viewport);
        view_port_free(app->viewport);
    }
    
    if(app->gui != NULL) {
        furi_record_close("gui");
    }
    
    if(app->matrix_ctx.char_pool != NULL) {
        free(app->matrix_ctx.char_pool);
    }
    
    free(app);
}

int32_t matrix_app(void* p) {
    UNUSED(p);
    
    MatrixApp* app = malloc(sizeof(MatrixApp));
    app->exit = false;
    app->state = AppStateMainMenu;
    app->selected_menu_item = 0;
    app->menu_scroll_position = 0;
    
    init_matrix_context(&app->matrix_ctx);
    
    app->gui = furi_record_open("gui");
    app->viewport = view_port_alloc();
    view_port_input_callback_set(app->viewport, matrix_input_callback, app);
    view_port_draw_callback_set(app->viewport, matrix_render, app);
    gui_add_view_port(app->gui, app->viewport, GuiLayerFullscreen);
    
    app->timer = furi_timer_alloc(matrix_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, 50);
    
    while(!app->exit) {
        furi_delay_ms(10);
    }
    
    matrix_app_free(app);
    return 0;
}