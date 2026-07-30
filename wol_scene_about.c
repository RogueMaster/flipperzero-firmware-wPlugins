#include "wol_flipper.h"

void wol_scene_about_on_enter(void* context) {
    WolApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "\e#WoL Flipper\n"
        "Wake-on-LAN over the WiFi dev\n"
        "board.\n\n"
        "\e#Board\n"
        "Official ESP32-S2 dev board on\n"
        "the GPIO header, running the\n"
        "companion firmware from this\n"
        "project at 115200 baud. 5V on\n"
        "pin 1 is switched on while the\n"
        "board is in use.\n\n"
        "\e#How it works\n"
        "The Flipper sends one line with\n"
        "SSID, MAC and broadcast address.\n"
        "The board joins Wi-Fi and puts\n"
        "the magic packet (6x FF + 16x\n"
        "MAC) on the wire three times as\n"
        "a UDP broadcast to port 9.\n\n"
        "\e#Flasher\n"
        "ESP board menu can dump the\n"
        "current ESP flash to SD, write\n"
        "the WoL firmware and put the old\n"
        "image back. Bootloader mode is a\n"
        "manual BOOT+RESET, the header\n"
        "carries no DTR/RTS.\n\n"
        "\e#Notes\n"
        "Enable Wake-on-LAN in the target\n"
        "BIOS and NIC settings. Use the\n"
        "subnet broadcast (e.g.\n"
        "192.168.1.255) if the router\n"
        "drops 255.255.255.255.\n");

    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewWidget);
}

bool wol_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wol_scene_about_on_exit(void* context) {
    WolApp* app = context;
    widget_reset(app->widget);
}
