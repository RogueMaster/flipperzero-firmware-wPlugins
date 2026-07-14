/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_bmp_editor.h"
static const char* TAG = "401_LightMsgBmpEditor";

static uint8_t bmp_editor_toggle(AppBmpEditor* BmpEditor) {
    BmpEditor->model_data->bitmap
        ->array[BmpEditor->model_data->cursor.y][BmpEditor->model_data->cursor.x] ^= 0xFF;
    return 0;
}

static uint8_t bmp_editor_move(AppBmpEditor* BmpEditor, int8_t x, int8_t y) {
    int dy = BmpEditor->model_data->cursor.y + y;
    int dx = BmpEditor->model_data->cursor.x + x;
    if(dx < 0)
        BmpEditor->model_data->cursor.x = 0;
    else if(dx > BmpEditor->model_data->bmp_w - 1)
        BmpEditor->model_data->cursor.x = BmpEditor->model_data->bmp_w - 1;
    else
        BmpEditor->model_data->cursor.x = dx;
    if(dy < 0)
        BmpEditor->model_data->cursor.y = 0;
    else if(dy > BmpEditor->model_data->bmp_h - 1)
        BmpEditor->model_data->cursor.y = BmpEditor->model_data->bmp_h - 1;
    else
        BmpEditor->model_data->cursor.y = dy;
    return 0;
}

static uint8_t bmp_editor_resize(AppBmpEditor* BmpEditor, int8_t w, int8_t h) {
    // Compute the new dimensions given increments/decrements in size. Usually only height
    int dh = BmpEditor->model_data->bmp_h + h;
    int dw = BmpEditor->model_data->bmp_w + w;
    if(dh < PARAM_BMP_EDITOR_MIN_RES_H) {
        BmpEditor->model_data->bmp_h = PARAM_BMP_EDITOR_MIN_RES_H;
    } else if(dh > PARAM_BMP_EDITOR_MAX_RES_H) {
        BmpEditor->model_data->bmp_h = PARAM_BMP_EDITOR_MAX_RES_H;
    } else {
        BmpEditor->model_data->bmp_h = dh;
    }

    if(dw < PARAM_BMP_EDITOR_MIN_RES_W) {
        BmpEditor->model_data->bmp_w = PARAM_BMP_EDITOR_MIN_RES_W;
    } else if(dw > PARAM_BMP_EDITOR_MAX_RES_W) {
        BmpEditor->model_data->bmp_w = PARAM_BMP_EDITOR_MAX_RES_W;
    } else {
        BmpEditor->model_data->bmp_w = dw;
    }

    BmpEditor->model_data->cursor.x = (BmpEditor->model_data->bmp_w / 2);
    BmpEditor->model_data->cursor.y = (BmpEditor->model_data->bmp_h / 2);
    return 0;
}

/**
 * Computes stuffs for the model_data structure, utilized to display the interface
 *
 * @param appBmpEditor Pointer to the contexts of the app.
 * @param model_data Pointer to the Model data.
 */
static void bmp_compute_model(AppBmpEditor* BmpEditor, bmpEditorData* BmpEditorData) {
    UNUSED(BmpEditor);
    BmpEditorData->bmp_frame_spacing = 1;
    BmpEditorData->bmp_frame_w =
        ((BmpEditorData->bmp_pixel_size + BmpEditorData->bmp_pixel_spacing) *
         BmpEditorData->bmp_w) +
        (BmpEditorData->bmp_frame_spacing * 2) + 2;
    BmpEditorData->bmp_frame_h =
        ((BmpEditorData->bmp_pixel_size + BmpEditorData->bmp_pixel_spacing) *
         BmpEditorData->bmp_h) +
        (BmpEditorData->bmp_frame_spacing * 2) + 2;
    BmpEditorData->bmp_frame_x = (128 - BmpEditorData->bmp_frame_w) >> 1;
    BmpEditorData->bmp_frame_y = 0;
}

static void bmp_editor_text_input_callback(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    snprintf(
        BmpEditor->bitmapPath,
        LIGHTMSG_MAX_BITMAPPATH_LEN,
        "%s/%s%s",
        LIGHTMSGCONF_SAVE_FOLDER,
        BmpEditor->bitmapName,
        ".bmp");
    view_dispatcher_send_custom_event(app->view_dispatcher, AppBmpEditorEventSaveText);
}

