#include "fmtx_vfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <furi_hal.h>

#define MF_RADIO_DEFAULT_FREQUENCY_HZ  433075000U
#define MF_RADIO_FREQ_DIGITS           6U

typedef struct {
    uint32_t frequency_hz;
    bool frequency_dirty;
    bool tx_allowed;
} MfRadioSnapshot;

typedef struct {
    bool (*frequency_valid)(void* context, uint32_t frequency_hz);
    bool (*tx_allowed)(void* context, uint32_t frequency_hz);
    uint32_t (*default_frequency)(void* context);
    void* context;
} MfRadioHardwareOps;

typedef struct {
    MfRadioSnapshot snapshot;
    MfRadioHardwareOps hardware;
    uint32_t edit_khz;
    uint32_t edit_original_hz;
    uint8_t frequency_focus;
} MfRadioState;

struct FmtxVfo {
    MfRadioState state;
};

typedef struct {
    uint32_t min_hz;
    uint32_t max_hz;
} MfRadioBand;

static const MfRadioBand vfo_bands[] = {
    {300000000U, 348000000U},
    {387000000U, 464000000U},
    {779000000U, 928000000U},
};

bool mf_radio_frequency_in_vfo(uint32_t frequency_hz) {
    size_t i;
    for(i = 0U; i < sizeof(vfo_bands) / sizeof(vfo_bands[0]); i++) {
        if(frequency_hz >= vfo_bands[i].min_hz && frequency_hz <= vfo_bands[i].max_hz) return true;
    }
    return false;
}

static bool hal_frequency_valid(void* context, uint32_t frequency_hz) {
    (void)context;
    return furi_hal_subghz_is_frequency_valid(frequency_hz);
}

static bool hal_tx_allowed(void* context, uint32_t frequency_hz) {
    return hal_frequency_valid(context, frequency_hz) &&
           furi_hal_region_is_frequency_allowed(frequency_hz);
}

typedef struct {
    uint32_t min_hz;
    uint32_t max_hz;
} HalRadioBand;

static const HalRadioBand hal_vfo_bands[] = {
    {300000000U, 348000000U},
    {387000000U, 464000000U},
    {779000000U, 928000000U},
};

static bool hal_region_wide_open(void) {
    const char* name = furi_hal_region_get_name();
    const FuriHalRegion* region = furi_hal_region_get();
    if(name != NULL && strcmp(name, "00") == 0) return true;
    if(region != NULL && region->bands_count == 1U && region->bands[0].start == 0U &&
       region->bands[0].end >= 1000000000U)
        return true;
    return furi_hal_region_is_frequency_allowed(hal_vfo_bands[0].min_hz) &&
           furi_hal_region_is_frequency_allowed(hal_vfo_bands[1].min_hz) &&
           furi_hal_region_is_frequency_allowed(hal_vfo_bands[2].min_hz);
}

static uint32_t hal_region_band_default(void) {
    const FuriHalRegion* region = furi_hal_region_get();
    size_t region_i;
    size_t vfo_i;
    if(region == NULL) return MF_RADIO_DEFAULT_FREQUENCY_HZ;
    for(region_i = 0U; region_i < region->bands_count; region_i++) {
        for(vfo_i = 0U; vfo_i < sizeof(hal_vfo_bands) / sizeof(hal_vfo_bands[0]); vfo_i++) {
            uint32_t region_min_khz = (region->bands[region_i].start + 999U) / 1000U;
            uint32_t region_max_khz = region->bands[region_i].end / 1000U;
            uint32_t vfo_min_khz = hal_vfo_bands[vfo_i].min_hz / 1000U;
            uint32_t vfo_max_khz = hal_vfo_bands[vfo_i].max_hz / 1000U;
            uint32_t min_khz = region_min_khz > vfo_min_khz ? region_min_khz : vfo_min_khz;
            uint32_t max_khz = region_max_khz < vfo_max_khz ? region_max_khz : vfo_max_khz;
            uint32_t candidate;
            if(min_khz > max_khz) continue;
            candidate = (min_khz + ((max_khz - min_khz) / 2U)) * 1000U;
            if(hal_tx_allowed(NULL, candidate)) return candidate;
        }
    }
    return MF_RADIO_DEFAULT_FREQUENCY_HZ;
}

