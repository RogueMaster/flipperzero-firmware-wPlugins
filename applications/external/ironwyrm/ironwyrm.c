#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <stdlib.h>

#include "core/game.h"
#include "renderer.h"

typedef struct {
    Gui* gui;
    ViewPort* viewPort;
    FuriMessageQueue* inputQueue;
    FuriMutex* gameMutex;
    GameState gameState;
} FlipperApp;

static void DrawCallback(Canvas* canvas, void* context) {
    FlipperApp* app = context;

    furi_mutex_acquire(app->gameMutex, FuriWaitForever);
    DrawGame(canvas, &app->gameState);
    furi_mutex_release(app->gameMutex);
}

static void InputCallback(InputEvent* inputEvent, void* context) {
    FuriMessageQueue* inputQueue = context;
    furi_message_queue_put(inputQueue, inputEvent, 0);
}

static GameAction TranslateInput(const InputEvent* inputEvent) {
    bool isDirectionalEvent =
        inputEvent->type == InputTypeShort ||
        inputEvent->type == InputTypeRepeat;

    if(isDirectionalEvent) {
        switch(inputEvent->key) {
        case InputKeyUp:
            return GameActionUp;
        case InputKeyDown:
            return GameActionDown;
        case InputKeyLeft:
            return GameActionLeft;
        case InputKeyRight:
            return GameActionRight;
        default:
            break;
        }
    }

    if(inputEvent->type == InputTypeShort && inputEvent->key == InputKeyOk) {
        return GameActionConfirm;
    }

    return GameActionNone;
}

int32_t ironwyrm_app(void* context) {
    UNUSED(context);

    FlipperApp* app = malloc(sizeof(FlipperApp));
    furi_check(app != NULL);
    *app = (FlipperApp){0};

    app->inputQueue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gameMutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->viewPort = view_port_alloc();

    furi_check(app->inputQueue != NULL);
    furi_check(app->gameMutex != NULL);
    furi_check(app->viewPort != NULL);

    InitializeGame(&app->gameState, furi_hal_random_get());

    view_port_draw_callback_set(app->viewPort, DrawCallback, app);
    view_port_input_callback_set(app->viewPort, InputCallback, app->inputQueue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->viewPort, GuiLayerFullscreen);

    bool shouldExit = false;

    while(!shouldExit) {
        InputEvent inputEvent;

        if(
            furi_message_queue_get(
                app->inputQueue,
                &inputEvent,
                FuriWaitForever
            ) != FuriStatusOk
        ) {
            continue;
        }

        if(inputEvent.key == InputKeyBack && inputEvent.type == InputTypeShort) {
            furi_mutex_acquire(app->gameMutex, FuriWaitForever);

            if(app->gameState.mode == GameModeShopPlacement || app->gameState.mode == GameModeIntro) {
                HandleGameAction(&app->gameState, GameActionCancel);
                
            } else {
                shouldExit = true;
            }

            furi_mutex_release(app->gameMutex);

            if(!shouldExit) {
                view_port_update(app->viewPort);
            }

            continue;
        }

        GameAction action = TranslateInput(&inputEvent);

        if(action != GameActionNone) {
            furi_mutex_acquire(app->gameMutex, FuriWaitForever);
            HandleGameAction(&app->gameState, action);
            furi_mutex_release(app->gameMutex);
            view_port_update(app->viewPort);
        }
    }

    gui_remove_view_port(app->gui, app->viewPort);
    furi_record_close(RECORD_GUI);

    view_port_free(app->viewPort);
    furi_mutex_free(app->gameMutex);
    furi_message_queue_free(app->inputQueue);
    free(app);

    return 0;
}
