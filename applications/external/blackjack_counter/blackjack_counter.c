#include "blackjack_counter.h"

static bool blackjack_counter_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BlackjackCounterApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool blackjack_counter_back_event_callback(void* context) {
    furi_assert(context);
    BlackjackCounterApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void blackjack_counter_menu_callback(void* context, uint32_t index) {
    BlackjackCounterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Index view callbacks - semplici e funzionanti
static void blackjack_counter_index_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Index");

    canvas_set_font(canvas, FontBatteryPercent);
    canvas_draw_str(canvas, 2, 20, "Hi-Lo: 2-6(+1) 7-9(0) 10-A(-1)");
    canvas_draw_str(canvas, 2, 28, "Hi-Opt I: 3-6(+1) 2,7-9(0) 10-K(-1)");
    canvas_draw_str(canvas, 2, 36, "Hi-Opt II: 2,3,6,7(+1) 4,5(+2)");
    canvas_draw_str(canvas, 2, 44, "          8,9,A(0) 10-K(-2)");
    canvas_draw_str(canvas, 2, 52, "Omega II: 2,3,7(+1) 4-6(+2)");
    canvas_draw_str(canvas, 2, 60, "          9(-1) 10-K(-2)");
}

static bool blackjack_counter_index_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
static void blackjack_counter_about_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    // Placeholder text
    canvas_draw_str(canvas, 35, 32, "Credits");

    // TODO: Add image when available
    // canvas_draw_icon(canvas, 0, 0, &I_Credits_128x64);
}

static bool blackjack_counter_about_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

BlackjackCounterApp* blackjack_counter_app_alloc() {
    BlackjackCounterApp* app = malloc(sizeof(BlackjackCounterApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&blackjack_counter_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, blackjack_counter_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, blackjack_counter_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Menu
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Hi-Lo (Classic)", 0, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "Hi-Opt I", 1, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "Hi-Opt II", 2, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "Omega II", 3, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "True Count", 4, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "Hi-Lo (Side Count)", 5, blackjack_counter_menu_callback, app);
    submenu_add_item(
        app->submenu, "Hi-Opt I (Side Count)", 6, blackjack_counter_menu_callback, app);
    submenu_add_item(
        app->submenu, "Hi-Opt II (Side Count)", 7, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "Index", 8, blackjack_counter_menu_callback, app);
    submenu_add_item(app->submenu, "About", 9, blackjack_counter_menu_callback, app);

    submenu_set_header(app->submenu, "BlackJack Counter");
    view_dispatcher_add_view(
        app->view_dispatcher, BlackjackCounterViewMenu, submenu_get_view(app->submenu));

    // Counter views
    for(int i = 0; i < 8; i++) {
        app->counter_views[i] = blackjack_counter_view_alloc();
        view_dispatcher_add_view(
            app->view_dispatcher,
            BlackjackCounterViewCounter + i,
            blackjack_counter_view_get_view(app->counter_views[i]));
    }

    // Index view - semplice e funzionante
    app->index_view = view_alloc();
    view_set_draw_callback(app->index_view, blackjack_counter_index_draw_callback);
    view_set_input_callback(app->index_view, blackjack_counter_index_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, BlackjackCounterViewIndex, app->index_view);

    // About view
    app->about_view = view_alloc();
    view_set_draw_callback(app->about_view, blackjack_counter_about_draw_callback);
    view_set_input_callback(app->about_view, blackjack_counter_about_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, BlackjackCounterViewAbout, app->about_view);

    scene_manager_next_scene(app->scene_manager, BlackjackCounterSceneMenu);

    return app;
}

void blackjack_counter_app_free(BlackjackCounterApp* app) {
    furi_assert(app);

    // Views
    view_dispatcher_remove_view(app->view_dispatcher, BlackjackCounterViewMenu);
    submenu_free(app->submenu);

    // Remove Index and About views
    view_dispatcher_remove_view(app->view_dispatcher, BlackjackCounterViewIndex);
    view_dispatcher_remove_view(app->view_dispatcher, BlackjackCounterViewAbout);
    view_free(app->index_view);
    view_free(app->about_view);

    for(int i = 0; i < 8; i++) {
        view_dispatcher_remove_view(app->view_dispatcher, BlackjackCounterViewCounter + i);
        blackjack_counter_view_free(app->counter_views[i]);
    }

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t blackjack_counter_app(void* p) {
    UNUSED(p);

    BlackjackCounterApp* app = blackjack_counter_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    blackjack_counter_app_free(app);

    return 0;
}
