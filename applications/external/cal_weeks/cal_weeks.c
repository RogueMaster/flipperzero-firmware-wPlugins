#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <gui/elements.h> // to access button drawing functions
#include <dialogs/dialogs.h> // for the file-open-dialog
#include <stdio.h>
#include <string.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h> // for getting the current date
#include <cal_weeks_icons.h>
#include "utils/ics_extract.h" // ICS parser header

#define TAG "cal-weeks" // Tag for logging purposes

// Layout constants for week view screen
#define WEEK_VIEW_RIGHT_MARGIN 0
#define WEEK_VIEW_WIDTH        (128 - WEEK_VIEW_RIGHT_MARGIN)
#define WEEK_VIEW_CENTER       (WEEK_VIEW_WIDTH / 2)
#define WEEK_VIEW_SLOT_WIDTH   17
#define WEEK_VIEW_MAX_X        (WEEK_VIEW_WIDTH - 1)
#define EVENT_BOX_HEIGHT       16 // Height of event boxes in day view
#define DAY_MENU_ITEMS         2 // Number of menu items on the screen "day view"
#define CAL_WEEKS_DATA_PATH    "/ext/apps_data/cal_weeks" // Default app data directory

const char* days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

typedef enum {
    ScreenSplash,
    ScreenDayView,
    ScreenEventDetail,
    ScreenConfig
} AppScreen;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue; // Queue for handling input events
    ViewPort* view_port; // ViewPort for rendering UI
    Gui* gui; // GUI instance
    uint8_t current_screen;
    int selected_week_offset; // Offset from current week (-1, 0, 1, etc.)
    int selected_day; // Selected day of week (0=Mon, 6=Sun)
    int selected_day_menu; // Selected menu item in day view (0=folder, 1=calendar)
    FuriString* selected_file_path; // Path to selected file
    CalendarEventList* events; // List of events for selected day
    int selected_event; // Selected event index
    bool parsing_error; // Flag for ICS parsing errors
} AppState;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

// Calculate absolute date from base date plus week offset
void calculate_date_with_offset(
    DateTime* base_datetime,
    int week_offset,
    DateTime* result_datetime) {
    // Days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Copy base date
    result_datetime->year = base_datetime->year;
    result_datetime->month = base_datetime->month;
    result_datetime->day = base_datetime->day;
    result_datetime->hour = base_datetime->hour;
    result_datetime->minute = base_datetime->minute;
    result_datetime->second = base_datetime->second;
    result_datetime->weekday = base_datetime->weekday;

    // Add week offset (in days)
    int day_offset = week_offset * 7;
    int new_day = result_datetime->day + day_offset;

    // Adjust for month/year boundaries
    while(new_day < 1 || new_day > days_in_month[result_datetime->month - 1]) {
        // Check for leap year
        if((result_datetime->year % 4 == 0 && result_datetime->year % 100 != 0) ||
           (result_datetime->year % 400 == 0)) {
            days_in_month[1] = 29;
        } else {
            days_in_month[1] = 28;
        }

        if(new_day < 1) {
            result_datetime->month--;
            if(result_datetime->month < 1) {
                result_datetime->month = 12;
                result_datetime->year--;
            }
            new_day += days_in_month[result_datetime->month - 1];
        } else if(new_day > days_in_month[result_datetime->month - 1]) {
            new_day -= days_in_month[result_datetime->month - 1];
            result_datetime->month++;
            if(result_datetime->month > 12) {
                result_datetime->month = 1;
                result_datetime->year++;
            }
        }
    }

    result_datetime->day = new_day;
}

