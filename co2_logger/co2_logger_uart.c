#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/view.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include "co2_logger.h"

#define CSV_FILE_PATH "/ext/apps_data/co2_logger/co2_log.csv"
#define SENSOR_WARMUP_TIME_MS (60 * 1000) // 60 seconds warmup time

typedef enum {
    Co2LoggerViewMain,
    Co2LoggerViewSettings
} Co2LoggerView;

typedef enum {
    LogInterval15s,
    LogInterval30s,
    LogInterval60s
} LogIntervalOption;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    Storage* storage;
    File* csv_file;
    co2_logger* co2_logger;
    FuriMutex* mutex;
    uint32_t last_log_time;
    uint32_t startup_time;
    uint32_t connection_start_time;
    uint32_t co2_value;
    bool connected;
    bool csv_enabled;
    Co2LoggerView current_view;
    LogIntervalOption log_interval_option;
    bool auto_dim_enabled;
    bool csv_logging_enabled;
    int settings_selection; // 0 = interval, 1 = auto_dim, 2 = csv_logging
} co2_loggerUart;

static uint32_t co2_logger_uart_get_log_interval_ms(LogIntervalOption option) {
    switch(option) {
        case LogInterval15s:
            return 15 * 1000;
        case LogInterval30s:
            return 30 * 1000;
        case LogInterval60s:
            return 60 * 1000;
        default:
            return 30 * 1000;
    }
}

static const char* co2_logger_uart_get_log_interval_text(LogIntervalOption option) {
    switch(option) {
        case LogInterval15s:
            return "15s";
        case LogInterval30s:
            return "30s";
        case LogInterval60s:
            return "60s";
        default:
            return "30s";
    }
}

static void co2_logger_uart_format_time(uint32_t seconds, char* buffer, size_t buffer_size) {
    if(seconds < 60) {
        // Less than 1 minute: show seconds
        snprintf(buffer, buffer_size, "%lus", seconds);
    } else if(seconds < 3600) {
        // 1 minute to 59 minutes: show mm:ss
        uint32_t minutes = seconds / 60;
        uint32_t secs = seconds % 60;
        snprintf(buffer, buffer_size, "%lu:%02lu", minutes, secs);
    } else if(seconds < 86400) {
        // 1 hour to 23 hours: show hh:mm:ss
        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        uint32_t secs = seconds % 60;
        snprintf(buffer, buffer_size, "%lu:%02lu:%02lu", hours, minutes, secs);
    } else {
        // 24 hours and above: show dd:hh:mm:ss
        uint32_t days = seconds / 86400;
        uint32_t hours = (seconds % 86400) / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        uint32_t secs = seconds % 60;
        snprintf(buffer, buffer_size, "%lu:%02lu:%02lu:%02lu", days, hours, minutes, secs);
    }
}

static void co2_logger_uart_apply_auto_dim(co2_loggerUart* state) {
    FURI_LOG_D("AutoDim", "Applying auto-dim setting: %s", 
               state->auto_dim_enabled ? "ON" : "OFF");
    
    if(state->auto_dim_enabled) {
        // Enable auto-dim: let system handle dimming naturally
        notification_message(state->notification, &sequence_display_backlight_enforce_auto);
        FURI_LOG_D("AutoDim", "Auto-dim enabled - system will handle backlight dimming");
    } else {
        // Disable auto-dim: force backlight to stay on
        notification_message(state->notification, &sequence_display_backlight_enforce_on);
        FURI_LOG_D("AutoDim", "Auto-dim disabled - backlight will stay on");
    }
}

static bool co2_logger_uart_check_sd_card(co2_loggerUart* state) {
    FURI_LOG_D("CSV", "Checking SD card status...");
    
    // Check if SD card is mounted
    FS_Error sd_status = storage_sd_status(state->storage);
    if(sd_status != FSE_OK) {
        FURI_LOG_W("CSV", "SD card not ready, status: %s", storage_error_get_desc(sd_status));
        return false;
    }
    
    FURI_LOG_D("CSV", "SD card is ready");
    return true;
}

