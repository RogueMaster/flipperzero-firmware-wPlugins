#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <storage/storage.h>

#include <stdint.h>
#include <stddef.h>

#define BR_APP_ID "bambu_rfid"
#define BR_APP_NAME "Bambu RFID"
#define BR_APP_VERSION "0.2.0"

#define BR_DATA_DIR EXT_PATH("apps_data/bambu_rfid")
#define BR_TAGS_DIR BR_DATA_DIR "/tags"

#define BR_UID_SIZE 4U
#define BR_SECTOR_COUNT 16U
#define BR_BLOCK_COUNT 64U
#define BR_BLOCK_SIZE 16U
#define BR_TAG_SIZE (BR_BLOCK_COUNT * BR_BLOCK_SIZE)

#define BR_MAX_SAVED 64U
#define BR_PATH_MAX 192U
#define BR_NAME_MAX 64U
#define BR_DETAIL_ROWS 19U

struct BrTagDump {
    uint8_t blocks[BR_BLOCK_COUNT][BR_BLOCK_SIZE];
    bool read[BR_BLOCK_COUNT];
};

struct BrTagInfo {
    bool valid;
    bool complete;
    uint8_t uid[BR_UID_SIZE];
    char uid_hex[BR_UID_SIZE * 2U + 1U];

    char variant_id[9];
    char material_id[9];
    char filament_type[17];
    char detailed_filament_type[17];

    uint8_t color_rgba[4];
    uint8_t color_count;
    uint8_t second_color_rgba[4];
    char color_hex[10];
    char second_color_hex[10];

    uint16_t spool_weight_g;
    float filament_diameter_mm;
    uint16_t filament_length_m;
    float spool_width_mm;
    float nozzle_diameter_mm;

    uint16_t drying_temp_c;
    uint16_t drying_time_h;
    uint16_t bed_temp_type;
    uint16_t bed_temp_c;
    uint16_t hotend_max_c;
    uint16_t hotend_min_c;

    char production_date[17];
    char short_date[17];
    char tray_uid_hex[33];
    char xcam_hex[25];
    char block17_hex[5];
};

struct BrSavedEntry {
    char filename[BR_PATH_MAX];
    char display_name[BR_NAME_MAX];
    BrTagInfo info;
};

struct BrApp {
    Storage* storage;
    Gui* gui;
};
