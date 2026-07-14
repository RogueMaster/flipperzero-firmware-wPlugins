#include "scenes.h"
#include "../flipper_spi_terminal.h"

const char* about_text =
    "SPI TERMINAL\n"
    "\n"
    "By JAN WIESEMANN.de\n"
    "https://github.com/janwiesemann/flipper-spi-terminal\n"
    "\n"
    "SPI TERMINAL is a SPI App, which allows you to control external devices using SPI. Your Flipper can act as a SPI Master or Slave device. The Slave mode allows you to sniff the communication between different SPI peripherals.\n"
    "\n"
    "The App uses the Low-Level SPI Interface of the STM32WB55RG Microprocessor. All data is transmitted with DMA Sub-module and can reach speeds of up to 32 Mbit/s in Master and up to 24 Mbit/s in Slave mode.\n"
    "\n"
    "The inbuild documentation is based on the 'STM32F7 - SPI' (Revision 1.0) presentation and the STM 'RM0434' Reference Manual. Both referenced are linked to in the GitHub README.MD."
    "\n"
    "=== Pin configuration ===\n"
    "CAUTION: Flipper Zero's pins are only 5V tolerant, if they are configured as an input. Due to this limitation, only 3.3V signals should be used!\n"
    "\n"
    "MOSI: 2 (PA7) Master In / Slave Out\n"
    "MISO: 3 (PA6) Master Out / Salve In\n"
    "  CS: 4 (PA4, NSS) Non Slave Select\n"
    " SCK: 5 (PB3) Serial Clock\n"
    "\n"
    "=== Terminal View ===\n"
    "Use the 'Up' and 'Down' keys to scroll.\n"
    "Press 'Back' to navigate to the main menu.\n"
    "Hold 'Back' to clear the screen buffer.";

void flipper_spi_terminal_scene_about_alloc(FlipperSPITerminalApp* app) {
    app->about_screen = text_box_alloc();

    text_box_set_text(app->about_screen, about_text);

    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperSPITerminalAppSceneAbout,
        text_box_get_view(app->about_screen));
}

void flipper_spi_terminal_scene_about_free(FlipperSPITerminalApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FlipperSPITerminalAppSceneAbout);

    text_box_free(app->about_screen);
}

void flipper_spi_terminal_scene_about_on_enter(void* context) {
    SPI_TERM_CONTEXT_TO_APP(context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperSPITerminalAppSceneAbout);
}

bool flipper_spi_terminal_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void flipper_spi_terminal_scene_about_on_exit(void* context) {
    UNUSED(context);
}
