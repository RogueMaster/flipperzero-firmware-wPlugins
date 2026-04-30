#pragma once

#include "../flippass.h"

bool flippass_output_transport_begin(App* app, FlipPassOutputTransport transport);
void flippass_output_transport_end(App* app, FlipPassOutputTransport transport);
bool flippass_output_transport_press_prepared(
    App* app,
    FlipPassOutputTransport transport,
    uint16_t hid_key);
bool flippass_output_transport_release_prepared(
    App* app,
    FlipPassOutputTransport transport,
    uint16_t hid_key);
void flippass_output_transport_release_all_prepared(App* app, FlipPassOutputTransport transport);
bool flippass_output_transport_is_connected(const App* app, FlipPassOutputTransport transport);
bool flippass_output_transport_is_advertising(const App* app, FlipPassOutputTransport transport);
bool flippass_output_transport_advertise(App* app, FlipPassOutputTransport transport);
bool flippass_output_transport_prewarm(App* app, FlipPassOutputTransport transport);
void flippass_output_transport_get_name(
    App* app,
    FlipPassOutputTransport transport,
    char* buffer,
    size_t size);
void flippass_output_transport_cleanup(App* app, FlipPassOutputTransport transport);
