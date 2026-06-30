#include "nearby_files.h"
#include "scenes/nearby_files_scene.h"
#include <furi_hal.h>
#include <math.h>
#include <expansion/expansion.h>
#include <toolbox/saved_struct.h>

#include "nearby_files_icons.h"

#define TAG "NearbyFiles"

// File extensions to scan for
static const char* file_extensions[] = {".sub", ".nfc", ".rfid", ".ibtn"};
static const size_t file_extensions_count = sizeof(file_extensions) / sizeof(file_extensions[0]);

// Root directories to scan
static const char* scan_directories[] = {"/ext/subghz", "/ext/nfc", "/ext/lfrfid", "/ext/ibutton"};
static const size_t scan_directories_count =
    sizeof(scan_directories) / sizeof(scan_directories[0]);

// App names for launching
static const char* app_names[] = {"Sub-GHz", "NFC", "125 kHz RFID", "iButton"};

#define NEARBY_FILES_CONFIG_PATH    APP_DATA_PATH("config.bin")
#define NEARBY_FILES_CONFIG_MAGIC   (0x47)
#define NEARBY_FILES_CONFIG_VERSION (1)

typedef struct {
    uint32_t gps_baudrate;
} NearbyFilesConfig;

static bool nearby_files_load_config_baudrate(uint32_t* baudrate) {
    furi_check(baudrate);

    NearbyFilesConfig config = {0};
    if(!saved_struct_load(
           NEARBY_FILES_CONFIG_PATH,
           &config,
           sizeof(config),
           NEARBY_FILES_CONFIG_MAGIC,
           NEARBY_FILES_CONFIG_VERSION)) {
        return false;
    }

    *baudrate = config.gps_baudrate;
    return true;
}

bool nearby_files_save_config_baudrate(uint32_t baudrate) {
    NearbyFilesConfig config = {
        .gps_baudrate = baudrate,
    };

    return saved_struct_save(
        NEARBY_FILES_CONFIG_PATH,
        &config,
        sizeof(config),
        NEARBY_FILES_CONFIG_MAGIC,
        NEARBY_FILES_CONFIG_VERSION);
}

// Directory filter callback
static bool nearby_files_dir_filter(const char* path, FileInfo* file_info, void* context) {
    UNUSED(context);

    if(file_info->flags & FSF_DIRECTORY) {
        // Get directory name
        const char* dir_name = strrchr(path, '/');
        if(dir_name) {
            dir_name++; // Skip the '/'

            // Exclude directories named "assets" or starting with "."
            if(strcmp(dir_name, "assets") == 0 || dir_name[0] == '.') {
                FURI_LOG_I(TAG, "Filtering out directory: %s", path);
                return false;
            }
        }
    }

    return true;
}

// File filter callback
static bool nearby_files_file_filter(const char* path, FileInfo* file_info, void* context) {
    UNUSED(context);

    if(!(file_info->flags & FSF_DIRECTORY)) {
        // Check if path contains "/assets/" directory (additional protection)
        if(strstr(path, "/assets/") != NULL) {
            FURI_LOG_I(TAG, "Filtering out file from assets: %s", path);
            return false;
        }

        // Get filename from path
        const char* filename = strrchr(path, '/');
        if(filename) {
            filename++; // Skip the '/'
        } else {
            filename = path;
        }

        // Ignore files starting with dot
        if(filename[0] == '.') {
            FURI_LOG_I(TAG, "Filtering out dotfile: %s", path);
            return false;
        }

        // Check if file has one of the target extensions
        for(size_t i = 0; i < file_extensions_count; i++) {
            size_t path_len = strlen(path);
            size_t ext_len = strlen(file_extensions[i]);
            if(path_len >= ext_len && strcmp(path + path_len - ext_len, file_extensions[i]) == 0) {
                return true;
            }
        }
    }

    return false;
}

