#ifndef CAIXIANLIN_UI_H
#define CAIXIANLIN_UI_H

#include "caixianlin_types.h"

// Initialize UI (ViewPort, callbacks)
void caixianlin_ui_init(CaixianlinRemoteApp* app);

// Cleanup UI
void caixianlin_ui_deinit(CaixianlinRemoteApp* app);

// Main draw callback (renders current screen)
void caixianlin_ui_draw(Canvas* canvas, void* ctx);

// Input callback (queues input events)
void caixianlin_ui_input(InputEvent* event, void* ctx);

// Process input event for current screen
void caixianlin_ui_handle_event(CaixianlinRemoteApp* app, InputEvent* event);

#endif // CAIXIANLIN_UI_H
