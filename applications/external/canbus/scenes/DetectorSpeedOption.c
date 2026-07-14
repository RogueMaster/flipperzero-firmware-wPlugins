#include "../app_user.h"

bool interr = false;

const char* bitrates_names[] = {
    "125KBPS",
    "250KBPS",
    "500KBPS",
    "1000KBPS",
};

MCP_BITRATE bitrates[] = {
    MCP_125KBPS,
    MCP_250KBPS,
    MCP_500KBPS,
    MCP_1000KBPS,
};

static uint32_t time = 0;
static const uint32_t iteration = 20;
static const uint32_t time_to_wait = 10; // seconds

// Thread to work with the speed detector
static int32_t thread_to_detect_speed(void* context);

// Scene On enter
void app_scene_speed_detector_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    app->thread = furi_thread_alloc_ex("Auto-detector Speed", 1024, thread_to_detect_speed, app);
    furi_thread_start(app->thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

// Scene On event
bool app_scene_speed_detector_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    return consumed;
}

// Scene On Exit
void app_scene_speed_detector_on_exit(void* context) {
    App* app = context;
    furi_thread_join(app->thread);
    furi_thread_free(app->thread);
    widget_reset(app->widget);
}

// Draw detecting widget
void draw_detecting_speed(App* app) {
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_multiline_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Detecting Canbus\nSpeed...");
}

// Draw when the bitrate is not detected
void draw_speed_not_detected(App* app) {
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_multiline_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Canbus Speed \nNot Detected");
}

// Draw the detected bitrate
void draw_speed_detected(App* app, const char* bitrate) {
    Widget* widget = app->widget;
    widget_reset(widget);

    furi_string_reset(app->text);
    furi_string_cat_printf(app->text, "Canbus Speed \nDetected\n%s", bitrate);

    widget_add_string_multiline_element(
        widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, furi_string_get_cstr(app->text));
}

// Callback for the timer
void timer_callback(void* context) {
    UNUSED(context);
    time++;
}

//Thread to detect the bitrate
static int32_t thread_to_detect_speed(void* context) {
    App* app = context;

    MCP2515* mcp_can = app->mcp_can;
    mcp_can->mode = MCP_LISTENONLY;

    bool debug = (mcp2515_init(mcp_can) == ERROR_OK) ? true : false;

    if(!debug)
        draw_device_no_connected(app);

    else
        draw_detecting_speed(app);

    furi_delay_ms(100);

    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, app);

    furi_timer_start(timer, 1000);

    uint32_t counter = 0;
    uint32_t bitrate_selector = 0;

    bool was_break = false;

    ERROR_CAN response = ERROR_NOMSG;

    while(debug) {
        response = is_this_bitrate(mcp_can, bitrates[bitrate_selector]);

        if(response == ERROR_OK) {
            counter++;
            time = 0;
        }

        if((counter > 0) && (response == ERROR_FAIL)) {
            bitrate_selector++;
            counter = 0;
        }

        if(counter > iteration) break;

        if(bitrate_selector > 3) break;

        if(time > time_to_wait) break;

        if(!furi_hal_gpio_read(&gpio_button_back)) {
            was_break = true;
            break;
        }

        furi_delay_ms(1);
    }

    if(response == ERROR_NOMSG && debug && !was_break) {
        draw_speed_not_detected(app);
    }

    if(response != ERROR_NOMSG && debug && !was_break) {
        if(bitrate_selector > 3) {
            draw_speed_not_detected(app);
        }

        if(bitrate_selector <= 3) {
            mcp_can->bitRate = bitrates[bitrate_selector];
            draw_speed_detected(app, bitrates_names[bitrate_selector]);
        }
        furi_delay_ms(3000);

        view_dispatcher_send_custom_event(app->view_dispatcher, 0xff);
    }

    time = 0;

    furi_timer_stop(timer);
    furi_timer_free(timer);

    deinit_mcp2515(mcp_can);

    return 0;
}