// Navigation callback wrapper
static bool nearby_files_navigation_callback(void* context) {
    NearbyFilesApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// Custom event callback wrapper
static bool nearby_files_custom_event_callback(void* context, uint32_t event) {
    NearbyFilesApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static void nearby_files_show_message(DialogsApp* dialogs, const char* header, const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 4, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, "OK", NULL, NULL);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);
}

static bool nearby_files_line_has_prefix(const char* line, size_t line_len, const char* prefix) {
    const size_t prefix_len = strlen(prefix);
    return (line_len >= prefix_len) && (strncmp(line, prefix, prefix_len) == 0);
}

static bool nearby_files_path_has_extension(const char* path, const char* extension) {
    const size_t path_len = strlen(path);
    const size_t extension_len = strlen(extension);
    return (path_len >= extension_len) &&
           (strcmp(path + path_len - extension_len, extension) == 0);
}

static bool nearby_files_line_is_coordinate(const char* line, size_t line_len) {
    return nearby_files_line_has_prefix(line, line_len, "Lat: ") ||
           nearby_files_line_has_prefix(line, line_len, "Latitude: ") ||
           nearby_files_line_has_prefix(line, line_len, "Latitute: ") ||
           nearby_files_line_has_prefix(line, line_len, "Lon: ") ||
           nearby_files_line_has_prefix(line, line_len, "Longitude: ");
}

static void
    nearby_files_append_coordinates(FuriString* output, double latitude, double longitude) {
    furi_string_cat_printf(output, "Lat: %.6f\n", latitude);
    furi_string_cat_printf(output, "Lon: %.6f\n", longitude);
}

static bool nearby_files_update_coordinates_in_file(
    Storage* storage,
    const char* file_path,
    double latitude,
    double longitude) {
    bool success = false;
    const bool is_sub_file = nearby_files_path_has_extension(file_path, ".sub");
    File* file = storage_file_alloc(storage);
    if(!file) return false;

    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    const size_t file_size = storage_file_size(file);
    char* input_data = malloc(file_size + 1);
    if(!input_data) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    size_t input_size = 0;
    while(input_size < file_size) {
        const size_t bytes_read =
            storage_file_read(file, &input_data[input_size], file_size - input_size);
        if(bytes_read == 0) break;
        input_size += bytes_read;
    }
    input_data[input_size] = '\0';

    storage_file_close(file);

    FuriString* output = furi_string_alloc();
    size_t line_start = 0;
    bool inserted_coordinates = false;

    for(size_t i = 0; i <= input_size; i++) {
        const bool at_end = (i == input_size);
        if(at_end || input_data[i] == '\n' || input_data[i] == '\r') {
            const size_t line_len = i - line_start;
            const char* line = &input_data[line_start];
            const bool is_coordinate_line = (line_len > 0) &&
                                            nearby_files_line_is_coordinate(line, line_len);

            if(is_sub_file && !inserted_coordinates &&
               nearby_files_line_has_prefix(line, line_len, "Protocol: ")) {
                nearby_files_append_coordinates(output, latitude, longitude);
                inserted_coordinates = true;
            }

            if(line_len > 0 && !is_coordinate_line) {
                furi_string_cat_printf(output, "%.*s\n", (int)line_len, line);
            }

            if(!at_end && input_data[i] == '\r' && (i + 1 < input_size) &&
               input_data[i + 1] == '\n') {
                i++;
            }
            line_start = i + 1;
        }
    }

    if(!inserted_coordinates || !is_sub_file) {
        nearby_files_append_coordinates(output, latitude, longitude);
    }

    if(!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        furi_string_free(output);
        free(input_data);
        storage_file_free(file);
        return false;
    }

    const size_t output_size = furi_string_size(output);
    const size_t bytes_written =
        storage_file_write(file, (const uint8_t*)furi_string_get_cstr(output), output_size);
    success = bytes_written == output_size;

    storage_file_close(file);
    furi_string_free(output);
    free(input_data);
    storage_file_free(file);
    return success;
}

