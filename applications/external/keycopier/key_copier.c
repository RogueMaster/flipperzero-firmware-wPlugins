#include "key_copier.h"

void exit_widget_callback(GuiButtonType result, InputType type, void* context) {
    KeyCopierApp* app = context;
    UNUSED(result);
    if(type == InputTypeShort) {
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSubmenu);
    }
}

void initialize_model(KeyCopierModel* model) {
    if(model->depth != NULL) {
        free(model->depth);
    }
    model->format_index = 0;
    memcpy(&model->format, &all_formats[model->format_index], sizeof(KeyFormat));
    model->depth = (uint8_t*)malloc((model->format.pin_num + 1) * sizeof(uint8_t));
    for(uint8_t i = 0; i <= model->format.pin_num; i++) {
        model->depth[i] = model->format.min_depth_ind;
    }
    model->pin_slc = 1;
    model->data_loaded = 0;
    model->key_name_str = furi_string_alloc();
}

static uint32_t key_copier_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

static uint32_t key_copier_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return KeyCopierViewSubmenu;
}

static uint32_t key_copier_navigation_manufacturer_list_callback(void* _context) {
    UNUSED(_context);
    return KeyCopierViewManufacturerList;
}

static void key_copier_submenu_callback(void* context, uint32_t index) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    switch(index) {
    case KeyCopierSubmenuIndexMeasure:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewMeasure);
        break;
    case KeyCopierSubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewConfigure_e);
        break;
    case KeyCopierSubmenuIndexSave:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSave);
        break;
    case KeyCopierSubmenuIndexLoad:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewLoad);
        break;
    case KeyCopierSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewAbout);
        break;
    case KeyCopierSubmenuIndexQRCode:
        view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewQRCode);
        break;
    default:
        break;
    }
}

char* manufacturers[COUNT_OF(all_formats)];
void initialize_manufacturers(char** manufacturers) {
    // Populate the manufacturers array
    for(size_t i = 0; i < COUNT_OF(all_formats); i++) {
        manufacturers[i] = all_formats[i].manufacturer;
    }
}

static void manufacturer_selected_callback(void* context, uint32_t index);
static void format_selected_callback(void* context, uint32_t index);

static void key_copier_config_enter_callback(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;

    // Clear manufacturer list
    submenu_reset(app->manufacturer_list);

    // Track added manufacturers to avoid duplicates
    char* added_manufacturers[COUNT_OF(all_formats)];
    size_t added_count = 0;

    // Add unique manufacturers
    for(size_t i = 0; i < COUNT_OF(all_formats); i++) {
        bool already_added = false;
        for(size_t j = 0; j < added_count; j++) {
            if(strcmp(all_formats[i].manufacturer, added_manufacturers[j]) == 0) {
                already_added = true;
                break;
            }
        }

        if(!already_added) {
            submenu_add_item(
                app->manufacturer_list,
                all_formats[i].manufacturer,
                i,
                manufacturer_selected_callback,
                app);
            added_manufacturers[added_count++] = all_formats[i].manufacturer;
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewManufacturerList);
}

static const char* key_name_entry_text = "Enter name";
static void key_copier_file_saver(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    KeyCopierModel* model = view_get_model(app->view_measure);
    bool redraw = true;
    with_view_model(
        app->view_measure,
        KeyCopierModel * model,
        { furi_string_set(model->key_name_str, app->temp_buffer); },
        redraw);
    FuriString* file_path = furi_string_alloc();
    furi_string_printf(
        file_path,
        "%s/%s%s",
        STORAGE_APP_DATA_PATH_PREFIX,
        furi_string_get_cstr(model->key_name_str),
        KEY_COPIER_FILE_EXTENSION);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    FURI_LOG_D(TAG, "mkdir finished");
    FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
    do {
        const uint32_t version = 1;
        const uint32_t pin_num_buffer = (uint32_t)model->format.pin_num;
        const uint32_t macs_buffer = (uint32_t)model->format.macs;
        FuriString* buffer = furi_string_alloc();
        if(!flipper_format_file_open_always(flipper_format, furi_string_get_cstr(file_path)))
            break;
        if(!flipper_format_write_header_cstr(flipper_format, "Flipper Key Copier File", version))
            break;
        if(!flipper_format_write_string_cstr(
               flipper_format, "Manufacturer", model->format.manufacturer))
            break;
        if(!flipper_format_write_string_cstr(
               flipper_format, "Format Name", model->format.format_name))
            break;
        if(!flipper_format_write_string_cstr(
               flipper_format, "Data Sheet", model->format.format_link))
            break;
        if(!flipper_format_write_uint32(flipper_format, "Number of Pins", &pin_num_buffer, 1))
            break;
        if(!flipper_format_write_uint32(
               flipper_format, "Maximum Adjacent Cut Specification (MACS)", &macs_buffer, 1))
            break;
        for(int i = 0; i < model->format.pin_num; i++) {
            if(i < model->format.pin_num - 1) {
                furi_string_cat_printf(buffer, "%d-", model->depth[i]);
            } else {
                furi_string_cat_printf(buffer, "%d", model->depth[i]);
            }
        }
        if(!flipper_format_write_string(flipper_format, "Bitting Pattern", buffer)) break;
        furi_string_free(buffer);
        // signal that the file was written successfully
    } while(0);
    flipper_format_free(flipper_format);

    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSubmenu);
}

