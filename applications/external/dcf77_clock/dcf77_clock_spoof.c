#include <furi_hal.h>
#include <furi_hal_pwm.h>
#include <gui/gui.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <datetime/datetime.h>
#include <locale/locale.h>

#include "dcf77.h"

#define SCREEN_SIZE_X 128
#define SCREEN_SIZE_Y 64
#define DCF77_FREQ    77500
#define DCF77_OFFSET  60
#define SYNC_DELAY    50

// Weekday translator
char* WEEKDAYS[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// Struct for the app
typedef struct {
    DateTime dt;
    bool is_dst;
    FuriString* str;
    LocaleTimeFormat tim_fmt;
    LocaleDateFormat dat_fmt;
    uint32_t time_offset;
    int selection;
    bool blink_led;
} AppData;

/**
 * Draw an arrow
 * @param canvas Canvas to draw to
 * @param x X
 * @param y Y
 * @param up To make the arrow point upwards
 */
static void canvas_draw_arrow(Canvas* canvas, int32_t x, int32_t y, bool up) {
    canvas_draw_box(canvas, x + 2, y + (up ? 0 : 2), 2, 1);
    canvas_draw_box(canvas, x + 1, y + 1, 4, 1);
    canvas_draw_box(canvas, x, y + (up ? 2 : 0), 6, 1);
}

/**
 * Draw the apps UI
 * @param canvas The canvas of the UI to draw to
 * @param context Context (the app data)
 */
static void app_draw_callback(Canvas* canvas, void* context) {
    // Load app data
    AppData* app = (AppData*)context;
    furi_assert(app->str);

    // Get hour and format it according to 12h/24h time format
    uint8_t hour = app->dt.hour;
    bool fmt_12h = false;
    if(app->tim_fmt == LocaleTimeFormat12h) {
        hour = hour == 0 ? 12 : hour % 12;
        fmt_12h = true;
    }

    // Create string for displayed time
    furi_string_printf(app->str, "%2u:%02u:%02u", hour, app->dt.minute, app->dt.second);
    const char* tim_cstr = furi_string_get_cstr(app->str);

    // Print time string to UI
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(
        canvas, SCREEN_SIZE_X / 2, SCREEN_SIZE_Y / 2, AlignCenter, AlignCenter, tim_cstr);

    // Print AM/PM to the UI for 12h format
    if(fmt_12h) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            SCREEN_SIZE_X,
            (SCREEN_SIZE_Y / 2),
            AlignRight,
            AlignTop,
            (app->dt.hour >= 12 ? "PM" : "AM"));
    }

    // Create the date string ("lite")
    FuriString* dat = furi_string_alloc();
    locale_format_date(dat, &app->dt, app->dat_fmt, "-");

    // Create strings for weekday and dst
    const char* dow_str = WEEKDAYS[(app->dt.weekday - 1) % 7];
    const char* dst_str = app->is_dst ? "CEST" : "CET";

    // Combine to overall date string and free "lite" date string
    furi_string_printf(app->str, "%s %s %s", dow_str, furi_string_get_cstr(dat), dst_str);
    furi_string_free(dat);

    // Print date string to the UI
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, SCREEN_SIZE_X / 2, 0, AlignCenter, AlignTop, furi_string_get_cstr(app->str));

    // Print signal data string to the UI
    char* data = get_dcf77_data(app->dt.second);
    canvas_draw_str_aligned(canvas, SCREEN_SIZE_X, SCREEN_SIZE_Y, AlignRight, AlignBottom, data);

    // Add markers for selection
    if(app->selection > 0) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_arrow(canvas, 19 + 36 * (app->selection - 1), SCREEN_SIZE_Y / 2 + 8, true);
        canvas_draw_arrow(canvas, 31 + 36 * (app->selection - 1), SCREEN_SIZE_Y / 2 + 8, false);
    }
}

/**
 * Handle an input event
 * @param input_event The input event to handle
 * @param context Context (the message queue)
 */
