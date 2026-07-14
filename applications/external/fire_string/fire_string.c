#include "fire_string.h"
#include "fire_string_i.h"

/** collection of all scene on_enter handlers - in the same order as their enum */
void (*const fire_string_scene_on_enter_handlers[])(void*) = {
    fire_string_scene_on_enter_main_menu,
    fire_string_scene_on_enter_settings,
    fire_string_scene_on_enter_string_generator,
    fire_string_scene_on_enter_step_two_menu,
    fire_string_scene_on_enter_loading_usb,
    fire_string_scene_on_enter_usb,
    fire_string_scene_on_enter_bluetooth,
    fire_string_scene_on_enter_load_string,
    fire_string_scene_on_enter_save_string,
    fire_string_scene_on_enter_about,
    fire_string_scene_on_enter_loading_word_list};

/** collection of all scene on event handlers - in the same order as their enum */
bool (*const fire_string_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    fire_string_scene_on_event_main_menu,
    fire_string_scene_on_event_settings,
    fire_string_scene_on_event_string_generator,
    fire_string_scene_on_event_step_two_menu,
    fire_string_scene_on_event_loading_usb,
    fire_string_scene_on_event_usb,
    fire_string_scene_on_event_bluetooth,
    fire_string_scene_on_event_load_string,
    fire_string_scene_on_event_save_string,
    fire_string_scene_on_event_about,
    fire_string_scene_on_event_loading_word_list};

/** collection of all scene on exit handlers - in the same order as their enum */
void (*const fire_string_scene_on_exit_handlers[])(void*) = {
    fire_string_scene_on_exit_main_menu,
    fire_string_scene_on_exit_settings,
    fire_string_scene_on_exit_string_generator,
    fire_string_scene_on_exit_step_two_menu,
    fire_string_scene_on_exit_loading_usb,
    fire_string_scene_on_exit_usb,
    fire_string_scene_on_exit_bluetooth,
    fire_string_scene_on_exit_load_string,
    fire_string_scene_on_exit_save_string,
    fire_string_scene_on_exit_about,
    fire_string_scene_on_exit_loading_word_list};

/** collection of all on_enter, on_event, on_exit handlers */
const SceneManagerHandlers fire_string_scene_event_handlers = {
    .on_enter_handlers = fire_string_scene_on_enter_handlers,
    .on_event_handlers = fire_string_scene_on_event_handlers,
    .on_exit_handlers = fire_string_scene_on_exit_handlers,
    .scene_num = FireStringScene_count};