static void key_copier_view_save_callback(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    // Header to display on the text input screen.
    text_input_set_header_text(app->text_input, key_name_entry_text);

    // Copy the current name into the temporary buffer.
    bool redraw = false;
    with_view_model(
        app->view_measure,
        KeyCopierModel * model,
        {
            strncpy(
                app->temp_buffer,
                furi_string_get_cstr(model->key_name_str),
                app->temp_buffer_size);
        },
        redraw);

    // Configure the text input.  When user enters text and clicks OK, key_copier_file_saver be called.
    bool clear_previous_text = false;
    text_input_set_result_callback(
        app->text_input,
        key_copier_file_saver,
        app,
        app->temp_buffer,
        app->temp_buffer_size,
        clear_previous_text);

    view_set_previous_callback(
        text_input_get_view(app->text_input), key_copier_navigation_submenu_callback);

    // Show text input dialog.
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewTextInput);
}

static void key_copier_view_load_callback(void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    KeyCopierModel* model = view_get_model(app->view_measure);
    DialogsFileBrowserOptions browser_options;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    dialog_file_browser_set_basic_options(&browser_options, KEY_COPIER_FILE_EXTENSION, &I_icon);
    browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;
    furi_string_set(app->file_path, browser_options.base_path);
    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &browser_options)) {
        FlipperFormat* flipper_format = flipper_format_file_alloc(storage);
        do {
            if(!flipper_format_file_open_existing(
                   flipper_format, furi_string_get_cstr(app->file_path)))
                break;
            FuriString* format_buffer = furi_string_alloc();
            FuriString* depth_buffer = furi_string_alloc();
            if(!flipper_format_read_string(flipper_format, "Format Name", format_buffer)) break;
            if(!flipper_format_read_string(flipper_format, "Bitting Pattern", depth_buffer)) break;
            for(size_t i = 0; i < COUNT_OF(all_formats); i++) {
                if(!strcmp(furi_string_get_cstr(format_buffer), all_formats[i].format_name)) {
                    model->format_index = (uint32_t)i;
                    model->format = all_formats[model->format_index];
                }
            }

            for(int i = 0; i < model->format.pin_num; i++) {
                model->depth[i] = (uint8_t)(furi_string_get_char(depth_buffer, i * 2) - '0');
            }
            model->data_loaded = true;
            // signal that the file was read successfully
        } while(0);
        flipper_format_free(flipper_format);
        furi_record_close(RECORD_STORAGE);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSubmenu);
}