static bool co2_logger_uart_create_csv_file(co2_loggerUart* state) {
    FURI_LOG_I("CSV", "Setting up CSV logging...");
    
    // Check SD card first
    if(!co2_logger_uart_check_sd_card(state)) {
        FURI_LOG_W("CSV", "SD card not available - disabling CSV logging");
        return false;
    }
    
    // Create directories step by step with proper error checking
    FURI_LOG_D("CSV", "Creating directory structure...");
    
    FS_Error dir_result = storage_common_mkdir(state->storage, "/ext/apps_data");
    if(dir_result != FSE_OK && dir_result != FSE_EXIST) {
        FURI_LOG_E("CSV", "Failed to create /ext/apps_data: %s", storage_error_get_desc(dir_result));
        return false;
    }
    
    dir_result = storage_common_mkdir(state->storage, "/ext/apps_data/co2_logger");
    if(dir_result != FSE_OK && dir_result != FSE_EXIST) {
        FURI_LOG_E("CSV", "Failed to create /ext/apps_data/co2_logger: %s", storage_error_get_desc(dir_result));
        return false;
    }
    
    FURI_LOG_D("CSV", "Directory structure created successfully");
    
    // Allocate file handle
    state->csv_file = storage_file_alloc(state->storage);
    if(!state->csv_file) {
        FURI_LOG_E("CSV", "Failed to allocate file handle");
        return false;
    }
    
    // Check if file exists
    bool file_exists = storage_file_exists(state->storage, CSV_FILE_PATH);
    FURI_LOG_D("CSV", "File exists: %s", file_exists ? "YES" : "NO");
    
    if(file_exists) {
        // File exists, open for append
        FURI_LOG_D("CSV", "Opening existing file for append...");
        if(!storage_file_open(state->csv_file, CSV_FILE_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
            FURI_LOG_E("CSV", "Failed to open file for append: %s", 
                       storage_file_get_error_desc(state->csv_file));
            storage_file_free(state->csv_file);
            state->csv_file = NULL;
            return false;
        }
        FURI_LOG_I("CSV", "CSV file opened for append");
    } else {
        // File doesn't exist, create new one
        FURI_LOG_D("CSV", "Creating new CSV file...");
        if(!storage_file_open(state->csv_file, CSV_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E("CSV", "Failed to create new file: %s", 
                       storage_file_get_error_desc(state->csv_file));
            storage_file_free(state->csv_file);
            state->csv_file = NULL;
            return false;
        }
        
        // Write header
        const char* header = "Timestamp,CO2_PPM\n";
        size_t header_len = strlen(header);
        size_t written = storage_file_write(state->csv_file, header, header_len);
        
        if(written != header_len) {
            FURI_LOG_E("CSV", "Failed to write header (wrote %zu of %zu bytes)", written, header_len);
            storage_file_close(state->csv_file);
            storage_file_free(state->csv_file);
            state->csv_file = NULL;
            return false;
        }
        
        // Force write to disk
        if(!storage_file_sync(state->csv_file)) {
            FURI_LOG_W("CSV", "Warning: Failed to sync file after header write");
        }
        
        FURI_LOG_I("CSV", "New CSV file created with header");
    }
    
    return true;
}

static void co2_logger_uart_log_to_csv(co2_loggerUart* state, uint32_t co2_value, bool connected) {
    if(!state->csv_enabled || !state->csv_file) {
        FURI_LOG_D("CSV", "CSV logging disabled or file not open");
        return;
    }
    
    // Don't log if sensor hasn't warmed up yet
    uint32_t current_time = furi_get_tick();
    if(current_time - state->startup_time < SENSOR_WARMUP_TIME_MS) {
        FURI_LOG_D("CSV", "Skipping log - sensor warming up (%lu seconds remaining)", 
                   (SENSOR_WARMUP_TIME_MS - (current_time - state->startup_time)) / 1000);
        return;
    }
    
    // Don't log if not connected or invalid reading
    if(!connected) {
        FURI_LOG_D("CSV", "Skipping log - not connected: %lu ppm, connected: %s", 
                   co2_value, connected ? "true" : "false");
        return;
    }
    
    FURI_LOG_D("CSV", "Logging to CSV: %lu ppm", co2_value);
    
    // Get current timestamp using the correct DateTime API
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    
    // Format: YYYY-MM-DD HH:MM:SS,CO2_PPM
    char log_entry[128];
    int entry_len = snprintf(log_entry, sizeof(log_entry), 
                            "%04d-%02d-%02d %02d:%02d:%02d,%lu\n",
                            datetime.year, datetime.month, datetime.day,
                            datetime.hour, datetime.minute, datetime.second,
                            co2_value);
    
    if(entry_len < 0 || entry_len >= (int)sizeof(log_entry)) {
        FURI_LOG_E("CSV", "Failed to format log entry");
        return;
    }
    
    FURI_LOG_D("CSV", "Writing entry: %s", log_entry);
    
    size_t written = storage_file_write(state->csv_file, log_entry, entry_len);
    if(written != (size_t)entry_len) {
        FURI_LOG_E("CSV", "Failed to write log entry (wrote %zu of %d bytes)", written, entry_len);
        
        // Get detailed error info
        FS_Error error = storage_file_get_error(state->csv_file);
        FURI_LOG_E("CSV", "File error: %s", storage_error_get_desc(error));
        
        // If write failed, disable CSV to prevent further errors
        FURI_LOG_W("CSV", "Disabling CSV logging due to write failure");
        state->csv_enabled = false;
        return;
    }
    
    // Sync every write to ensure data is saved
    if(!storage_file_sync(state->csv_file)) {
        FURI_LOG_W("CSV", "Warning: Failed to sync file after write");
    }
    
    FURI_LOG_D("CSV", "Successfully logged entry to CSV");
}

static void co2_logger_uart_draw_main_view(Canvas* canvas, co2_loggerUart* app) {
    if(app->connected) {
        // Show connection time in top right corner (small font)
        uint32_t current_time = furi_get_tick();
        uint32_t connected_seconds = (current_time - app->connection_start_time) / 1000;
        char formatted_time[16];
        co2_logger_uart_format_time(connected_seconds, formatted_time, sizeof(formatted_time));
        
        canvas_set_font(canvas, FontSecondary);
        // Measure text width to right-align it
        size_t time_width = canvas_string_width(canvas, formatted_time);
        canvas_draw_str(canvas, 128 - time_width - 2, 10, formatted_time);
        
        // Draw CO2 value in biggest font (clean, single draw, positioned higher)
        canvas_set_font(canvas, FontBigNumbers);
        char co2_buffer[16];
        snprintf(co2_buffer, sizeof(co2_buffer), "%lu", app->co2_value);
        canvas_draw_str(canvas, 2, 18, co2_buffer);
        
        // Draw "ppm" right after the number (hugging the reading)
        canvas_set_font(canvas, FontPrimary);
        size_t co2_width = canvas_string_width(canvas, co2_buffer)*2;
        canvas_draw_str(canvas, 2 + co2_width + 5, 18, "ppm");
    } else {
        // Show disconnected state
        canvas_set_font(canvas, FontBigNumbers);
        char disconnected_text[] = "---";
        canvas_draw_str(canvas, 2, 18, disconnected_text);
        
        // Draw "ppm" right after the dashes (hugging the reading)
        canvas_set_font(canvas, FontPrimary);
        size_t dash_width = canvas_string_width(canvas, disconnected_text)*2;
        canvas_draw_str(canvas, 2 + dash_width + 10, 18, "ppm");
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 37, "Disconnected");
    }
    
    // Show logging status at bottom left
    canvas_set_font(canvas, FontSecondary);
    if(app->csv_logging_enabled && app->csv_enabled) {
        char logging_status[32];
        const char* interval_text = co2_logger_uart_get_log_interval_text(app->log_interval_option);
        snprintf(logging_status, sizeof(logging_status), "Logging every %s", interval_text);
        canvas_draw_str(canvas, 2, 58, logging_status);
    } else {
        canvas_draw_str(canvas, 2, 58, "Not logging");
    }
    
    // Settings button with inverted background (white text on black)
    canvas_set_font(canvas, FontSecondary);
    const char* settings_text = "Settings >";
    size_t settings_width = canvas_string_width(canvas, settings_text);
    uint8_t settings_x = 85;
    uint8_t settings_y = 58;
    
    // Draw black background rectangle
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, settings_x - 2, settings_y - 8, settings_width + 4, 10, 2);
    
    // Draw white text on black background
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, settings_x, settings_y, settings_text);
    
    // Reset color to black for future draws
    canvas_set_color(canvas, ColorBlack);
}