/** custom event handler - passes the event to the scene manager */
bool fire_string_scene_manager_custom_event_callback(void* context, uint32_t custom_event) {
    FURI_LOG_T(TAG, "fire_string_scene_manager_custom_event_callback");
    furi_assert(context);
    FireString* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

/** navigation event handler - passes the event to the scene manager */
bool fire_string_scene_manager_navigation_event_callback(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_manager_navigation_event_callback");
    furi_assert(context);
    FireString* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void fire_string_tick_event_callback(void* context) {
    furi_assert(context);
    FireString* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/** initilize scene manager with all handlers */
void fire_string_scene_manager_init(FireString* app) {
    FURI_LOG_T(TAG, "fire_string_scene_manager_init");
    app->scene_manager = scene_manager_alloc(&fire_string_scene_event_handlers, app);
}

/** initialise the views, and initialise the view dispatcher with all views */
void fire_string_view_dispatcher_init(FireString* app) {
    FURI_LOG_T(TAG, "fire_string_view_dispatcher_init");
    app->view_dispatcher = view_dispatcher_alloc();

    // allocate each view
    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init allocating views");
    app->menu = menu_alloc();
    app->submenu = submenu_alloc();
    app->widget = widget_alloc();
    app->loading = loading_alloc();
    app->text_input = text_input_alloc();

    app->variable_item_list = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list);

    // allocate each view
    FURI_LOG_T(TAG, "fire_string_view_dispatcher_init setting callbacks");
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fire_string_scene_manager_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fire_string_scene_manager_navigation_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, fire_string_tick_event_callback, 10);

    // add views to the dispatcher, indexed by their enum value
    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init adding view menu");
    view_dispatcher_add_view(app->view_dispatcher, FireStringView_Menu, menu_get_view(app->menu));

    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init adding view submenu");
    view_dispatcher_add_view(
        app->view_dispatcher, FireStringView_SubMenu, submenu_get_view(app->submenu));

    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init adding view variable_item_list");
    view_dispatcher_add_view(
        app->view_dispatcher,
        FireStringView_VariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init adding view widget");
    view_dispatcher_add_view(
        app->view_dispatcher, FireStringView_Widget, widget_get_view(app->widget));

    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init adding view loading");
    view_dispatcher_add_view(
        app->view_dispatcher, FireStringView_Loading, loading_get_view(app->loading));

    FURI_LOG_D(TAG, "fire_string_view_dispatcher_init adding view text_input");
    view_dispatcher_add_view(
        app->view_dispatcher, FireStringView_TextInput, text_input_get_view(app->text_input));
}

/** initilise app data, scene manager, and view dispatcher */
FireString* fire_string_init() {
    FURI_LOG_T(TAG, "fire_string_init");
    FireString* app = malloc(sizeof(FireString));

    // Set variable_item_list Defaults
    app->settings = malloc(sizeof(FireStringSettings));
    app->settings->str_len = 16;
    app->settings->str_type = StrType_AlphaNumSymb;
    app->settings->use_ir = true;
    app->settings->file_loaded = false;

    // Set HID defaults
    app->hid = malloc(sizeof(FireStringHID));
    app->hid->api = NULL;
    app->hid->usb_if_prev = NULL;
    app->hid->hid_inst = NULL;
    // Set default keyboard layout
    // TODO: Alternate keyboard layouts - lang assets are included at APP_ASSETS_PATH("layouts")
    memset(app->hid->layout, HID_KEYBOARD_NONE, sizeof(app->hid->layout));
    memcpy(app->hid->layout, hid_asciimap, MIN(sizeof(hid_asciimap), sizeof(app->hid->layout)));

    // Init notifications app
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Init dictionary
    app->dict = malloc(sizeof(FireStringDictionary));
    app->dict->char_list = NULL;
    app->dict->word_list = NULL;
    app->dict->len = 0;

    app->fire_string = furi_string_alloc();
    furi_string_reserve(app->fire_string, STR_RESERVE_LEN);

    fire_string_scene_manager_init(app);
    fire_string_view_dispatcher_init(app);

    app->gui = furi_record_open(RECORD_GUI);

    return app;
}

/** free all app data, scene manager, and view dispatcher */
void fire_string_free(FireString* app) {
    FURI_LOG_T(TAG, "fire_string_free");
    scene_manager_free(app->scene_manager);
    view_dispatcher_remove_view(app->view_dispatcher, FireStringView_Menu);
    view_dispatcher_remove_view(app->view_dispatcher, FireStringView_SubMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FireStringView_VariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, FireStringView_Widget);
    view_dispatcher_remove_view(app->view_dispatcher, FireStringView_Loading);
    view_dispatcher_remove_view(app->view_dispatcher, FireStringView_TextInput);

    view_dispatcher_free(app->view_dispatcher);
    app->view_dispatcher = NULL;
    menu_free(app->menu);
    app->menu = NULL;
    submenu_free(app->submenu);
    app->submenu = NULL;
    variable_item_list_free(app->variable_item_list);
    app->variable_item_list = NULL;
    widget_free(app->widget);
    app->widget = NULL;
    loading_free(app->loading);
    app->loading = NULL;
    furi_string_free(app->fire_string);
    app->fire_string = NULL;
    text_input_free(app->text_input);
    app->text_input = NULL;

    if(app->dict->word_list != NULL) {
        for(uint16_t i = 0; i < app->dict->len; i++) {
            furi_string_free(app->dict->word_list[i]);
            app->dict->word_list[i] = NULL;
        }
        free(app->dict->word_list);
        app->dict->word_list = NULL;
    }
    if(app->dict->char_list != NULL) {
        furi_string_free(app->dict->char_list);
        app->dict->char_list = NULL;
    }

    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    free(app->settings);
    app->settings = NULL;
    free(app->hid);
    app->hid = NULL;
    free(app->dict);
    app->dict = NULL;
    free(app);
    app = NULL;
}

/** go to trace log level in dev environment */
void fire_string_set_log_level() {
#ifdef FURI_DEBUG
    furi_log_set_level(FuriLogLevelTrace);
#else
    furi_log_set_level(FuriLogLevelInfo);
#endif
}

/** entrypoint */
int32_t fire_string_app(void* p) {
    UNUSED(p);
    fire_string_set_log_level();

    // create the app context struct, scene manager, and view dispatcher
    FURI_LOG_I(TAG, "Fire String Heating Up...");

    FireString* app = fire_string_init();

    // set the scene and launch the main loop
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FireStringScene_MainMenu);
    FURI_LOG_T(TAG, "Starting dispatcher...");
    view_dispatcher_run(app->view_dispatcher);

    // free all memory
    FURI_LOG_T(TAG, "Fire String extinguishing...");
    furi_record_close(RECORD_GUI);
    fire_string_free(app);

    return 0;
}