static void key_copier_view_measure_draw_callback(Canvas* canvas, void* model) {
    static double inches_per_px = (double)INCHES_PER_PX;
    canvas_set_bitmap_mode(canvas, true);
    KeyCopierModel* my_model = (KeyCopierModel*)model;
    KeyFormat my_format = my_model->format;
    FuriString* buffer = furi_string_alloc();
    int pin_half_width_px = (int)round((my_format.pin_width_inch / inches_per_px) / 2);
    int pin_step_px = (int)round(my_format.pin_increment_inch / inches_per_px);
    double drill_radians =
        (180 - my_format.drill_angle) / 2 / 180 * (double)M_PI; // Convert angle to radians
    double tangent = tan(drill_radians);
    int top_contour_px = (int)round(62 - my_format.uncut_depth_inch / inches_per_px);
    int bottom_contour_px = 0;

    if(my_format.sides == 2)
        bottom_contour_px =
            top_contour_px + (int)round(my_format.uncut_depth_inch / inches_per_px);
    int post_extra_x_px = 0;
    int pre_extra_x_px = 0;
    int bottom_post_extra_x_px = 0; // new
    int bottom_pre_extra_x_px = 0; // new
    int level_contour_px =
        (int)round((my_format.last_pin_inch + my_format.elbow_inch) / inches_per_px);
    for(int current_pin = 1; current_pin <= my_model->format.pin_num; current_pin += 1) {
        double current_center_px =
            my_format.first_pin_inch + (current_pin - 1) * my_format.pin_increment_inch;
        int pin_center_px = (int)round(current_center_px / inches_per_px);

        furi_string_printf(buffer, "%d", my_model->depth[current_pin - 1]);
        canvas_draw_str_aligned(
            canvas,
            pin_center_px,
            top_contour_px - 12,
            AlignCenter,
            AlignCenter,
            furi_string_get_cstr(buffer));

        canvas_draw_line(
            canvas,
            pin_center_px,
            top_contour_px - 5,
            pin_center_px,
            top_contour_px); // the vertical line to indicate pin center
        int current_depth = my_model->depth[current_pin - 1] - my_format.min_depth_ind;
        int current_depth_px =
            (int)round(current_depth * my_format.depth_step_inch / inches_per_px);
        canvas_draw_line(
            canvas,
            pin_center_px - pin_half_width_px,
            top_contour_px + current_depth_px,
            pin_center_px + pin_half_width_px,
            top_contour_px + current_depth_px); // draw top pin width horizontal line

        if(my_format.sides == 2) { // new
            int last_depth = my_model->depth[current_pin - 2] - my_format.min_depth_ind;
            int next_depth = my_model->depth[current_pin] - my_format.min_depth_ind;
            int current_depth = my_model->depth[current_pin - 1] - my_format.min_depth_ind;
            int current_depth_px =
                (int)round(current_depth * my_format.depth_step_inch / inches_per_px);

            // Draw horizontal line for bottom pin
            canvas_draw_line(
                canvas,
                pin_center_px - pin_half_width_px,
                bottom_contour_px - current_depth_px,
                pin_center_px + pin_half_width_px,
                bottom_contour_px - current_depth_px);

            // Handle first pin for bottom
            if(current_pin == 1) {
                canvas_draw_line(
                    canvas,
                    0,
                    bottom_contour_px,
                    pin_center_px - pin_half_width_px - current_depth_px,
                    bottom_contour_px);
                last_depth = 0;
                bottom_pre_extra_x_px = max(current_depth_px + pin_half_width_px, 0);
            }

            // Handle left side intersection for bottom
            if((last_depth + current_depth) > my_format.clearance) {
                if(current_pin != 1) {
                    bottom_pre_extra_x_px =
                        min(max(pin_step_px - bottom_post_extra_x_px, pin_half_width_px),
                            pin_step_px - pin_half_width_px);
                }
                canvas_draw_line(
                    canvas,
                    pin_center_px - bottom_pre_extra_x_px,
                    bottom_contour_px -
                        max((int)round(
                                (current_depth_px - (bottom_pre_extra_x_px - pin_half_width_px)) *
                                tangent),
                            0),
                    pin_center_px - pin_half_width_px,
                    bottom_contour_px - (int)round(current_depth_px * tangent));
            } else {
                int last_depth_px =
                    (int)round(last_depth * my_format.depth_step_inch / inches_per_px);
                int up_slope_start_x_px = pin_center_px - pin_half_width_px - current_depth_px;
                canvas_draw_line(
                    canvas,
                    pin_center_px - pin_half_width_px - current_depth_px,
                    bottom_contour_px,
                    pin_center_px - pin_half_width_px,
                    bottom_contour_px - (int)round(current_depth_px * tangent));
                canvas_draw_line(
                    canvas,
                    min(pin_center_px - pin_step_px + pin_half_width_px + last_depth_px,
                        up_slope_start_x_px),
                    bottom_contour_px,
                    up_slope_start_x_px,
                    bottom_contour_px);
            }

            // Handle right side intersection for bottom
            if((current_depth + next_depth) > my_format.clearance) {
                double numerator = (double)current_depth;
                double denominator = (double)(current_depth + next_depth);
                double product = (numerator / denominator) * pin_step_px;
                bottom_post_extra_x_px =
                    (int)min(max(product, pin_half_width_px), pin_step_px - pin_half_width_px);
                canvas_draw_line(
                    canvas,
                    pin_center_px + pin_half_width_px,
                    bottom_contour_px - current_depth_px,
                    pin_center_px + bottom_post_extra_x_px,
                    bottom_contour_px -
                        max(current_depth_px -
                                (int)round((bottom_post_extra_x_px - pin_half_width_px) * tangent),
                            0));
            } else {
                canvas_draw_line(
                    canvas,
                    pin_center_px + pin_half_width_px,
                    bottom_contour_px - (int)round(current_depth_px * tangent),
                    pin_center_px + pin_half_width_px + current_depth_px,
                    bottom_contour_px);
            }
        }
        // new end

        int last_depth = my_model->depth[current_pin - 2] - my_format.min_depth_ind;
        int next_depth = my_model->depth[current_pin] - my_format.min_depth_ind;
        if(current_pin == 1) {
            canvas_draw_line(
                canvas,
                0,
                top_contour_px,
                pin_center_px - pin_half_width_px - current_depth_px,
                top_contour_px); // draw top shoulder
            last_depth = 0;
            pre_extra_x_px = max(current_depth_px + pin_half_width_px, 0);
            if(my_format.sides == 2) {
                canvas_draw_line(
                    canvas,
                    0,
                    bottom_contour_px,
                    pin_center_px - pin_half_width_px - current_depth_px,
                    bottom_contour_px); // draw bottom shoulder (hidden by level contour)
            } else {
                canvas_draw_line(canvas, 0, 62, level_contour_px, 62);
            }
        }
        if(current_pin == my_model->format.pin_num) {
            next_depth = 0;
        }
        if((last_depth + current_depth) > my_format.clearance) { // yes
            // intersection

            if(current_pin != 1) {
                pre_extra_x_px =
                    min(max(pin_step_px - post_extra_x_px, pin_half_width_px),
                        pin_step_px - pin_half_width_px);
            }
            canvas_draw_line(
                canvas,
                pin_center_px - pre_extra_x_px,
                top_contour_px +
                    max((int)round(
                            (current_depth_px - (pre_extra_x_px - pin_half_width_px)) * tangent),
                        0),
                pin_center_px - pin_half_width_px,
                top_contour_px + (int)round(current_depth_px * tangent));
        } else {
            int last_depth_px = (int)round(last_depth * my_format.depth_step_inch / inches_per_px);
            int down_slope_start_x_px = pin_center_px - pin_half_width_px - current_depth_px;
            canvas_draw_line(
                canvas,
                pin_center_px - pin_half_width_px - current_depth_px,
                top_contour_px,
                pin_center_px - pin_half_width_px,
                top_contour_px + (int)round(current_depth_px * tangent));
            canvas_draw_line(
                canvas,
                min(pin_center_px - pin_step_px + pin_half_width_px + last_depth_px,
                    down_slope_start_x_px),
                top_contour_px,
                down_slope_start_x_px,
                top_contour_px);
        }
        if((current_depth + next_depth) > my_format.clearance) { //yes intersection
            double numerator = (double)current_depth;
            double denominator = (double)(current_depth + next_depth);
            double product = (numerator / denominator) * pin_step_px;
            post_extra_x_px =
                (int)min(max(product, pin_half_width_px), pin_step_px - pin_half_width_px);
            canvas_draw_line(
                canvas,
                pin_center_px + pin_half_width_px,
                top_contour_px + current_depth_px,
                pin_center_px + post_extra_x_px,
                top_contour_px +
                    max(current_depth_px -
                            (int)round((post_extra_x_px - pin_half_width_px) * tangent),
                        0));
        } else { // no intersection
            canvas_draw_line(
                canvas,
                pin_center_px + pin_half_width_px,
                top_contour_px + (int)round(current_depth_px * tangent),
                pin_center_px + pin_half_width_px + current_depth_px,
                top_contour_px);
        }
    }

    int elbow_px = (int)round(my_format.elbow_inch / inches_per_px);
    canvas_draw_line(canvas, level_contour_px, 62, level_contour_px + elbow_px, 62 - elbow_px);
    canvas_draw_line(canvas, 0, top_contour_px - 6, 0, top_contour_px);
    if(my_format.stop == 2) {
        // Draw a line using level_contour_px if stop equals 2 elbow must be firt pin inch
        canvas_draw_line(canvas, level_contour_px, top_contour_px, level_contour_px, 63);
        //  } else {
        // Otherwise, draw a default line
        //    canvas_draw_line(canvas, 0, top_contour_px, 0, 63); // too confusing but may want later
    }

    int slc_pin_px = (int)round(
        (my_format.first_pin_inch + (my_model->pin_slc - 1) * my_format.pin_increment_inch) /
        inches_per_px);
    canvas_draw_icon(canvas, slc_pin_px - 2, top_contour_px - 25, &I_arrow_down);

    furi_string_printf(buffer, "%s", my_format.format_name);
    canvas_draw_str(canvas, 100, 10, furi_string_get_cstr(buffer));
    furi_string_free(buffer);
}