static void app_input_callback(InputEvent* input_event, void* context) {
    // Get the event ant put it into the context message queue
    furi_assert(context);
    FuriMessageQueue* event_queue = context;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

/**
 * Set time for dcf77 converter
 * @param app The app data
 * @param offset The time offset to current time in seconds
 */
void set_signal_time(AppData* app, int offset) {
    // Prepare date
    DateTime dcf_dt;

    // Get time and add offset because a later time will be transmitted
    uint32_t timestamp = datetime_datetime_to_timestamp(&app->dt) + offset;

    // Convert timestamp to date and date to dcf77 signal
    datetime_timestamp_to_datetime(timestamp, &dcf_dt);
    set_dcf77_time(&dcf_dt, app->is_dst);
}

/**
 * Get datetime current offset
 * @param app The app to update datetime for
 */
void update_offset_datetime(AppData* app) {
    // Set offset time to current time from flipper
    DateTime offset_time;
    furi_hal_rtc_get_datetime(&offset_time);

    // Get unix timestamp
    uint32_t unix_timestamp = datetime_datetime_to_timestamp(&offset_time);

    // Add offset
    unix_timestamp += app->time_offset;

    // Save to offset time
    datetime_timestamp_to_datetime(unix_timestamp, &offset_time);

    // Write to
    app->dt = offset_time;
}

/**
 * Init point of the app
 * @param p Args (I guess)
 */
int dcf77_clock_sync_app_main(void* p) {
    // Ignore args
    UNUSED(p);

    // Init app data struct
    AppData* app = malloc(sizeof(AppData));
    app->is_dst = true;
    app->str = furi_string_alloc();
    app->tim_fmt = locale_get_time_format();
    app->dat_fmt = locale_get_date_format();
    app->time_offset = 0;
    app->selection = 0;
    app->blink_led = true;
    update_offset_datetime(app);

    // Set the current signal time (add offset of one minute because signal of next minute is sent)
    set_signal_time(app, DCF77_OFFSET);

    // Create viewport as well as message queue for input events (max 8 events)
    ViewPort* view_port = view_port_alloc();
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Set viewport callbacks for drawing the UI and handling input events (using app and event queue as context parameters)
    view_port_draw_callback_set(view_port, app_draw_callback, app);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Create gui and display already created viewport
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Event store for later
    InputEvent event;

    // Flag to show that signal is being sent
    bool running = false;

    // Flag to show that user pressed back
    bool exit = false;

    // Current second
    int sec = app->dt.second;

    // Settings backup
    uint32_t offset = app->time_offset;

    // Main app loop
    while(!exit) {
        // Silence between signals
        int silence_ms = 0;

        // Wait until next second starts
        while(app->dt.second == sec) {
            update_offset_datetime(app);
        }

        // Control antennas
        if(app->dt.second < 59) {
            // Stop if running
            if(running) {
                // Turn led off
                if(app->blink_led) {
                    furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
                }

                // Stop signal
                furi_hal_rfid_tim_read_stop();
                furi_hal_pwm_stop(FuriHalPwmOutputIdLptim2PA4);
                furi_hal_gpio_init(
                    &gpio_ext_pa4, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
            }

            // Pause 200ms for a one and 100ms for a 0
            silence_ms = get_dcf77_bit(app->dt.second) ? 200 : 100;
            furi_delay_ms(silence_ms);

            // Send signal
            furi_hal_rfid_tim_read_start(DCF77_FREQ, 0.5);
            furi_hal_pwm_start(FuriHalPwmOutputIdLptim2PA4, DCF77_FREQ, 50);

            // Turn led on
            if(app->blink_led) {
                furi_hal_light_set(LightRed | LightGreen, 0xFF);
            }

            // Set flag that signal is bein sent
            running = true;
        } else {
            // Update time for next minute (add offset because time of one minute later than next minute will be sent)
            set_signal_time(app, DCF77_OFFSET + 1);
        }

        // Get current time and calculate wait time until just before next second
        sec = app->dt.second;
        int wait_ms = 1000 - silence_ms - SYNC_DELAY;
        uint32_t tick_start = furi_get_tick();

        // Number of selectible parts
        int max_selection = 3;

        // Wait and handle stuff
        while((int)(furi_get_tick() - tick_start) < wait_ms) {
            // Handle key inputs
            FuriStatus status = furi_message_queue_get(event_queue, &event, wait_ms);
            if(status == FuriStatusOk && event.type == InputTypePress) {
                if(event.key == InputKeyBack) {
                    if(app->selection != 0) {
                        // End without saving if in edit mode (restore settings backup)
                        app->time_offset = offset;
                        update_offset_datetime(app);
                        app->selection = 0;
                    } else {
                        // Signal to exit if not in edit mode
                        exit = true;
                        break;
                    }
                } else if(event.key == InputKeyRight) {
                    // Circle selection
                    app->selection = app->selection == max_selection ? 1 : app->selection + 1;
                } else if(event.key == InputKeyLeft) {
                    // Circle selection
                    app->selection = app->selection <= 1 ? max_selection : app->selection - 1;
                } else if(event.key == InputKeyOk) {
                    if(app->selection != 0) {
                        // End with saving if in edit mode (update settings backup)
                        offset = app->time_offset;
                        app->selection = 0;
                    } else {
                        // Toggle LED if not in edit mode
                        app->blink_led = !app->blink_led;
                        if(!app->blink_led) {
                            furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
                        }
                    }
                } else if(event.key == InputKeyUp) {
                    if(app->selection == 1) {
                        app->time_offset += 60 * 60;
                    } else if(app->selection == 2) {
                        app->time_offset += 60;
                    } else if(app->selection == 3) {
                        app->time_offset += 1;
                    } else if(app->selection == 0) {
                        app->is_dst = !app->is_dst;
                    }
                    update_offset_datetime(app);
                } else if(event.key == InputKeyDown) {
                    if(app->selection == 1) {
                        app->time_offset -= 60 * 60;
                    } else if(app->selection == 2) {
                        app->time_offset -= 60;
                    } else if(app->selection == 3) {
                        app->time_offset -= 1;
                    } else if(app->selection == 0) {
                        app->is_dst = !app->is_dst;
                    }
                    update_offset_datetime(app);
                }
            }

            // Refresh screen
            view_port_update(view_port);

            // Handle other stuff
            if(status == FuriStatusErrorTimeout) {
                break;
            }
        }
    }

    // Stop if running
    if(running) {
        furi_hal_rfid_tim_read_stop();
        furi_hal_pwm_stop(FuriHalPwmOutputIdLptim2PA4);

        if(app->blink_led) {
            furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
        }
    }

    // Disable the viewport
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);

    // Close records previously opened
    furi_record_close(RECORD_GUI);

    // Free input event queue and viewport
    furi_message_queue_free(event_queue);
    view_port_free(view_port);

    // Free string in app data as well as app data itself
    furi_string_free(app->str);
    free(app);

    // Exit normally
    return 0;
}
