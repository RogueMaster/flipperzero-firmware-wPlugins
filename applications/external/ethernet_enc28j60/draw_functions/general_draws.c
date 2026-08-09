#include "../app_user.h"

// Draws a developmet
void draw_in_development(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 41, 0, &I_WIP45x38);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "WORK IN\nPROGRESS");
}

// Draws device not connected
void draw_device_no_connected(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 4, 0, &I_NOC119x38);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "DEVICE NOT\nCONNECTED");
}

// Draws if the network is not link
void draw_network_not_connected(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 41, 0, &I_NCR);
    widget_add_string_multiline_element(
        app->widget, 64, 60, AlignCenter, AlignBottom, FontPrimary, "Network\nNot Detected");
}

// Function to draw when is waiting for the IP of the DORA process
void draw_waiting_for_ip(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Waiting for IP");
}

// Function to draw when a Ip is got it
void draw_your_ip_is(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Your IP is: ");

    furi_string_reset(app->text);

    furi_string_cat_printf(
        app->text,
        "%u:%u:%u:%u",
        app->ethernet->ip_address[0],
        app->ethernet->ip_address[1],
        app->ethernet->ip_address[2],
        app->ethernet->ip_address[3]);

    widget_add_string_element(
        app->widget,
        64,
        40,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

// Draw if you didnt got it the IP address
void draw_ip_not_got_it(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 15, AlignCenter, AlignCenter, FontPrimary, "IP didnt got it");
    widget_add_string_element(
        app->widget, 64, 30, AlignCenter, AlignCenter, FontSecondary, "IP by default: ");

    furi_string_reset(app->text);

    furi_string_cat_printf(
        app->text,
        "%u:%u:%u:%u",
        app->ethernet->ip_address[0],
        app->ethernet->ip_address[1],
        app->ethernet->ip_address[2],
        app->ethernet->ip_address[3]);

    widget_add_string_element(
        app->widget,
        64,
        40,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        furi_string_get_cstr(app->text));
}

void draw_dora_needed(App* app) {
    widget_reset(app->widget);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "DORA PROCESS NEEDED");
    widget_add_string_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(app->text));
}

void draw_text(App* app, const char* text) {
    widget_reset(app->widget);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "THE OS IS %s", text);
    widget_add_string_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(app->text));
}

// Draws port open
void draw_port_open(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 41, 0, &I_PO119x38);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "PORT OPEN");
}

// Draws port not open
void draw_port_not_open(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 41, 0, &I_NCR);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "PORT NOT OPEN");
}

void draw_dora_failed(App* app) {
    widget_reset(app->widget);
    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "DORA PROCESS FAILED");
    widget_add_string_element(
        app->widget,
        64,
        32,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        furi_string_get_cstr(app->text));
}

// Function to draw if the user wants to set the IP
void draw_ask_for_ip(App* app) {
    widget_reset(app->widget);
    widget_add_string_multiline_element(
        app->widget,
        64,
        0,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Do you want to continue with\nthe same IP\nor\ndo you want to get\nit from the network?");

    widget_add_button_element(app->widget, GuiButtonTypeLeft, "No", NULL, app);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "Yes", NULL, app);
}
