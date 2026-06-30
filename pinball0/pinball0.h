#pragma once

#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <cmath>
// #include <furi_hal_resources.h>
// #include <furi_hal_gpio.h>
#include <furi.h>
#include <dolphin/dolphin.h>
#include <storage/storage.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "vec2.h"
#include "objects.h"
#include "settings.h"

// #define DRAW_NORMALS

#define TAG     "Pinball0"
#define VERSION "v0.5.2"

// Vertical orientation
#define LCD_WIDTH  64
#define LCD_HEIGHT 128

typedef enum GameMode {
    GM_TableSelect,
    GM_Playing,
    GM_GameOver,
    GM_Error,
    GM_Settings,
    GM_Tilted
} GameMode;

class TableList {
public:
    TableList() = default;
    ~TableList() {
        for(auto& mi : menu_items) {
            furi_string_free(mi.name);
            furi_string_free(mi.filename);
        }
    }

    typedef struct {
        FuriString* name;
        FuriString* filename;
    } TableMenuItem;

    std::vector<TableMenuItem> menu_items;
    int display_size; // how many can fit on screen
    int selected;
};

class Table;

typedef struct PinballApp {
    PinballApp();
    ~PinballApp();

    bool initialized;

    FuriMutex* mutex;

    TableList table_list;

    GameMode game_mode;
    Table* table; // data for the current table
    uint32_t tick;

    bool keys[4]; // which key was pressed?
    bool processing; // controls game loop and game objects
    uint32_t idle_start; // tracks time of last key press

    // user settings
    PinballSettings settings;

    // system objects
    Storage* storage;
    NotificationApp* notify; // allows us to blink/buzz during game
    char text[256]; // general temp buffer

} PinballApp;
