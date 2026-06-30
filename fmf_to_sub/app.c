#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <flipper_format.h>
#include "fmf_to_sub_icons.h"

#define TAG "FMF to SUB"

#define FMF_FILE_EXTENSION ".fmf"

#define FMF_LOAD_PATH     \
    EXT_PATH("apps_data") \
    "/"                   \
    "music_player"

// Our application menu.
typedef enum {
    Fmf2SubSubmenuIndexConfigure,
    Fmf2SubSubmenuIndexConvert,
    Fmf2SubSubmenuIndexAbout,
} Fmf2SubSubmenuIndex;

// Each view is a screen we show the user.
typedef enum {
    Fmf2SubViewSubmenu, // The menu when the app starts
    Fmf2SubViewTextInput, // Input for configuring text settings
    Fmf2SubViewConfigure, // The configuration screen
    Fmf2SubViewConvert, // The main screen
    Fmf2SubViewAbout, // The about screen with directions, link to social channel, etc.
} Fmf2SubView;

typedef enum {
    Fmf2SubEventIdRedrawScreen = 0, // Custom event to redraw the screen
    Fmf2SubEventIdOkPressed = 1, // Custom event to process OK button getting pressed down
    Fmf2SubEventIdLoadFile = 2, // Custom event to load the file
    Fmf2SubEventIdCreateSub = 3, // Custom event to create the sub file
} Fmf2SubEventId;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    DialogsApp* dialogs; // Shows dialogs like file browser
    Submenu* submenu; // The application menu
    TextInput* text_input; // The text input screen
    VariableItemList* variable_item_list_config; // The configuration screen
    VariableItem* variable_item_button; // The button on FlipBoard
    View* view_convert; // The main screen
    Widget* widget_about; // The about screen
    FuriString* file_path; // The path to the file
    char* temp_buffer; // Temporary buffer for text input
    uint32_t temp_buffer_size; // Size of temporary buffer
    FuriTimer* timer; // Timer for redrawing the screen
} Fmf2SubApp;

typedef enum {
    Fmf2SubStateIdle,
    Fmf2SubStateLoading,
    Fmf2SubStateConverting,
    Fmf2SubStateConverted,
    Fmf2SubStateError,
} Fmf2SubState;

typedef struct {
    uint32_t bpm;
    uint32_t duration;
    uint32_t octave;
    FuriString* notes;
} Fmf2SubData;

typedef struct {
    uint8_t setting_frequency_index; // The frequency
    uint8_t setting_modulation_index; // The modulation
    uint8_t setting_button_index; // The button on FlipBoard
    Fmf2SubState state; // The state of the application
    Fmf2SubData data; // The data from the file
} Fmf2SubConvertModel;

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id (VIEW_NONE)
*/
static uint32_t fmf2sub_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return ViewSubmenu to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id (ViewSubmenu)
*/
static uint32_t fmf2sub_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return Fmf2SubViewSubmenu;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - Fmf2SubApp object.
 * @param      index     The Fmf2SubSubmenuIndex item that was clicked.
*/
static void fmf2sub_submenu_callback(void* context, uint32_t index) {
    Fmf2SubApp* app = (Fmf2SubApp*)context;
    switch(index) {
    case Fmf2SubSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, Fmf2SubViewConfigure);
        break;
    case Fmf2SubSubmenuIndexConvert:
        view_dispatcher_switch_to_view(app->view_dispatcher, Fmf2SubViewConvert);
        break;
    case Fmf2SubSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, Fmf2SubViewAbout);
        break;
    default:
        break;
    }
}

/**
 * Frequency settings and values.
*/
static const char* setting_frequency_label = "Frequency";
static char* setting_frequency_values[] = {
    "300000000", "302757000", "303875000", "303900000", "304250000", "307000000", "307500000",
    "307800000", "309000000", "310000000", "312000000", "312100000", "313000000", "313850000",
    "314000000", "314350000", "314980000", "315000000", "318000000", "330000000", "345000000",
    "348000000", "387000000", "390000000", "418000000", "430000000", "431000000", "431500000",
    "433075000", "433220000", "433420000", "433657070", "433889000", "433920000", "434075000",
    "434176948", "434390000", "434420000", "434775000", "438900000", "440175000", "464000000",
    "779000000", "868350000", "868400000", "868800000", "868950000", "906400000", "915000000",
    "925000000", "928000000"};