void get_week_days(int year, int month, int day, int week_offset, char out[7][4]) {
    // Simple day-of-week calculation using Zeller's congruence (modified for Monday=0)
    int m = month;
    int y = year;
    if(m < 3) {
        m += 12;
        y -= 1;
    }
    int k = y % 100;
    int j = y / 100;
    int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    int wday = (h + 5) % 7; // Convert to Monday=0

    // Days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // Check for leap year
    if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }

    // Calculate the 7 days of the week starting from Monday
    int curr_day = day - wday + (week_offset * 7); // Add week offset here
    int curr_month = month;
    int curr_year = year;

    for(int i = 0; i < 7; i++) {
        // Handle month/year boundaries
        if(curr_day < 1) {
            curr_month--;
            if(curr_month < 1) {
                curr_month = 12;
                curr_year--;
            }
            curr_day += days_in_month[curr_month - 1];
        } else if(curr_day > days_in_month[curr_month - 1]) {
            curr_day -= days_in_month[curr_month - 1];
            curr_month++;
            if(curr_month > 12) {
                curr_month = 1;
                curr_year++;
            }
        }

        snprintf(out[i], 4, "%d", curr_day);
        curr_day++;
    }
}

int get_iso_week_number(int year, int month, int day) {
    // ISO 8601 week number calculation
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }
    // Calculate day of year for the given date
    int day_of_year = day;
    for(int i = 0; i < month - 1; i++) {
        day_of_year += days_in_month[i];
    }

    // Simplified week calculation (approximate)
    int week = (day_of_year - 1) / 7 + 1;
    return week;
}

// Helper function to determine if today appears in a week with given offset
// Returns column index (0-6) if today is in that week, -1 otherwise
int get_today_column(DateTime* today, DateTime* ref_date, int week_offset) {
    // Get the 7 days of the target week
    char days_week[7][4];
    get_week_days(ref_date->year, ref_date->month, ref_date->day, week_offset, days_week);

    // Calculate the start of the week (Monday)
    DateTime week_start;
    calculate_date_with_offset(ref_date, week_offset, &week_start);

    int ref_wday = (week_start.weekday + 6) % 7; // Monday=0

    // Adjust week_start to actually be Monday
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if((week_start.year % 4 == 0 && week_start.year % 100 != 0) || (week_start.year % 400 == 0)) {
        days_in_month[1] = 29;
    }

    int curr_day = week_start.day - ref_wday;
    int curr_month = week_start.month;
    int curr_year = week_start.year;

    if(curr_day < 1) {
        curr_month--;
        if(curr_month < 1) {
            curr_month = 12;
            curr_year--;
        }
        curr_day += days_in_month[curr_month - 1];
    }

    // Check each day of the week
    for(int i = 0; i < 7; i++) {
        if(today->year == curr_year && today->month == curr_month && today->day == curr_day) {
            return i;
        }

        // Move to next day
        curr_day++;
        if(curr_day > days_in_month[curr_month - 1]) {
            curr_day = 1;
            curr_month++;
            if(curr_month > 12) {
                curr_month = 1;
                curr_year++;
            }
        }
    }
    return -1;
}

// =============================================================================
// SCREEN DRAWING FUNCTIONS
// =============================================================================
// Helper function to draw a menu item icon with optional selection highlight
static void draw_menu_item(Canvas* canvas, int x, int y, const Icon* icon, bool selected) {
    if(selected) {
        canvas_draw_frame(canvas, x, y, 12, 12);
        canvas_invert_color(canvas);
    }
    canvas_draw_icon(canvas, x + 1, y + 1, icon);
    if(selected) {
        canvas_invert_color(canvas);
    }
}

static void draw_screen_config(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Configuration");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 11, 127, 11);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "No settings yet");
    // Navigation hints
    canvas_draw_icon(canvas, 1, 55, &I_back);
    canvas_draw_str_aligned(canvas, 11, 62, AlignLeft, AlignBottom, "Back");
}

