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
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include "numpad.h"
#include "tag_view.h"
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

    // More temperatures (degrees C, 0 = not stored) and drying (time in minutes)
    int32_t preheat_temperature;
    int32_t min_chamber_temperature;
    int32_t max_chamber_temperature;
    int32_t chamber_temperature;
    int32_t drying_temperature;
    uint32_t drying_time;

    // Identification
    bool has_color;
    uint8_t color[3]; // Primary color: red, green, blue
    bool has_instance_uuid;
    uint8_t instance_uuid[16];
    char brand_specific_instance_id[17];

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
    uint32_t ndef_payload_offset; // byte offset of the OpenPrintTag payload in tag memory
} OpenPrintTagData;

// App scenes
typedef enum {
    OpenPrintTagSceneStart,
    OpenPrintTagSceneRead,
    OpenPrintTagSceneReadSuccess,
    OpenPrintTagSceneReadError,
    OpenPrintTagSceneDisplay,
    OpenPrintTagSceneWrite,
    OpenPrintTagSceneCreate,
    OpenPrintTagSceneNum,
} OpenPrintTagScene;

// App views
typedef enum {
    OpenPrintTagViewSubmenu,
    OpenPrintTagViewWidget,
    OpenPrintTagViewPopup,
    OpenPrintTagViewLoading,
    OpenPrintTagViewVariableItemList,
    OpenPrintTagViewNumberInput,
    OpenPrintTagViewTextInput,
    OpenPrintTagViewDialog,
    OpenPrintTagViewTagView,
} OpenPrintTagView;

// Custom events sent by openprinttag_tag_write_callback() to the scene that started it
#define OpenPrintTagEventWriteDone   (0x100U)
#define OpenPrintTagEventWriteFailed (0x101U)

// Longest brand / material name, as many bytes as the specification allows
#define OPENPRINTTAG_BRAND_MAX    (31U)
#define OPENPRINTTAG_MATERIAL_MAX (63U)

// Data entered for a new tag
typedef struct {
    char brand[OPENPRINTTAG_BRAND_MAX + 1];
    char material[OPENPRINTTAG_MATERIAL_MAX + 1];
    uint32_t type_index; // Index into material_types[] (see material_types.h)
    uint32_t weight; // Weight of a full spool, in g (stored as nominal and actual weight)
    uint32_t empty_weight; // Weight of the empty spool, in g, 0 = not stored
    uint32_t nozzle_min; // Print temperatures in degrees C, 0 = not stored
    uint32_t nozzle_max;
    uint32_t bed_min;
    uint32_t bed_max;
    bool has_color; // The primary colour is stored
    uint8_t color[3]; // Primary colour: red, green, blue
    uint8_t diameter_index; // Filament diameter: 0 = 1.75 mm, 1 = 2.85 mm
    uint8_t instance_uuid[16]; // Identifies the spool, all zero = not stored
} OpenPrintTagCreateData;

#define OPENPRINTTAG_CREATE_ITEMS_MAX (12U)