bool nearby_files_add_gps_to_file(
    NearbyFilesApp* app,
    const char* extension,
    const char* base_path) {
    furi_assert(app);
    furi_assert(extension);
    furi_assert(base_path);

    const Icon* browser_icon = NULL;
    if(strcmp(extension, ".rfid") == 0) {
        browser_icon = &I_125_10px;
    } else if(strcmp(extension, ".ibtn") == 0) {
        browser_icon = &I_ibutt_10px;
    } else if(strcmp(extension, ".nfc") == 0) {
        browser_icon = &I_Nfc_10px;
    } else if(strcmp(extension, ".sub") == 0) {
        browser_icon = &I_sub1_10px;
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, extension, browser_icon);
    browser_options.base_path = base_path;

    FuriString* selected_path = furi_string_alloc_set(base_path);
    const bool selected =
        dialog_file_browser_show(app->dialogs, selected_path, selected_path, &browser_options);
    if(!selected) {
        furi_string_free(selected_path);
        return false;
    }

    const GpsCoordinates current_coords = gps_reader_get_coordinates(app->gps_reader);
    if(!current_coords.valid) {
        nearby_files_show_message(
            app->dialogs, "GPS not ready", "No valid coordinates\nfrom GPS module");
        furi_string_free(selected_path);
        return false;
    }

    const bool updated = nearby_files_update_coordinates_in_file(
        app->storage,
        furi_string_get_cstr(selected_path),
        (double)current_coords.latitude,
        (double)current_coords.longitude);

    if(updated) {
        nearby_files_show_message(app->dialogs, "Coordinates saved", "File GPS updated");
    } else {
        nearby_files_show_message(app->dialogs, "Write failed", "Cannot update file");
    }

    furi_string_free(selected_path);
    return updated;
}

NearbyFilesApp* nearby_files_app_alloc(void) {
    NearbyFilesApp* app = malloc(sizeof(NearbyFilesApp));
    uint32_t config_baudrate = 0;

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->loader = furi_record_open(RECORD_LOADER);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nearby_files_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nearby_files_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nearby_files_navigation_callback);

    // Initialize variable item list
    app->variable_item_list = variable_item_list_alloc();
    variable_item_list_set_enter_callback(
        app->variable_item_list, nearby_files_file_selected_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        NearbyFilesViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    // Initialize widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NearbyFilesViewWidget, widget_get_view(app->widget));

    // Initialize submenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NearbyFilesViewSubmenu, submenu_get_view(app->submenu));

    // Initialize about widget
    app->about_widget = widget_alloc();
    widget_add_text_scroll_element(
        app->about_widget,
        0,
        0,
        128,
        64,
        "\e#" NEARBY_FILES_APP_NAME " v" NEARBY_FILES_VERSION "\n"
        "\n"
        "Lists Sub-GHz, NFC, RFID,\niButton\n"
        "files sorted by distance from\n"
        "current location.\n"
        "(GPS module required)\n"
        "\n"
        "Files must include Lat: and\n"
        "Lon: entries to be able to\n"
        "calculate the distance.\n"
        "\n"
        "You can add these\ncoordinates to\n"
        "files using the (Add GPS)\noption.\n"
        "\n"
        "Some custom firmwares\n"
        "And apps like\nSubGHz Wardriving\n"
        "add these coordinates at the\n"
        "time of recording if the GPS\n"
        "option is enabled.\n"
        "\n"
        "Click a file to launch it in the\n"
        "appropriate app.\n"
        "\n"
        "Author: @Stichoza\n"
        "Baudrate & Add GPS:\n@xMasterX (MMX)\n"
        "\n"
        "For information or issues,\n"
        "go to https://github.com/Stichoza/flipper-nearby-files");
    view_dispatcher_add_view(
        app->view_dispatcher, NearbyFilesViewAbout, widget_get_view(app->about_widget));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Load config before GPS reader starts serial init.
    nearby_files_load_config_baudrate(&config_baudrate);

    // Initialize GPS reader
    app->gps_reader = gps_reader_alloc(config_baudrate);

    // Initialize GPS timer
    app->gps_timer = furi_timer_alloc(nearby_files_gps_timer_callback, FuriTimerTypePeriodic, app);

    // Initialize file list
    app->files = NULL;
    app->file_count = 0;
    app->file_capacity = 0;

    return app;
}