static void bmp_editor_select_name(void* ctx) {
    AppContext* app = (AppContext*)ctx; // Main app struct
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;

    ValidatorIsFile* validator_is_file =
        validator_is_file_alloc_init(LIGHTMSGCONF_SAVE_FOLDER, ".bmp", BmpEditor->bitmapPath);

    text_input_reset(BmpEditor->text_input);
    // Ensures the name don't exists yet
    text_input_set_validator(BmpEditor->text_input, validator_is_file_callback, validator_is_file);
    text_input_set_header_text(BmpEditor->text_input, "Name of the new file:");
    text_input_set_result_callback(
        BmpEditor->text_input,
        bmp_editor_text_input_callback,
        app,
        BmpEditor->bitmapName,
        LIGHTMSG_MAX_BITMAPNAME_LEN,
        false);

    // view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSetText);
    view_dispatcher_switch_to_view(app->view_dispatcher, BmpEditorViewSelectName);
}

static void bmp_editor_select_file(void* ctx) {
    AppContext* app = (AppContext*)ctx; // Main app struct
    AppData* appData = (AppData*)app->data;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;

    bmpEditorData* BmpEditorData = BmpEditor->model_data;
    Configuration* light_msg_data = (Configuration*)appData->config;
    DialogsFileBrowserOptions browser_options;
    FuriString* bitmapPath;
    BmpEditor->dialogs = furi_record_open(RECORD_DIALOGS);
    bitmapPath = furi_string_alloc();

    furi_string_set(bitmapPath, light_msg_data->bitmapPath);

    dialog_file_browser_set_basic_options(&browser_options, ".bmp", &I_401_lghtmsg_icon_10px);
    browser_options.base_path = LIGHTMSGCONF_SAVE_FOLDER;
    browser_options.skip_assets = true;
    if(dialog_file_browser_show(BmpEditor->dialogs, bitmapPath, bitmapPath, &browser_options)) {
        if(BmpEditorData->bitmap) bitmapMatrix_free(BmpEditorData->bitmap);
        BmpEditorData->bitmap = bmp_to_bitmapMatrix(furi_string_get_cstr(bitmapPath));
        if(BmpEditorData->bitmap->width > PARAM_BMP_EDITOR_MAX_RES_W) {
            //if(BmpEditorData->bitmap) bitmapMatrix_free(BmpEditorData->bitmap);
            //furi_string_free(bitmapPath);
            BmpEditorData->state = BmpEditorStateSizeError;
            BmpEditorData->error = L401_ERR_WIDTH;
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewBmpEditor);
            return;
        } else {
            BmpEditorData->bmp_w = BmpEditorData->bitmap->width;
            BmpEditorData->bmp_h = BmpEditorData->bitmap->height;
            BmpEditorData->state = BmpEditorStateDrawing;
            memcpy(
                BmpEditor->bitmapPath,
                furi_string_get_cstr(bitmapPath),
                strlen(furi_string_get_cstr(bitmapPath)));
            bmp_compute_model(BmpEditor, BmpEditor->model_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewBmpEditor);
        }
    }
    furi_string_free(bitmapPath);
}

static bool bmp_editor_mainmenu_input_callback(InputEvent* input_event, void* ctx) {
    UNUSED(input_event);
    UNUSED(ctx);
    return false;
}

static bool bmp_editor_error_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    bool consumed = false;

    if((input_event->type == InputTypePress) || (input_event->type == InputTypeRepeat)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, AppBmpEditorEventQuit);
        consumed = true;
    }
    return consumed;
}

