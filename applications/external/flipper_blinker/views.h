#pragma once

#include <gui/view.h>

typedef struct FlipperBlinkerTransmitView FlipperBlinkerTransmitView;

FlipperBlinkerTransmitView* flipper_blinker_transmit_view_alloc(void);
void flipper_blinker_transmit_view_free(FlipperBlinkerTransmitView* transmit_view);
View* flipper_blinker_transmit_view_get_view(FlipperBlinkerTransmitView* transmit_view);
void flipper_blinker_transmit_view_set_callback(
    FlipperBlinkerTransmitView* transmit_view,
    ViewNavigationCallback callback,
    void* context);
void flipper_blinker_transmit_view_reset(
    FlipperBlinkerTransmitView* transmit_view,
    const char* message);
void flipper_blinker_transmit_view_update(
    FlipperBlinkerTransmitView* transmit_view,
    char current_char,
    size_t current_index,
    size_t total);
void flipper_blinker_transmit_view_set_status(
    FlipperBlinkerTransmitView* transmit_view,
    const char* status);
