#include "../trident_i.h"

void trident_scene_about_on_enter(void* context) {
    TridentApp* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_icon_element(widget, 2, 2, &I_trident_10px);
    widget_add_string_element(
        widget, 16, 2, AlignLeft, AlignTop, FontPrimary, "Trident v" TRIDENT_VERSION);
    widget_add_string_element(
        widget, 16, 14, AlignLeft, AlignTop, FontSecondary, "3-in-1 board control");

    widget_add_text_scroll_element(
        widget,
        0,
        26,
        128,
        38,
        "One control surface for a\n"
        "3-in-1 expansion board:\n"
        "ESP32 + NRF24 + CC1101.\n"
        " \n"
        "== ESP32 (Wi-Fi / BT / GPS) ==\n"
        "Front-end for an ESP32\n"
        "Marauder board over the GPIO\n"
        "UART at 115200. Scan, sniff,\n"
        "target, attack, BLE spam,\n"
        "wardrive, plus a live console.\n"
        "Pins in Settings > UART:\n"
        "  13/14 - standard wiring\n"
        "  15/16 - alternate wiring\n"
        " \n"
        "== NRF24 (2.4 GHz) ==\n"
        "Spectrum analyzer across all\n"
        "126 channels (2400-2525 MHz)\n"
        "using the Received Power\n"
        "Detector. Read-only.\n"
        "Wiring: 2=MOSI 3=MISO 4=CSN\n"
        "  5=SCK 6=CE  3V3=VCC GND=GND\n"
        " \n"
        "== CC1101 (Sub-GHz) ==\n"
        "RSSI sweep over 300-348,\n"
        "387-464 and 779-928 MHz.\n"
        "Pick Internal or External\n"
        "CC1101 in Settings (External\n"
        "needs cc1101_ext firmware).\n"
        " \n"
        "OK on an analyzer resets the\n"
        "peak / activity hold.\n"
        " \n"
        "Speaks the upstream ESP32\n"
        "Marauder CLI by justcallmekoko.\n"
        " \n"
        "For authorised testing and RF\n"
        "exploration only - use it on\n"
        "hardware and networks you own\n"
        "or are permitted to assess.\n"
        " \n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb/Trident-FlipperZero");

    view_dispatcher_switch_to_view(app->view_dispatcher, TridentViewWidget);
}

bool trident_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void trident_scene_about_on_exit(void* context) {
    TridentApp* app = context;
    widget_reset(app->widget);
}
