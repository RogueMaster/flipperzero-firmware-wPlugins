#pragma once

// OpenPrintTag field definitions from specification
// https://github.com/prusa3d/OpenPrintTag/tree/main/data

// Meta section fields
// https://github.com/prusa3d/OpenPrintTag/blob/main/data/meta_fields.yaml
#define META_MAIN_REGION_OFFSET 0
#define META_MAIN_REGION_SIZE   1
#define META_AUX_REGION_OFFSET  2
#define META_AUX_REGION_SIZE    3

// Main section fields
// https://github.com/prusa3d/OpenPrintTag/blob/main/data/main_fields.yaml

// UUIDs (0-3)
#define MAIN_INSTANCE_UUID 0
#define MAIN_PACKAGE_UUID  1
#define MAIN_MATERIAL_UUID 2
#define MAIN_BRAND_UUID    3

// Product information (4-11)
#define MAIN_GTIN                       4
#define MAIN_BRAND_SPECIFIC_INSTANCE_ID 5
#define MAIN_BRAND_SPECIFIC_PACKAGE_ID  6
#define MAIN_BRAND_SPECIFIC_MATERIAL_ID 7
#define MAIN_MATERIAL_CLASS             8
#define MAIN_MATERIAL_TYPE              9
#define MAIN_MATERIAL_NAME              10
#define MAIN_BRAND_NAME                 11

// Temporal and weight (14-18)
#define MAIN_MANUFACTURED_DATE         14
#define MAIN_EXPIRATION_DATE           15
#define MAIN_NOMINAL_NETTO_FULL_WEIGHT 16
#define MAIN_ACTUAL_NETTO_FULL_WEIGHT  17
#define MAIN_EMPTY_CONTAINER_WEIGHT    18

// Colors (19-24, 52)
#define MAIN_PRIMARY_COLOR         19
#define MAIN_SECONDARY_COLOR_0     20
#define MAIN_SECONDARY_COLOR_1     21
#define MAIN_SECONDARY_COLOR_2     22
#define MAIN_SECONDARY_COLOR_3     23
#define MAIN_SECONDARY_COLOR_4     24
#define MAIN_MATERIAL_ABBREVIATION 52

// Material properties (27-29)
#define MAIN_TRANSMISSION_DISTANCE 27
#define MAIN_TAGS                  28
#define MAIN_DENSITY               29

// FFF-specific (30-45, 53-54)
#define MAIN_FILAMENT_DIAMETER        30
#define MAIN_SHORE_HARDNESS_A         31
#define MAIN_SHORE_HARDNESS_D         32
#define MAIN_MIN_NOZZLE_DIAMETER      33
#define MAIN_MIN_PRINT_TEMPERATURE    34
#define MAIN_MAX_PRINT_TEMPERATURE    35
#define MAIN_PREHEAT_TEMPERATURE      36
#define MAIN_MIN_BED_TEMPERATURE      37
#define MAIN_MAX_BED_TEMPERATURE      38
#define MAIN_MIN_CHAMBER_TEMPERATURE  39
#define MAIN_MAX_CHAMBER_TEMPERATURE  40
#define MAIN_CHAMBER_TEMPERATURE      41
#define MAIN_CONTAINER_WIDTH          42
#define MAIN_CONTAINER_OUTER_DIAMETER 43
#define MAIN_CONTAINER_INNER_DIAMETER 44
#define MAIN_CONTAINER_HOLE_DIAMETER  45
#define MAIN_NOMINAL_FULL_LENGTH      53
#define MAIN_ACTUAL_FULL_LENGTH       54

// SLA-specific (46-51)
#define MAIN_VISCOSITY_18C                 46
#define MAIN_VISCOSITY_25C                 47
#define MAIN_VISCOSITY_40C                 48
#define MAIN_VISCOSITY_60C                 49
#define MAIN_CONTAINER_VOLUMETRIC_CAPACITY 50
#define MAIN_CURE_WAVELENGTH               51

// Auxiliary section fields
// https://github.com/prusa3d/OpenPrintTag/blob/main/data/aux_fields.yaml
#define AUX_CONSUMED_WEIGHT            0
#define AUX_WORKGROUP                  1
#define AUX_GENERAL_PURPOSE_RANGE_USER 2
#define AUX_LAST_STIR_TIME             3

// Vendor-specific ranges
#define AUX_PRUSA_RANGE_START     65300
#define AUX_PRUSA_RANGE_END       65399
#define AUX_GENERAL_PURPOSE_START 65400
#define AUX_GENERAL_PURPOSE_END   65534