static void co2_logger_uart_draw_settings_view(Canvas* canvas, co2_loggerUart* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Settings");

    canvas_set_font(canvas, FontSecondary);
    
    // Draw "Logging Interval:" with selection indicator
    if(app->settings_selection == 0) {
        canvas_draw_str(canvas, 2, 28, "> Logging Interval:");
    } else {
        canvas_draw_str(canvas, 10, 28, "Logging Interval:");
    }
    
    // Show current selection on same line as label (no dropdown)
    const char* current_interval = co2_logger_uart_get_log_interval_text(app->log_interval_option);
    canvas_draw_str(canvas, 100, 28, current_interval);
    
    // Auto-dim setting - show value on same line as label
    int auto_dim_y = 40;
    if(app->settings_selection == 1) {
        canvas_draw_str(canvas, 2, auto_dim_y, "> Auto Dim:");
    } else {
        canvas_draw_str(canvas, 10, auto_dim_y, "Auto Dim:");
    }
    canvas_draw_str(canvas, 100, auto_dim_y, app->auto_dim_enabled ? "ON" : "OFF");
    
    // CSV Logging setting - show value on same line as label
    int csv_logging_y = 52;
    if(app->settings_selection == 2) {
        canvas_draw_str(canvas, 2, csv_logging_y, "> CSV Logging:");
    } else {
        canvas_draw_str(canvas, 10, csv_logging_y, "CSV Logging:");
    }
    canvas_draw_str(canvas, 100, csv_logging_y, app->csv_logging_enabled ? "ON" : "OFF");
}

