#pragma once

#include "app_state.h"
#include "flipperham.h"

typedef struct {
    const char* name;
    const uint8_t* regs;
} FlipperHamPreset;

enum {
    FlipperHamModemProfileDefault = 1,
    FlipperHamPresetDefault = 16,
};

#define FHMP_DIRECT_FSK 1
#define FHMP_SCRAMBLING_G3RUH 2

typedef struct {
    const char* name;
    uint16_t baud;
    uint16_t mark_hz;
    uint16_t space_hz;
    uint16_t flags;
} FlipperHamModemProfile;

#define WAVE_N 28672

extern const FlipperHamPreset flipperham_presets[18];
extern const FlipperHamModemProfile flipperham_modem_profiles[3];

void flipperham_radio_start(FlipperHamApp* app);
void flipperham_radio_start_ext(FlipperHamApp* app);
bool flipperham_radio_ext_is_complete(void);
void preset_fix(FlipperHamApp* app);
uint32_t tx_freq_get(FlipperHamApp* app);

void txstart(FlipperHamApp* app);
void flipperham_radio_stop(FlipperHamApp* app);
void flipperham_radio_stop_ext(FlipperHamApp* app);