static void draw_screen_splash(Canvas* canvas, DateTime* datetime, AppState* state) {
    char buffer[64]; // buffer for string concatination

    // Calculate the reference date based on selected week offset
    DateTime ref_datetime;
    calculate_date_with_offset(datetime, state->selected_week_offset, &ref_datetime);

    canvas_draw_icon(canvas, 1, 1, &I_splash); // 51 is a pixel above the buttons
    // Highlight app icon if selected
    if(state->selected_day == -1) {
        canvas_draw_frame(canvas, 0, -2, 12, 12); // 1px frame around icon
        canvas_draw_box(canvas, 1, -1, 10, 10); // black box
        canvas_set_color(canvas, ColorWhite); // icon will be draw inverted now
    }
    canvas_draw_icon(canvas, 1, 0, &I_icon_10x10); // App icon
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    // Display selected week number and year as title
    int week_num = get_iso_week_number(ref_datetime.year, ref_datetime.month, ref_datetime.day);
    snprintf(buffer, sizeof(buffer), "%04d Week %02d", ref_datetime.year, week_num);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, buffer);
    canvas_set_font(canvas, FontSecondary);
    // Write day names
    for(int i = 0; i < 7; i++) {
        if(i == 5) canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 3 + i * 18, 12, AlignLeft, AlignTop, days[i]);
    }
    canvas_set_color(canvas, ColorBlack);

    // Print previous week's day numbers (relative to selected week)
    int prev_week_today_col = get_today_column(datetime, &ref_datetime, -1);
    char days_prev_week[7][4]; // output array to be populated by next function call
    get_week_days(ref_datetime.year, ref_datetime.month, ref_datetime.day, -1, days_prev_week);
    for(int i = 0; i < 7; i++) {
        if(i == prev_week_today_col) {
            canvas_set_font(canvas, FontPrimary); // Bold font if today is in this week
        }
        canvas_draw_str_aligned(canvas, 18 + i * 18, 22, AlignRight, AlignTop, days_prev_week[i]);
        if(i == prev_week_today_col) {
            canvas_set_font(canvas, FontSecondary); // Back to normal font
        }
    };

    // Print current/selected week day numbers
    char days_current_week[7][4]; // output array to be populated by next function call
    get_week_days(ref_datetime.year, ref_datetime.month, ref_datetime.day, 0, days_current_week);
    int current_week_today_col = get_today_column(datetime, &ref_datetime, 0);

    for(int i = 0; i < 7; i++) {
        // Draw cursor box for selected day
        if(i == state->selected_day) {
            canvas_draw_box(canvas, 2 + i * 18, 30, 18, 11); // Draw filled black box
            canvas_set_color(canvas, ColorWhite); // Switch to white for text
        }
        // Make actual current day bold
        if(i == current_week_today_col) {
            canvas_set_font(canvas, FontPrimary); // Switch to bold font
        }

        canvas_draw_str_aligned(
            canvas, 18 + i * 18, 32, AlignRight, AlignTop, days_current_week[i]);

        if(i == state->selected_day) {
            canvas_set_color(canvas, ColorBlack); // Switch back to black
        }
        // Reset font if we made it bold
        if(i == current_week_today_col) {
            canvas_set_font(canvas, FontSecondary); // Reset to normal font
        }
    }

    // Print week day numbers of succeeding week (relative to selected week)
    int next_week_today_col = get_today_column(datetime, &ref_datetime, 1);
    char days_next_week[7][4]; // output array
    get_week_days(ref_datetime.year, ref_datetime.month, ref_datetime.day, 1, days_next_week);
    for(int i = 0; i < 7; i++) {
        if(i == next_week_today_col) {
            canvas_set_font(canvas, FontPrimary); // Bold for today
        }
        canvas_draw_str_aligned(canvas, 18 + i * 18, 42, AlignRight, AlignTop, days_next_week[i]);
        if(i == next_week_today_col) {
            canvas_set_font(canvas, FontSecondary); // Back to normal
        }
    };
    // Navigation hint
    canvas_draw_icon(canvas, 1, 55, &I_arrows);
    canvas_draw_str_aligned(canvas, 11, 63, AlignLeft, AlignBottom, "Choose");
    // Only show "Today" hint if cursor is not already on today
    int today_day = (datetime->weekday + 6) % 7;
    canvas_draw_icon(canvas, 121, 55, &I_back);
    if(state->selected_week_offset != 0 || state->selected_day != today_day) {
        canvas_draw_str_aligned(canvas, 120, 62, AlignRight, AlignBottom, "Today");
    } else {
        canvas_draw_str_aligned(canvas, 120, 62, AlignRight, AlignBottom, "Hold: exit");
    }
    // Version info
    canvas_draw_str_aligned(canvas, 105, 1, AlignRight, AlignTop, "v0.2");
    // Draw OK button using elements library
    elements_button_center(canvas, "OK"); // for the OK button
}

