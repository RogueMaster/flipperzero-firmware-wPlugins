#include "flipper_printer.h"

// Menu selection callback
static void menu_callback(void* context, uint32_t index) {
    FlipperPrinterApp* app = context;

    switch(index) {
    case MenuItemCoinFlip:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdCoinFlip);
        break;

    case MenuItemTextPrint:
        // Clear text buffer before showing keyboard
        memset(app->text_buffer, 0, TEXT_BUFFER_SIZE);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdTextInput);
        break;

        // DISABLED: Printer setup view causes system crashes
        // case MenuItemPrinterSetup:
        //     view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdPrinterSetup);
        //     break;

    case MenuItemExit:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }
}

// Allocate and initialize menu
Submenu* menu_alloc(FlipperPrinterApp* app) {
    Submenu* menu = submenu_alloc();

    submenu_set_header(menu, "Flipper Printer");
    submenu_add_item(menu, "Coin Flip Game", MenuItemCoinFlip, menu_callback, app);
    submenu_add_item(menu, "Print Custom Text", MenuItemTextPrint, menu_callback, app);
    // DISABLED: Printer setup causes crashes - submenu_add_item(menu, "Printer Setup Info", MenuItemPrinterSetup, menu_callback, app);
    submenu_add_item(menu, "Exit", MenuItemExit, menu_callback, app);

    return menu;
}

// Free menu
void menu_free(Submenu* menu) {
    submenu_free(menu);
}