void nearby_files_app_free(NearbyFilesApp* app) {
    furi_assert(app);

    FURI_LOG_I(TAG, "Starting app cleanup");

    // Free file list
    FURI_LOG_I(TAG, "Clearing file list");
    nearby_files_clear_files(app);

    // Free GPS reader
    FURI_LOG_I(TAG, "Freeing GPS reader");
    gps_reader_free(app->gps_reader);
    app->gps_reader = NULL;

    // Stop and free GPS timer
    FURI_LOG_I(TAG, "Stopping and freeing GPS timer");
    furi_timer_stop(app->gps_timer);
    furi_timer_free(app->gps_timer);
    app->gps_timer = NULL;

    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, NearbyFilesViewAbout);
    variable_item_list_free(app->variable_item_list);
    widget_free(app->widget);
    submenu_free(app->submenu);
    widget_free(app->about_widget);

    // Free managers
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Close records
    FURI_LOG_I(TAG, "Closing records");
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_LOADER);
    furi_record_close(RECORD_DIALOGS);

    FURI_LOG_I(TAG, "App cleanup complete");
    free(app);
}

void nearby_files_add_file(
    NearbyFilesApp* app,
    const char* path,
    const char* name,
    const char* app_name) {
    // Expand capacity if needed
    if(app->file_count >= app->file_capacity) {
        app->file_capacity = app->file_capacity == 0 ? 16 : app->file_capacity * 2;
        app->files = realloc(app->files, app->file_capacity * sizeof(NearbyFileItem));
    }

    // Add new file
    NearbyFileItem* item = &app->files[app->file_count];
    item->path = furi_string_alloc_set(path);

    // Create display name without extension
    FuriString* display_name = furi_string_alloc_set(name);
    const char* dot = strrchr(name, '.');
    if(dot != NULL) {
        // Remove extension from display name
        size_t name_len = dot - name;
        furi_string_set_strn(display_name, name, name_len);
    }
    item->name = display_name;
    item->app_name = app_name;

    // Initialize GPS fields - will be populated later for performance
    item->latitude = 0.0;
    item->longitude = 0.0;
    item->has_coordinates = false;
    item->distance = 0.0;

    app->file_count++;
}

void nearby_files_clear_files(NearbyFilesApp* app) {
    if(app->files) {
        for(size_t i = 0; i < app->file_count; i++) {
            furi_string_free(app->files[i].path);
            furi_string_free(app->files[i].name);
        }
        free(app->files);
        app->files = NULL;
    }
    app->file_count = 0;
    app->file_capacity = 0;
}

bool nearby_files_scan_directories(NearbyFilesApp* app) {
    bool success = true;

    // Clear existing files
    nearby_files_clear_files(app);

    // Scan each root directory
    for(size_t dir_idx = 0; dir_idx < scan_directories_count; dir_idx++) {
        const char* root_dir = scan_directories[dir_idx];
        const char* app_name = app_names[dir_idx];

        // Check if directory exists
        if(!storage_dir_exists(app->storage, root_dir)) {
            continue;
        }

        // Use dir_walk for recursive scanning
        DirWalk* dir_walk = dir_walk_alloc(app->storage);
        dir_walk_set_recursive(dir_walk, true);
        dir_walk_set_filter_cb(dir_walk, nearby_files_dir_filter, app);

        if(dir_walk_open(dir_walk, root_dir)) {
            FuriString* path = furi_string_alloc();
            FileInfo file_info;

            while(dir_walk_read(dir_walk, path, &file_info) == DirWalkOK) {
                // Check if it's a file with target extension
                if(!(file_info.flags & FSF_DIRECTORY) &&
                   nearby_files_file_filter(furi_string_get_cstr(path), &file_info, app)) {
                    // Extract filename from path
                    const char* full_path = furi_string_get_cstr(path);
                    const char* filename = strrchr(full_path, '/');
                    if(filename) {
                        filename++; // Skip the '/'
                        nearby_files_add_file(app, full_path, filename, app_name);
                    }
                }
            }

            furi_string_free(path);
            dir_walk_close(dir_walk);
        } else {
            success = false;
        }

        dir_walk_free(dir_walk);
    }

    FURI_LOG_I(TAG, "Found %zu files", app->file_count);

    // Process GPS coordinates for all files (filter out files without coordinates)
    nearby_files_process_gps_coordinates(app);

    // Sort files by distance from current location
    nearby_files_sort_by_distance(app);

    return success;
}

