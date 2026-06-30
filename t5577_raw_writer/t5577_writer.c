#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <applications/services/storage/storage.h>
#include <applications/services/dialogs/dialogs.h>
#include <dolphin/dolphin.h>
#include <flipper_format.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <t5577_config.h>
#include <t5577_writer.h>

#include "t5577_writer_icons.h"
#define TAG                        "T5577 Writer"
#define MAX_REPEAT_WRITING_FRAMES  10
#define ENDING_WRITING_ICON_FRAMES 5

typedef enum {
    T5577WriterSubmenuIndexLoad,
    T5577WriterSubmenuIndexSave,
    T5577WriterSubmenuIndexConfigure,
    T5577WriterSubmenuIndexWrite,
    T5577WriterSubmenuIndexAbout,
} T5577WriterSubmenuIndex;

// Each view is a screen we show the user.
typedef enum {
    T5577WriterViewSubmenu, // The menu when the app starts
    T5577WriterViewTextInput, // Input for configuring text settings
    T5577WriterViewByteInput,
    T5577WriterViewLoad,
    T5577WriterViewSave,
    T5577WriterViewConfigure_i, // The configuration screen
    T5577WriterViewConfigure_e, // The configuration screen
    T5577WriterViewWrite, // The main screen
    T5577WriterViewAbout, // The about screen with directions, link to social channel, etc.
} T5577WriterView;

typedef enum {
    T5577WriterEventIdRepeatWriting = 0, // Custom event to redraw the screen
    T5577WriterEventIdMaxWriteRep = 42, // Custom event to process OK button getting pressed down
} T5577WriterEventId;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    NotificationApp* notifications; // Used for controlling the backlight
    Submenu* submenu; // The application menu

    TextInput* text_input; // The text input screen
    VariableItemList* variable_item_list_config; // The configuration screen
    View* view_config_e; // The configuration screen
    View* view_save;
    View* view_write; // The main screen
    Widget* widget_about; // The about screen
    View* view_load; // The load view

    VariableItem* mod_item; //
    VariableItem* clock_item; //
    VariableItem* block_num_item; //
    VariableItem* block_slc_item; //
    VariableItem* byte_buffer_item; //
    ByteInput* byte_input; // The byte input view
    uint8_t bytes_buffer[4];
    uint8_t bytes_count;

    char* temp_buffer; // Temporary buffer for text input
    uint32_t temp_buffer_size; // Size of temporary buffer

    DialogsApp* dialogs;
    FuriString* file_path;
    FuriTimer* timer; // Timer for redrawing the screen
} T5577WriterApp;

typedef struct {
    uint8_t modulation_index; // The index for total number of pins
    uint8_t rf_clock_index; // The index for total number of pins
    FuriString* tag_name_str; // The name setting
    uint8_t user_block_num; // The total number of pins we are adjusting
    uint32_t content[LFRFID_T5577_BLOCK_COUNT]; // The cutting content
    t5577_modulation modulation;
    t5577_rf_clock rf_clock;
    bool data_loaded[3];
    uint8_t edit_block_slc;
    uint8_t writing_repeat_times;
} T5577WriterModel;

void initialize_config(T5577WriterModel* model) {
    model->modulation_index = 0;
    memcpy(&model->modulation, &all_mods[model->modulation_index], sizeof(t5577_modulation));
    model->rf_clock_index = 0;
    memcpy(&model->rf_clock, &all_mods[model->rf_clock_index], sizeof(t5577_rf_clock));
}

void initialize_model(T5577WriterModel* model) {
    initialize_config(model);
    model->user_block_num = 0;
    model->edit_block_slc = 1;
    model->writing_repeat_times = 0;
    for(uint32_t i = 0; i < LFRFID_T5577_BLOCK_COUNT; i++) {
        model->content[i] = 0;
    }
    memset(model->data_loaded, false, sizeof(model->data_loaded));
}

