#include "../barcode_app.h"
#include "create_view.h"
#include <math.h>

#define LINE_HEIGHT      16
#define TEXT_PADDING     4
#define TOTAL_MENU_ITEMS 5

typedef enum {
    TypeMenuItem,
    FileNameMenuItem,
    BarcodeDataMenuItem,
    SaveMenuButton,
    DeleteMenuButton
} MenuItems;

/**
 * Took this function from blackjack
 * @author @teeebor
*/
void draw_menu_item(
    Canvas* const canvas,
    const char* text,
    const char* value,
    int y,
    bool left_caret,
    bool right_caret,
    bool selected) {
    UNUSED(selected);
    if(y < 0 || y >= 64) {
        return;
    }

    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 123, LINE_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
    }

    canvas_draw_str_aligned(canvas, 4, y + TEXT_PADDING, AlignLeft, AlignTop, text);
    if(left_caret) {
        canvas_draw_str_aligned(canvas, 60, y + TEXT_PADDING, AlignLeft, AlignTop, "<");
    }

    canvas_draw_str_aligned(canvas, 90, y + TEXT_PADDING, AlignCenter, AlignTop, value);
    if(right_caret) {
        canvas_draw_str_aligned(canvas, 120, y + TEXT_PADDING, AlignRight, AlignTop, ">");
    }

    canvas_set_color(canvas, ColorBlack);
}

void draw_button(Canvas* const canvas, const char* text, int y, bool selected) {
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 123, LINE_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
    }

    canvas_draw_str_aligned(canvas, 64, y + TEXT_PADDING, AlignCenter, AlignTop, text);

    canvas_set_color(canvas, ColorBlack);
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);

    CreateViewModel* create_view_model = ctx;

    BarcodeTypeObj* type_obj = create_view_model->barcode_type;
    if(create_view_model->barcode_type == NULL) {
        return;
    }
    BarcodeType selected_type = type_obj->type;

    int selected_menu_item = create_view_model->selected_menu_item;

    int total_menu_items = create_view_model->mode == EditMode ? TOTAL_MENU_ITEMS :
                                                                 TOTAL_MENU_ITEMS - 1;

    int startY = 0;

    //the menu items index that is/would be in view
    //int current_last_menu_item = selected_menu_item + 3;
    if(selected_menu_item > 1) {
        int offset = 2;
        if(selected_menu_item + offset > total_menu_items) {
            offset = 3;
        }
        startY -= (LINE_HEIGHT * (selected_menu_item - offset));
    }

    //ensure that the scroll height is atleast 1
    int scrollHeight = ceil(64.0 / total_menu_items);
    int scrollPos = scrollHeight * selected_menu_item;

    canvas_set_color(canvas, ColorBlack);
    //draw the scroll bar box
    canvas_draw_box(canvas, 125, scrollPos, 3, scrollHeight);
    //draw the scroll bar track
    canvas_draw_box(canvas, 126, 0, 1, 64);

    draw_menu_item(
        canvas,
        "Type",
        type_obj->name,
        TypeMenuItem * LINE_HEIGHT + startY,
        selected_type > 0,
        selected_type < NUMBER_OF_BARCODE_TYPES - 2,
        selected_menu_item == TypeMenuItem);

    draw_menu_item(
        canvas,
        "Name",
        furi_string_empty(create_view_model->file_name) ?
            "--" :
            furi_string_get_cstr(create_view_model->file_name),
        FileNameMenuItem * LINE_HEIGHT + startY,
        false,
        false,
        selected_menu_item == FileNameMenuItem);

    draw_menu_item(
        canvas,
        "Data",
        furi_string_empty(create_view_model->barcode_data) ?
            "--" :
            furi_string_get_cstr(create_view_model->barcode_data),
        BarcodeDataMenuItem * LINE_HEIGHT + startY,
        false,
        false,
        selected_menu_item == BarcodeDataMenuItem);

    draw_button(
        canvas,
        "Save",
        SaveMenuButton * LINE_HEIGHT + startY,
        selected_menu_item == SaveMenuButton);

    if(create_view_model->mode == EditMode) {
        draw_button(
            canvas,
            "Delete",
            DeleteMenuButton * LINE_HEIGHT + startY,
            selected_menu_item == DeleteMenuButton);
    }
}