static char* setting_frequency_names[] = {
    "300.00", "302.75", "303.88", "303.90", "304.25", "307.00", "307.50", "307.80", "309.00",
    "310.00", "312.00", "312.10", "313.00", "313.85", "314.00", "314.35", "314.98", "315.00",
    "318.00", "330.00", "345.00", "348.00", //
    "387.00", "390.00", "418.00", "430.00", "431.00", "431.50", "433.07", "433.22", "433.42",
    "433.66", "433.89", "433.92", "434.07", "434.18", "434.39", "434.42", "434.78", "438.90",
    "440.18", "464.00", //
    "779.00", "868.35", "868.40", "868.80", "868.95", "906.40", "915.00", "925.00", "928.00"};
static void fmf2sub_setting_frequency_change(VariableItem* item) {
    Fmf2SubApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_frequency_names[index]);
    Fmf2SubConvertModel* model = view_get_model(app->view_convert);
    model->setting_frequency_index = index;
}

/**
 * Modulation
*/
static const char* setting_modulation_label = "Modulation";
static char* setting_modulation_values[] = {
    "FuriHalSubGhzPresetOok270Async",
    "FuriHalSubGhzPresetOok650Async",
    "FuriHalSubGhzPreset2FSKDev238Async",
    "FuriHalSubGhzPreset2FSKDev476Async"};
static char* setting_modulation_names[] = {"AM270", "AM650", "FM238", "FM476"};
static void fmf2sub_setting_modulation_change(VariableItem* item) {
    Fmf2SubApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_modulation_names[index]);
    Fmf2SubConvertModel* model = view_get_model(app->view_convert);
    model->setting_modulation_index = index;
}

/**
 * Flipboard button
*/
static const char* setting_button_label = "FlipButtons";
static char* setting_button_values[] = {
    "Flip1.sub",
    "Flip2.sub",
    "Flip3.sub",
    "Flip4.sub",
    "Flip5.sub",
    "Flip6.sub",
    "Flip7.sub",
    "Flip8.sub",
    "Flip9.sub",
    "Flip10.sub",
    "Flip11.sub",
    "Flip12.sub",
    "Flip13.sub",
    "Flip14.sub",
    "Flip15.sub"};
static char* setting_button_names[] = {
    "1",
    "2",
    "1+2",
    "3",
    "1+3",
    "2+3",
    "1+2+3",
    "4",
    "1+4",
    "2+4",
    "1+2+4",
    "3+4",
    "1+3+4",
    "2+3+4",
    "All"};
static void fmf2sub_setting_button_change(VariableItem* item) {
    Fmf2SubApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, setting_button_names[index]);
    Fmf2SubConvertModel* model = view_get_model(app->view_convert);
    model->setting_button_index = index;
}

/**
 * @brief      Callback for drawing the convert screen.
 * @details    This function is called when the screen needs to be redrawn, like when the model gets updated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void fmf2sub_view_convert_draw_callback(Canvas* canvas, void* model) {
    Fmf2SubConvertModel* my_model = (Fmf2SubConvertModel*)model;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 10, "Press OK to select Flipper");
    canvas_draw_str(canvas, 1, 20, "Music File (.FMF) to convert");
    canvas_draw_str(canvas, 1, 30, "to Sub-GHz format (.SUB).");
    canvas_draw_str(canvas, 10, 40, "FlipBoard buttons:");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 90, 40, setting_button_names[my_model->setting_button_index]);

    if(my_model->data.notes && my_model->state == Fmf2SubStateConverting) {
        canvas_draw_str(canvas, 1, 50, furi_string_get_cstr(my_model->data.notes));
    } else {
        canvas_draw_str(canvas, 40, 50, setting_button_values[my_model->setting_button_index]);
    }

    canvas_set_font(canvas, FontPrimary);
    if(my_model->state == Fmf2SubStateLoading) {
        canvas_draw_str(canvas, 1, 60, "Loading...");
    } else if(my_model->state == Fmf2SubStateError) {
        canvas_draw_str(canvas, 1, 60, "Error!");
    } else if(my_model->state == Fmf2SubStateConverting) {
        canvas_draw_str(canvas, 1, 60, "Converting...");
    } else if(my_model->state == Fmf2SubStateConverted) {
        canvas_draw_str(canvas, 1, 60, "Saved in Sub-GHz folder");
    } else {
        canvas_draw_str(canvas, 1, 60, "Press OK to choose file");
    }
}

/**
 * @brief      Callback when the user starts the convert screen.
 * @details    This function is called when the user enters the convert screen.
 * @param      context  The context - Fmf2SubApp object.
*/
static void fmf2sub_view_convert_enter_callback(void* context) {
    UNUSED(context);
}