static bool key_copier_view_measure_input_callback(InputEvent* event, void* context) {
    KeyCopierApp* app = (KeyCopierApp*)context;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft: {
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierModel * model,
                {
                    if(model->pin_slc > 1) {
                        model->pin_slc--;
                    }
                },
                redraw);
            break;
        }
        case InputKeyRight: {
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierModel * model,
                {
                    if(model->pin_slc < model->format.pin_num) {
                        model->pin_slc++;
                    }
                },
                redraw);
            break;
        }
        case InputKeyUp: {
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierModel * model,
                {
                    if(model->depth[model->pin_slc - 1] > model->format.min_depth_ind) {
                        if(model->pin_slc == 1) { //first pin only limited by the next one
                            if(model->depth[model->pin_slc] - model->depth[model->pin_slc - 1] <
                               model->format.macs)
                                model->depth[model->pin_slc - 1]--;
                        } else if(model->pin_slc == model->format.pin_num) { // last pin only limited by
                            // the previous one
                            if(model->depth[model->pin_slc - 2] -
                                   model->depth[model->pin_slc - 1] <
                               model->format.macs) {
                                model->depth[model->pin_slc - 1]--;
                            }
                        } else {
                            if(model->depth[model->pin_slc] - model->depth[model->pin_slc - 1] <
                                   model->format.macs &&
                               model->depth[model->pin_slc - 2] -
                                       model->depth[model->pin_slc - 1] <
                                   model->format.macs) {
                                model->depth[model->pin_slc - 1]--;
                            }
                        }
                    }
                },
                redraw);
            break;
        }
        case InputKeyDown: {
            bool redraw = true;
            with_view_model(
                app->view_measure,
                KeyCopierModel * model,
                {
                    if(model->depth[model->pin_slc - 1] < model->format.max_depth_ind) {
                        if(model->pin_slc == 1) { //first pin only limited by the next one
                            if(model->depth[model->pin_slc - 1] - model->depth[model->pin_slc] <
                               model->format.macs)
                                model->depth[model->pin_slc - 1]++;
                        } else if(model->pin_slc == model->format.pin_num) { // last pin only limited by
                            // the previous one
                            if(model->depth[model->pin_slc - 1] -
                                   model->depth[model->pin_slc - 2] <
                               model->format.macs) {
                                model->depth[model->pin_slc - 1]++;
                            }
                        } else {
                            if(model->depth[model->pin_slc - 1] - model->depth[model->pin_slc] <
                                   model->format.macs &&
                               model->depth[model->pin_slc - 1] -
                                       model->depth[model->pin_slc - 2] <
                                   model->format.macs) {
                                model->depth[model->pin_slc - 1]++;
                            }
                        }
                    }
                },
                redraw);
            break;
        }
        default:
            // Handle other keys or do nothing
            break;
        }
    }

    return false;
}