uint8_t rf_clock_choices[COUNT_OF(all_rf_clocks)];
void initialize_rf_clock_choices(uint8_t* rf_clock_choices) {
    // Populate the rf_clock_choices array
    for(size_t i = 0; i < COUNT_OF(all_rf_clocks); i++) {
        rf_clock_choices[i] = all_rf_clocks[i].rf_clock_num;
    }
}

char* modulation_names[COUNT_OF(all_mods)];
void initialize_mod_names(char** modulation_names) {
    // Populate the modulation_names array
    for(size_t i = 0; i < COUNT_OF(all_mods); i++) {
        modulation_names[i] = all_mods[i].modulation_name;
    }
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t t5577_writer_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id
*/
static uint32_t t5577_writer_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return T5577WriterViewSubmenu;
}

static uint32_t t5577_writer_navigation_config_e_callback(void* _context) {
    UNUSED(_context);
    return T5577WriterViewConfigure_e;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - T5577WriterApp object.
 * @param      index     The T5577WriterSubmenuIndex item that was clicked.
*/
static void t5577_writer_submenu_callback(void* context, uint32_t index) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    switch(index) {
    case T5577WriterSubmenuIndexLoad:
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewLoad);
        break;
    case T5577WriterSubmenuIndexSave:
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewSave);
        break;
    case T5577WriterSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewConfigure_e);
        break;
    case T5577WriterSubmenuIndexWrite:
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewWrite);
        break;
    case T5577WriterSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewAbout);
        break;
    default:
        break;
    }
}

static const char* modulation_config_label = "Modulation";
static void t5577_writer_modulation_change(VariableItem* item) {
    T5577WriterApp* app = variable_item_get_context(item);
    T5577WriterModel* model = view_get_model(app->view_write);
    if(model->data_loaded[0]) {
        variable_item_set_current_value_index(item, model->modulation_index);
    } else {
        uint8_t modulation_index = variable_item_get_current_value_index(item);
        model->modulation_index = modulation_index;
        model->modulation = all_mods[modulation_index];
    }
    model->data_loaded[0] = false;
    variable_item_set_current_value_text(item, modulation_names[model->modulation_index]);
}

static const char* rf_clock_config_label = "RF Clock";
static void t5577_writer_rf_clock_change(VariableItem* item) {
    T5577WriterApp* app = variable_item_get_context(item);
    T5577WriterModel* model = view_get_model(app->view_write);
    if(model->data_loaded[1]) {
        variable_item_set_current_value_index(item, model->rf_clock_index);
    } else {
        uint8_t rf_clock_index = variable_item_get_current_value_index(item);
        model->rf_clock_index = rf_clock_index;
        model->rf_clock = all_rf_clocks[rf_clock_index];
    }
    model->data_loaded[1] = false;
    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%u", rf_clock_choices[model->rf_clock_index]);
    variable_item_set_current_value_text(item, furi_string_get_cstr(buffer));
    furi_string_free(buffer);
}

static const char* user_block_num_config_label = "Max User Block";
static void t5577_writer_user_block_num_change(VariableItem* item) {
    T5577WriterApp* app = variable_item_get_context(item);
    T5577WriterModel* model = view_get_model(app->view_write);
    if(model->data_loaded[2]) {
        variable_item_set_current_value_index(item, model->user_block_num);
    } else {
        uint8_t user_block_num_index = variable_item_get_current_value_index(item);
        model->user_block_num = user_block_num_index;
    }
    model->data_loaded[2] = false;
    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%u", model->user_block_num);
    variable_item_set_current_value_text(item, furi_string_get_cstr(buffer));
    for(uint8_t i = model->user_block_num + 1; i < LFRFID_T5577_BLOCK_COUNT; i++) {
        model->content[i] = 0; // pad the unneeded blocks with zeros
    }
    furi_string_free(buffer);
}

