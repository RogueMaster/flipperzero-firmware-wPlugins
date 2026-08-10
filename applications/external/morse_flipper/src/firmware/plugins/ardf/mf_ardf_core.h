#pragma once

#include "mf_ardf_api.h"

#define MF_ARDF_SEQUENCE_CAPACITY 64U
#define MF_ARDF_PTT_LEAD_MS       250U
#define MF_ARDF_CWFM_ACQUIRE_MS   250U
#define MF_ARDF_REPEAT_GAP_MS     5000U
#define MF_ARDF_REDRAW_MS         50U

typedef struct {
    uint8_t units;
    bool mark;
} MfArdfSequenceStep;

typedef struct {
    MfArdfSequenceStep steps[MF_ARDF_SEQUENCE_CAPACITY];
    uint8_t count;
    uint32_t duration_ms;
} MfArdfSequence;

typedef struct {
    bool (*frequency_allowed)(void* context, uint32_t frequency_hz, MfArdfModulation modulation);
    bool (*prepare)(void* context, uint32_t frequency_hz, MfArdfModulation modulation);
    bool (*set_mark)(void* context, bool mark);
    void (*stop)(void* context);
    void (*set_p15)(void* context, bool high);
    void (*set_p16)(void* context, bool high);
    void (*set_led)(void* context, bool high);
    bool (*set_clock)(void* context, MfArdfClockTime time);
    void* context;
} MfArdfHardwareOps;

typedef struct {
    MfArdfSnapshot snapshot;
    MfArdfHardwareOps hardware;
    MfArdfSequence sequence;
    uint32_t frequency_hz;
    uint32_t sequence_next_ms;
    uint32_t run_started_ms;
    uint32_t slot_end_ms;
    uint32_t cycle_deadline_ms;
    uint32_t deadline_wall_s;
    uint32_t accepted_edge_ms;
    uint32_t accepted_wall_s;
    uint32_t previous_before_ms;
    uint32_t previous_wall_s;
    uint32_t provisional_deadline_ms;
    uint32_t custom_anchor_wall_s;
    uint32_t next_redraw_ms;
    uint8_t sequence_index;
    MfArdfClockTime live_time;
    MfArdfClockTime draft_time;
    bool entered;
    bool prepared;
    bool calibration_valid;
    bool previous_sample_valid;
    bool sampling;
    bool uncalibrated_join;
    bool provisional_custom;
    bool repeat_gap;
    bool continuous;
    bool wake_pending;
    bool backlight_requested;
    bool backlight_off_pending;
    bool modal;
    bool preamble;
    bool slot_active;
    bool join_active;
    bool redraw_pending;
    void* settings_list;
    void* setting_items[MF_ARDF_SETTING_COUNT];
} MfArdfState;

bool mf_ardf_hardware_ops_valid(const MfArdfHardwareOps* ops);
const char* mf_ardf_identifier_text(MfArdfMessage message);
const char* mf_ardf_identifier_morse(MfArdfMessage message);
uint16_t mf_ardf_wpm_to_dit_ms(uint8_t wpm);
bool mf_ardf_sequence_build(MfArdfSequence* sequence, const char* text, uint8_t wpm);
bool mf_ardf_time_reached(uint32_t now_ms, uint32_t deadline_ms);
uint32_t mf_ardf_countdown_seconds(uint32_t now_ms, uint32_t deadline_ms);
uint32_t mf_ardf_cycle_seconds(MfArdfMode mode);
uint32_t mf_ardf_target_phase(MfArdfMode mode, MfArdfMessage message);
uint32_t mf_ardf_next_cycle_wall_s(uint32_t wall_s, MfArdfMode mode, MfArdfMessage message);
uint32_t mf_ardf_custom_next_wall_s(
    uint32_t anchor_wall_s,
    uint32_t interval_s,
    uint32_t not_before_wall_s);
uint8_t mf_ardf_progress_width(uint32_t now_ms, uint32_t start_ms, uint32_t deadline_ms);

bool mf_ardf_core_enter(
    MfArdfState* state,
    const MfArdfEnterArgs* args,
    const MfArdfHardwareOps* hardware,
    MorseFlipperMappedFalResult* initial);
void mf_ardf_core_leave(MfArdfState* state);
MorseFlipperMappedFalResult
    mf_ardf_core_set_view(MfArdfState* state, MfArdfView view, uint32_t now_ms);
MorseFlipperMappedFalResult mf_ardf_core_activate_run(MfArdfState* state, uint32_t now_ms);
MorseFlipperMappedFalResult
    mf_ardf_core_input(MfArdfState* state, const InputEvent* event, uint32_t now_ms);
MorseFlipperMappedFalResult mf_ardf_core_tick(MfArdfState* state, uint32_t now_ms);
MorseFlipperMappedFalResult mf_ardf_core_text_input_result(
    MfArdfState* state,
    const char* text,
    bool accepted,
    uint32_t now_ms);
MorseFlipperMappedFalResult mf_ardf_core_host_action_result(
    MfArdfState* state,
    MfArdfHostAction action,
    bool accepted,
    uint32_t now_ms);
bool mf_ardf_core_snapshot(const MfArdfState* state, MfArdfSnapshot* snapshot);
void mf_ardf_core_rtc_sample(
    MfArdfState* state,
    MfArdfClockTime time,
    uint32_t sample_before_ms,
    uint32_t sample_after_ms);
