#include "scenes.h"
#include "flipper_spi_terminal_icons.h"
#include "../flipper_spi_terminal.h"

static void
    flipper_spi_terminal_scene_main_dialog_result_callback(DialogExResult result, void* context) {
    FlipperSPITerminalApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void flipper_spi_terminal_scene_main_alloc(FlipperSPITerminalApp* app) {
    app->main_screen = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneMain,
        dialog_ex_get_view(app->main_screen));

    dialog_ex_set_context(app->main_screen, app);

    dialog_ex_set_left_button_text(app->main_screen, "Config");
    dialog_ex_set_center_button_text(app->main_screen, "Run");
    dialog_ex_set_right_button_text(app->main_screen, "About");

    dialog_ex_set_icon(app->main_screen, 0, 0, &I_flipper_spi_terminal_connection_diagram);

    dialog_ex_set_result_callback(
        app->main_screen, flipper_spi_terminal_scene_main_dialog_result_callback);
}

void flipper_spi_terminal_scene_main_free(FlipperSPITerminalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneMain);
    dialog_ex_free(app->main_screen);
}

void flipper_spi_terminal_scene_main_on_enter(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneMain);
}

bool flipper_spi_terminal_scene_main_on_event(void* context, SceneManagerEvent event) {
    SPI_TERM_CONTEXT_TO_APP(context);

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneConfig);
            return true;
        } else if(event.event == DialogExResultCenter) {
            scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneTerminal);
            return true;
        } else if(event.event == DialogExResultRight) {
            scene_manager_next_scene(app->scene_manager, FlipperSPITerminalAppSceneAbout);
            return true;
        }
    }

    return false;
}

void flipper_spi_terminal_scene_main_on_exit(void* context) {
    UNUSED(context);
}
