#include <gui/canvas.h>
#include <input/input.h>
#include <dialogs/dialogs.h>
#include <gui/view_holder.h>
#include <toolbox/api_lock.h>
#include <toolbox/path.h>
#include <gui/modules/text_input.h>

#include "panels.h"
#include "iconedit.h"
#include "utils/draw.h"
#include "utils/file_utils.h"
#include "utils/notification.h"
#include <iconedit_icons.h>

#define TOOL_WIDTH  14
#define TOOL_HEIGHT 14

typedef enum {
    File_START,
    File_New = File_START,
    File_Open,
    File_Save,
    File_Rename,
    File_SendAs,
    File_COUNT,
    File_NONE,
    File_TABBAR
} FileTool;

// const char* FileTool_Names[File_COUNT] = {"New", "Open", "Save", "Rename"};
const char* FileTool_Desc[File_COUNT] =
    {"New Icon", "Open Icon", "Save To...", "Rename...", "Send to PC"};

const Icon* FileTool_Icon[File_COUNT] = {
    [File_New] = &I_iet_New,
    [File_Open] = &I_iet_Open,
    [File_Save] = &I_iet_Save,
    [File_Rename] = &I_iet_Rename,
    [File_SendAs] = &I_iet_SendAs,
};

// should UP go to the tab bar?
const FileTool FileTool_UP[File_COUNT] =
    {File_TABBAR, File_TABBAR, File_TABBAR, File_TABBAR, File_New};
const FileTool FileTool_DOWN[File_COUNT] =
    {File_SendAs, File_SendAs, File_SendAs, File_SendAs, File_NONE};
const FileTool FileTool_LEFT[File_COUNT] = {File_NONE, File_New, File_Open, File_Save, File_NONE};
const FileTool FileTool_RIGHT[File_COUNT] =
    {File_Open, File_Save, File_Rename, File_NONE, File_NONE};

static FileTool tool = File_New;

typedef struct {
    FuriApiLock lock;
    bool result;
} TextInputContext;

static void text_input_back_callback(void* context) {
    TextInputContext* ti_context = context;
    ti_context->result = false;
    api_lock_unlock(ti_context->lock);
}
static void text_input_callback(void* context) {
    TextInputContext* ti_context = context;
    ti_context->result = true;
    api_lock_unlock(ti_context->lock);
}

void file_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;

    // draw icon name
    canvas_draw_str_aligned(
        canvas, 1, 64 - 10, AlignLeft, AlignBottom, furi_string_get_cstr(app->icon->name));

    // draw tools
    int tool_R = 4;
    int tool_x = 3; // These should be #defines
    int tool_y = 11;
    for(int t = File_START; t < File_COUNT; t++) {
        canvas_draw_icon(
            canvas,
            tool_x + 2 + (t % tool_R) * TOOL_WIDTH,
            tool_y + 2 + (t / tool_R) * TOOL_HEIGHT,
            FileTool_Icon[t]);

        if((FileTool)t == tool) {
            if(app->panel == Panel_File) {
                ie_draw_str(canvas, 1, 64 - 7, AlignLeft, AlignTop, Font5x7, FileTool_Desc[t]);
                canvas_set_color(canvas, ColorXOR);
                canvas_draw_rbox(
                    canvas,
                    tool_x + (t % tool_R) * TOOL_WIDTH,
                    tool_y + (t / tool_R) * TOOL_HEIGHT,
                    TOOL_WIDTH,
                    TOOL_HEIGHT,
                    0);
                canvas_set_color(canvas, ColorBlack);
            }
        }
    }
}

void file_new_icon_dialog_cb(void* context, DialogButton button) {
    IconEdit* app = context;
    if(button == DialogBtn_OK) {
        app->panel = Panel_New;
    } else {
        app->panel = Panel_File;
    }
}

// forward declare
void file_input_handle_ok(void* context);

// OK, this is sneaky. Since the File Open dialog appears INSIDE Panel_File, we have no panel
// to switch to in this callback, like with file_new_icon. So instead, we reset the dirty
// flag, if btn OK, and then re-call the handle_ok method. Ensuring that for both OK and Cancel
// we switch back to Panel_File from Panel_Dialog
void file_clear_dirty_dialog_cb(void* context, DialogButton button) {
    IconEdit* app = context;
    app->panel = Panel_File;
    if(button == DialogBtn_OK) {
        app->dirty = false;
        file_input_handle_ok(app);
    }
}

bool file_open_file_browser_callback(
    FuriString* path,
    void* context,
    uint8_t** icon,
    FuriString* item_name) {
    UNUSED(context);
    UNUSED(item_name);
    char ext[5];
    path_extract_extension(path, ext, sizeof(ext));
    if(!strcmp(ext, ".png")) {
        memcpy(*icon, icon_get_frame_data(&I_iet_PNG, 0), 32);
    } else if(!strcmp(ext, ".bmx")) {
        memcpy(*icon, icon_get_frame_data(&I_iet_BMX, 0), 32);
    } else {
        return false;
    }
    return true;
}

