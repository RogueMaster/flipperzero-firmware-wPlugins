/***
 * Noptel LRF rangefinder sampler for the Flipper Zero
 * Version: 2.4
 *
 * Test boot time view
***/

/*** Includes ***/
#include "common.h"
#include "lrf_power_control.h"
#include "noptel_lrf_sampler_icons.h" /* Generated from images in assets */

/*** Routines ***/

/** Time difference in milliseconds between system ticks in milliseconds,
    taking the timestamp overflow into account **/
static uint32_t ms_tick_time_diff_ms(uint32_t tstamp1, uint32_t tstamp2) {
    if(tstamp1 >= tstamp2)
        return tstamp1 - tstamp2;

    else
        return 0xffffffff - tstamp2 + 1 + tstamp1;
}

/** LRF boot information handler
    Called when a LRF information from a boot string is available from the LRF
    serial communication app **/
static void lrf_boot_info_handler(LRFBootInfo* lrf_boot_info, void* ctx) {
    App* app = (App*)ctx;
    TestBootTimeModel* testboottime_model = view_get_model(app->testboottime_view);

    /* Copy the boot information */
    memcpy(&(testboottime_model->boot_info), lrf_boot_info, sizeof(LRFBootInfo));

    /* Were we waiting for a boot string? */
    if(testboottime_model->await_boot_info) {
        /* Calculate the boot time */
        testboottime_model->boot_time_ms = ms_tick_time_diff_ms(
            testboottime_model->boot_info.boot_string_rx_tstamp,
            testboottime_model->power_on_tstamp);

        /* We're not waiting for a boot string anymore */
        testboottime_model->await_boot_info = false;
    } else
        testboottime_model->boot_time_ms = 0;

    /* Mark the boot information as valid */
    testboottime_model->has_boot_info = true;

    /* Trigger a test boot time view redraw */
    with_view_model(app->testboottime_view, TestBootTimeModel * _model, { UNUSED(_model); }, true);
}

/** Test boot time view enter callback **/
void testboottime_view_enter_callback(void* ctx) {
    App* app = (App*)ctx;

    with_view_model(
        app->testboottime_view,
        TestBootTimeModel * testboottime_model,
        {
            /* Currently not waiting for a boot string */
            testboottime_model->await_boot_info = false;

            /* Start the UART at the correct baudrate */
            start_uart(app->lrf_serial_comm_app, app->config.baudrate);

            /* Invalidate the current identification - if any */
            testboottime_model->has_boot_info = false;
            testboottime_model->boot_time_ms = 0;

            /* Setup the callback to receive decoded LRF boot information */
            set_lrf_boot_info_handler(app->lrf_serial_comm_app, lrf_boot_info_handler, app);

            /* Turn off the LRF */
            FURI_LOG_I(TAG, "LRF power off");
            power_lrf(false, NULL);

            /* Wait one second */
            furi_delay_ms(1000);

            /* Now we wait for a boot string */
            testboottime_model->await_boot_info = true;

            /* Turn the LRF back on and mark the power-on timestamp */
            FURI_LOG_I(TAG, "LRF power on");
            power_lrf(true, &testboottime_model->power_on_tstamp);
        },
        false);
}

/** Test boot time view exit callback **/
void testboottime_view_exit_callback(void* ctx) {
    App* app = (App*)ctx;

    /* Unset the callback to receive decoded LRF boot information */
    set_lrf_boot_info_handler(app->lrf_serial_comm_app, NULL, app);

    /* Stop the UART */
    stop_uart(app->lrf_serial_comm_app);
}

/** Draw callback for the test boot time view **/
void testboottime_view_draw_callback(Canvas* canvas, void* model) {
    TestBootTimeModel* testboottime_model = (TestBootTimeModel*)model;
    uint8_t boot_time_str_halfsize;

    /* First print all the things we need to print in the FontPrimary font
     (bold, proportional) */
    canvas_set_font(canvas, FontPrimary);

    /* Do we have a boot time to display? */
    if(testboottime_model->boot_time_ms > 0 && testboottime_model->boot_time_ms < 10000) {
        /* Work out the string for the numerical value to display */
        snprintf(
            testboottime_model->spstr,
            sizeof(testboottime_model->spstr),
            "%ld",
            testboottime_model->boot_time_ms);

        /* Work out the half-size of the string */
        boot_time_str_halfsize = (strlen(testboottime_model->spstr) * 12) / 2;

        /* Print "ms" right of the boot time value */
        canvas_draw_str(canvas, 64 + boot_time_str_halfsize, 39, "ms");
    }

    else
        boot_time_str_halfsize = 0;

    /* Do we have a valid identification to display? */
    if(testboottime_model->has_boot_info) {
        /* Draw the identification fields' names */
        canvas_draw_str(canvas, 13, 8, "ID");
        canvas_draw_str(canvas, 2, 17, "F/W");
    }

    /* Print the OK button symbol followed by "Test" in a frame at the
     right-hand side */
    canvas_draw_frame(canvas, 77, 52, 51, 12);
    canvas_draw_icon(canvas, 79, 54, &I_ok_button);
    canvas_draw_str(canvas, 102, 62, "Test");

    /* Draw a dividing line between the LRF information and the bottom line */
    canvas_draw_line(canvas, 0, 48, 128, 48);

    /* Do we have a valid identification to display? */
    if(testboottime_model->has_boot_info) {
        /* Second draw the identification values in the FontSecondary font
       (normal, proportional) */
        canvas_set_font(canvas, FontSecondary);

        /* Draw the identification values */
        canvas_draw_str(canvas, 26, 8, testboottime_model->boot_info.id);
        canvas_draw_str(canvas, 26, 17, testboottime_model->boot_info.fwversion);
    }

    /* Do we have a boot time value to display? */
    if(boot_time_str_halfsize) {
        /* Print the boot time in the FontBigNumber font */
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 64 - boot_time_str_halfsize, 39, testboottime_model->spstr);
    }
}

/** Input callback for the test boot time view **/
bool testboottime_view_input_callback(InputEvent* evt, void* ctx) {
    App* app = (App*)ctx;
    TestBootTimeModel* testboottime_model = view_get_model(app->testboottime_view);

    /* If the user pressed the OK button, power-cycle the LRF */
    if(evt->type == InputTypePress && evt->key == InputKeyOk) {
        FURI_LOG_D(TAG, "OK button pressed");

        /* Invalidate the current identification - if any */
        testboottime_model->has_boot_info = false;
        testboottime_model->boot_time_ms = 0;

        /* Trigger an LRF info view redraw to clear the information currently
       displayed - if any */
        with_view_model(
            app->testboottime_view, TestBootTimeModel * _model, { UNUSED(_model); }, true);

        /* Turn off the LRF */
        FURI_LOG_I(TAG, "LRF power off");
        power_lrf(false, NULL);

        /* Wait one second */
        furi_delay_ms(1000);

        /* Now we wait for a boot string */
        testboottime_model->await_boot_info = true;

        /* Turn the LRF back on and mark the power-on timestamp */
        FURI_LOG_I(TAG, "LRF power on");
        power_lrf(true, &testboottime_model->power_on_tstamp);

        return true;
    }

    /* We haven't handled this event */
    return false;
}
