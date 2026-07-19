#include "../trident_i.h"
#include <stdio.h>

/*
 * NRF24 promiscuous sniffer (experimental). Camps one channel with CRC off and
 * a preamble-as-address filter, printing each captured frame's leading bytes to
 * the log. Real-world capture is hardware- and luck-dependent; this is a best
 * effort RF-exploration tool, not a guaranteed decoder.
 *   Left / Right - change channel      OK - clear the log
 */

// worker-thread callback: push a captured frame line into the log
static void trident_scene_nrf24sniff_line(void* context, const char* line) {
    TridentApp* app = context;
    console_view_push_line(app->console_view, line);
}

static void trident_scene_nrf24sniff_ok(void* context) {
    TridentApp* app = context;
    console_view_clear(app->console_view);
}

static void trident_scene_nrf24sniff_key(void* context, InputKey key) {
    TridentApp* app = context;
    int ch = nrf24_radio_get_channel(app->nrf24);
    if(key == InputKeyLeft && ch > 0) ch--;
    if(key == InputKeyRight && ch < 125) ch++;
    nrf24_radio_set_channel(app->nrf24, (uint8_t)ch);
}

void trident_scene_nrf24sniff_on_enter(void* context) {
    TridentApp* app = context;
    ConsoleView* cv = app->console_view;

    trident_link_disarm(app);

    console_view_clear(cv);
    console_view_set_header(cv, "NRF24 Sniffer");
    console_view_set_autoscroll(cv, true);
    console_view_set_ok_callback(cv, trident_scene_nrf24sniff_ok, app);
    console_view_set_key_callback(cv, trident_scene_nrf24sniff_key, app);
    console_view_set_footer_left(cv, "OK:clr");
    console_view_set_empty(cv, "Listening (promiscuous)", "<> channel   OK clear");

    nrf24_radio_set_line_callback(app->nrf24, trident_scene_nrf24sniff_line, app);
    nrf24_radio_set_mode(app->nrf24, Nrf24ModeSniff);
    nrf24_radio_start(app->nrf24);

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewConsole);
}

bool trident_scene_nrf24sniff_on_event(void* context, SceneManagerEvent event) {
    TridentApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        char foot[16];
        snprintf(foot, sizeof(foot), "Ch %u", nrf24_radio_get_channel(app->nrf24));
        console_view_set_footer_right(app->console_view, foot);
        console_view_set_live(app->console_view, nrf24_radio_is_running(app->nrf24));
        console_view_tick(app->console_view);
        consumed = true;
    }
    return consumed;
}

void trident_scene_nrf24sniff_on_exit(void* context) {
    TridentApp* app = context;
    nrf24_radio_stop(app->nrf24);
    nrf24_radio_set_line_callback(app->nrf24, NULL, NULL);
}