static const char* edit_block_slc_config_label = "Edit Block";
static void t5577_writer_edit_block_slc_change(VariableItem* item) {
    T5577WriterApp* app = variable_item_get_context(item);
    T5577WriterModel* model = view_get_model(app->view_write);
    uint8_t edit_block_slc_index = variable_item_get_current_value_index(item);
    model->edit_block_slc = edit_block_slc_index + 1;
    variable_item_set_current_value_index(item, model->edit_block_slc - 1);
    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%u", model->edit_block_slc);
    variable_item_set_current_value_text(item, furi_string_get_cstr(buffer));

    furi_string_printf(buffer, "%08lX", model->content[model->edit_block_slc]);
    variable_item_set_current_value_text(app->byte_buffer_item, furi_string_get_cstr(buffer));

    furi_string_free(buffer);
}

static const char* tag_name_entry_text = "Enter name";
static const char* tag_name_default_value = "Tag_1";
static void t5577_writer_file_saver(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* model = view_get_model(app->view_write);
    model->content[0] = 0; // clean up first block before deciding to write or save
    model->content[0] |= model->modulation.mod_page_zero;
    model->content[0] |= model->rf_clock.clock_page_zero;
    model->content[0] |= (model->user_block_num << LFRFID_T5577_MAXBLOCK_SHIFT);
    bool redraw = true;
    with_view_model(
        app->view_write,
        T5577WriterModel * model,
        { furi_string_set(model->tag_name_str, app->temp_buffer); },
        redraw);
    FuriString* file_path = furi_string_alloc();
    furi_string_printf(
        file_path,
        "%s/%s%s",
        STORAGE_APP_DATA_PATH_PREFIX,
        furi_string_get_cstr(model->tag_name_str),
        T5577_WRITER_FILE_EXTENSION);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    FuriString* buffer = furi_string_alloc();
    FlipperFormat* format = flipper_format_file_alloc(storage);
    do {
        const uint32_t version = 2;
        const uint32_t clock_buffer = (uint32_t)model->rf_clock.rf_clock_num;
        const uint32_t block_num_buffer = (uint32_t)model->user_block_num;
        if(!flipper_format_file_open_always(format, furi_string_get_cstr(file_path))) break;
        if(!flipper_format_write_header_cstr(format, "Flipper T5577 Raw File", version)) break;
        if(!flipper_format_write_string_cstr(
               format, "Modulation", model->modulation.modulation_name))
            break;
        if(!flipper_format_write_uint32(format, "RF Clock", &clock_buffer, 1)) break;
        if(!flipper_format_write_uint32(format, "Max User Block", &block_num_buffer, 1)) break;
        if(!flipper_format_write_string_cstr(format, "Raw Data", "")) break; // raw data begins
        for(int i = 0; i < LFRFID_T5577_BLOCK_COUNT; i++) {
            furi_string_printf(buffer, "Block %u", i);
            uint8_t byte_array_buffer[app->bytes_count];
            uint32_to_byte_buffer(model->content[i], byte_array_buffer);
            if(!flipper_format_write_hex(
                   format, furi_string_get_cstr(buffer), byte_array_buffer, app->bytes_count))
                break;
        }
        // signal that the file was written successfully
    } while(0);
    flipper_format_free(format);

    view_dispatcher_switch_to_view(
        app->view_dispatcher, T5577WriterViewSubmenu); // maybe add a pop up later
}

void t5577_writer_update_config_from_load(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* my_model = view_get_model(app->view_write);
    for(size_t i = 0; i < COUNT_OF(all_mods); i++) {
        if((my_model->content[0] & all_mods[i].mod_page_zero) == all_mods[i].mod_page_zero) {
            my_model->modulation_index = (uint8_t)i;
            my_model->modulation = all_mods[my_model->modulation_index];
        }
    }

    for(size_t i = 0; i < COUNT_OF(all_rf_clocks); i++) {
        if((my_model->content[0] & all_rf_clocks[i].clock_page_zero) ==
           all_rf_clocks[i].clock_page_zero) {
            my_model->rf_clock_index = (uint8_t)i;
            my_model->rf_clock = all_rf_clocks[my_model->rf_clock_index];
        }
    }
    my_model->user_block_num = ((my_model->content[0] >> LFRFID_T5577_MAXBLOCK_SHIFT) & 0x7);
    FURI_LOG_D(TAG, "BLOCK 0 %08lX", my_model->content[0]);
    FURI_LOG_D(TAG, "bit 25-27 %ld", (my_model->content[0] >> LFRFID_T5577_MAXBLOCK_SHIFT) & 0x7);
    memset(my_model->data_loaded, true, sizeof(my_model->data_loaded)); // Everything is loaded
}

