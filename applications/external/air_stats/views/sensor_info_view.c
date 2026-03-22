/*
 * sensor_info_view.c — sensor technical info screen.
 * Shows sensor type, interface and address/GPIO — same as unitemp CAROUSEL_INFO.
 * NOT readings — use the main screen for live data.
 */
#include "../air_stats_i.h"
#include "../sensors/unitemp/interfaces/SingleWireSensor.h"
#include "../sensors/unitemp/interfaces/OneWireSensor.h"
#include "../sensors/unitemp/interfaces/I2CSensor.h"

static View* view;
static Sensor* info_sensor;

#define VIEW_ID ViewSensorInfo

static uint32_t _exit_callback(void* context) {
    UNUSED(context);
    return ViewSensorActions;
}

static void draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    if(!info_sensor || !app->sensors_ready) return;

    /* Sensor name — large font at top */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 12, info_sensor->name);

    /* "Type:" label */
    canvas_draw_str(canvas, 0, 24, "Type:");

    /* Interface-specific info — mirrors unitemp _draw_carousel_info */
    canvas_set_font(canvas, FontSecondary);

    if(info_sensor->type->interface == &I2C) {
        canvas_draw_str(canvas, 36, 24, info_sensor->type->typename);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 36, "I2C addr:");
        canvas_draw_str(canvas, 0, 47, "SCL:");
        canvas_draw_str(canvas, 0, 58, "SDA:");

        canvas_set_font(canvas, FontSecondary);
        snprintf(
            app->buff,
            BUFF_SIZE,
            "0x%02X",
            ((I2CSensor*)info_sensor->instance)->currentI2CAdr >> 1);
        canvas_draw_str(canvas, 60, 36, app->buff);
        canvas_draw_str(canvas, 36, 47, "C1 (pin 15) -> SCL");
        canvas_draw_str(canvas, 36, 58, "C0 (pin 16) -> SDA");

    } else if(info_sensor->type->interface == &SINGLE_WIRE) {
        canvas_draw_str(canvas, 36, 24, info_sensor->type->typename);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 36, "GPIO:");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 36, 36, ((SingleWireSensor*)info_sensor->instance)->gpio->name);

    } else if(info_sensor->type->interface == &ONE_WIRE) {
        OneWireSensor* ow = info_sensor->instance;
        canvas_draw_str(canvas, 36, 24, unitemp_onewire_sensor_getModel(info_sensor));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 36, "GPIO:");
        canvas_draw_str(canvas, 0, 47, "ID:");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 36, 36, ow->bus->gpio->name);
        snprintf(
            app->buff,
            BUFF_SIZE,
            "%02X%02X%02X%02X%02X%02X%02X%02X",
            ow->deviceID[0],
            ow->deviceID[1],
            ow->deviceID[2],
            ow->deviceID[3],
            ow->deviceID[4],
            ow->deviceID[5],
            ow->deviceID[6],
            ow->deviceID[7]);
        canvas_draw_str(canvas, 18, 47, app->buff);

    } else if(info_sensor->type == &MHZ19C_UART) {
        /* MHZ19C UART pinout */
        canvas_draw_str(canvas, 36, 24, "MHZ19C UART");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 36, "C1 (pin 15) -> RXD (pin 3)");
        canvas_draw_str(canvas, 0, 47, "C0 (pin 16) -> TXD (pin 2)");
        canvas_draw_str(canvas, 0, 58, "5V (pin 1) GND (pin 8) 9600");

    } else {
        /* MHZ19C PWM pinout */
        canvas_draw_str(canvas, 36, 24, "MHZ19C PWM");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 36, "PA6 (pin 3) -> PWM (pin 1)");
        canvas_draw_str(canvas, 0, 47, "5V (pin 1) -> Vin (pin 4)");
        canvas_draw_str(canvas, 0, 58, "GND (pin 8) -> GND (pin 5)");
    }
}

void view_sensor_info_alloc(void) {
    view = view_alloc();
    view_set_draw_callback(view, draw_callback);
    view_set_previous_callback(view, _exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID, view);
}

void view_sensor_info_switch(Sensor* sensor) {
    info_sensor = sensor;
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID);
}

void view_sensor_info_free(void) {
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID);
    view_free(view);
}
