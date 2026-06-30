#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/dir_walk.h>
#include <loader/loader.h>
#include "gps_reader.h"

#define NEARBY_FILES_VERSION  "1.7"
#define NEARBY_FILES_APP_NAME "Nearby Files"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NearbyFilesApp NearbyFilesApp;

typedef enum {
    NearbyFilesViewVariableItemList,
    NearbyFilesViewWidget,
    NearbyFilesViewSubmenu,
    NearbyFilesViewAbout,
} NearbyFilesView;

typedef enum {
    NearbyFilesCustomEventNone,
    NearbyFilesCustomEventFileSelected,
    NearbyFilesCustomEventRefreshList,
    NearbyFilesCustomEventAddGpsPosition,
    NearbyFilesCustomEventAddGpsSub,
    NearbyFilesCustomEventAddGpsNfc,
    NearbyFilesCustomEventAddGpsRfid,
    NearbyFilesCustomEventAddGpsIbtn,
    NearbyFilesCustomEventGpsBaudrateMenu,
    NearbyFilesCustomEventSetBaudrate4800,
    NearbyFilesCustomEventSetBaudrate9600,
    NearbyFilesCustomEventSetBaudrate19200,
    NearbyFilesCustomEventSetBaudrate38400,
    NearbyFilesCustomEventSetBaudrate57600,
    NearbyFilesCustomEventSetBaudrate115200,
    NearbyFilesCustomEventAbout,
    NearbyFilesCustomEventExit,
    NearbyFilesCustomEventStartScan,
} NearbyFilesCustomEvent;

typedef struct {
    FuriString* path;
    FuriString* name;
    const char* app_name;
    double latitude;
    double longitude;
    double distance; // Distance from current location in meters
    bool has_coordinates; // Whether GPS coordinates were found in file
} NearbyFileItem;

struct NearbyFilesApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    VariableItemList* variable_item_list;
    Widget* widget;
    Submenu* submenu;
    Widget* about_widget;
    Storage* storage;
    Loader* loader;
    DialogsApp* dialogs;
    GpsReader* gps_reader;
    FuriTimer* gps_timer;

    NearbyFileItem* files;
    size_t file_count;
    size_t file_capacity;
};

// App lifecycle
NearbyFilesApp* nearby_files_app_alloc(void);
void nearby_files_app_free(NearbyFilesApp* app);
int32_t nearby_files_app(void* p);

// File scanning
bool nearby_files_scan_directories(NearbyFilesApp* app);
void nearby_files_add_file(
    NearbyFilesApp* app,
    const char* path,
    const char* name,
    const char* app_name);
void nearby_files_clear_files(NearbyFilesApp* app);

// GPS and distance functions
bool nearby_files_parse_coordinates(const char* file_path, double* latitude, double* longitude);
double nearby_files_calculate_distance(double lat1, double lon1, double lat2, double lon2);
void nearby_files_process_gps_coordinates(NearbyFilesApp* app);
void nearby_files_sort_by_distance(NearbyFilesApp* app);
void nearby_files_start_gps_wait(NearbyFilesApp* app);
void nearby_files_gps_timer_callback(void* context);

// UI helpers
void nearby_files_populate_list(NearbyFilesApp* app);
void nearby_files_refresh_and_populate(NearbyFilesApp* app);
void nearby_files_file_selected_callback(void* context, uint32_t index);
bool nearby_files_add_gps_to_file(
    NearbyFilesApp* app,
    const char* extension,
    const char* base_path);
bool nearby_files_save_config_baudrate(uint32_t baudrate);

#ifdef __cplusplus
}
#endif
