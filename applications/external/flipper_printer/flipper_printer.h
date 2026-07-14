#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/submenu.h>
#include <gui/view.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#define TEXT_BUFFER_SIZE 256
#define TAG              "FlipperPrinter"

// View identifiers
typedef enum {
    ViewIdMenu, // Main menu
    ViewIdCoinFlip, // Coin flip game view
    ViewIdTextInput, // Text input keyboard
    ViewIdPrinterSetup, // Printer setup info view
} ViewId;

// Menu items
typedef enum {
    MenuItemCoinFlip,
    MenuItemTextPrint,
    MenuItemPrinterSetup,
    MenuItemExit,
} MenuItem;

// Application state
typedef struct {
    // GUI components
    ViewDispatcher* view_dispatcher;
    Gui* gui;
    NotificationApp* notification;

    // Views
    Submenu* menu;
    View* coin_flip_view;
    TextInput* text_input;
    View* printer_setup_view;

    // Data
    char text_buffer[TEXT_BUFFER_SIZE];

    // Coin flip statistics
    int32_t total_flips;
    int32_t heads_count;
    int32_t tails_count;
    bool last_flip_was_heads;

    // Streak tracking
    int32_t current_streak;
    int32_t longest_streak;
    bool streak_is_heads;
} FlipperPrinterApp;

// Printer functions (printer.c)
void printer_init(void);
void printer_deinit(void);
void printer_print_text(const char* text);
void printer_print_coin_flip(int32_t flip_number, bool is_heads);

// Coin flip view functions (coin_flip.c)
View* coin_flip_view_alloc(FlipperPrinterApp* app);
void coin_flip_view_free(View* view);

// Menu functions (menu.c)
Submenu* menu_alloc(FlipperPrinterApp* app);
void menu_free(Submenu* menu);

// Printer setup view functions (printer_setup.c)
View* printer_setup_view_alloc();
void printer_setup_view_free(View* view);