// Helper function to draw the right-side navigation menu
static void draw_day_view_menu(Canvas* canvas, AppState* state) {
    draw_menu_item(canvas, 103, 0, &I_folder, state->selected_day_menu == 0);
    draw_menu_item(canvas, 116, 0, &I_icon_10x10, state->selected_day_menu == 1);
}

static void draw_screen_day(Canvas* canvas, DateTime* datetime, AppState* state) {
    char buffer[64]; // buffer for string concatenation

    // Draw navigation menu items on the right
    draw_day_view_menu(canvas, state);

    // Calculate the selected date
    DateTime selected_datetime;
    calculate_date_with_offset(datetime, state->selected_week_offset, &selected_datetime);

    // Get the start of the selected week
    char days_selected_week[7][4];
    get_week_days(
        selected_datetime.year,
        selected_datetime.month,
        selected_datetime.day,
        0,
        days_selected_week);

    // Adjust to get the actual selected day's date
    int selected_day_num = atoi(days_selected_week[state->selected_day]);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    // Draw title with selected date
    snprintf(
        buffer,
        sizeof(buffer),
        "%s, %04d-%02d-%02d",
        days[state->selected_day],
        selected_datetime.year,
        selected_datetime.month,
        selected_day_num);
    canvas_draw_str_aligned(canvas, 1, 1, AlignLeft, AlignTop, buffer);
    canvas_set_font(canvas, FontSecondary);
    // Draw header line
    canvas_draw_line(canvas, 0, 11, 128, 11); // upper line of header

    // Draw events as boxes in the grid area
    if(state->events && state->events->count > 0) {
        int y_pos = 13; // Start just below first grid line
        int max_events = (60 - y_pos) / (EVENT_BOX_HEIGHT + 1); // Calculate how many events fit

        for(size_t i = 0; i < state->events->count && i < (size_t)max_events; i++) {
            CalendarEvent* event = &state->events->events[i];

            // Draw event box (filled if selected, frame if not)
            if((int)i == state->selected_event) {
                canvas_draw_box(canvas, 1, y_pos, WEEK_VIEW_MAX_X - 1, EVENT_BOX_HEIGHT);
                canvas_set_color(canvas, ColorWhite);
            }
            // Format event text: show start-end time and title, or "Day." in bold for all-day events
            if(event->all_day) {
                canvas_set_font(canvas, FontPrimary); // Bold font for "Day."
                canvas_draw_str_aligned(canvas, 2, y_pos + 1, AlignLeft, AlignTop, "Day.");
                canvas_set_font(canvas, FontSecondary); // Back to normal font
                snprintf(buffer, sizeof(buffer), "%s", furi_string_get_cstr(event->summary));
                canvas_draw_str_aligned(canvas, 2, y_pos + 7, AlignLeft, AlignTop, buffer);
                buffer[0] = '\0'; // Clear buffer so we don't draw again below
            } else { // Construct text line
                snprintf(
                    buffer,
                    sizeof(buffer),
                    "%02d:%02d",
                    event->start_time.hour,
                    event->start_time.minute);
                canvas_draw_str_aligned(canvas, 30, y_pos + 1, AlignRight, AlignTop, buffer);
                snprintf(
                    buffer,
                    sizeof(buffer),
                    "-%02d:%02d",
                    event->end_time.hour,
                    event->end_time.minute);
                canvas_draw_str_aligned(canvas, 30, y_pos + 9, AlignRight, AlignTop, buffer);
                canvas_draw_str_aligned(
                    canvas,
                    32,
                    y_pos + 1,
                    AlignLeft,
                    AlignTop,
                    furi_string_get_cstr(event->summary));
            }

            // Restore black color if we inverted for selection
            if((int)i == state->selected_event) {
                canvas_set_color(canvas, ColorBlack);
            }

            y_pos += EVENT_BOX_HEIGHT + 1; // Move down for next event
        }
    } else {
        // No events to show
        canvas_set_font(canvas, FontSecondary);
        if(state->parsing_error) {
            canvas_draw_str_aligned(
                canvas, WEEK_VIEW_CENTER, 35, AlignCenter, AlignTop, "Error parsing file!");
            canvas_draw_str_aligned(
                canvas, WEEK_VIEW_CENTER, 44, AlignCenter, AlignTop, "Check ICS format");
            state->parsing_error = false; // Reset flag
        } else if(furi_string_size(state->selected_file_path) > 0) {
            // Show filename if a file is loaded
            const char* full_path = furi_string_get_cstr(state->selected_file_path);
            const char* filename = strrchr(full_path, '/');
            if(filename) {
                filename++; // Skip the '/'
            } else {
                filename = full_path;
            }
            canvas_draw_str_aligned(canvas, WEEK_VIEW_CENTER, 35, AlignCenter, AlignTop, filename);
            canvas_draw_str_aligned(
                canvas, WEEK_VIEW_CENTER, 44, AlignCenter, AlignTop, "No events on this day");
        } else {
            canvas_draw_str_aligned(
                canvas, WEEK_VIEW_CENTER, 35, AlignCenter, AlignTop, "No file loaded");
        }
    }

    // Draw navigation hints at bottom
    canvas_draw_icon(canvas, 1, 55, &I_back);
    canvas_draw_str_aligned(canvas, 11, 62, AlignLeft, AlignBottom, "Back");
    // Only show "Details" button when calendar icon is selected
    if(state->selected_day_menu == 1) {
        elements_button_center(canvas, "Details");
    }
}