void file_input_handle_ok(void* context) {
    IconEdit* app = context;
    switch(tool) {
    case File_New:
        if(app->dirty) {
            app->panel = Panel_Dialog;
            dialog_setup("Discard changes?", Dialog_OK_CANCEL, file_new_icon_dialog_cb, app);
        } else {
            app->panel = Panel_New;
        }
        break;
    case File_Open: {
        if(app->dirty) {
            app->panel = Panel_Dialog;
            dialog_setup("Discard changes?", Dialog_OK_CANCEL, file_clear_dirty_dialog_cb, app);
            break;
        }
        DialogsFileBrowserOptions ieOptions;
        dialog_file_browser_set_basic_options(&ieOptions, "", NULL);
        ieOptions.base_path = STORAGE_EXT_PATH_PREFIX;
        ieOptions.skip_assets = true;
        ieOptions.item_loader_callback = file_open_file_browser_callback;
        DialogsApp* dialog = furi_record_open(RECORD_DIALOGS);
        FuriString* filename = furi_string_alloc_set("/data");
        if(dialog_file_browser_show(dialog, filename, filename, &ieOptions)) {
            // FURI_LOG_I(TAG, "Selected %s to open", furi_string_get_cstr(tmp_str));
            char ext[5];
            path_extract_extension(filename, ext, sizeof(ext));
            IEIcon* icon = NULL;
            if(!strcmp(ext, ".png")) {
                icon = png_file_open(furi_string_get_cstr(filename));
            }
            if(!strcmp(ext, ".bmx")) {
                icon = bmx_file_open(furi_string_get_cstr(filename));
            }
            if(icon) {
                ie_icon_free(app->icon);
                app->icon = icon;
                canvas_initialize(app->icon, app->settings.canvas_scale);
            } else {
                // unsupported file type
                dialog_info_dialog(app, "Unsupported type", app->panel);
            }
        }
        furi_record_close(RECORD_DIALOGS);
        furi_string_free(filename);
        break;
    }
    case File_Save:
        app->panel = Panel_SaveAs;
        break;
    case File_Rename: {
        // prompt user with filename editor
        FURI_LOG_I("Editor", "Rename");

        Gui* gui = furi_record_open(RECORD_GUI);

        TextInputContext* ti_context = malloc(sizeof(TextInputContext));
        ti_context->lock = api_lock_alloc_locked();

        ViewHolder* view_holder = view_holder_alloc();
        view_holder_attach_to_gui(view_holder, gui);

        view_holder_set_back_callback(view_holder, text_input_back_callback, ti_context);

        TextInput* text_input = text_input_alloc();
        char tmp_cstr[64] = {0};
        strncpy(tmp_cstr, furi_string_get_cstr(app->icon->name), sizeof(tmp_cstr));
        text_input_set_result_callback(
            text_input, text_input_callback, ti_context, tmp_cstr, 64, false);
        text_input_set_header_text(text_input, "Rename icon:");

        view_holder_set_view(view_holder, text_input_get_view(text_input));
        api_lock_wait_unlock(ti_context->lock);

        if(ti_context->result) {
            FURI_LOG_I(TAG, "Renaming file to: %s", tmp_cstr);
            furi_string_set_str(app->icon->name, tmp_cstr);
        } else {
            FURI_LOG_I(TAG, "Not renaming file!");
        }

        view_holder_set_view(view_holder, NULL);
        text_input_free(text_input);
        view_holder_free(view_holder);
        api_lock_free(ti_context->lock);
        free(ti_context);
        furi_record_close(RECORD_GUI);

        break;
    }
    case File_SendAs:
        app->panel = Panel_SendAs;
        break;
    default:
        break;
    }
}

bool file_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    bool consumed = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            FileTool next_tool = FileTool_UP[tool];
            if(next_tool == File_TABBAR) {
                app->panel = Panel_TabBar;
            } else if(next_tool != File_NONE) {
                tool = next_tool;
            }
            break;
        }
        case InputKeyDown: {
            FileTool next_tool = FileTool_DOWN[tool];
            if(next_tool != File_NONE) {
                tool = next_tool;
            }
            break;
        }
        case InputKeyLeft: {
            FileTool next_tool = FileTool_LEFT[tool];
            if(next_tool != File_NONE) {
                tool = next_tool;
            }
            break;
        }
        case InputKeyRight: {
            FileTool next_tool = FileTool_RIGHT[tool];
            if(next_tool != File_NONE) {
                tool = next_tool;
            }
            break;
        }
        case InputKeyOk: {
            file_input_handle_ok(context);
            break;
        }
        case InputKeyBack:
            app->panel = Panel_TabBar;
            break;
        default:
            break;
        }
    }
    return consumed;
}