// Format distance according to specifications:
// < 1km: 123m (no decimals)
// 1-10km: 4.3km (1 decimal)
// > 10km: 14km (no decimals)
static void
    nearby_files_format_distance(double distance_meters, char* buffer, size_t buffer_size) {
    float distance_f = (float)distance_meters;
    if(distance_f < 1000.0f) {
        // Less than 1km - show in meters with no decimals
        snprintf(buffer, buffer_size, "%.0fm", (double)distance_f);
    } else if(distance_f < 10000.0f) {
        // 1km to 10km - show in km with 1 decimal place
        snprintf(buffer, buffer_size, "%.1fkm", (double)(distance_f / 1000.0f));
    } else {
        // More than 10km - show in km with no decimals
        snprintf(buffer, buffer_size, "%.0fkm", (double)(distance_f / 1000.0f));
    }
}

void nearby_files_populate_list(NearbyFilesApp* app) {
    variable_item_list_reset(app->variable_item_list);

    for(size_t i = 0; i < app->file_count; i++) {
        // Format distance and create display string
        char distance_str[16];
        char display_name[256];

        nearby_files_format_distance(app->files[i].distance, distance_str, sizeof(distance_str));
        snprintf(
            display_name,
            sizeof(display_name),
            "[%s] %s",
            distance_str,
            furi_string_get_cstr(app->files[i].name));

        variable_item_list_add(
            app->variable_item_list,
            display_name,
            0, // No values count for simple list items
            NULL, // No change callback
            NULL); // No item context needed
    }
}

void nearby_files_refresh_and_populate(NearbyFilesApp* app) {
    // Scan directories to refresh file list
    nearby_files_scan_directories(app);

    // Populate the UI list with refreshed files
    nearby_files_populate_list(app);
}

bool nearby_files_parse_coordinates(const char* file_path, double* latitude, double* longitude) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char buffer[256];
    bool lat_found = false, lon_found = false;

    // Read file character by character to build lines
    FuriString* line = furi_string_alloc();

    while(storage_file_read(file, buffer, 1) == 1) {
        if(buffer[0] == '\n' || buffer[0] == '\r') {
            // Process the line
            const char* line_str = furi_string_get_cstr(line);

            // Check for latitude keys
            if(!lat_found) {
                if(strncmp(line_str, "Lat: ", 5) == 0) {
                    *latitude = strtod(line_str + 5, NULL);
                    lat_found = true;
                } else if(strncmp(line_str, "Latitude: ", 10) == 0) {
                    *latitude = strtod(line_str + 10, NULL);
                    lat_found = true;
                } else if(strncmp(line_str, "Latitute: ", 10) == 0) { // Typo version
                    *latitude = strtod(line_str + 10, NULL);
                    lat_found = true;
                }
            }

            // Check for longitude keys
            if(!lon_found) {
                if(strncmp(line_str, "Lon: ", 5) == 0) {
                    *longitude = strtod(line_str + 5, NULL);
                    lon_found = true;
                } else if(strncmp(line_str, "Longitude: ", 11) == 0) {
                    *longitude = strtod(line_str + 11, NULL);
                    lon_found = true;
                }
            }

            // If both found, we can stop reading
            if(lat_found && lon_found) break;

            // Reset line for next iteration
            furi_string_reset(line);
        } else {
            // Add character to current line
            furi_string_push_back(line, buffer[0]);
        }
    }

    furi_string_free(line);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return lat_found && lon_found;
}