void text_input_callback(void* ctx) {
    CreateView* create_view_object = ctx;

    with_view_model(
        create_view_object->view,
        CreateViewModel * model,
        {
            if(create_view_object->setter == FileNameSetter) {
                furi_string_set_str(model->file_name, create_view_object->input);
            }
            if(create_view_object->setter == BarcodeDataSetter) {
                furi_string_set_str(model->barcode_data, create_view_object->input);
            }
        },
        true);

    text_input_show_illegal_symbols(create_view_object->barcode_app->text_input, true);
    view_dispatcher_switch_to_view(
        create_view_object->barcode_app->view_dispatcher, CreateBarcodeView);
}

static bool app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    if(input_event->key == InputKeyBack) {
        return false;
    }

    CreateView* create_view_object = ctx;

    //get the currently selected menu item from the model
    int selected_menu_item = 0;
    BarcodeTypeObj* barcode_type = NULL;
    FuriString* file_name;
    FuriString* barcode_data;
    CreateMode mode;

    with_view_model(
        create_view_object->view,
        CreateViewModel * model,
        {
            selected_menu_item = model->selected_menu_item;
            barcode_type = model->barcode_type;
            file_name = model->file_name;
            barcode_data = model->barcode_data;
            mode = model->mode;
        },
        true);

    int total_menu_items = mode == EditMode ? TOTAL_MENU_ITEMS : TOTAL_MENU_ITEMS - 1;

    if(input_event->type == InputTypePress) {
        if(input_event->key == InputKeyUp && selected_menu_item > 0) {
            selected_menu_item--;
        } else if(input_event->key == InputKeyDown && selected_menu_item < total_menu_items - 1) {
            selected_menu_item++;
        } else if(input_event->key == InputKeyLeft) {
            if(selected_menu_item == TypeMenuItem && barcode_type != NULL) { //Select Barcode Type
                if(barcode_type->type > 0) {
                    barcode_type = barcode_type_objs[barcode_type->type - 1];
                }
            }
        } else if(input_event->key == InputKeyRight) {
            if(selected_menu_item == TypeMenuItem && barcode_type != NULL) { //Select Barcode Type
                if(barcode_type->type < NUMBER_OF_BARCODE_TYPES - 2) {
                    barcode_type = barcode_type_objs[barcode_type->type + 1];
                }
            }
        } else if(input_event->key == InputKeyOk) {
            if(selected_menu_item == FileNameMenuItem && barcode_type != NULL) {
                create_view_object->setter = FileNameSetter;

                snprintf(
                    create_view_object->input,
                    sizeof(create_view_object->input),
                    "%s",
                    furi_string_get_cstr(file_name));

                text_input_set_result_callback(
                    create_view_object->barcode_app->text_input,
                    text_input_callback,
                    create_view_object,
                    create_view_object->input,
                    TEXT_BUFFER_SIZE - BARCODE_EXTENSION_LENGTH, //remove the barcode length
                    //clear default text
                    false);
                text_input_set_header_text(
                    create_view_object->barcode_app->text_input, "File Name");
                text_input_show_illegal_symbols(
                    create_view_object->barcode_app->text_input, false);
                text_input_show_illegal_symbols(
                    create_view_object->barcode_app->text_input, false);
                view_dispatcher_switch_to_view(
                    create_view_object->barcode_app->view_dispatcher, TextInputView);
            }
            if(selected_menu_item == BarcodeDataMenuItem && barcode_type != NULL) {
                create_view_object->setter = BarcodeDataSetter;

                snprintf(
                    create_view_object->input,
                    sizeof(create_view_object->input),
                    "%s",
                    furi_string_get_cstr(barcode_data));

                text_input_set_result_callback(
                    create_view_object->barcode_app->text_input,
                    text_input_callback,
                    create_view_object,
                    create_view_object->input,
                    TEXT_BUFFER_SIZE,
                    //clear default text
                    false);
                text_input_set_header_text(
                    create_view_object->barcode_app->text_input, "Barcode Data");
                text_input_show_illegal_symbols(create_view_object->barcode_app->text_input, true);
                text_input_show_illegal_symbols(create_view_object->barcode_app->text_input, true);
                view_dispatcher_switch_to_view(
                    create_view_object->barcode_app->view_dispatcher, TextInputView);
            }
            if(selected_menu_item == SaveMenuButton && barcode_type != NULL) {
                save_barcode(create_view_object);
            }
            if(selected_menu_item == DeleteMenuButton && barcode_type != NULL) {
                if(mode == EditMode) {
                    remove_barcode(create_view_object);
                } else if(mode == NewMode) {
                    view_dispatcher_switch_to_view(
                        create_view_object->barcode_app->view_dispatcher, MainMenuView);
                }
            }
        }
    }

    //change the currently selected menu item
    with_view_model(
        create_view_object->view,
        CreateViewModel * model,
        {
            model->selected_menu_item = selected_menu_item;
            model->barcode_type = barcode_type;
        },
        true);

    return true;
}

