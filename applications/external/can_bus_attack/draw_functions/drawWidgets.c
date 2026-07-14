#include "../app_user.h"

// Draws a developmet
void draw_in_development(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 41, 0, &I_WIP);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "WORK IN\nPROGRESS");
}

// Draws device not connected
void draw_device_no_connected(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 4, 0, &I_Icon_No_canbusModule);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "DEVICE NOT\nCONNECTED");
}

// draw when a message is not recognized
void draw_transmition_failure(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 43, 0, &I_Icon_FLP_Error);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "TRANSMISSION\nFAILURE");
}

// draw when a message is send OK
void draw_send_wrong(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 43, 0, &I_Icon_FLP_Error);
    widget_add_string_multiline_element(
        app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "ERROR SENDING\nMESSAGE");
}

// draw when a message is send ok
void draw_send_ok(App* app) {
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 43, 0, &I_MSGOK);
    widget_add_string_element(
        app->widget, 65, 50, AlignCenter, AlignCenter, FontPrimary, "MESSAGE SENT OK");
}
