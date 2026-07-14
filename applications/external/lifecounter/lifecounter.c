#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include "lifecounter_icons.h"

#define TAG          "Lifecounter"
#define CFG_FILENAME "lifecounter.cfg"

static int default_life_values[] = {0, 10, 20, 40, 100};
static char* default_life_names[] = {"Zero", "Ten", "Twenty", "Forty", "Hundred"};
static int toggle_state_values[] = {0, 1};
static char* toggle_states_names[] = {"Off", "On"};

typedef enum {
    LifecounterSubmenuIndexConfigure,
    LifecounterSubmenuIndexMain,
    LifecounterSubmenuIndexReset,
} LifecounterSubmenuIndex;

// Each view is a screen we show for the user.
typedef enum {
    LifecounterViewSplash,
    LifecounterViewSubmenu,
    LifecounterViewConfigure,
    LifecounterViewMain,
} LifecounterView;

typedef enum {
    SoundReset,
    SoundLifeChanged,
    SoundPlayerChanged,
} LifecounterSound;

typedef enum {
    LifecounterEventIdRedrawScreen = 0, // Custom event to redraw the screen
    LifecounterEventIdOkPressed = 42, // Custom event to process OK button getting pressed down
} LifecounterEventId;

typedef struct {
    ViewDispatcher* view_dispatcher; // View switcher
    NotificationApp* notifications; // Used for controlling the backlight
    Submenu* submenu;
    VariableItemList* variable_item_list_settings;
    View* view_main;
    View* splash_screen;

    FuriTimer* timer; // Timer for redrawing the screen
} LifecounterApp;

typedef struct {
    int default_life;
    uint8_t selected_player;
    int player_1_life;
    int player_2_life;
    bool backlight_on;
    bool sound_on;
} LifecounterModel;

/**
 * Callback for exiting the application.
 *
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *             indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * Callback for returning to submenu.
*/
static uint32_t navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return LifecounterViewSubmenu;
}

static uint32_t navigation_main_callback(void* _context) {
    UNUSED(_context);
    return LifecounterViewMain;
}

/**
 * Make some noise party people!
 *
 * @param      frequency  The frequency of the beep sound.
 * @param      duration   The duration of the beep sound.
 * @param      volume     The volume of the beep sound.
 */
static void beep(float frequency, float duration, float volume) {
    uint32_t timeout = 500;
    if(furi_hal_speaker_acquire(timeout)) {
        furi_hal_speaker_start(frequency, volume);
        furi_delay_ms(duration);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

/**
 * Play audio.
 *
 * @param      model  Model to check whether sounds are on.
 * @param      sound  The sound we want to play.
 */
static void audio_feedback(LifecounterModel* model, LifecounterSound sound) {
    if(!model->sound_on) {
        return;
    }

    switch(sound) {
    case SoundReset:
        beep(320, 400, 0.8);
        break;
    case SoundLifeChanged:
        beep(440, 100, 0.8);
        break;
    case SoundPlayerChanged:
        beep(580, 100, 0.8);
        break;
    default:
        break;
    }
}

/**
 * Write the configuration to a file.
 */
void write_config(LifecounterModel* model) {
    int life = model->default_life;
    int backlight = model->backlight_on;
    int sound = model->sound_on;
    const char* path = APP_DATA_PATH(CFG_FILENAME);

    FURI_LOG_D(TAG, "Saving configuration to %s", path);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    FuriString* settings = furi_string_alloc();
    furi_string_printf(settings, "%d\n%d\n%d\n", life, backlight, sound);
    const char* buffer = furi_string_get_cstr(settings);

    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", path);
    }
    if(!storage_file_write(file, buffer, strlen(buffer))) {
        FURI_LOG_E(TAG, "Failed to write to file");
    }

    FURI_LOG_T(TAG, "Configuration saved - (%s)", buffer);

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(settings);
    furi_record_close(RECORD_STORAGE);
}

/**
 * Read the configuration from a file.
 */
LifecounterModel* read_config(LifecounterModel* model) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    const char* path = APP_DATA_PATH(CFG_FILENAME);
    int default_life = 20;
    bool backlight_on = false;
    bool sound_on = false;

    FURI_LOG_D(TAG, "Reading config from %s", path);

    FuriString* line = furi_string_alloc();
    if(file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        for(int i = 0; i < 3; i++) {
            if(!stream_read_line(stream, line)) {
                FURI_LOG_E(TAG, "Failed to read line %d", i);
                break;
            }
            int value = atoi(furi_string_get_cstr(line));
            FURI_LOG_T(TAG, "Read value %d: %d", i, value);
            switch(i) {
            case 0:
                default_life = value;
                break;
            case 1:
                backlight_on = (bool)value;
                break;
            case 2:
                sound_on = (bool)value;
                break;
            }
        }
    } else {
        FURI_LOG_E(TAG, "Failed to open file");
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_string_free(line);
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_T(
        TAG,
        "Configuration state - Life: %d, Backlight: %d, Sound: %d",
        default_life,
        backlight_on,
        sound_on);

    model->default_life = default_life;
    model->player_1_life = default_life;
    model->player_2_life = default_life;
    model->selected_player = 0;
    model->backlight_on = backlight_on;
    model->sound_on = sound_on;

    return model;
}

/**
 * @note  It's bit confusing that this is called submenu when the menu is actually the top level menu
 *        This is because the component's name is 'submenu'.
 *
 * This function is called when user selects an item from the submenu.
 *
 * @param      context   The context - LifecounterApp object.
 * @param      index     The LifecounterSubmenuIndex item that was clicked.
*/
static void submenu_callback(void* context, uint32_t index) {
    LifecounterApp* app = (LifecounterApp*)context;
    switch(index) {
    case LifecounterSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewConfigure);
        break;
    case LifecounterSubmenuIndexMain:
        view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewMain);
        break;
    case LifecounterSubmenuIndexReset:
        LifecounterModel* model = view_get_model(app->view_main);
        model->player_1_life = model->default_life;
        model->player_2_life = model->default_life;
        audio_feedback(model, SoundReset);
        view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewMain);
        break;
    default:
        break;
    }
}