static void draw_screen_event_detail(Canvas* canvas, DateTime* datetime, AppState* state) {
    UNUSED(datetime); // Mark parameter as intentionally unused
    if(!state->events || state->selected_event >= (int)state->events->count) return;

    CalendarEvent* event = &state->events->events[state->selected_event];
    char buffer[64];
    canvas_set_color(canvas, ColorBlack);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    // Title
    canvas_draw_str_aligned(
        canvas, 1, 1, AlignLeft, AlignTop, furi_string_get_cstr(event->summary));
    canvas_draw_line(canvas, 0, 11, 127, 11);
    canvas_set_font(canvas, FontSecondary);
    // Time
    if(event->all_day) {
        snprintf(buffer, sizeof(buffer), "All day");
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "%02d:%02d - %02d:%02d",
            event->start_time.hour,
            event->start_time.minute,
            event->end_time.hour,
            event->end_time.minute);
    }
    canvas_draw_str_aligned(canvas, 1, 14, AlignLeft, AlignTop, buffer);
    // Location
    if(furi_string_size(event->location) > 0) {
        canvas_draw_str_aligned(canvas, 1, 24, AlignLeft, AlignTop, "Location:");
        canvas_draw_str_aligned(
            canvas, 1, 32, AlignLeft, AlignTop, furi_string_get_cstr(event->location));
    }
    // Description (truncated)
    if(furi_string_size(event->description) > 0) {
        canvas_draw_str_aligned(canvas, 1, 44, AlignLeft, AlignTop, "Details:");
        // Simple truncation - just show first line
        const char* desc = furi_string_get_cstr(event->description);
        canvas_draw_str_aligned(canvas, 1, 52, AlignLeft, AlignTop, desc);
    }
    canvas_draw_icon(canvas, 1, 55, &I_back);
    canvas_draw_str_aligned(canvas, 11, 62, AlignLeft, AlignBottom, "Back");
}

// =============================================================================
// MAIN CALLBACKS
// =============================================================================
void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context; // Get app context to check current screen
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    // Get current time
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    switch(state->current_screen) {
    case ScreenSplash: // Weeks screen =====================================
        draw_screen_splash(canvas, &datetime, state);
        break;
    case ScreenDayView: // Day View ========================================
        draw_screen_day(canvas, &datetime, state);
        break;
    case ScreenEventDetail: // Event Detail =================================
        draw_screen_event_detail(canvas, &datetime, state);
        break;
    case ScreenConfig: // Configuration ====================================
        draw_screen_config(canvas);
        break;
    }
}

