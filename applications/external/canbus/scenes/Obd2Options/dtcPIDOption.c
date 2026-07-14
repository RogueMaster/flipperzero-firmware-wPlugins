#include "../../app_user.h"

// This variable works to know if it wishes to delete the DTC storage
static bool delete_dtc = false;

// Function of the thread
static int32_t obdii_thread_dtc_on_work(void* context);

/*
    Scene to watch the errors in the car
*/

void app_scene_obdii_get_errors_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    app->thread = furi_thread_alloc_ex("ShowDTC", 1024, obdii_thread_dtc_on_work, app);
    furi_thread_start(app->thread);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_obdii_get_errors_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    UNUSED(app);
    UNUSED(event);
    return false;
}

void app_scene_obdii_get_errors_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
    widget_reset(app->widget);
}

/*
    Scene to Delete DTC en car
*/

void app_scene_obdii_delete_dtc_on_enter(void* context) {
    App* app = context;
    delete_dtc = true;
    widget_reset(app->widget);
    app->thread = furi_thread_alloc_ex("ShowDTC", 1024, obdii_thread_dtc_on_work, app);
    furi_thread_start(app->thread);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool app_scene_obdii_delete_dtc_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);

    return consumed;
}

void app_scene_obdii_delete_dtc_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
    delete_dtc = false;
    widget_reset(app->widget);
}

/*
    Thread to request the DTC (Diagnostic Trouble Codes)
*/

static int32_t obdii_thread_dtc_on_work(void* context) {
    App* app = context;

    OBDII scanner;

    FuriString* text = app->text;

    scanner.bitrate = app->mcp_can->bitRate;

    bool loop = false;

    char* codes[20];

    for(uint8_t i = 0; i < 20; i++) {
        codes[i] = (char*)malloc(5 * sizeof(char));
    }

    uint8_t dtc_selector = 0, count_dtc = 0;

    bool run = pid_init(&scanner);

    if(run) {
        furi_delay_ms(500); // Time added to initialize
    }

    if(delete_dtc && run) {
        if(clear_dtc(&scanner)) {
            widget_reset(app->widget);
            widget_add_icon_element(app->widget, 40, 0, &I_DTCLN47x20);
            widget_add_string_multiline_element(
                app->widget, 65, 43, AlignCenter, AlignBottom, FontPrimary, "ALL DTC\nCLEARED");

        } else {
            widget_reset(app->widget);

            widget_add_string_multiline_element(
                app->widget, 65, 43, AlignCenter, AlignBottom, FontPrimary, "E R R O R\nCLEARING");
        }
    } else if(!delete_dtc && run) {
        if(request_dtc(&scanner, &(count_dtc), codes)) {
            if(count_dtc == 0) {
                widget_add_string_multiline_element(
                    app->widget, 65, 43, AlignCenter, AlignBottom, FontPrimary, "NO DTC\nDETECTED");

            } else
                loop = true;

        } else {
            widget_add_icon_element(app->widget, 43, 0, &I_MSGERROR41x38);
            widget_add_string_multiline_element(
                app->widget, 65, 60, AlignCenter, AlignBottom, FontPrimary, "REQUEST\nERROR");
        }

    } else {
        draw_device_no_connected(app);
    }

    uint8_t past_selector = 1;

    while(loop) {
        if(!furi_hal_gpio_read(&gpio_button_right)) {
            dtc_selector++;
            if(dtc_selector > (count_dtc - 1)) dtc_selector = 0;
            furi_delay_ms(500);
        }

        if(!furi_hal_gpio_read(&gpio_button_left)) {
            dtc_selector--;
            if(dtc_selector > count_dtc) dtc_selector = (count_dtc - 1);
            furi_delay_ms(500);
        }

        if(past_selector != dtc_selector) {
            widget_reset(app->widget);
            furi_string_reset(text);

            furi_string_printf(text, "%u of %u DTC", dtc_selector + 1, count_dtc);

            widget_add_string_element(
                app->widget,
                65,
                20,
                AlignCenter,
                AlignBottom,
                FontSecondary,
                furi_string_get_cstr(text));

            furi_string_reset(text);

            furi_string_printf(text, "%s", codes[dtc_selector]);

            widget_add_string_element(
                app->widget,
                65,
                35,
                AlignCenter,
                AlignBottom,
                FontPrimary,
                furi_string_get_cstr(text));

            past_selector = dtc_selector;
        }

        if(!furi_hal_gpio_read(&gpio_button_back)) {
            break;
        }
        furi_delay_ms(1);
    }

    for(uint8_t i = 0; i < 20; i++) {
        free(codes[i]);
    }

    furi_string_reset(text);

    pid_deinit(&scanner);

    return 0;
}
