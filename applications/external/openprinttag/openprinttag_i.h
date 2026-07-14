#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>

#define TAG                    "OpenPrintTag"
#define OPENPRINTTAG_MIME_TYPE "application/vnd.openprinttag"

// OpenPrintTag data structures
typedef struct {
    uint32_t main_region_offset;
    uint32_t main_region_size;
    uint32_t aux_region_offset;
    uint32_t aux_region_size;
} OpenPrintTagMeta;

typedef struct {
    // Brand and material identification
    FuriString* brand_name;
    FuriString* material_name;
    FuriString* material_type_str;
    FuriString* material_abbreviation;
    uint32_t material_type_enum;
    uint32_t material_class;
    uint64_t gtin;

    // Weights (in grams)
    uint32_t nominal_netto_full_weight;
    uint32_t actual_netto_full_weight;
    uint32_t empty_container_weight;

    // Timestamps
    uint64_t manufactured_date;
    uint64_t expiration_date;

    // FFF-specific
    float filament_diameter;
    float nominal_full_length;
    float actual_full_length;
    int32_t min_print_temperature;
    int32_t max_print_temperature;
    int32_t min_bed_temperature;
    int32_t max_bed_temperature;

    // Material properties
    float density;
    uint32_t* tags;
    size_t tags_count;

    bool has_data;
    bool has_material_type_enum;
} OpenPrintTagMain;

typedef struct {
    uint32_t consumed_weight; // in grams
    FuriString* workgroup;
    uint64_t last_stir_time; // timestamp
    bool has_data;
} OpenPrintTagAux;

typedef struct {
    OpenPrintTagMeta meta;
    OpenPrintTagMain main;
    OpenPrintTagAux aux;
    uint8_t* raw_data;
    size_t raw_data_size;
} OpenPrintTagData;

// App scenes
typedef enum {
    OpenPrintTagSceneStart,
    OpenPrintTagSceneRead,
    OpenPrintTagSceneReadSuccess,
    OpenPrintTagSceneReadError,
    OpenPrintTagSceneDisplay,
    OpenPrintTagSceneWrite,
    OpenPrintTagSceneNum,
} OpenPrintTagScene;

// App views
typedef enum {
    OpenPrintTagViewSubmenu,
    OpenPrintTagViewWidget,
    OpenPrintTagViewPopup,
    OpenPrintTagViewLoading,
} OpenPrintTagView;

// Main app structure
typedef struct OpenPrintTag {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    Loading* loading;

    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcScanner* nfc_scanner;
    NfcPoller* nfc_poller;
    NfcProtocol detected_protocol;

    OpenPrintTagData tag_data;
} OpenPrintTag;

// Scene handlers
void openprinttag_scene_start_on_enter(void* context);
bool openprinttag_scene_start_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_start_on_exit(void* context);

void openprinttag_scene_read_on_enter(void* context);
bool openprinttag_scene_read_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_read_on_exit(void* context);

void openprinttag_scene_read_success_on_enter(void* context);
bool openprinttag_scene_read_success_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_read_success_on_exit(void* context);

void openprinttag_scene_read_error_on_enter(void* context);
bool openprinttag_scene_read_error_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_read_error_on_exit(void* context);

void openprinttag_scene_display_on_enter(void* context);
bool openprinttag_scene_display_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_display_on_exit(void* context);

// Helper functions
bool openprinttag_parse_ndef(OpenPrintTag* app, const uint8_t* data, size_t size);
bool openprinttag_parse_cbor(OpenPrintTag* app, const uint8_t* payload, size_t size);
void openprinttag_free_data(OpenPrintTagData* data);