static void co2_logger_uart_draw_callback(Canvas* canvas, void* ctx) {
    co2_loggerUart* app = (co2_loggerUart*)ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    
    if(app->current_view == Co2LoggerViewMain) {
        co2_logger_uart_draw_main_view(canvas, app);
    } else {
        co2_logger_uart_draw_settings_view(canvas, app);
    }

    furi_mutex_release(app->mutex);
}

static void co2_logger_uart_input_callback(InputEvent* input_event, void* ctx) {
    co2_loggerUart* app = (co2_loggerUart*)ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

int32_t co2_logger_app(void* p) {
    UNUSED(p);
    
    // Log immediately at start
    FURI_LOG_D("App", "=== CO2 LOGGER APP STARTING ===");
    
    // Allocate state on heap instead of stack to prevent stack overflow
    co2_loggerUart* state = malloc(sizeof(co2_loggerUart));
    if(!state) {
        FURI_LOG_E("App", "Failed to allocate main state structure");
        return -1;
    }
    memset(state, 0, sizeof(co2_loggerUart));
    
    FURI_LOG_D("App", "Opening GUI record...");
    state->gui = furi_record_open(RECORD_GUI);
    if(!state->gui) {
        FURI_LOG_E("App", "Failed to open GUI record");
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Opening notification record...");
    state->notification = furi_record_open(RECORD_NOTIFICATION);
    if(!state->notification) {
        FURI_LOG_E("App", "Failed to open notification record");
        furi_record_close(RECORD_GUI);
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Opening storage record...");
    state->storage = furi_record_open(RECORD_STORAGE);
    if(!state->storage) {
        FURI_LOG_E("App", "Failed to open storage record");
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_GUI);
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Allocating view port...");
    state->view_port = view_port_alloc();
    if(!state->view_port) {
        FURI_LOG_E("App", "Failed to allocate view port");
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_GUI);
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Allocating event queue...");
    state->event_queue = furi_message_queue_alloc(32, sizeof(InputEvent));
    if(!state->event_queue) {
        FURI_LOG_E("App", "Failed to allocate event queue");
        view_port_free(state->view_port);
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_GUI);
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Allocating mutex...");
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state->mutex) {
        FURI_LOG_E("App", "Failed to allocate mutex");
        furi_message_queue_free(state->event_queue);
        view_port_free(state->view_port);
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_GUI);
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Allocating CO2 logger...");
    state->co2_logger = co2_logger_alloc();
    if(!state->co2_logger) {
        FURI_LOG_E("App", "Failed to allocate CO2 logger");
        furi_mutex_free(state->mutex);
        furi_message_queue_free(state->event_queue);
        view_port_free(state->view_port);
        furi_record_close(RECORD_STORAGE);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_GUI);
        free(state);
        return -1;
    }
    
    FURI_LOG_D("App", "Setting up CSV logging...");
    
    // Try to set up CSV logging with robust error handling
    if(co2_logger_uart_create_csv_file(state)) {
        state->csv_enabled = true;
        FURI_LOG_D("App", "CSV logging enabled successfully");
    } else {
        state->csv_enabled = false;
        state->csv_file = NULL;
        FURI_LOG_W("App", "CSV logging disabled - continuing without CSV");
    }

    FURI_LOG_D("App", "Setting up view port callbacks...");
    view_port_draw_callback_set(state->view_port, co2_logger_uart_draw_callback, state);
    view_port_input_callback_set(state->view_port, co2_logger_uart_input_callback, state);

    FURI_LOG_D("App", "Adding view port to GUI...");
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);

    FURI_LOG_D("App", "Opening CO2 logger connection...");
    co2_logger_open(state->co2_logger);

    // Record startup time for sensor warmup period
    state->startup_time = furi_get_tick();
    
    // Initialize UI state
    state->current_view = Co2LoggerViewMain;
    state->log_interval_option = LogInterval30s; // Default to 30s
    state->connection_start_time = furi_get_tick();
    state->auto_dim_enabled = true; // Auto-dim ON by default (natural Flipper behavior)
    state->csv_logging_enabled = true; // CSV logging ON by default
    state->settings_selection = 0; // Start with interval selection
    
    // Apply initial auto-dim setting
    co2_logger_uart_apply_auto_dim(state);

    FURI_LOG_D("App", "=== APP SETUP COMPLETE - ENTERING MAIN LOOP ===");

    InputEvent event;
    bool running = true;
    uint32_t last_measurement_time = 0;
    uint32_t previous_co2_value = 0;
    bool previous_connected = false;

    while(running) {
        FuriStatus status = furi_message_queue_get(state->event_queue, &event, 100);
        
        uint32_t current_time = furi_get_tick();
        
        // Handle backlight control based on auto-dim setting
        // When auto-dim is disabled and there's input, ensure backlight stays on
        
        // Check for input events
        if(status == FuriStatusOk) {
            FURI_LOG_D("Input", "Received input: type=%d key=%d", event.type, event.key);
            
            // If auto-dim is disabled, turn on backlight when user interacts
            if(!state->auto_dim_enabled) {
                notification_message(state->notification, &sequence_display_backlight_on);
                FURI_LOG_D("AutoDim", "User input detected - turning on backlight (auto-dim disabled)");
            }
            
            if(event.type == InputTypePress) {
                bool view_updated = false;
                
                if(state->current_view == Co2LoggerViewMain) {
                    if(event.key == InputKeyBack) {
                        FURI_LOG_D("App", "Back button pressed, exiting");
                        running = false;
                    } else if(event.key == InputKeyRight) {
                        FURI_LOG_D("App", "Entering settings view");
                        state->current_view = Co2LoggerViewSettings;
                        view_updated = true;
                    }
                } else if(state->current_view == Co2LoggerViewSettings) {
                    if(event.key == InputKeyLeft || event.key == InputKeyBack) {
                        state->current_view = Co2LoggerViewMain;
                        view_updated = true;
                    } else if(event.key == InputKeyOk) {
                        if(state->settings_selection == 0) {
                            // Cycle through interval options: 15s -> 30s -> 60s -> 15s
                            if(state->log_interval_option == LogInterval15s) {
                                state->log_interval_option = LogInterval30s;
                            } else if(state->log_interval_option == LogInterval30s) {
                                state->log_interval_option = LogInterval60s;
                            } else {
                                state->log_interval_option = LogInterval15s;
                            }
                            view_updated = true;
                        } else if(state->settings_selection == 1) {
                            // Toggle auto-dim setting
                            state->auto_dim_enabled = !state->auto_dim_enabled;
                            co2_logger_uart_apply_auto_dim(state);
                            view_updated = true;
                        } else if(state->settings_selection == 2) {
                            // Toggle csv_logging setting
                            state->csv_logging_enabled = !state->csv_logging_enabled;
                            view_updated = true;
                        }
                    } else if(event.key == InputKeyUp) {
                        // Navigate between settings
                        if(state->settings_selection > 0) {
                            state->settings_selection--;
                            view_updated = true;
                        }
                    } else if(event.key == InputKeyDown) {
                        // Navigate between settings
                        if(state->settings_selection < 2) { // 0 = interval, 1 = auto_dim, 2 = csv_logging
                            state->settings_selection++;
                            view_updated = true;
                        }
                    }
                }
                
                if(view_updated) {
                    view_port_update(state->view_port);
                }
            }
        }
        
        // Take CO2 measurement every second for display
        if(current_time - last_measurement_time > 1000) {
            FURI_LOG_D("Measure", "Taking CO2 measurement...");
            furi_mutex_acquire(state->mutex, FuriWaitForever);
            
            bool was_connected = state->connected;
            state->connected = co2_logger_read_gas_concentration(state->co2_logger, &state->co2_value);
            
            // Update connection start time when transitioning from disconnected to connected
            if(!was_connected && state->connected) {
                state->connection_start_time = current_time;
            }
            
            if(state->connected) {
                FURI_LOG_D("Measure", "CO2 reading: %lu ppm", state->co2_value);
            } else {
                FURI_LOG_D("Measure", "Failed to read CO2 sensor");
            }
            
            furi_mutex_release(state->mutex);
            last_measurement_time = current_time;
            
            // Log to CSV at configurable interval
            uint32_t log_interval_ms = co2_logger_uart_get_log_interval_ms(state->log_interval_option);
            if(current_time - state->last_log_time > log_interval_ms) {
                if(state->csv_enabled && state->csv_logging_enabled) {
                    co2_logger_uart_log_to_csv(state, state->co2_value, state->connected);
                } else {
                    if(!state->csv_enabled) {
                        FURI_LOG_D("CSV", "CSV file not available - skipping log entry");
                    } else {
                        FURI_LOG_D("CSV", "CSV logging disabled by user - skipping log entry");
                    }
                }
                state->last_log_time = current_time;
            }
            
            // Only update display if something actually changed, or every 10 seconds for time updates (main view only)
            bool needs_update = (state->co2_value != previous_co2_value || state->connected != previous_connected);
            bool time_update = (state->current_view == Co2LoggerViewMain && (current_time % 10000) < 1000);
            
            if(needs_update || time_update) {
                view_port_update(state->view_port);
                previous_co2_value = state->co2_value;
                previous_connected = state->connected;
            }
        }
    }

    FURI_LOG_D("App", "=== CLEANING UP AND EXITING ===");

    // Cleanup
    gui_remove_view_port(state->gui, state->view_port);
    co2_logger_close(state->co2_logger);
    co2_logger_free(state->co2_logger);
    
    if(state->csv_file) {
        FURI_LOG_D("CSV", "Closing CSV file");
        storage_file_close(state->csv_file);
        storage_file_free(state->csv_file);
    }
    
    furi_mutex_free(state->mutex);
    furi_message_queue_free(state->event_queue);
    view_port_free(state->view_port);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(state);

    FURI_LOG_D("App", "=== CO2 LOGGER APP EXITED SUCCESSFULLY ===");
    return 0;
}