/**
 * @brief      Callback when the user exits the convert screen.
 * @details    This function is called when the user exits the convert screen.
 * @param      context  The context - Fmf2SubApp object.
*/
static void fmf2sub_view_convert_exit_callback(void* context) {
    Fmf2SubApp* app = (Fmf2SubApp*)context;
    with_view_model(
        app->view_convert,
        Fmf2SubConvertModel * model,
        {
            model->state = Fmf2SubStateIdle;
            if(model->data.notes) {
                furi_string_free(model->data.notes);
                model->data.notes = NULL;
            }
        },
        false);
}

static uint32_t
    fmf2sub_extract_param(FuriString* song_settings, char param, uint32_t default_value) {
    uint16_t value = 0;
    char param_equal[3] = {param, '=', 0};
    size_t index = furi_string_search_str(song_settings, param_equal);
    if(index != FURI_STRING_FAILURE) {
        index += 2;
        do {
            char ch = furi_string_get_char(song_settings, index++);
            if(ch < '0' || ch > '9') {
                break;
            }
            value *= 10;
            value += ch - '0';
        } while(true);
    } else {
        value = default_value;
    }
    return value;
}

static void fmf2sub_load_txt_file(Fmf2SubApp* app, Fmf2SubConvertModel* model) {
    UNUSED(app);
    UNUSED(model);

    bool error = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    do {
        if(storage_file_open(
               file, furi_string_get_cstr(app->file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            char ch;
            while(storage_file_read(file, &ch, 1) && !storage_file_eof(file)) {
                if(ch == ':') {
                    break;
                }
            }
            if(storage_file_eof(file)) {
                FURI_LOG_E(TAG, "Failed to find first delimiter.");
                error = true;
                break;
            }

            FuriString* song_settings = furi_string_alloc();
            while(storage_file_read(file, &ch, 1) && !storage_file_eof(file)) {
                if(ch == ':') {
                    break;
                }
                furi_string_push_back(song_settings, ch);
            }
            model->data.duration = fmf2sub_extract_param(song_settings, 'd', 4);
            model->data.octave = fmf2sub_extract_param(song_settings, 'o', 5);
            model->data.bpm = fmf2sub_extract_param(song_settings, 'b', 120);
            furi_string_free(song_settings);

            if(storage_file_eof(file)) {
                FURI_LOG_E(TAG, "Failed to find second delimiter.");
                error = true;
                break;
            }
            model->data.notes = furi_string_alloc();
            while(storage_file_read(file, &ch, 1) && !storage_file_eof(file)) {
                furi_string_push_back(model->data.notes, ch);
            }
        }
    } while(false);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(error) {
        model->state = Fmf2SubStateError;
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, Fmf2SubEventIdCreateSub);
    }
}

static void fmf2sub_load_fmf_file(Fmf2SubApp* app, Fmf2SubConvertModel* model) {
    UNUSED(model);
    FlipperFormat* ff;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* buf = furi_string_alloc();

    bool error = false;

    if(model->data.notes) {
        furi_string_free(model->data.notes);
    }
    model->data.notes = NULL;

    ff = flipper_format_buffered_file_alloc(storage);
    do {
        uint32_t format_version;
        if(!flipper_format_buffered_file_open_existing(ff, furi_string_get_cstr(app->file_path))) {
            FURI_LOG_E(TAG, "Failed to open file: %s", furi_string_get_cstr(app->file_path));
            error = true;
            break;
        }
        if(!flipper_format_read_header(ff, buf, &format_version)) {
            FURI_LOG_E(TAG, "Failed to read settings header.");
            error = true;
            break;
        }

        flipper_format_read_uint32(ff, "BPM", &(model->data.bpm), 120);
        flipper_format_read_uint32(ff, "Duration", &(model->data.duration), 4);
        flipper_format_read_uint32(ff, "Octave", &(model->data.octave), 5);
        model->data.notes = furi_string_alloc();
        if(!flipper_format_read_string(ff, "Notes", model->data.notes)) {
            FURI_LOG_E(TAG, "Failed to read notes.");
            furi_string_free(model->data.notes);
            model->data.notes = NULL;
            error = true;
        }
    } while(false);

    flipper_format_buffered_file_close(ff);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(buf);

    if(error) {
        fmf2sub_load_txt_file(app, model);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, Fmf2SubEventIdCreateSub);
    }
}