static bool bmp_editor_select_size_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    bmpEditorData* BmpEditorData = BmpEditor->model_data;
    bool consumed = false;

    if((input_event->type == InputTypePress) || (input_event->type == InputTypeRepeat)) {
        switch(input_event->key) {
        case InputKeyLeft:
            bmp_editor_resize(BmpEditor, -1, 0);
            bmp_compute_model(BmpEditor, BmpEditor->model_data);
            consumed = true;
            break;
        case InputKeyRight:
            bmp_editor_resize(BmpEditor, 1, 0);
            bmp_compute_model(BmpEditor, BmpEditor->model_data);
            consumed = true;
            break;
        case InputKeyOk:
            BmpEditorData->state = BmpEditorStateSelectName;
            bmp_editor_select_name(ctx);
            consumed = true;
            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, AppBmpEditorEventQuit);
            consumed = false;
            break;
        default:
            consumed = false;
            break;
        }
        with_view_model(
            BmpEditor->view, bmpEditorModel * model, model->data = BmpEditor->model_data;, true);
    }
    return consumed;
}

static bool bmp_editor_draw_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    bmpEditorData* BmpEditorData = BmpEditor->model_data;
    bool consumed = false;
    BMP_err res = BMP_OK;
    if((input_event->type == InputTypePress) || (input_event->type == InputTypeRepeat) ||
       (input_event->type == InputTypeLong)) {
        switch(input_event->key) {
        case InputKeyUp:
            bmp_editor_move(BmpEditor, 0, -1);
            if(BmpEditorData->draw_mode == BmpEditorDrawModeContinuous) {
                bmp_editor_toggle(BmpEditor);
            }
            consumed = true;
            break;
        case InputKeyDown:
            bmp_editor_move(BmpEditor, 0, 1);
            if(BmpEditorData->draw_mode == BmpEditorDrawModeContinuous) {
                bmp_editor_toggle(BmpEditor);
            }
            consumed = true;
            break;
        case InputKeyLeft:
            bmp_editor_move(BmpEditor, -1, 0);
            if(BmpEditorData->draw_mode == BmpEditorDrawModeContinuous) {
                bmp_editor_toggle(BmpEditor);
            }
            consumed = true;
            break;
        case InputKeyRight:
            bmp_editor_move(BmpEditor, 1, 0);
            if(BmpEditorData->draw_mode == BmpEditorDrawModeContinuous) {
                bmp_editor_toggle(BmpEditor);
            }
            consumed = true;
            break;
        case InputKeyOk:
            if(input_event->type == InputTypeLong) {
                BmpEditorData->draw_mode = (BmpEditorData->draw_mode == BmpEditorDrawModeOneshot) ?
                                               BmpEditorDrawModeContinuous :
                                               BmpEditorDrawModeOneshot;
            } else {
                if(BmpEditorData->draw_mode == BmpEditorDrawModeOneshot) {
                    bmp_editor_toggle(BmpEditor);
                }
            }
            consumed = false;
            break;
        case InputKeyBack:

            res = bmp_write(BmpEditor->bitmapPath, BmpEditor->model_data->bitmap);
            if(res == BMP_OK) {
                view_dispatcher_send_custom_event(app->view_dispatcher, AppBmpEditorEventQuit);
            } else {
                FURI_LOG_E(
                    TAG, "Could not write file to %s. Reason: %d", BmpEditor->bitmapPath, res);
            }
            consumed = false;
            break;
        default:
            consumed = false;
            break;
        }
        with_view_model(
            BmpEditor->view, bmpEditorModel * model, model->data = BmpEditor->model_data;, true);
    }
    return consumed;
}
/**
 * Handles input events for the bmp_editor scene. Sends custom events based on
 * the user's keypresses.
 *
 * @param input_event The input event structure.
 * @param ctx The application context.
 * @return true if the event was consumed, false otherwise.
 */
static bool app_scene_bmp_editor_input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    bmpEditorData* BmpEditorData = BmpEditor->model_data;

    bool consumed = false;
    switch(BmpEditorData->state) {
    case BmpEditorStateMainMenu:
        consumed = bmp_editor_mainmenu_input_callback(input_event, ctx);
        break;
    case BmpEditorStateSelectSize:
        consumed = bmp_editor_select_size_input_callback(input_event, ctx);
        break;
    case BmpEditorStateDrawing:
        consumed = bmp_editor_draw_input_callback(input_event, ctx);
        break;
    case BmpEditorStateSizeError:
        consumed = bmp_editor_error_input_callback(input_event, ctx);
        break;
    default:
        break;
    }
    return consumed;
}

