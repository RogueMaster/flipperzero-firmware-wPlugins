#pragma once

#include <furi.h>

#define SPECTRUM_NUM_BINS       128
#define SPECTRUM_WATERFALL_ROWS 48
#define SPECTRUM_RSSI_MIN       (-100.0f)
#define SPECTRUM_RSSI_MAX       (-30.0f)
#define SPECTRUM_HEADER_HEIGHT  10
#define SPECTRUM_GRAPH_HEIGHT   54

typedef enum {
    SpectrumViewBar,
    SpectrumViewWaterfall,
} SpectrumViewMode;

typedef enum {
    SpectrumBand315,
    SpectrumBand433,
    SpectrumBand868,
    SpectrumBand915,
    SpectrumBandCustom,
    SpectrumBandCount,
} SpectrumBand;

typedef struct {
    uint32_t start_freq;
    uint32_t end_freq;
    uint32_t step_khz;
    const char* name;
} SpectrumBandConfig;

extern const SpectrumBandConfig spectrum_band_configs[];

typedef struct {
    float rssi[SPECTRUM_NUM_BINS];
    float peak_rssi[SPECTRUM_NUM_BINS];
    uint32_t freq_start;
    uint32_t freq_step;
    uint8_t num_bins;
    float max_rssi;
    uint32_t max_rssi_freq;
    uint8_t cursor_pos;
    SpectrumViewMode view_mode;
    bool peak_hold;
    uint8_t waterfall[SPECTRUM_WATERFALL_ROWS][SPECTRUM_NUM_BINS];
    uint8_t waterfall_row;
} SpectrumData;