// Input callback - puts input events into the queue
static void input_callback(InputEvent* input_event, void* context) {
    AppState* app = context;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

// =============================================================================
// MAIN APPLICATION
// =============================================================================
int32_t cal_weeks_main(void* p) {
    UNUSED(p);
    AppState app; // Application state struct
    app.current_screen = ScreenSplash; // Start on splash screen
    app.selected_week_offset = 0; // Start with current week
    app.selected_day = 0; // Start with Monday
    app.selected_day_menu = 0; // Start with folder selected
    app.selected_file_path = furi_string_alloc(); // Allocate string for file path
    app.events = NULL; // No events loaded initially
    app.selected_event = 0; // Start with first event
    app.parsing_error = false; // No parsing error initially

    // Ensure app data directory exists
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_dir_exists(storage, CAL_WEEKS_DATA_PATH)) {
        FURI_LOG_I(TAG, "Creating app data directory: %s", CAL_WEEKS_DATA_PATH);
        storage_simply_mkdir(storage, CAL_WEEKS_DATA_PATH);
    }
    furi_record_close(RECORD_STORAGE);

    // Initialize selected_day to current day
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    app.selected_day = (datetime.weekday + 6) % 7; // Convert Sun=0 to Mon=0

    app.input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app.view_port = view_port_alloc(); // for rendering

    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);
    app.gui = furi_record_open("gui"); // Initialize GUI
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    // Input handling
    InputEvent input;
    uint8_t exit_loop = 0; // Flag to exit main loop

    FURI_LOG_I(TAG, "Starting main app loop.");
    while(1) {
        furi_check(
            furi_message_queue_get(app.input_queue, &input, FuriWaitForever) == FuriStatusOk);

        // Handle button presses and holds based on selected screen
        if(input.type == InputTypePress) {
            switch(input.key) {
            case InputKeyUp:
                if(app.current_screen == ScreenSplash) {
                    // Shift to previous week
                    app.selected_week_offset--;
                } else if(app.current_screen == ScreenDayView) {
                    // Select previous event
                    if(app.events && app.events->count > 0) {
                        app.selected_event--;
                        if(app.selected_event < 0) {
                            app.selected_event = app.events->count - 1;
                        }
                    }
                }
                break;

            case InputKeyDown:
                if(app.current_screen == ScreenSplash) {
                    // Shift to next week
                    app.selected_week_offset++;
                } else if(app.current_screen == ScreenDayView) {
                    // Select next event
                    if(app.events && app.events->count > 0) {
                        app.selected_event++;
                        if(app.selected_event >= (int)app.events->count) {
                            app.selected_event = 0;
                        }
                    }
                }
                break;

            case InputKeyLeft:
                if(app.current_screen == ScreenSplash) {
                    // Move cursor left (no wrapping)
                    app.selected_day--;
                    if(app.selected_day < -1) {
                        app.selected_day++;
                    }
                } else if(app.current_screen == ScreenDayView) {
                    // Move menu selection up (with wrapping)
                    app.selected_day_menu =
                        (app.selected_day_menu - 1 + DAY_MENU_ITEMS) % DAY_MENU_ITEMS;
                }
                break;

            case InputKeyRight:
                if(app.current_screen == ScreenSplash) {
                    // Move cursor right (no wrapping)
                    app.selected_day++;
                    if(app.selected_day > 6) {
                        app.selected_day--;
                    }
                } else if(app.current_screen == ScreenDayView) {
                    // Move menu selection down (with wrapping)
                    app.selected_day_menu = (app.selected_day_menu + 1) % DAY_MENU_ITEMS;
                }
                break;

            case InputKeyOk:
                switch(app.current_screen) {
                case ScreenSplash:
                    if(app.selected_day == -1) {
                        app.current_screen = ScreenConfig;
                        break;
                    }
                    // Load events for selected day
                    if(app.events) {
                        calendar_event_list_free(app.events);
                    }

                    DateTime selected_date;
                    calculate_date_with_offset(
                        &datetime, app.selected_week_offset, &selected_date);
                    char days_week[7][4];
                    get_week_days(
                        selected_date.year, selected_date.month, selected_date.day, 0, days_week);

                    DateTime day_start = selected_date;
                    day_start.day = atoi(days_week[app.selected_day]);
                    day_start.hour = 0;
                    day_start.minute = 0;

                    DateTime day_end = day_start;
                    day_end.hour = 23;
                    day_end.minute = 59;

                    app.events = extract_calendar_events(
                        furi_string_get_cstr(app.selected_file_path), &day_start, &day_end);
                    app.selected_event = 0;
                    // Check for parsing errors
                    if(!app.events && furi_string_size(app.selected_file_path) > 0) {
                        app.parsing_error = true;
                    }

                    app.current_screen = ScreenDayView;
                    break;
                case ScreenDayView:
                    if(app.selected_day_menu == 0) {
                        // Open file browser
                        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
                        FuriString* start_path = furi_string_alloc_set_str(CAL_WEEKS_DATA_PATH);
                        DialogsFileBrowserOptions browser_options;
                        dialog_file_browser_set_basic_options(
                            &browser_options, ".ics", &I_icon_10x10);
                        browser_options.hide_ext = false; // Show file extensions
                        bool result = dialog_file_browser_show(
                            dialogs, app.selected_file_path, start_path, &browser_options);
                        furi_string_free(start_path);
                        furi_record_close(RECORD_DIALOGS);
                        if(!result) {
                            furi_string_reset(app.selected_file_path); // Clear if cancelled
                        } else {
                            // Reload events from newly selected file
                            if(app.events) {
                                calendar_event_list_free(app.events);
                            }

                            DateTime selected_date;
                            calculate_date_with_offset(
                                &datetime, app.selected_week_offset, &selected_date);
                            char days_week[7][4];
                            get_week_days(
                                selected_date.year,
                                selected_date.month,
                                selected_date.day,
                                0,
                                days_week);

                            DateTime day_start = selected_date;
                            day_start.day = atoi(days_week[app.selected_day]);
                            day_start.hour = 0;
                            day_start.minute = 0;

                            DateTime day_end = day_start;
                            day_end.hour = 23;
                            day_end.minute = 59;

                            app.events = extract_calendar_events(
                                furi_string_get_cstr(app.selected_file_path),
                                &day_start,
                                &day_end);
                            app.selected_event = 0;
                            // Check for parsing errors
                            if(!app.events) {
                                app.parsing_error = true;
                            }

                            app.selected_event = 0;
                        }
                    } else if(app.selected_day_menu == 1) {
                        // Show event details
                        if(app.events && app.events->count > 0) {
                            app.current_screen = ScreenEventDetail;
                        }
                    }
                    break;
                case ScreenEventDetail:
                    app.current_screen = ScreenDayView;
                    break;
                }
                break;

            case InputKeyBack:
                if(app.current_screen == ScreenConfig) {
                    app.current_screen = ScreenSplash;
                } else if(app.current_screen == ScreenEventDetail) {
                    app.current_screen = ScreenDayView;
                } else if(app.current_screen == ScreenDayView) {
                    app.current_screen = ScreenSplash;
                } else if(app.current_screen == ScreenSplash) {
                    // Reset to current week and today
                    app.selected_week_offset = 0;
                    DateTime current_datetime;
                    furi_hal_rtc_get_datetime(&current_datetime);
                    app.selected_day =
                        (current_datetime.weekday + 6) % 7; // Convert Sun=0 to Mon=0
                }
                break;

            default:
                break;
            }
        } else if(input.type == InputTypeLong && input.key == InputKeyBack) {
            exit_loop = 1; // Exit app after long press of Back
        }

        // Exit main app loop if exit flag is set
        if(exit_loop) break;

        // Trigger screen redraw
        view_port_update(app.view_port);
    }

    // Cleanup: Free all allocated resources
    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    furi_record_close("gui");
    view_port_free(app.view_port);
    furi_message_queue_free(app.input_queue);
    furi_string_free(app.selected_file_path);
    if(app.events) {
        calendar_event_list_free(app.events);
    }
    return 0;
}