static const char* edit_block_data_config_label = "Block Data";

static void t5577_writer_content_byte_input_confirmed(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* my_model = view_get_model(app->view_write);
    my_model->content[my_model->edit_block_slc] = byte_buffer_to_uint32(app->bytes_buffer);
    view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewConfigure_e);
}

static void t5577_writer_content_byte_changed(void* context) {
    UNUSED(context);
}

static void t5577_writer_config_item_clicked(void* context, uint32_t index) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* my_model = view_get_model(app->view_write);
    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "Enter Block %u Data", my_model->edit_block_slc);
    // Our hex input UI is the 5th in the config menue.
    if(index == 4) {
        // Header to display on the text input screen.
        byte_input_set_header_text(app->byte_input, furi_string_get_cstr(buffer));

        // Copy the current name into the temporary buffer.
        bool redraw = false;
        with_view_model(
            app->view_write,
            T5577WriterModel * model,
            { uint32_to_byte_buffer(model->content[model->edit_block_slc], app->bytes_buffer); },
            redraw);

        // Configure the text input.  When user enters text and clicks OK, key_copier_setting_text_updated be called.
        byte_input_set_result_callback(
            app->byte_input,
            t5577_writer_content_byte_input_confirmed,
            t5577_writer_content_byte_changed,
            app,
            app->bytes_buffer,
            app->bytes_count);

        // Pressing the BACK button will reload the configure screen.
        view_set_previous_callback(
            byte_input_get_view(app->byte_input), t5577_writer_navigation_config_e_callback);

        // Show text input dialog.
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewByteInput);
    }
}
static void t5577_writer_config_enter_callback(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* my_model = view_get_model(app->view_write);
    variable_item_list_reset(app->variable_item_list_config);
    // Recreate this view every time we enter it so that it's always updated
    app->mod_item = variable_item_list_add(
        app->variable_item_list_config,
        modulation_config_label,
        COUNT_OF(modulation_names),
        t5577_writer_modulation_change,
        app);
    app->clock_item = variable_item_list_add(
        app->variable_item_list_config,
        rf_clock_config_label,
        COUNT_OF(rf_clock_choices),
        t5577_writer_rf_clock_change,
        app);
    app->block_num_item = variable_item_list_add(
        app->variable_item_list_config,
        user_block_num_config_label,
        LFRFID_T5577_BLOCK_COUNT,
        t5577_writer_user_block_num_change,
        app);
    app->block_slc_item = variable_item_list_add(
        app->variable_item_list_config,
        edit_block_slc_config_label,
        LFRFID_T5577_BLOCK_COUNT - 1,
        t5577_writer_edit_block_slc_change,
        app);
    app->byte_buffer_item = variable_item_list_add(
        app->variable_item_list_config, edit_block_data_config_label, 1, NULL, app);
    variable_item_list_set_enter_callback(
        app->variable_item_list_config, t5577_writer_config_item_clicked, app);

    View* view_config_i = variable_item_list_get_view(app->variable_item_list_config);

    variable_item_set_current_value_index(app->mod_item, my_model->modulation_index);
    variable_item_set_current_value_index(app->clock_item, my_model->rf_clock_index);
    variable_item_set_current_value_index(app->block_num_item, my_model->user_block_num);
    variable_item_set_current_value_index(app->block_slc_item, my_model->edit_block_slc - 1);

    t5577_writer_modulation_change(app->mod_item);
    t5577_writer_rf_clock_change(app->clock_item);
    t5577_writer_user_block_num_change(app->block_num_item);
    t5577_writer_edit_block_slc_change(app->block_slc_item);
    view_set_previous_callback(view_config_i, t5577_writer_navigation_submenu_callback);
    view_dispatcher_remove_view(
        app->view_dispatcher, T5577WriterViewConfigure_i); // delete the last one
    view_dispatcher_add_view(app->view_dispatcher, T5577WriterViewConfigure_i, view_config_i);
    view_dispatcher_switch_to_view(
        app->view_dispatcher, T5577WriterViewConfigure_i); // recreate it
}