CreateView* create_view_allocate(BarcodeApp* barcode_app) {
    furi_assert(barcode_app);

    CreateView* create_view_object = malloc(sizeof(CreateView));

    create_view_object->view = view_alloc();
    create_view_object->barcode_app = barcode_app;

    view_set_context(create_view_object->view, create_view_object);
    view_allocate_model(create_view_object->view, ViewModelTypeLocking, sizeof(CreateViewModel));
    view_set_draw_callback(create_view_object->view, app_draw_callback);
    view_set_input_callback(create_view_object->view, app_input_callback);

    return create_view_object;
}

void create_view_free_model(CreateView* create_view_object) {
    with_view_model(
        create_view_object->view,
        CreateViewModel * model,
        {
            if(model->file_path != NULL) {
                furi_string_free(model->file_path);
            }
            if(model->file_name != NULL) {
                furi_string_free(model->file_name);
            }
            if(model->barcode_data != NULL) {
                furi_string_free(model->barcode_data);
            }
        },
        true);
}

void remove_barcode(CreateView* create_view_object) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    bool success = false;

    with_view_model(
        create_view_object->view,
        CreateViewModel * model,
        {
            FURI_LOG_I(TAG, "Attempting to remove file");
            if(model->file_path != NULL) {
                FURI_LOG_I(TAG, "Removing File: %s", furi_string_get_cstr(model->file_path));
                if(storage_simply_remove(storage, furi_string_get_cstr(model->file_path))) {
                    FURI_LOG_I(
                        TAG,
                        "File: \"%s\" was successfully removed",
                        furi_string_get_cstr(model->file_path));
                    success = true;
                } else {
                    FURI_LOG_E(TAG, "Unable to remove file!");
                    success = false;
                }
            } else {
                FURI_LOG_E(TAG, "Could not remove barcode file");
                success = false;
            }
        },
        true);
    furi_record_close(RECORD_STORAGE);

    MessageView* message_view = create_view_object->barcode_app->message_view;
    if(success) {
        message_view_show(message_view, "File Deleted", MainMenuView);
    } else {
        //stay in the edit view so the user can retry
        message_view_show(message_view, "Could not delete file", CreateBarcodeView);
    }
}

