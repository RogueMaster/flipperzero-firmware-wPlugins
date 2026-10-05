#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define K2_PRINTER_K2 (1 << 0)
#define K2_PRINTER_K1 (1 << 1)
#define K2_PRINTER_HI (1 << 2)

typedef struct {
    const char* id;                  /* 5-char code, e.g. "01001" */
    const char* brand;               /* e.g. "Creality", "Generic", "eSUN", "Polymaker" */
    const char* name;                /* e.g. "Hyper PLA" */
    const char* type;                /* e.g. "PLA", "PETG", "ABS" */
    const char* default_color_hex;   /* e.g. "FFFFFF" */
    uint16_t nozzle_temp;
    uint16_t bed_temp;
    uint8_t supported_printers;      /* Bitmask of K2_PRINTER_* */
} K2Material;

typedef struct {
    const char* name;
    const char* hex;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} K2ColorPreset;

typedef struct {
    const char* label;   /* "1 KG", "750 G", etc. */
    const char* code;    /* "0330", "0247", etc. */
    uint16_t length_m;   /* 330, 247, etc. */
    uint16_t weight_g;   /* 1000, 750, etc. */
} K2WeightOption;

typedef struct {
    char printer_model[8];      /* "K2", "K1", "HI" */
    char material_id[8];        /* e.g. "01001" */
    char color_hex[8];          /* 6-char hex, e.g. "FFFFFF" */
    char weight_code[8];        /* 4-char code, e.g. "0330" */
    char serial[8];             /* 6-char serial, e.g. "000001" */
    char batch[4];              /* default "A2" */
    char vendor_id[8];          /* default "0276" */
    char date[8];               /* default "AB124" */
} K2SpoolConfig;

typedef struct {
    bool valid;
    uint8_t uid[4];
    char material_id[8];
    const K2Material* material;
    char color_hex[8];
    char color_name[32];
    char length_code[8];
    char weight_label[16];
    char serial[8];
    char batch[4];
    char vendor_id[8];
    char date[8];
    char printer_model[16];
    uint8_t raw_sector1[48];
    uint8_t raw_sector2[48];
} K2SpoolInfo;

/* Database accessors */
size_t k2_db_get_material_count(void);
const K2Material* k2_db_get_material(size_t index);
const K2Material* k2_db_find_material_by_id(const char* id);

size_t k2_db_get_color_count(void);
const K2ColorPreset* k2_db_get_color(size_t index);
const char* k2_db_find_closest_color_name(const char* hex_str);

size_t k2_db_get_weight_count(void);
const K2WeightOption* k2_db_get_weight(size_t index);
const char* k2_db_find_weight_label_by_code(const char* code);

size_t k2_db_get_printer_count(void);
const char* k2_db_get_printer_name(size_t index);

/* Payload builder & parser */
bool k2_build_payload(const K2SpoolConfig* config, uint8_t sector1_out[48], uint8_t sector2_out[48]);
bool k2_parse_payload(const uint8_t sector1[48], const uint8_t sector2[48], K2SpoolInfo* info_out);

#ifdef __cplusplus
}
#endif