static uint32_t hal_default_frequency(void* context) {
    static const uint32_t candidates[] = {
        MF_RADIO_DEFAULT_FREQUENCY_HZ,
        315000000U,
        868350000U,
        915000000U,
        920500000U,
    };
    size_t i;
    (void)context;
    if(hal_region_wide_open()) return MF_RADIO_DEFAULT_FREQUENCY_HZ;
    for(i = 0U; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if(hal_tx_allowed(NULL, candidates[i])) return candidates[i];
    }
    return hal_region_band_default();
}

static bool mf_radio_frequency_valid(const MfRadioState* state, uint32_t frequency_hz) {
    return mf_radio_frequency_in_vfo(frequency_hz) &&
           state->hardware.frequency_valid(state->hardware.context, frequency_hz);
}

static uint32_t mf_radio_edit_place(uint8_t focus) {
    static const uint32_t places[MF_RADIO_FREQ_DIGITS] = {100000U, 10000U, 1000U, 100U, 10U, 1U};
    return places[focus % MF_RADIO_FREQ_DIGITS];
}

static void mf_radio_change_digit(MfRadioState* state, int direction) {
    uint32_t place = mf_radio_edit_place(state->frequency_focus);
    uint32_t khz = state->edit_khz % 1000000U;
    uint8_t digit = (uint8_t)((khz / place) % 10U);
    uint8_t next = direction < 0 ? (uint8_t)((digit + 9U) % 10U) : (uint8_t)((digit + 1U) % 10U);
    state->edit_khz = (uint32_t)((int32_t)khz + ((int32_t)next - (int32_t)digit) * (int32_t)place);
}

static void mf_radio_commit_frequency(MfRadioState* state) {
    uint32_t frequency_hz = (state->edit_khz % 1000000U) * 1000U;
    if(!mf_radio_frequency_in_vfo(frequency_hz)) {
        state->edit_khz = state->edit_original_hz / 1000U;
        return;
    }
    if(!state->hardware.frequency_valid(state->hardware.context, frequency_hz)) {
        frequency_hz = state->hardware.default_frequency(state->hardware.context);
        if(!mf_radio_frequency_valid(state, frequency_hz))
            frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ;
    }
    if(state->snapshot.frequency_hz != frequency_hz) {
        state->snapshot.frequency_hz = frequency_hz;
        state->snapshot.frequency_dirty = true;
    }
    state->snapshot.tx_allowed =
        state->hardware.tx_allowed(state->hardware.context, state->snapshot.frequency_hz);
    state->edit_khz = state->snapshot.frequency_hz / 1000U;
}

static void frequency_text(char* text, size_t size, uint32_t frequency_hz) {
    snprintf(text, size, "%06lu", (unsigned long)((frequency_hz / 1000U) % 1000000U));
}

static void draw_frequency_digit(Canvas* canvas, int32_t x, bool focused, char digit) {
    char text[2] = {digit, '\0'};
    if(focused) {
        canvas_draw_triangle(canvas, x + 9, 9, 5, 3, CanvasDirectionBottomToTop);
        canvas_draw_triangle(canvas, x + 9, 33, 5, 3, CanvasDirectionTopToBottom);
        canvas_draw_rbox(canvas, x, 11, 18, 20, 1);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, 11, 18, 20, 1);
    }
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, x + 9, 21, AlignCenter, AlignCenter, text);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_frequency(const MfRadioState* state, Canvas* canvas) {
    char digits[MF_RADIO_FREQ_DIGITS + 1U];
    const char* status;
    uint32_t frequency_hz = (state->edit_khz % 1000000U) * 1000U;
    uint8_t i;
    frequency_text(digits, sizeof(digits), frequency_hz);
    for(i = 0U; i < MF_RADIO_FREQ_DIGITS; i++)
        draw_frequency_digit(
            canvas, 7 + (19 * (int32_t)i), i == state->frequency_focus, digits[i]);
    canvas_set_font(canvas, FontSecondary);
    if(!mf_radio_frequency_in_vfo(frequency_hz)) {
        canvas_draw_str_aligned(canvas, 64, 47, AlignCenter, AlignCenter, "RX not available");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "PLL lock failed");
        return;
    } else if(!state->hardware.frequency_valid(state->hardware.context, frequency_hz))
        status = "Invalid freq";
    else
        status = state->hardware.tx_allowed(state->hardware.context, frequency_hz) ? "TX allowed" :
                                                                                     "RX only";
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, status);
}