static void bmp_editor_drawSizePicker(Canvas* canvas, void* ctx) {
    // UNUSED(ctx);
    bmpEditorModel* BmpEditorModel = (bmpEditorModel*)ctx;
    bmpEditorData* BmpEditorData = BmpEditorModel->data;
    char text[32];
    snprintf(text, sizeof(text), "%d", BmpEditorData->bmp_w);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Custom image width:");

    canvas_set_font(canvas, FontBigNumbers);

    canvas_draw_icon(canvas, 30, 28, &I_btn_left_10x10);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, text);
    canvas_draw_icon(canvas, 88, 28, &I_btn_right_10x10);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_icon(canvas, 25 + 0, 56, &I_btn_back_7x7);
    canvas_draw_str(canvas, 25 + 10, 62, "Exit");
    canvas_draw_icon(canvas, 25 + 35, 56, &I_btn_ok_7x7);
    canvas_draw_str(canvas, 25 + 45, 62, "OK");
}

static void bmp_editor_drawError(Canvas* canvas, void* ctx) {
    // UNUSED(ctx);
    bmpEditorModel* BmpEditorModel = (bmpEditorModel*)ctx;
    bmpEditorData* BmpEditorData = BmpEditorModel->data;

    switch(BmpEditorData->error) {
    case L401_ERR_WIDTH:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "BMP File too large to");
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "be edited on flipper!");
        break;
    default:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Unknown error");
        break;
    }
    elements_button_center(canvas, "Return");
}

static void bmp_editor_drawBoard(Canvas* canvas, void* ctx) {
    bmpEditorModel* BmpEditorModel = (bmpEditorModel*)ctx;
    bmpEditorData* BmpEditorData = BmpEditorModel->data;
    bmpEditorCursor cursor = BmpEditorData->cursor;
    uint8_t x;
    uint8_t y;
    char text[32];
    snprintf(text, sizeof(text), "W: %d", BmpEditorData->bmp_w);
    canvas_clear(canvas);
    // Frame
    canvas_draw_rframe(
        canvas,
        BmpEditorData->bmp_frame_x,
        BmpEditorData->bmp_frame_y,
        BmpEditorData->bmp_frame_w,
        BmpEditorData->bmp_frame_h,
        (BmpEditorData->bmp_pixel_size + BmpEditorData->bmp_pixel_spacing));
    // UX elements btn_13x13
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_icon(canvas, 0, 56, &I_btn_back_7x7);
    canvas_draw_str(canvas, 10, 62, "Save");
    canvas_draw_icon(canvas, 35, 56, &I_btn_ok_7x7);
    canvas_draw_str(canvas, 45, 62, "Toggle/");
    // Indicates continuous mode
    if(BmpEditorData->draw_mode == BmpEditorDrawModeContinuous) {
        canvas_set_font(canvas, FontPrimary);
    }
    canvas_draw_str(canvas, 82, 62, "Hold");
    // Bitmap
    for(x = 0; x < BmpEditorData->bmp_w; x++) {
        for(y = 0; y < BmpEditorData->bmp_h; y++) {
            if(BmpEditorData->bitmap->array[y][x] > 0x00) {
                canvas_draw_box(
                    canvas,
                    (BmpEditorData->bmp_frame_x + BmpEditorData->bmp_frame_spacing + 1) +
                        (x * (BmpEditorData->bmp_pixel_spacing + BmpEditorData->bmp_pixel_size)),
                    (BmpEditorData->bmp_frame_y + BmpEditorData->bmp_frame_spacing + 1) +
                        (y * (BmpEditorData->bmp_pixel_spacing + BmpEditorData->bmp_pixel_size)),
                    (BmpEditorData->bmp_pixel_size),
                    (BmpEditorData->bmp_pixel_size));
            }
        }
    }

    // Cursor
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_frame(
        canvas,
        (BmpEditorData->bmp_frame_x + BmpEditorData->bmp_frame_spacing) +
            (cursor.x * (BmpEditorData->bmp_pixel_spacing + BmpEditorData->bmp_pixel_size)),
        (BmpEditorData->bmp_frame_y + BmpEditorData->bmp_frame_spacing) +
            (cursor.y * (BmpEditorData->bmp_pixel_spacing + BmpEditorData->bmp_pixel_size)),
        (BmpEditorData->bmp_pixel_size + 2),
        (BmpEditorData->bmp_pixel_size + 2));
    canvas_set_color(canvas, ColorBlack);
}