void t5577_writer_view_load_callback(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* model = view_get_model(app->view_write);
    DialogsFileBrowserOptions browser_options;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    dialog_file_browser_set_basic_options(&browser_options, T5577_WRITER_FILE_EXTENSION, &I_icon);
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;
    furi_string_set(app->file_path, browser_options.base_path);
    FuriString* buffer = furi_string_alloc();
    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options)) {
        FlipperFormat* format = flipper_format_file_alloc(storage);
        do {
            if(!flipper_format_file_open_existing(format, furi_string_get_cstr(app->file_path)))
                break;
            uint8_t byte_array_buffer[app->bytes_count];
            for(int i = 0; i < LFRFID_T5577_BLOCK_COUNT; i++) {
                furi_string_printf(buffer, "Block %u", i);
                if(!flipper_format_read_hex(
                       format, furi_string_get_cstr(buffer), byte_array_buffer, app->bytes_count))
                    break;
                model->content[i] = byte_buffer_to_uint32(
                    byte_array_buffer); // we only extract the raw data. configs are then updated from block 0
            }
            // signal that the file was read successfully
        } while(0);
        flipper_format_free(format);
        t5577_writer_update_config_from_load(app);
        furi_record_close(RECORD_STORAGE);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewSubmenu);
}

/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the text input screen.
 * @param      context  The context - T5577WriterApp object.
 * @param      index - The index of the item that was clicked.
*/
static void t5577_writer_view_save_callback(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    // Header to display on the text input screen.
    text_input_set_header_text(app->text_input, tag_name_entry_text);
    // Copy the current name into the temporary buffer.
    bool redraw = false;
    with_view_model(
        app->view_write,
        T5577WriterModel * model,
        {
            strncpy(
                app->temp_buffer,
                furi_string_get_cstr(model->tag_name_str),
                app->temp_buffer_size);
        },
        redraw);
    // Configure the text input.  When user enters text and clicks OK, t5577_writer_setting_text_updated be called.
    bool clear_previous_text = false;
    text_input_set_result_callback(
        app->text_input,
        t5577_writer_file_saver,
        app,
        app->temp_buffer,
        app->temp_buffer_size,
        clear_previous_text);
    // Pressing the BACK button will return to the main menu.
    view_set_previous_callback(
        text_input_get_view(app->text_input), t5577_writer_navigation_submenu_callback);

    // Show text input dialog.
    view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewTextInput);
}

static void t5577_writer_actual_writing(void* model) {
    T5577WriterModel* my_model = (T5577WriterModel*)model;
    my_model->content[0] = 0; // clear up block 0
    my_model->content[0] |= my_model->modulation.mod_page_zero;
    my_model->content[0] |= my_model->rf_clock.clock_page_zero;
    my_model->content[0] |= (my_model->user_block_num << LFRFID_T5577_MAXBLOCK_SHIFT);
    LFRFIDT5577* data = (LFRFIDT5577*)malloc(sizeof(LFRFIDT5577));
    data->blocks_to_write = my_model->user_block_num + 1;
    for(size_t i = 0; i < data->blocks_to_write; i++) {
        data->block[i] = my_model->content[i];
    }
    t5577_write(data);
    free(data);
}

