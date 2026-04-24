#pragma once

#include "app_state.h"
#include "flipperham.h"

typedef struct
{
    const char *name;
    const uint8_t *regs;
} FlipperHamPreset;

enum
{
    FlipperHamModemProfileDefault = 1,
    FlipperHamPresetDefault = 16,
};

typedef struct
{
    const char *name;
    uint16_t baud;
    uint16_t mark_hz;
    uint16_t space_hz;
} FlipperHamModemProfile;

#define WAVE_N 28672

extern const FlipperHamPreset flipperham_presets[18];
extern const FlipperHamModemProfile flipperham_modem_profiles[2];

void flipperham_radio_start(FlipperHamApp *app);
void preset_fix(FlipperHamApp *app);
uint32_t tx_freq_get(FlipperHamApp *app);

void txstart(FlipperHamApp *app);
void flipperham_radio_stop(FlipperHamApp *app);