/**
 * Renders the bmp_editor scene. Draws the bmp_editor icon and text on the canvas.
 *
 * @param canvas The canvas to draw on.
 * @param ctx The application context.
 */
// Invoked by the draw callback to render the AppBmpEditor.
static void app_scene_bmp_editor_render_callback(Canvas* canvas, void* _model) {
    // UNUSED(_model);
    bmpEditorModel* BmpEditorModel = (bmpEditorModel*)_model;
    bmpEditorData* BmpEditorData = BmpEditorModel->data;

    switch(BmpEditorData->state) {
    case BmpEditorStateSelectSize:
        bmp_editor_drawSizePicker(canvas, _model);
        break;
    case BmpEditorStateDrawing:
        bmp_editor_drawBoard(canvas, _model);
        break;
    case BmpEditorStateSizeError:
        bmp_editor_drawError(canvas, _model);
        break;
    default:
        break;
    }
}

static void bmp_editor_mainmenu_callback(void* ctx, uint32_t index) {
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    bmpEditorData* BmpEditorData = BmpEditor->model_data;

    switch(index) {
    case BmpEditorMainmenuIndex_New:
        BmpEditorData->state = BmpEditorStateSelectSize;
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewBmpEditor);
        break;
    case BmpEditorMainmenuIndex_Open:
        BmpEditorData->state = BmpEditorStateSelectFile;
        bmp_editor_select_file(ctx);

        break;
    default:
        break;
    }
}

/**
 * Allocates and initializes a new AppBmpEditor instance.
 *
 * @param ctx The application context.
 * @return Pointer to the newly allocated AppBmpEditor instance.
 */
AppBmpEditor* app_bmp_editor_alloc(void* ctx) {
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* appBmpEditor = malloc(sizeof(AppBmpEditor));
    appBmpEditor->model_data = malloc(sizeof(bmpEditorData));
    appBmpEditor->model_data->bmp_pixel_size = 3;
    appBmpEditor->model_data->bmp_pixel_spacing = 0;
    appBmpEditor->model_data->bmp_w = 32;
    appBmpEditor->model_data->bmp_h = 16;
    appBmpEditor->model_data->error = L401_OK;
    appBmpEditor->mainmenu = submenu_alloc();

    submenu_add_item(
        appBmpEditor->mainmenu,
        "New",
        BmpEditorMainmenuIndex_New,
        bmp_editor_mainmenu_callback,
        app);
    submenu_add_item(
        appBmpEditor->mainmenu,
        "Open",
        BmpEditorMainmenuIndex_Open,
        bmp_editor_mainmenu_callback,
        app);

    view_dispatcher_add_view(
        app->view_dispatcher, BmpEditorViewMainMenu, submenu_get_view(appBmpEditor->mainmenu));

    appBmpEditor->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BmpEditorViewSelectName,
        text_input_get_view(appBmpEditor->text_input));

    bmp_compute_model(appBmpEditor, appBmpEditor->model_data);

    appBmpEditor->view = view_alloc();
    view_allocate_model(appBmpEditor->view, ViewModelTypeLocking, sizeof(bmpEditorModel));
    view_set_context(appBmpEditor->view, app);
    view_set_draw_callback(appBmpEditor->view, app_scene_bmp_editor_render_callback);
    view_set_input_callback(appBmpEditor->view, app_scene_bmp_editor_input_callback);
    return appBmpEditor;
}

/**
 * Frees the resources associated with an AppBmpEditor instance.
 *
 * @param appBmpEditor Pointer to the AppBmpEditor instance to be freed.
 */