/**
 * @brief      Callback for drawing the writing screen.
 * @details    This function is called when the screen needs to be redrawn, so that the writing command is repeated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void t5577_writer_view_write_callback(Canvas* canvas, void* model) {
    T5577WriterModel* my_model = (T5577WriterModel*)model;
    if(my_model->writing_repeat_times < MAX_REPEAT_WRITING_FRAMES) {
        t5577_writer_actual_writing(model);
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 0, 8, &I_NFC_manual_60x50);
        canvas_draw_str_aligned(canvas, 97, 15, AlignCenter, AlignTop, "Writing");
        canvas_draw_str_aligned(canvas, 94, 27, AlignCenter, AlignTop, "Hold card next");
        canvas_draw_str_aligned(canvas, 93, 39, AlignCenter, AlignTop, "to Flipper's back");
    } else {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 0, 9, &I_DolphinSuccess_91x55);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 75, 16, "Finished!");
    }
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - T5577WriterApp object.
*/
static void t5577_writer_view_write_timer_callback(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* model = view_get_model(app->view_write);
    if(model->writing_repeat_times < MAX_REPEAT_WRITING_FRAMES + ENDING_WRITING_ICON_FRAMES) {
        model->writing_repeat_times += 1;
        view_dispatcher_send_custom_event(app->view_dispatcher, T5577WriterEventIdRepeatWriting);
        if(model->writing_repeat_times == MAX_REPEAT_WRITING_FRAMES) {
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_success);
        }
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, T5577WriterEventIdMaxWriteRep);
    }
}

/**
 * @brief      Callback when the user starts the writing screen.
 * @details    This function is called when the user enters the writing screen.  We start a timer to
 *           redraw the screen periodically. (exactly like PsychToolBox lol)
 * @param      context  The context - T5577WriterApp object.
*/
static void t5577_writer_view_write_enter_callback(void* context) {
    uint32_t repeat_writing_period = furi_ms_to_ticks(200);
    T5577WriterApp* app = (T5577WriterApp*)context;
    furi_assert(app->timer == NULL);
    app->timer =
        furi_timer_alloc(t5577_writer_view_write_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, repeat_writing_period);
    dolphin_deed(DolphinDeedRfidEmulate);
    notification_message(app->notifications, &sequence_blink_start_magenta);
}

/**
 * @brief      Callback when the user exits the writing screen.
 * @details    This function is called when the user exits the writing screen.  We stop the timer.
 * @param      context  The context - T5577WriterApp object.
*/
static void t5577_writer_view_write_exit_callback(void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    T5577WriterModel* model = view_get_model(app->view_write);
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
    model->writing_repeat_times = 0;
    notification_message(app->notifications, &sequence_blink_stop);
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - T5577WriterEventId value.
 * @param      context  The context - T5577WriterApp object.
*/
static bool t5577_writer_view_write_custom_event_callback(uint32_t event, void* context) {
    T5577WriterApp* app = (T5577WriterApp*)context;
    switch(event) {
    case T5577WriterEventIdRepeatWriting:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->view_write, T5577WriterModel * _model, { UNUSED(_model); }, redraw);
            return true;
        }
    case T5577WriterEventIdMaxWriteRep:
        // Process the OK button.  We go to the saving scene.
        view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewSubmenu);
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Allocate the t5577_writer application.
 * @details    This function allocates the t5577_writer application resources.
 * @return     T5577WriterApp object.