FmtxVfo* fmtx_vfo_alloc(void)
{
    return calloc(1, sizeof(FmtxVfo));
}

void fmtx_vfo_free(FmtxVfo* vfo)
{
    free(vfo);
}

bool fmtx_vfo_frequency_valid(uint32_t frequency_hz)
{
    return mf_radio_frequency_in_vfo(frequency_hz) &&
           hal_frequency_valid(NULL, frequency_hz);
}

uint32_t fmtx_vfo_default_frequency(void)
{
    return hal_default_frequency(NULL);
}

void fmtx_vfo_begin(FmtxVfo* vfo, uint32_t frequency_hz)
{
    MfRadioState* state;
    if(!vfo) return;
    memset(vfo, 0, sizeof(*vfo));
    state = &vfo->state;
    state->hardware.frequency_valid = hal_frequency_valid;
    state->hardware.tx_allowed = hal_tx_allowed;
    state->hardware.default_frequency = hal_default_frequency;
    if(!mf_radio_frequency_in_vfo(frequency_hz) ||
       !state->hardware.frequency_valid(state->hardware.context, frequency_hz)) {
        frequency_hz = state->hardware.default_frequency(state->hardware.context);
        if(!mf_radio_frequency_valid(state, frequency_hz))
            frequency_hz = MF_RADIO_DEFAULT_FREQUENCY_HZ;
    }
    state->snapshot.frequency_hz = frequency_hz;
    state->snapshot.tx_allowed =
        state->hardware.tx_allowed(state->hardware.context, frequency_hz);
    state->edit_khz = frequency_hz / 1000U;
    state->edit_original_hz = frequency_hz;
}

bool fmtx_vfo_input(FmtxVfo* vfo, const InputEvent* event, bool* accepted)
{
    MfRadioState* state;
    if(accepted) *accepted = false;
    if(!vfo || !event) return false;
    state = &vfo->state;
    if(event->key == InputKeyBack &&
       (event->type == InputTypeShort || event->type == InputTypeLong)) {
        mf_radio_commit_frequency(state);
        if(!state->snapshot.tx_allowed) return true;
        if(accepted) *accepted = true;
        return true;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key == InputKeyLeft)
        state->frequency_focus = state->frequency_focus == 0U ?
                                     MF_RADIO_FREQ_DIGITS - 1U :
                                     (uint8_t)(state->frequency_focus - 1U);
    else if(event->key == InputKeyRight)
        state->frequency_focus =
            (uint8_t)((state->frequency_focus + 1U) % MF_RADIO_FREQ_DIGITS);
    else if(event->key == InputKeyUp)
        mf_radio_change_digit(state, 1);
    else if(event->key == InputKeyDown)
        mf_radio_change_digit(state, -1);
    else
        return false;
    return true;
}

void fmtx_vfo_draw(const FmtxVfo* vfo, Canvas* canvas)
{
    if(!vfo || !canvas) return;
    canvas_clear(canvas);
    draw_frequency(&vfo->state, canvas);
}

uint32_t fmtx_vfo_accept(FmtxVfo* vfo)
{
    if(!vfo) return fmtx_vfo_default_frequency();
    mf_radio_commit_frequency(&vfo->state);
    return vfo->state.snapshot.frequency_hz;
}

uint32_t fmtx_vfo_frequency(const FmtxVfo* vfo)
{
    return vfo ? vfo->state.snapshot.frequency_hz : fmtx_vfo_default_frequency();
}
