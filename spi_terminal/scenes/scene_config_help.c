#include "scenes.h"
#include "../flipper_spi_terminal.h"

void flipper_spi_terminal_scene_config_help_alloc(FlipperSPITerminalApp* app) {
    app->config_screen.help_view = text_box_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneConfigHelp,
        text_box_get_view(app->config_screen.help_view));
}

void flipper_spi_terminal_scene_config_help_free(FlipperSPITerminalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfigHelp);
    text_box_free(app->config_screen.help_view);
}

void flipper_spi_terminal_scene_config_help_on_enter(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    text_box_set_text(app->config_screen.help_view, app->config_screen.help_string);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneConfigHelp);
}

bool flipper_spi_terminal_scene_config_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_config_help_on_exit(void* context) {
    UNUSED(context);
}