*/
static T5577WriterApp* t5577_writer_app_alloc() {
    T5577WriterApp* app = (T5577WriterApp*)malloc(sizeof(T5577WriterApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->file_path = furi_string_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Write", T5577WriterSubmenuIndexWrite, t5577_writer_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "Config",
        T5577WriterSubmenuIndexConfigure,
        t5577_writer_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "Save", T5577WriterSubmenuIndexSave, t5577_writer_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Load", T5577WriterSubmenuIndexLoad, t5577_writer_submenu_callback, app);
    submenu_add_item(
        app->submenu, "About", T5577WriterSubmenuIndexAbout, t5577_writer_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), t5577_writer_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, T5577WriterViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, T5577WriterViewSubmenu);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, T5577WriterViewTextInput, text_input_get_view(app->text_input));

    app->view_load = view_alloc();
    view_set_previous_callback(app->view_load, t5577_writer_navigation_submenu_callback);
    view_set_enter_callback(app->view_load, t5577_writer_view_load_callback);
    view_set_context(app->view_load, app);
    view_dispatcher_add_view(app->view_dispatcher, T5577WriterViewLoad, app->view_load);

    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->view_write = view_alloc();
    view_set_draw_callback(app->view_write, t5577_writer_view_write_callback);
    view_set_previous_callback(app->view_write, t5577_writer_navigation_submenu_callback);
    view_set_enter_callback(app->view_write, t5577_writer_view_write_enter_callback);
    view_set_exit_callback(app->view_write, t5577_writer_view_write_exit_callback);
    view_set_context(app->view_write, app);
    view_set_custom_callback(app->view_write, t5577_writer_view_write_custom_event_callback);
    view_allocate_model(app->view_write, ViewModelTypeLockFree, sizeof(T5577WriterModel));
    view_dispatcher_add_view(app->view_dispatcher, T5577WriterViewWrite, app->view_write);

    T5577WriterModel* model = view_get_model(app->view_write); // initialize model

    FuriString* tag_name_str = furi_string_alloc();
    furi_string_set_str(tag_name_str, tag_name_default_value);

    model->tag_name_str = tag_name_str;
    initialize_model(model);
    initialize_rf_clock_choices(rf_clock_choices);
    initialize_mod_names(modulation_names);

    app->view_save = view_alloc();
    view_set_previous_callback(app->view_save, t5577_writer_navigation_submenu_callback);
    view_set_enter_callback(app->view_save, t5577_writer_view_save_callback);
    view_set_context(app->view_save, app);
    view_dispatcher_add_view(app->view_dispatcher, T5577WriterViewSave, app->view_save);

    app->bytes_count = 4;
    memset(app->bytes_buffer, 0, sizeof(app->bytes_buffer));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, T5577WriterViewByteInput, byte_input_get_view(app->byte_input));

    app->variable_item_list_config = variable_item_list_alloc();
    app->view_config_e = view_alloc();
    view_set_previous_callback(app->view_config_e, t5577_writer_navigation_submenu_callback);
    view_set_enter_callback(app->view_config_e, t5577_writer_config_enter_callback);
    view_set_context(app->view_config_e, app);
    view_dispatcher_add_view(app->view_dispatcher, T5577WriterViewConfigure_e, app->view_config_e);

    View* view_buffer = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, T5577WriterViewConfigure_i, view_buffer);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "T5577 Raw Writer v1.2\n\nAuthor: @Torron\n\nGithub: https://github.com/zinongli/T5577_Raw_Writer");
    view_set_previous_callback(
        widget_get_view(app->widget_about), t5577_writer_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, T5577WriterViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    return app;
}

/**
 * @brief      Free the t5577_writer application.
 * @details    This function frees the t5577_writer application resources.
 * @param      app  The t5577_writer application object.
*/
static void t5577_writer_app_free(T5577WriterApp* app) {
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewTextInput);
    text_input_free(app->text_input);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewWrite);
    view_free(app->view_write);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewLoad);
    view_free(app->view_load);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewConfigure_i);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewConfigure_e);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewByteInput);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewSave);
    view_free(app->view_save);
    view_dispatcher_remove_view(app->view_dispatcher, T5577WriterViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/**
 * @brief      Main function for t5577_writer application.
 * @details    This function is the entry point for the t5577_writer application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t main_t5577_writer_app(void* _p) {
    UNUSED(_p);

    T5577WriterApp* app = t5577_writer_app_alloc();

    view_dispatcher_run(app->view_dispatcher);

    t5577_writer_app_free(app);
    return 0;
}