static void fmf2sub_file_write(File* file, const char* str) {
    storage_file_write(file, str, strlen(str));
}

void fmf2sub_save_sub_file(Fmf2SubApp* app, Fmf2SubConvertModel* model) {
    UNUSED(app);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    FuriString* file_path = furi_string_alloc();
    furi_string_cat_printf(
        file_path,
        "%s/%s",
        EXT_PATH("/subghz/"),
        setting_button_values[model->setting_button_index]);
    if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
        storage_file_truncate(file);
        FuriString* tmp_string = furi_string_alloc();
        fmf2sub_file_write(file, "Filetype: Flipper SubGhz RAW File\n");
        fmf2sub_file_write(file, "Version: 1\n");
        fmf2sub_file_write(file, "Frequency: ");
        fmf2sub_file_write(file, setting_frequency_values[model->setting_frequency_index]);
        fmf2sub_file_write(file, "\nPreset: ");
        fmf2sub_file_write(file, setting_modulation_values[model->setting_modulation_index]);
        fmf2sub_file_write(file, "\nProtocol: RAW");

        // process the notes
        FuriString* notes = model->data.notes;
        int16_t duration = -1;
        int16_t octave = -1;
        bool dot = false;

        for(size_t i = 0; i < furi_string_size(notes); i++) {
            char ch = furi_string_get_char(notes, i);
            if(ch == ' ' || ch == ',') {
                // skip spaces and commas.
                continue;
            }

            // is is a duration?
            while(ch >= '0' && ch <= '9') {
                if(duration == -1) {
                    duration = 0;
                }
                duration *= 10;
                duration += ch - '0';
                ch = furi_string_get_char(notes, ++i);
            }
            if(duration == -1) {
                duration = model->data.duration;
            }

            // it should be note.
            if(ch < 'A' && ch > 'G' && ch < 'a' && ch > 'g' && ch != 'P' && ch != 'p') {
                FURI_LOG_D(TAG, "Invalid note: %c", ch);
                // invalid note
                continue;
            }
            bool sharp = furi_string_get_char(notes, i + 1) == '#';

            // convert to frequency (octave 2)
            float frequency = 0;
            ch = toupper(ch);
            if(ch == 'P') {
                frequency = 6.0;
            } else if(ch == 'C') {
                frequency = !sharp ? 130.0 : 138.6;
            } else if(ch == 'D') {
                frequency = !sharp ? 146.8 : 155.6;
            } else if(ch == 'E') {
                frequency = 164.8;
            } else if(ch == 'F') {
                frequency = !sharp ? 174.6 : 185.0;
            } else if(ch == 'G') {
                frequency = !sharp ? 196.0 : 207.7;
            } else if(ch == 'A') {
                frequency = !sharp ? 220.0 : 233.1;
            } else if(ch == 'B') {
                frequency = 246.9;
            } else {
                FURI_LOG_D(TAG, "Invalid note: %c, %d", ch, (int)ch);
                // invalid note
                continue;
            }

            if(sharp) {
                i++;
            }

            ch = furi_string_get_char(notes, ++i);
            if(ch == '.') {
                dot = true; // 50% longer
                ch = furi_string_get_char(notes, ++i);
            }

            while(ch >= '0' && ch <= '9') {
                if(octave == -1) {
                    octave = 0;
                }
                octave *= 10;
                octave += ch - '0';
                ch = furi_string_get_char(notes, ++i);
            }
            if(octave == -1) {
                octave = model->data.octave;
            }

            if(ch == '.') {
                dot = true; // 50% longer
            }

            if(octave < 2) {
                frequency /= 2.0;
            } else {
                for(int i = 2; i < octave; i++) {
                    frequency *= 2.0;
                }
            }

            uint32_t pulse = (1000000 / frequency) / 2;
            // 4/4 timing, duration of quarter note (4) is 500ms.
            float count = (1000000.0 / (pulse * 2.0)) * 2.0 / duration;
            count *= 120.0f;
            count /= model->data.bpm;

            if(dot) {
                count += (count / 2.0f);
            }

            if(count < 1.0f) {
                count = 1.0;
            }

            if(duration <= 1) {
                count *= 0.98; // whole note.
            } else if(duration <= 2) {
                count *= 0.95; // half note.
            } else if(duration <= 4) {
                count *= 0.90; // quarter note.
            } else {
                count *= 0.90;
            }

            float duration_tone = ((uint32_t)count) * pulse * 2;
            float duration_beat =
                (1000000.0 * 2.0 / duration * 120.0 / model->data.bpm) * (dot ? 1.5 : 1.0);
            float duration_rem = (duration_beat - duration_tone) / 2.0f;
            uint32_t rem_counter = 1;
            while(duration_rem > 20000.0f) {
                rem_counter *= 2;
                duration_rem /= 2.0;
            }

            FURI_LOG_D(
                TAG,
                "octave: %d, duration: %d, freq: %f, dot: %c, pulse: %ld, count: %f, bpm: %ld",
                octave,
                duration,
                (double)frequency,
                dot ? 'Y' : 'N',
                pulse,
                (double)count,
                model->data.bpm);

            FURI_LOG_D(
                TAG,
                "beat: %f  tone: %f  rem-us: %f  rem-cnt: %ld",
                (double)duration_beat,
                (double)duration_tone,
                (double)duration_rem,
                rem_counter);

            fmf2sub_file_write(file, "\nRAW_Data:");
            furi_string_printf(tmp_string, " %ld %ld", pulse, -pulse);
            for(uint32_t i = 0; i < (uint32_t)count; i++) {
                if(i % 256 == 255) {
                    fmf2sub_file_write(file, "\nRAW_Data:");
                }
                fmf2sub_file_write(file, furi_string_get_cstr(tmp_string));
            }

            fmf2sub_file_write(file, "\nRAW_Data:");
            furi_string_printf(
                tmp_string, " %ld -%ld", (uint32_t)duration_rem, (uint32_t)duration_rem);
            for(uint32_t i = 0; i < rem_counter; i++) {
                fmf2sub_file_write(file, furi_string_get_cstr(tmp_string));
            }

            octave = -1;
            duration = -1;
            dot = false;
        }
        furi_string_free(tmp_string);

        // write file
        model->state = Fmf2SubStateConverted;
    } else {
        FURI_LOG_D(TAG, "Failed to create file: %s", furi_string_get_cstr(file_path));
        model->state = Fmf2SubStateError;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(file_path);
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - Fmf2SubEventId value.
 * @param      context  The context - Fmf2SubApp object.
*/
static bool fmf2sub_view_convert_custom_event_callback(uint32_t event, void* context) {
    Fmf2SubApp* app = (Fmf2SubApp*)context;
    switch(event) {
    case Fmf2SubEventIdRedrawScreen:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_convert, Fmf2SubConvertModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    case Fmf2SubEventIdOkPressed: {
        with_view_model(
            app->view_convert,
            Fmf2SubConvertModel * model,
            { model->state = Fmf2SubStateIdle; },
            false);
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, "", &I_fmf_10x10);
        browser_options.hide_dot_files = true;
        browser_options.hide_ext = false;
        browser_options.base_path = FMF_LOAD_PATH;
        furi_string_set(app->file_path, browser_options.base_path);
        if(dialog_file_browser_show(
               app->dialogs, app->file_path, app->file_path, &browser_options)) {
            view_dispatcher_send_custom_event(app->view_dispatcher, Fmf2SubEventIdLoadFile);
        }
        return true;
    }
    case Fmf2SubEventIdLoadFile: {
        with_view_model(
            app->view_convert,
            Fmf2SubConvertModel * model,
            {
                model->state = Fmf2SubStateLoading;
                fmf2sub_load_fmf_file(app, model);
            },
            true);
        return true;
    }
    case Fmf2SubEventIdCreateSub: {
        with_view_model(
            app->view_convert,
            Fmf2SubConvertModel * model,
            {
                model->state = Fmf2SubStateConverting;
                FURI_LOG_D(TAG, "Loaded file: %s", furi_string_get_cstr(app->file_path));
                FURI_LOG_D(TAG, "BPM: %ld", model->data.bpm);
                FURI_LOG_D(TAG, "Duration: %ld", model->data.duration);
                FURI_LOG_D(TAG, "Octave: %ld", model->data.octave);
                FURI_LOG_D(TAG, "Notes: %s", furi_string_get_cstr(model->data.notes));

                fmf2sub_save_sub_file(app, model);
            },
            true);
        return true;
    }
    default:
        return false;
    }
}

