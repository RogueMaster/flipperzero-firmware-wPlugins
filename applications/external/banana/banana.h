#pragma once

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define MARGIN        3

#define TAG "Banana"

typedef struct {
    uint32_t counter;
    bool inverted;
} GameState;

enum MenuId {
    START,
    ABOUT,
    EXIT,
};

enum AppStatus {
    MENU,
    PLAYING,
    INFO,
};

typedef struct {
    uint8_t id;
    char* name;
} Menu;

typedef struct {
    FuriMutex* mutex;
    GameState* game_state;
    uint8_t app_status;
    uint8_t menu_selected;
} AppState;