void app_bmp_editor_free(void* ctx) {
    furi_assert(ctx);
    AppContext* app = (AppContext*)ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;

    view_free(BmpEditor->view);
    view_dispatcher_remove_view(app->view_dispatcher, BmpEditorViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, BmpEditorViewSelectName);
    if(BmpEditor->model_data->bitmap) bitmapMatrix_free(BmpEditor->model_data->bitmap);
    submenu_free(BmpEditor->mainmenu);
    free(BmpEditor->model_data);
    free(BmpEditor);
}

/**
 * Retrieves the view associated with the AppBmpEditor instance.
 *
 * @param appBmpEditor Pointer to the AppBmpEditor instance.
 * @return Pointer to the associated view.
 */
View* app_bitmap_editor_get_view(AppBmpEditor* appBmpEditor) {
    furi_assert(appBmpEditor);
    return appBmpEditor->view;
}
/**
 * Handles the entry into the bmp_editor scene. Switches the view to the bmp_editor scene.
 *
 * @param context The application context.
 */
void app_scene_bmp_editor_on_enter(void* context) {
    AppContext* app = context;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    BmpEditor->model_data->state = BmpEditorStateMainMenu;
    with_view_model(
        BmpEditor->view, bmpEditorModel * model, { model->data = BmpEditor->model_data; }, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, BmpEditorViewMainMenu);
}

static l401_err bmp_editor_init_bitmap(void* ctx) {
    AppContext* app = ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    bmpEditorData* BmpEditorData = BmpEditor->model_data;
    bitmapMatrix* bitmap = (bitmapMatrix*)malloc(sizeof(bitmapMatrix));

    if(bitmap == NULL) {
        return L401_ERR_INTERNAL; // Échec de l'allocation de mémoire
    }

    bitmap->width = BmpEditorData->bmp_w;
    bitmap->height = BmpEditorData->bmp_h;

    // Étape 2: Allouer la mémoire pour les lignes de l'array
    bitmap->array = (uint8_t**)malloc(bitmap->height * sizeof(uint8_t*));
    if(bitmap->array == NULL) {
        free(bitmap); // Libérer la structure bitmapMatrix allouée précédemment
        return L401_ERR_INTERNAL; // Échec de l'allocation de mémoire
    }

    // Étape 3: Allouer la mémoire pour chaque colonne de chaque ligne
    for(uint8_t i = 0; i < bitmap->height; i++) {
        bitmap->array[i] = (uint8_t*)malloc(bitmap->width * sizeof(uint8_t));
        if(bitmap->array[i] == NULL) {
            // En cas d'échec, libérer toute la mémoire déjà allouée
            for(uint8_t j = 0; j < i; j++) {
                free(bitmap->array[j]); // Libérer les lignes allouées précédemment
            }
            free(bitmap->array); // Libérer le tableau de lignes
            free(bitmap); // Libérer la structure bitmapMatrix
            return L401_ERR_INTERNAL; // Échec de l'allocation de mémoire
        }
    }
    BmpEditorData->bitmap = bitmap;
    return L401_OK;
}

/**
 * Handles events for the bmp_editor scene. Processes custom events to navigate to
 * other scenes based on the user's actions.
 *
 * @param context The application context.
 * @param event The scene manager event.
 * @return true if the event was consumed, false otherwise.
 */
bool app_scene_bmp_editor_on_event(void* ctx, SceneManagerEvent event) {
    AppContext* app = ctx;
    AppBmpEditor* BmpEditor = app->sceneBmpEditor;
    bmpEditorData* BmpEditorData = BmpEditor->model_data;

    // UNUSED(ctx);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AppBmpEditorEventSaveText:
            bmp_editor_init_bitmap(ctx);
            BmpEditorData->state = BmpEditorStateDrawing;
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewBmpEditor);
            consumed = true;
            break;
        case AppBmpEditorEventQuit:
            view_dispatcher_switch_to_view(app->view_dispatcher, BmpEditorViewMainMenu);
            consumed = true;
            break;
        }
    }
    // scene_manager_next_scene(app->scene_manager, AppSceneMainMenu);
    return consumed;
}
/**
 * Handles the exit from the bmp_editor scene. No specific actions are
 * currently performed during exit.
 *
 * @param context The application context.
 */
void app_scene_bmp_editor_on_exit(void* context) {
    UNUSED(context);
}
