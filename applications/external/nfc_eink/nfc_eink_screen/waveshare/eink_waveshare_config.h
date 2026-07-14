#pragma once
#include "eink_waveshare_i.h"

typedef enum {
    EinkScreenTypeWaveshareInvalid = 0,
    EinkScreenTypeWaveshare2n13inch = 4,
    EinkScreenTypeWaveshare2n7inch = 16,
    EinkScreenTypeWaveshare2n9inch = 7,
    EinkScreenTypeWaveshare4n2inch = 10,
    EinkScreenTypeWaveshare7n5inchHD = 12,
    EinkScreenTypeWaveshare7n5inch = 14,
} EinkScreenTypeWaveshare;

EinkScreenTypeWaveshare eink_waveshare_config_get_protocol_screen_type_by_name(FuriString* name);

const char* eink_waveshare_config_get_screen_name_by_protocol_screen_type(
    EinkScreenTypeWaveshare protocol_screen_type);