/**
 * @brief      Callback for convert screen input.
 * @details    This function is called when the user presses a button while on the convert screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - Fmf2SubApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool fmf2sub_view_convert_input_callback(InputEvent* event, void* context) {
    Fmf2SubApp* app = (Fmf2SubApp*)context;
    UNUSED(app);

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyLeft) {
            bool redraw = true;
            with_view_model(
                app->view_convert,
                Fmf2SubConvertModel * model,
                {
                    if(model->setting_button_index > 0) {
                        model->setting_button_index--;
                        variable_item_set_current_value_text(
                            app->variable_item_button,
                            setting_button_names[model->setting_button_index]);
                        variable_item_set_current_value_index(
                            app->variable_item_button, model->setting_button_index);
                    }
                },
                redraw);
        } else if(event->key == InputKeyRight) {
            bool redraw = true;
            with_view_model(
                app->view_convert,
                Fmf2SubConvertModel * model,
                {
                    if(model->setting_button_index + 1 <
                       (uint8_t)COUNT_OF(setting_button_values)) {
                        model->setting_button_index++;
                        variable_item_set_current_value_text(
                            app->variable_item_button,
                            setting_button_names[model->setting_button_index]);
                        variable_item_set_current_value_index(
                            app->variable_item_button, model->setting_button_index);
                    }
                },
                redraw);
        }
    } else if(event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(app->view_dispatcher, Fmf2SubEventIdOkPressed);
            return true;
        }
    }

    return false;
}

/**
 * @brief      Allocate the fmf2sub application.
 * @details    This function allocates the fmf2sub application resources.
 * @return     Fmf2SubApp object.
*/
static Fmf2SubApp* fmf2sub_app_alloc() {
    Fmf2SubApp* app = (Fmf2SubApp*)malloc(sizeof(Fmf2SubApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Configure", Fmf2SubSubmenuIndexConfigure, fmf2sub_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Convert", Fmf2SubSubmenuIndexConvert, fmf2sub_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", Fmf2SubSubmenuIndexAbout, fmf2sub_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), fmf2sub_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, Fmf2SubViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, Fmf2SubViewSubmenu);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Fmf2SubViewTextInput, text_input_get_view(app->text_input));
    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    //variable_item_list_set_header(app->variable_item_list_config, "Flipboard Signal Config");
    VariableItem* item = variable_item_list_add(
        app->variable_item_list_config,
        setting_frequency_label,
        COUNT_OF(setting_frequency_values),
        fmf2sub_setting_frequency_change,
        app);
    uint8_t setting_frequency_index = 33;
    variable_item_set_current_value_index(item, setting_frequency_index);
    variable_item_set_current_value_text(item, setting_frequency_names[setting_frequency_index]);

    item = variable_item_list_add(
        app->variable_item_list_config,
        setting_modulation_label,
        COUNT_OF(setting_modulation_values),
        fmf2sub_setting_modulation_change,
        app);
    uint8_t setting_modulation_index = 1;
    variable_item_set_current_value_index(item, setting_modulation_index);
    variable_item_set_current_value_text(item, setting_modulation_names[setting_modulation_index]);

    app->variable_item_button = variable_item_list_add(
        app->variable_item_list_config,
        setting_button_label,
        COUNT_OF(setting_button_values),
        fmf2sub_setting_button_change,
        app);
    uint8_t setting_button_index = 0;
    variable_item_set_current_value_index(app->variable_item_button, setting_button_index);
    variable_item_set_current_value_text(
        app->variable_item_button, setting_button_names[setting_button_index]);

    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config),
        fmf2sub_navigation_submenu_callback);

    view_dispatcher_add_view(
        app->view_dispatcher,
        Fmf2SubViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    app->view_convert = view_alloc();
    view_set_draw_callback(app->view_convert, fmf2sub_view_convert_draw_callback);
    view_set_input_callback(app->view_convert, fmf2sub_view_convert_input_callback);
    view_set_previous_callback(app->view_convert, fmf2sub_navigation_submenu_callback);
    view_set_enter_callback(app->view_convert, fmf2sub_view_convert_enter_callback);
    view_set_exit_callback(app->view_convert, fmf2sub_view_convert_exit_callback);
    view_set_context(app->view_convert, app);
    view_set_custom_callback(app->view_convert, fmf2sub_view_convert_custom_event_callback);
    view_allocate_model(app->view_convert, ViewModelTypeLockFree, sizeof(Fmf2SubConvertModel));
    Fmf2SubConvertModel* model = view_get_model(app->view_convert);
    model->setting_frequency_index = setting_frequency_index;
    model->setting_modulation_index = setting_modulation_index;
    model->setting_button_index = setting_button_index;
    view_dispatcher_add_view(app->view_dispatcher, Fmf2SubViewConvert, app->view_convert);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Music to Sub-GHz  v1.2!\n\n"
        "Converts music files (.FMF)\n"
        "or (.TXT) to Sub-GHz format\n"
        "(.SUB) Files.   Flip#.sub is\n"
        "written to the SD Card's\n"
        "subghz folder. Another\n"
        "Flipper Zero with sound\n"
        "turned on doing a Read RAW\n"
        "in the Sub-GHz app can\n"
        "listen to the music!\n"
        "Use Flipboard Signal app to\n"
        "send signals or use the\n"
        "Sub-GHz app. Enjoy!\n\n"
        "author: @codeallnight\nhttps://discord.com/invite/NsjCvqwPAd\nhttps://youtube.com/@MrDerekJamison");
    view_set_previous_callback(
        widget_get_view(app->widget_about), fmf2sub_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, Fmf2SubViewAbout, widget_get_view(app->widget_about));

    app->file_path = furi_string_alloc();
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    return app;
}

/**
 * @brief      Free the fmf2sub application.
 * @details    This function frees the fmf2sub application resources.
 * @param      app  The fmf2sub application object.
*/
static void fmf2sub_app_free(Fmf2SubApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, Fmf2SubViewTextInput);
    text_input_free(app->text_input);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, Fmf2SubViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, Fmf2SubViewConvert);
    view_free(app->view_convert);
    view_dispatcher_remove_view(app->view_dispatcher, Fmf2SubViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, Fmf2SubViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    furi_string_free(app->file_path);
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

/**
 * @brief      Main function for fmf2sub application.
 * @details    This function is the entry point for the fmf2sub application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t fmf_to_sub_app(void* _p) {
    UNUSED(_p);

    Fmf2SubApp* app = fmf2sub_app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    fmf2sub_app_free(app);
    return 0;
}
