#pragma once

#include <stdint.h>

typedef enum {
    CrystalSeries1_0C00,
    CrystalSeries1_0C01,
    CrystalSeries1_0C02,
    CrystalSeries1_0C03,
    CrystalSeries1_0C04,
    CrystalSeries1_0C05,
    CrystalSeries1_0C06,
    CrystalSeries1_0C07,
    CrystalSeries1_0C08,
    CrystalSeries1_0C09,
    CrystalSeries1_0C0A,
    CrystalSeries1_0C0B,
    CrystalSeries1_0C0C,
    CrystalSeries1_0C0D,
    CrystalSeries1_0C0E,
    CrystalSeries1_0C0F,
    CrystalSeries1_0C31,
    CrystalSeries1_0C32,
    CrystalSeries1_0C33,
    CrystalSeries1_MAX,
} CrystalSeries1ID;

typedef struct {
    CrystalSeries1ID id;
    const char* hilt_color;
    const char* blade_color;
    const char* jedi_voice;
    const char* sith_voice;
    const char* wayfinder_location;
    uint8_t em4100[2];
    uint8_t em4305[4]; // This stores address 6
    uint8_t t5577[4]; // This stores block 2 data, block 1 should be 0xFF800000
} CrystalSeries1;

typedef enum {
    CrystalSeries2_01,
    CrystalSeries2_02,
    CrystalSeries2_03,
    CrystalSeries2_04,
    CrystalSeries2_05,
    CrystalSeries2_06,
    CrystalSeries2_07,
    CrystalSeries2_08,
    CrystalSeries2_09,
    CrystalSeries2_10,
    CrystalSeries2_11,
    CrystalSeries2_12,
    CrystalSeries2_13,
    CrystalSeries2_14,
    CrystalSeries2_15,
    CrystalSeries2_16,
    CrystalSeries2_17,
    CrystalSeries2_18,
    CrystalSeries2_MAX,
} CrystalSeries2ID;

typedef struct {
    CrystalSeries2ID id;
    const char* crystal_color;
    CrystalSeries1ID s1_fallback;
    const char* voice;
    const char* wayfinder_location;
    uint8_t em4100[2];
    uint8_t em4305_a6[4];
    uint8_t em4305_a9[4];
} CrystalSeries2;

extern const CrystalSeries1 CrystalsSeries1[];
extern const CrystalSeries2 CrystalsSeries2[];