void save_barcode(CreateView* create_view_object) {
    BarcodeTypeObj* barcode_type = NULL;
    FuriString* file_path; //this may be empty
    FuriString* file_name;
    FuriString* barcode_data;
    CreateMode mode;

    with_view_model(
        create_view_object->view,
        CreateViewModel * model,
        {
            file_path = model->file_path;
            file_name = model->file_name;
            barcode_data = model->barcode_data;
            barcode_type = model->barcode_type;
            mode = model->mode;
        },
        true);

    MessageView* message_view = create_view_object->barcode_app->message_view;

    if(file_name == NULL || furi_string_empty(file_name)) {
        FURI_LOG_E(TAG, "File Name cannot be empty");
        message_view_show(message_view, "File name\ncannot be empty", CreateBarcodeView);
        return;
    }
    if(barcode_data == NULL || furi_string_empty(barcode_data)) {
        FURI_LOG_E(TAG, "Barcode Data cannot be empty");
        message_view_show(message_view, "Barcode data\ncannot be empty", CreateBarcodeView);
        return;
    }
    if(barcode_type == NULL) {
        FURI_LOG_E(TAG, "Type not defined");
        message_view_show(message_view, "Barcode type\nis not selected", CreateBarcodeView);
        return;
    }

    //NULL while the save is still going fine
    const char* error_message = NULL;

    FuriString* full_file_path = furi_string_alloc_set(DEFAULT_USER_BARCODES);
    furi_string_push_back(full_file_path, '/');
    furi_string_cat(full_file_path, file_name);
    furi_string_cat_str(full_file_path, BARCODE_EXTENSION);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    //ensure the barcodes folder exists before saving,
    //it could have been removed while the app was running
    if(!init_folder()) {
        error_message = "Cannot access\nthe SD card";
    }

    //overwriting the barcode's own file in edit mode is fine, but saving
    //over any other existing file would destroy it, fail with a clear message instead
    bool same_file = file_path != NULL && furi_string_equal(file_path, full_file_path);
    if(error_message == NULL && !same_file &&
       storage_file_exists(storage, furi_string_get_cstr(full_file_path))) {
        FURI_LOG_E(TAG, "File \"%s\" already exists", furi_string_get_cstr(full_file_path));
        error_message = "A file with this\nname already exists";
    }

    //in edit mode a changed name moves the old file before saving
    if(error_message == NULL && mode == EditMode && !same_file && !furi_string_empty(file_path)) {
        FS_Error error = storage_common_rename(
            storage, furi_string_get_cstr(file_path), furi_string_get_cstr(full_file_path));
        if(error != FSE_OK) {
            FURI_LOG_E(TAG, "Rename error: %s", storage_error_get_desc(error));
            error_message = "Could not rename\nthe barcode file";
        } else {
            FURI_LOG_I(TAG, "Rename Success");
            //keep the model path in sync so a failed save below
            //does not leave it pointing at the old, now removed file
            furi_string_set(file_path, full_file_path);
        }
    }

    FlipperFormat* ff = NULL;
    if(error_message == NULL) {
        FURI_LOG_I(TAG, "Saving Barcode to: %s", furi_string_get_cstr(full_file_path));
        ff = flipper_format_file_alloc(storage);

        //open_new fails on existing files as a backstop to the check above
        bool file_saved =
            mode == NewMode ?
                flipper_format_file_open_new(ff, furi_string_get_cstr(full_file_path)) :
                flipper_format_file_open_always(ff, furi_string_get_cstr(full_file_path));
        if(file_saved) {
            // Filetype: Barcode
            // Version: 1

            // # Types - UPC-A, EAN-8, EAN-13, CODE-39, CODE-128, Codabar
            // Type: CODE-39
            // Data: AB
            file_saved =
                flipper_format_write_string_cstr(ff, "Filetype", "Barcode") &&
                flipper_format_write_string_cstr(ff, "Version", FILE_VERSION) &&
                flipper_format_write_comment_cstr(
                    ff, "Types - UPC-A, EAN-8, EAN-13, CODE-39, CODE-128, Codabar") &&
                flipper_format_write_string_cstr(ff, "Type", barcode_type->name) &&
                flipper_format_write_string_cstr(ff, "Data", furi_string_get_cstr(barcode_data));
            if(!file_saved) {
                //the file may be partially written, e.g. if the SD card was removed mid-save
                FURI_LOG_E(TAG, "Could not write the barcode file");
            }
        } else {
            FURI_LOG_E(TAG, "Could not open file %s", furi_string_get_cstr(full_file_path));
        }
        if(!file_saved) {
            error_message = "A saving error\nhas occurred";
        }
    }

    furi_string_free(full_file_path);
    if(ff != NULL) {
        flipper_format_free(ff);
    }
    furi_record_close(RECORD_STORAGE);

    if(error_message == NULL) {
        message_view_show(message_view, "File Saved!", MainMenuView);
    } else {
        //go back to the create view so the entered data is not lost
        message_view_show(message_view, error_message, CreateBarcodeView);
    }
}

void create_view_free(CreateView* create_view_object) {
    furi_assert(create_view_object);

    create_view_free_model(create_view_object);
    view_free(create_view_object->view);
    free(create_view_object);
}

View* create_get_view(CreateView* create_view_object) {
    furi_assert(create_view_object);
    return create_view_object->view;
}