/**
 * Callback for changing the default life value.
 */
static void default_life_change(VariableItem* item) {
    LifecounterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, default_life_names[index]);
    LifecounterModel* model = view_get_model(app->view_main);
    model->default_life = default_life_values[index];
}

/**
 * Callback for changing the backlight setting.
 */
static void backlight_change(VariableItem* item) {
    LifecounterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_states_names[index]);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(index == 0) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    } else {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    }
    furi_record_close(RECORD_NOTIFICATION);
    LifecounterModel* model = view_get_model(app->view_main);
    model->backlight_on = index;
}

/**
 * Callback for changing the audio setting.
 */
static void audio_change(VariableItem* item) {
    LifecounterApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, toggle_states_names[index]);
    LifecounterModel* model = view_get_model(app->view_main);
    model->sound_on = index;
}

/**
 * Dummy callback for the save button (its value doesn't change).
 */
static void value_change_callback_dummy(VariableItem* item) {
    UNUSED(item);
}

/**
 * Callback when item in configuration screen is clicked.
 *
 * @param      context  The context - LifecounterApp object.
 * @param      index - The index of the item that was clicked.
*/
static void setting_item_clicked(void* context, uint32_t index) {
    LifecounterApp* app = (LifecounterApp*)context;
    LifecounterModel* model = view_get_model(app->view_main);

    /**
     * Index values in configuration menu:
     * 0 = default life setting
     * 1 = backlight setting
     * 2 = audio setting
     * 3 = save button
     */
    if(index == 3) {
        audio_feedback(model, SoundLifeChanged);
        write_config(model);
        view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewSubmenu);
    }
}

/**
 * Callback for drawing the main screen.
 *
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void view_main_draw_callback(Canvas* canvas, void* model) {
    LifecounterModel* my_model = (LifecounterModel*)model;
    FURI_LOG_T(TAG, "view_main_draw_callback");
    canvas_set_font(canvas, FontPrimary);
    //canvas_draw_str_aligned(canvas, 7, 6, AlignLeft, AlignTop, "Player 1");

    FuriString* life1 = furi_string_alloc();
    FuriString* life2 = furi_string_alloc();
    furi_string_printf(life1, "%d", my_model->player_1_life);
    furi_string_printf(life2, "%d", my_model->player_2_life);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 32, 32, AlignCenter, AlignCenter, furi_string_get_cstr(life1));
    canvas_draw_str_aligned(canvas, 96, 32, AlignCenter, AlignCenter, furi_string_get_cstr(life2));

    size_t radius = 4;
    canvas_draw_rframe(canvas, 0, 0, 64, 64, radius);
    canvas_draw_rframe(canvas, 64, 0, 64, 64, radius);

    size_t triangle_height = 6;
    size_t triangle_width = 8;
    if(my_model->selected_player == 0) {
        canvas_draw_rframe(canvas, 4, 4, 56, 56, radius);
        canvas_draw_triangle(
            canvas, 32, 20, triangle_width, triangle_height, CanvasDirectionBottomToTop);
        canvas_draw_triangle(
            canvas, 32, 44, triangle_width, triangle_height, CanvasDirectionTopToBottom);
    } else {
        canvas_draw_rframe(canvas, 68, 4, 56, 56, radius);
        canvas_draw_triangle(
            canvas, 96, 20, triangle_width, triangle_height, CanvasDirectionBottomToTop);
        canvas_draw_triangle(
            canvas, 96, 44, triangle_width, triangle_height, CanvasDirectionTopToBottom);
    }

    furi_string_free(life1);
    furi_string_free(life2);
}

/**
 * Draw the splash screen.
 */