double nearby_files_calculate_distance(double lat1, double lon1, double lat2, double lon2) {
    // Haversine formula for calculating distance between two GPS coordinates
    const float R = 6371000.0f; // Earth's radius in meters
    const float PI = 3.14159265f; // Define PI as float
    const float DEG_TO_RAD = PI / 180.0f;

    // Convert degrees to radians
    float lat1_rad = (float)lat1 * DEG_TO_RAD;
    float lat2_rad = (float)lat2 * DEG_TO_RAD;
    float delta_lat = ((float)lat2 - (float)lat1) * DEG_TO_RAD;
    float delta_lon = ((float)lon2 - (float)lon1) * DEG_TO_RAD;

    float half_delta_lat = delta_lat / 2.0f;
    float half_delta_lon = delta_lon / 2.0f;

    float sin_half_delta_lat = sin((double)half_delta_lat);
    float cos_lat1 = cos((double)lat1_rad);
    float cos_lat2 = cos((double)lat2_rad);
    float sin_half_delta_lon = sin((double)half_delta_lon);

    float a = sin_half_delta_lat * sin_half_delta_lat +
              cos_lat1 * cos_lat2 * sin_half_delta_lon * sin_half_delta_lon;
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

    return (double)(R * c); // Distance in meters, return as double
}

void nearby_files_process_gps_coordinates(NearbyFilesApp* app) {
    if(app->file_count == 0) return;

    // Get current GPS coordinates - must be valid
    GpsCoordinates current_coords = gps_reader_get_coordinates(app->gps_reader);

    if(!current_coords.valid) {
        FURI_LOG_W(TAG, "GPS coordinates not available yet");
        return;
    }

    double current_lat = (double)current_coords.latitude;
    double current_lon = (double)current_coords.longitude;

    FURI_LOG_I(TAG, "Using GPS coordinates: lat=%.6f, lon=%.6f", current_lat, current_lon);

    size_t valid_files = 0;

    // Process each file to extract GPS coordinates
    for(size_t i = 0; i < app->file_count; i++) {
        NearbyFileItem* item = &app->files[i];
        double latitude, longitude;

        // Parse GPS coordinates from file
        if(nearby_files_parse_coordinates(
               furi_string_get_cstr(item->path), &latitude, &longitude)) {
            // Check if coordinates are valid (not zero)
            if((float)latitude != 0.0f && (float)longitude != 0.0f) {
                // File has valid GPS coordinates
                item->latitude = latitude;
                item->longitude = longitude;
                item->has_coordinates = true;
                // Calculate distance from current location (real GPS or fallback)
                item->distance =
                    nearby_files_calculate_distance(current_lat, current_lon, latitude, longitude);

                // Move valid file to the front of the array
                if(valid_files != i) {
                    app->files[valid_files] = app->files[i];
                }
                valid_files++;
            } else {
                // File has zero coordinates, treat as invalid
                furi_string_free(item->path);
                furi_string_free(item->name);
            }
        } else {
            // File doesn't have GPS coordinates, free its resources
            furi_string_free(item->path);
            furi_string_free(item->name);
        }
    }

    // Update file count to only include files with GPS coordinates
    app->file_count = valid_files;

    FURI_LOG_I(TAG, "Processed GPS coordinates, %zu files with coordinates", valid_files);
}

void nearby_files_sort_by_distance(NearbyFilesApp* app) {
    if(app->file_count <= 1) return;

    // Insertion sort - faster than bubble sort, especially for small datasets
    for(size_t i = 1; i < app->file_count; i++) {
        NearbyFileItem key = app->files[i];
        size_t j = i;

        // Move elements that are greater than key one position ahead
        while(j > 0 && app->files[j - 1].distance > key.distance) {
            app->files[j] = app->files[j - 1];
            j--;
        }

        app->files[j] = key;
    }
}