static void manufacturer_selected_callback(void* context, uint32_t index) {
    KeyCopierApp* app = context;
    app->selected_manufacturer = all_formats[index].manufacturer;

    // Clear and populate format list for selected manufacturer
    submenu_reset(app->format_list);

    // Add all formats for this manufacturer
    for(size_t i = 0; i < COUNT_OF(all_formats); i++) {
        if(strcmp(all_formats[i].manufacturer, app->selected_manufacturer) == 0) {
            submenu_add_item(
                app->format_list, all_formats[i].format_name, i, format_selected_callback, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewFormatList);
}

static void format_selected_callback(void* context, uint32_t index) {
    KeyCopierApp* app = context;
    KeyCopierModel* model = view_get_model(app->view_measure);

    model->format_index = index;
    model->format = all_formats[index];
    if(model->depth != NULL) {
        free(model->depth);
    }
    model->depth = malloc((model->format.pin_num + 1) * sizeof(uint8_t));
    for(uint8_t i = 0; i <= model->format.pin_num; i++) {
        model->depth[i] = model->format.min_depth_ind;
    }
    model->pin_slc = 1;
    model->data_loaded = false;

    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewMeasure);
}

static KeyCopierApp* key_copier_app_alloc() {
    KeyCopierApp* app = (KeyCopierApp*)malloc(sizeof(KeyCopierApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->file_path = furi_string_alloc();
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Key Copier v1.5");
    submenu_add_item(
        app->submenu,
        "Select Key Format",
        KeyCopierSubmenuIndexConfigure,
        key_copier_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "Measure", KeyCopierSubmenuIndexMeasure, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Save", KeyCopierSubmenuIndexSave, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Load", KeyCopierSubmenuIndexLoad, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Help", KeyCopierSubmenuIndexAbout, key_copier_submenu_callback, app);
    submenu_add_item(
        app->submenu,
        "Video Instruction",
        KeyCopierSubmenuIndexQRCode,
        key_copier_submenu_callback,
        app);

    view_set_previous_callback(
        submenu_get_view(app->submenu), key_copier_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, KeyCopierViewSubmenu);

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewTextInput, text_input_get_view(app->text_input));
    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->view_measure = view_alloc();
    view_set_draw_callback(app->view_measure, key_copier_view_measure_draw_callback);
    view_set_input_callback(app->view_measure, key_copier_view_measure_input_callback);
    view_set_previous_callback(app->view_measure, key_copier_navigation_submenu_callback);
    view_set_context(app->view_measure, app);
    view_allocate_model(app->view_measure, ViewModelTypeLockFree, sizeof(KeyCopierModel));
    KeyCopierModel* model = view_get_model(app->view_measure);

    initialize_model(model);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewMeasure, app->view_measure);

    app->variable_item_list_config = variable_item_list_alloc();
    app->view_config_e = view_alloc();
    view_set_context(app->view_config_e, app);
    view_set_previous_callback(app->view_config_e, key_copier_navigation_submenu_callback);
    view_set_enter_callback(app->view_config_e, key_copier_config_enter_callback);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewConfigure_e, app->view_config_e);

    View* view_buffer = view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewConfigure_i, view_buffer);

    app->view_save = view_alloc();
    view_set_context(app->view_save, app);
    view_set_enter_callback(app->view_save, key_copier_view_save_callback);
    view_set_previous_callback(app->view_save, key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewSave, app->view_save);

    app->view_load = view_alloc();
    view_set_context(app->view_load, app);
    view_set_enter_callback(app->view_load, key_copier_view_load_callback);
    view_set_previous_callback(app->view_load, key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, KeyCopierViewLoad, app->view_load);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Key Maker App 1.5\nAuthor: @Torron\n\nTo measure your key:\n\n1. Place "
        "it on top of the screen.\n\n2. Use the contour to align your key.\n\n3. "
        "Adjust each pin's depth until they match. It's easier if you look with "
        "one eye closed.\n\nGithub: github.com/zinongli/KeyCopier \n\nSpecial "
        "thanks to Derek Jamison's Skeleton App Template.");
    view_set_previous_callback(
        widget_get_view(app->widget_about), key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewAbout, widget_get_view(app->widget_about));

    app->widget_qr_code = widget_alloc();
    widget_add_icon_element(app->widget_qr_code, 92, 7, &I_QR_Code);
    widget_add_string_element(
        app->widget_qr_code, 0, 10, AlignLeft, AlignBottom, FontSecondary, "Check out");
    widget_add_string_element(
        app->widget_qr_code, 0, 23, AlignLeft, AlignBottom, FontSecondary, "@TalkingSasquach's");
    widget_add_string_element(
        app->widget_qr_code, 0, 36, AlignLeft, AlignBottom, FontSecondary, "video from decoding");
    widget_add_string_element(
        app->widget_qr_code, 0, 49, AlignLeft, AlignBottom, FontSecondary, "a key to eventually");
    widget_add_string_element(
        app->widget_qr_code, 0, 62, AlignLeft, AlignBottom, FontSecondary, "3D-printing a copy!");
    widget_add_button_element(
        app->widget_qr_code, GuiButtonTypeRight, "Back", exit_widget_callback, app);
    view_set_previous_callback(
        widget_get_view(app->widget_qr_code), key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewQRCode, widget_get_view(app->widget_qr_code));

    app->manufacturer_list = submenu_alloc();
    view_set_previous_callback(
        submenu_get_view(app->manufacturer_list), key_copier_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        KeyCopierViewManufacturerList,
        submenu_get_view(app->manufacturer_list));

    app->format_list = submenu_alloc();
    view_set_previous_callback(
        submenu_get_view(app->format_list), key_copier_navigation_manufacturer_list_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, KeyCopierViewFormatList, submenu_get_view(app->format_list));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_on);
#endif

    return app;
}

static void key_copier_app_free(KeyCopierApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewTextInput);
    text_input_free(app->text_input);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewQRCode);
    widget_free(app->widget_qr_code);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewMeasure);
    with_view_model(
        app->view_measure,
        KeyCopierModel * model,
        {
            if(model->depth != NULL) {
                free(model->depth);
            }
        },
        false);
    view_free(app->view_measure);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewConfigure_e);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewConfigure_i);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewSave);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewLoad);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewManufacturerList);
    submenu_free(app->manufacturer_list);
    view_dispatcher_remove_view(app->view_dispatcher, KeyCopierViewFormatList);
    submenu_free(app->format_list);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t main_key_copier_app(void* _p) {
    UNUSED(_p);

    KeyCopierApp* app = key_copier_app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    key_copier_app_free(app);
    return 0;
}