static void view_splash_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_draw_icon(canvas, 0, 0, &I_Splash_128x64);
}

/**
 * Callback for timer elapsed.
 *
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - LifecounterApp object.
*/
static void view_main_timer_callback(void* context) {
    LifecounterApp* app = (LifecounterApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, LifecounterEventIdRedrawScreen);
}

/**
 * Callback when the user goes to the main screen.
 *
 * @param      context  The context - LifecounterApp object.
*/
static void view_main_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(200);
    LifecounterApp* app = (LifecounterApp*)context;
    furi_assert(app->timer == NULL);
    app->timer = furi_timer_alloc(view_main_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, period);
}

/**
 * Callback when the user exits the main screen.
 *
 * @param      context  The context - LifecounterApp object.
*/
static void view_main_exit_callback(void* context) {
    LifecounterApp* app = (LifecounterApp*)context;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
}

/**
 * Callback for custom events.
 *
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - LifecounterEventId value.
 * @param      context  The context - LifecounterApp object.
*/
static bool view_main_custom_event_callback(uint32_t event, void* context) {
    LifecounterApp* app = (LifecounterApp*)context;
    switch(event) {
    case LifecounterEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_main, LifecounterModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    default:
        return false;
    }
}

/**
 * Callback for splash screen input (in order to exit it).
 */
static bool view_splash_input_callback(InputEvent* event, void* context) {
    LifecounterApp* app = (LifecounterApp*)context;

    if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewMain);
            return true;
        }
    }

    return false;
}

/**
 * Callback for main screen input.
 *
 * @details    This function is called when the user presses a button while on the main screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - LifecounterApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool view_main_input_callback(InputEvent* event, void* context) {
    LifecounterApp* app = (LifecounterApp*)context;
    LifecounterModel* my_model = view_get_model(app->view_main);

    FURI_LOG_T(TAG, "view_main_input_callback");

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            if(my_model->selected_player == 0) {
                my_model->player_1_life++;
            } else {
                my_model->player_2_life++;
            }
            audio_feedback(my_model, SoundLifeChanged);
        } else if(event->key == InputKeyDown) {
            if(my_model->selected_player == 0) {
                my_model->player_1_life--;
            } else {
                my_model->player_2_life--;
            }
            audio_feedback(my_model, SoundLifeChanged);
        } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            if(my_model->selected_player == 0) {
                my_model->selected_player = 1;
            } else {
                my_model->selected_player = 0;
            }
            audio_feedback(my_model, SoundPlayerChanged);
        }
    } else if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewSubmenu);
            return true;
        }
    }

    with_view_model(app->view_main, LifecounterModel * my_model, { UNUSED(my_model); }, true);

    return false;
}

/**
* Find the index of a value in an array
*/
int find_index(const int a[], int size, int value) {
    int index = 0;
    while(index < size && a[index] != value)
        ++index;
    return (index == size ? -1 : index);
}