void nearby_files_file_selected_callback(void* context, uint32_t index) {
    NearbyFilesApp* app = context;

    if(index < app->file_count) {
        NearbyFileItem* item = &app->files[index];

        FURI_LOG_I(TAG, "Opening %s with %s", furi_string_get_cstr(item->path), item->app_name);

        // Get the current app's launch path for re-launching
        FuriString* current_app_path = furi_string_alloc();
        if(loader_get_application_launch_path(app->loader, current_app_path)) {
            // Queue the app launch to happen after our app exits
            loader_enqueue_launch(
                app->loader,
                item->app_name,
                furi_string_get_cstr(item->path),
                LoaderDeferredLaunchFlagGui);

            // Queue our app to launch again after the file handler exits
            loader_enqueue_launch(
                app->loader,
                furi_string_get_cstr(current_app_path),
                NULL,
                LoaderDeferredLaunchFlagNone);

            FURI_LOG_I(
                TAG,
                "Queued launch of %s with file %s, then return to Nearby Files at %s",
                item->app_name,
                furi_string_get_cstr(item->path),
                furi_string_get_cstr(current_app_path));
        } else {
            FURI_LOG_E(TAG, "Failed to get current app launch path");
            // Still launch the target app, but won't return to Nearby Files
            loader_enqueue_launch(
                app->loader,
                item->app_name,
                furi_string_get_cstr(item->path),
                LoaderDeferredLaunchFlagGui);
        }

        furi_string_free(current_app_path);

        // Exit our app to allow the queued apps to launch
        scene_manager_stop(app->scene_manager);
        view_dispatcher_stop(app->view_dispatcher);
    }
}

void nearby_files_start_gps_wait(NearbyFilesApp* app) {
    // Call GPS timer callback immediately to show initial status
    nearby_files_gps_timer_callback(app);

    // Start a timer to periodically check GPS status
    furi_timer_start(app->gps_timer, 1000); // Check every 1 second
}

void nearby_files_gps_timer_callback(void* context) {
    NearbyFilesApp* app = context;

    // Check if GPS coordinates are available
    GpsCoordinates coords = gps_reader_get_coordinates(app->gps_reader);

    FURI_LOG_D(
        TAG,
        "GPS timer check: valid=%s, module=%s, sats=%d, lat=%f, lon=%f",
        coords.valid ? "true" : "false",
        coords.module_detected ? "true" : "false",
        coords.satellite_count,
        (double)coords.latitude,
        (double)coords.longitude);

    if(coords.valid) {
        // GPS is ready, stop timer and trigger scanning scene
        furi_timer_stop(app->gps_timer);

        FURI_LOG_I(TAG, "GPS coordinates available, starting file scan");

        // Send custom event to trigger scanning
        view_dispatcher_send_custom_event(app->view_dispatcher, NearbyFilesCustomEventStartScan);
    } else {
        // Update GPS status display
        widget_reset(app->widget);

        if(!coords.module_detected) {
            // No GPS module detected
            widget_add_string_element(
                app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "No GPS Module");
            widget_add_string_element(
                app->widget,
                64,
                40,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                "Please connect GPS module");
        } else {
            // GPS module detected, show satellite count
            widget_add_string_element(
                app->widget, 64, 24, AlignCenter, AlignCenter, FontPrimary, "Waiting for GPS...");

            FuriString* sat_str =
                furi_string_alloc_printf("Satellites: %d", coords.satellite_count);
            widget_add_string_element(
                app->widget,
                64,
                40,
                AlignCenter,
                AlignCenter,
                FontSecondary,
                furi_string_get_cstr(sat_str));
            furi_string_free(sat_str);
        }
    }
    // If GPS is not ready yet, timer will continue and check again
}

int32_t nearby_files_app(void* p) {
    UNUSED(p);

    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    NearbyFilesApp* app = nearby_files_app_alloc();

    scene_manager_next_scene(app->scene_manager, NearbyFilesSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    nearby_files_app_free(app);

    // Re-enable expansion protocol
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
