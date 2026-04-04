#pragma once

#include "../libraries/mcp_can_2515.h"
#include <stdbool.h>
#include <stdint.h>

#define CAN_ID_STW_ACTN_RQ    0x045  // 69 - steering wheel stalk (Legacy follow distance)
#define CAN_ID_AP_LEGACY      0x3EE  // 1006 - autopilot control (Legacy)
#define CAN_ID_ISA_SPEED      0x399  // 921 - ISA speed chime (HW4)
#define CAN_ID_GTW_CAR_CONFIG 0x398  // 920 - HW version detection
#define CAN_ID_FOLLOW_DIST    0x3F8  // 1016 - follow distance / speed profile
#define CAN_ID_AP_CONTROL     0x3FD  // 1021 - autopilot control (HW3/HW4)

typedef enum {
    TeslaHW_Unknown = 0,
    TeslaHW_Legacy,
    TeslaHW_HW3,
    TeslaHW_HW4,
} TeslaHWVersion;

typedef struct {
    TeslaHWVersion hw_version;
    int speed_profile;
    int speed_offset;
    bool fsd_enabled;
    bool nag_suppressed;
    uint32_t frames_modified;

    bool force_fsd;
    bool suppress_speed_chime;
    bool emergency_vehicle_detect;
} FSDState;

void fsd_state_init(FSDState* state, TeslaHWVersion hw);
void fsd_set_bit(CANFRAME* frame, int bit, bool value);
uint8_t fsd_read_mux_id(const CANFRAME* frame);
bool fsd_is_selected_in_ui(const CANFRAME* frame, bool force_fsd);
TeslaHWVersion fsd_detect_hw_version(const CANFRAME* frame);

void fsd_handle_follow_distance(FSDState* state, const CANFRAME* frame);
bool fsd_handle_autopilot_frame(FSDState* state, CANFRAME* frame);

void fsd_handle_legacy_stalk(FSDState* state, const CANFRAME* frame);
bool fsd_handle_legacy_autopilot(FSDState* state, CANFRAME* frame);
bool fsd_handle_isa_speed_chime(CANFRAME* frame);