/**
* Setup and allocate the application resources
*/
static LifecounterApp* app_alloc() {
    LifecounterApp* app = (LifecounterApp*)malloc(sizeof(LifecounterApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    FURI_LOG_T(TAG, "allocate temporary model for settings");
    LifecounterModel* settings = (LifecounterModel*)malloc(sizeof(LifecounterModel));
    settings = read_config(settings);

    FURI_LOG_T(TAG, "allocate dispatcher");
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    FURI_LOG_T(TAG, "allocate menu");
    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Return to life view", LifecounterSubmenuIndexMain, submenu_callback, app);

    submenu_add_item(
        app->submenu, "Reset lifes", LifecounterSubmenuIndexReset, submenu_callback, app);

    submenu_add_item(
        app->submenu,
        "Configure settings",
        LifecounterSubmenuIndexConfigure,
        submenu_callback,
        app);

    view_set_previous_callback(submenu_get_view(app->submenu), navigation_exit_callback);

    view_dispatcher_add_view(
        app->view_dispatcher, LifecounterViewSubmenu, submenu_get_view(app->submenu));

    app->variable_item_list_settings = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_settings);
    VariableItem* item = variable_item_list_add(
        app->variable_item_list_settings,
        "Starting life",
        COUNT_OF(default_life_values),
        default_life_change,
        app);

    uint8_t default_life_index =
        find_index(default_life_values, sizeof(default_life_values), settings->default_life);
    variable_item_set_current_value_index(item, default_life_index);
    variable_item_set_current_value_text(item, default_life_names[default_life_index]);

    item = variable_item_list_add(
        app->variable_item_list_settings,
        "Backlight",
        COUNT_OF(toggle_state_values),
        backlight_change,
        app);

    uint8_t backlight_index =
        find_index(toggle_state_values, sizeof(toggle_state_values), settings->backlight_on);
    variable_item_set_current_value_index(item, backlight_index);
    variable_item_set_current_value_text(item, toggle_states_names[backlight_index]);

    item = variable_item_list_add(
        app->variable_item_list_settings,
        "Audio feedback",
        COUNT_OF(toggle_state_values),
        audio_change,
        app);

    uint8_t audio_state_index =
        find_index(toggle_state_values, sizeof(toggle_state_values), settings->sound_on);
    variable_item_set_current_value_index(item, audio_state_index);
    variable_item_set_current_value_text(item, toggle_states_names[audio_state_index]);

    item = variable_item_list_add(
        app->variable_item_list_settings, "Save settings", 0, value_change_callback_dummy, app);

    variable_item_list_set_enter_callback(
        app->variable_item_list_settings, setting_item_clicked, app);
    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_settings),
        navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        LifecounterViewConfigure,
        variable_item_list_get_view(app->variable_item_list_settings));

    FURI_LOG_T(TAG, "allocate main view");
    app->view_main = view_alloc();
    view_set_draw_callback(app->view_main, view_main_draw_callback);
    view_set_input_callback(app->view_main, view_main_input_callback);
    view_set_previous_callback(app->view_main, navigation_submenu_callback);
    view_set_enter_callback(app->view_main, view_main_enter_callback);
    view_set_exit_callback(app->view_main, view_main_exit_callback);
    view_set_context(app->view_main, app);
    view_set_custom_callback(app->view_main, view_main_custom_event_callback);

    view_allocate_model(app->view_main, ViewModelTypeLockFree, sizeof(LifecounterModel));
    LifecounterModel* model = view_get_model(app->view_main);

    model->default_life = default_life_values[default_life_index];
    model->player_1_life = model->default_life;
    model->player_2_life = model->default_life;
    model->backlight_on = settings->backlight_on;
    model->sound_on = settings->sound_on;
    view_dispatcher_add_view(app->view_dispatcher, LifecounterViewMain, app->view_main);

    FURI_LOG_T(TAG, "allocate splash screen");
    app->splash_screen = view_alloc();
    view_set_draw_callback(app->splash_screen, view_splash_draw_callback);
    view_set_input_callback(app->splash_screen, view_splash_input_callback);
    view_set_previous_callback(app->splash_screen, navigation_main_callback);
    view_set_context(app->splash_screen, app);
    view_dispatcher_add_view(app->view_dispatcher, LifecounterViewSplash, app->splash_screen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    if(settings->backlight_on) {
        notification_message(app->notifications, &sequence_display_backlight_enforce_on);
    } else {
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, LifecounterViewSplash);

    free(settings);

    return app;
}

/**
* Free the resources
*/
static void lifecounter_free(LifecounterApp* app) {
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    FURI_LOG_T(TAG, "remove splash");
    view_dispatcher_remove_view(app->view_dispatcher, LifecounterViewSplash);
    view_free(app->splash_screen);
    FURI_LOG_T(TAG, "remove main");
    view_dispatcher_remove_view(app->view_dispatcher, LifecounterViewMain);
    view_free(app->view_main);
    FURI_LOG_T(TAG, "remove config");
    view_dispatcher_remove_view(app->view_dispatcher, LifecounterViewConfigure);
    variable_item_list_free(app->variable_item_list_settings);
    FURI_LOG_T(TAG, "remove menu");
    view_dispatcher_remove_view(app->view_dispatcher, LifecounterViewSubmenu);
    submenu_free(app->submenu);
    FURI_LOG_T(TAG, "remove dispatch");
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    FURI_LOG_D(TAG, "remove app");
    free(app);
}

/**
* Main function
*/
int32_t lifecounter_app(void* params) {
    UNUSED(params);

    LifecounterApp* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    lifecounter_free(app);
    return 0;
}
