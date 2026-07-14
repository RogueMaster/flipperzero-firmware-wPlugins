#pragma once

#include <furi.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include "src/util/buffer.h"
#include "src/util/list.h"
#include "src/util/card.h"
#include <notification/notification.h>

typedef struct {
    Card* card;
    Vector position;
    Vector velocity;
} AnimatedCard;

typedef struct {
    Canvas* canvas;
    Gui* gui;
    FuriPubSub* input;
    FuriPubSubSubscription* input_subscription;
    bool exit;
    bool isDirty;
    bool clearBuffer;
    bool lateRender;
    uint8_t scene_switch;
    Buffer* buffer;
    NotificationApp* notification_app;
    uint8_t selected[2];
    uint8_t selected_card;

    List* deck;
    List* waste;
    List* hand;
    List* foundation[4];
    List* tableau[7];

    AnimatedCard animated_card;
    double delta_time;
    size_t game_start;
    size_t game_end;

} GameState;
