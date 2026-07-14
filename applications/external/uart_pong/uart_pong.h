#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

#define TAG "UARTPong"

#define FPS   30
#define SPEED 1.3f

#define degToRad(angleInDegrees) ((angleInDegrees) * M_PI / 180.0)

typedef struct {
    uint8_t y;
    uint8_t score;
} Player;

typedef struct {
    bool up;
    bool down;
} Controls;

typedef struct {
    float x;
    float y;
    float dx;
    float dy;
} Ball;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMutex* app_mutex;
    FuriMessageQueue* event_queue;

    FuriMutex* mutex;
    FuriTimer* timer;
    FuriHalSerialHandle* handle;

    Player me;
    Player enemy;
    Controls controls;
    Ball ball;
} UartPongApp;