// Main app structure
typedef struct OpenPrintTag {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    Widget* widget;
    Popup* popup;
    Loading* loading;
    VariableItemList* variable_item_list;
    NumPad* numpad;
    TagView* tag_view;
    TextInput* text_input;
    DialogEx* dialog_ex;
    char text_buffer[OPENPRINTTAG_MATERIAL_MAX + 1]; // Text being edited in the text input view

    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcScanner* nfc_scanner;
    NfcPoller* nfc_poller;
    NfcProtocol detected_protocol;

    OpenPrintTagData tag_data;
    uint8_t tag_uid[8]; // UID of the tag that was read, the first byte is E0 for NFC-V tags
    bool has_tag_uid;
    uint32_t temp_consumed_weight; // Temporary value for editing

    // Write state
    bool write_in_progress;
    uint8_t* write_data;
    size_t write_data_size;
    uint16_t write_start_block;
    uint16_t write_block_count;
    uint16_t write_current_block;
    uint8_t write_uid[ISO15693_3_UID_SIZE]; // UID of the tag that was read, in wire order
    uint8_t write_attempts; // Rounds in which the tag answered but the write did not stick
    uint8_t read_retries; // Times reading the tag was started again, see openprinttag_check_read()

    // Items of the edit screen, kept so their texts can follow the entered value
    VariableItem* write_remaining_item;
    VariableItem* write_consumed_item;
    bool write_number_input_active; // The number keyboard is the visible view
    bool write_number_input_additive; // The entered number is added to the total, not set

    // Creating a new tag
    OpenPrintTagCreateData create;
    VariableItem* create_items[OPENPRINTTAG_CREATE_ITEMS_MAX];
    uint8_t create_editing; // Row being edited in the text input / number pad
    uint8_t create_phase; // See the create scene
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

void openprinttag_scene_write_on_enter(void* context);
bool openprinttag_scene_write_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_write_on_exit(void* context);

void openprinttag_scene_create_on_enter(void* context);
bool openprinttag_scene_create_on_event(void* context, SceneManagerEvent event);
void openprinttag_scene_create_on_exit(void* context);

// Helper functions
bool openprinttag_parse_ndef(OpenPrintTag* app, const uint8_t* data, size_t size);
bool openprinttag_parse_cbor(OpenPrintTag* app, const uint8_t* payload, size_t size);
void openprinttag_free_data(OpenPrintTagData* data);

// How often reading a tag is started again when the poller reports it without any blocks
#define OPENPRINTTAG_READ_RETRIES        (20U)
#define OPENPRINTTAG_READ_RETRY_DELAY_MS (50U)

typedef enum {
    OpenPrintTagReadOk, // The poller read the tag's blocks
    OpenPrintTagReadRetried, // No blocks, a new poller was started and the callback runs again
    OpenPrintTagReadFailed, // No blocks and no retries left
} OpenPrintTagReadCheck;

// Call when the read poller (started with nfc_poller_start()) has reported the tag and its
// callback returned NfcCommandStop. A tag that answers the inventory request but not the ones
// after it (moved away, weak coupling) is reported with empty data, and the poller never reads it
// again by itself, so the poller is replaced by a new one that starts over with the same callback.
OpenPrintTagReadCheck openprinttag_check_read(OpenPrintTag* app, NfcGenericCallback callback);

// Writes app->write_data (blocks starting at app->write_start_block) to the tag whose UID is in
// app->write_uid. Start it with nfc_poller_start_ex() on an ISO15693-3 poller. It waits until the
// tag is in the field, skips blocks that already hold the data and verifies every block by reading
// it back. It sends OpenPrintTagEventWriteDone or OpenPrintTagEventWriteFailed when finished.
NfcCommand openprinttag_tag_write_callback(NfcGenericEventEx event, void* context);

// Builds the complete memory image of a new tag (capability container, NDEF message with the
// OpenPrintTag record, terminator). The message fills the tag and the auxiliary region starts on
// a block boundary. Returns the number of bytes used in out, which must hold capacity bytes, or 0
// if the data does not fit or the tag is not supported.
size_t openprinttag_build_tag_image(
    const OpenPrintTagCreateData* data,
    size_t capacity,
    size_t block_size,
    uint8_t* out);

// Fills the read-result screen with what was read from a tag. uid can be NULL.
void openprinttag_tag_view_set_data(
    TagView* tag_view,
    const OpenPrintTagData* data,
    const uint8_t* uid);

// Encodes the auxiliary section with a new consumed weight. Every other field already in the
// tag's auxiliary section is copied as it is, known or not, as the specification requires.
// Returns the encoded size, or 0 if it does not fit into buffer_size.
size_t openprinttag_encode_auxiliary(
    OpenPrintTag* app,
    uint8_t* buffer,
    size_t buffer_size,
    uint32_t consumed_weight);
