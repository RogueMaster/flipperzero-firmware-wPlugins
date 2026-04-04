#include "fsd_handler.h"
#include <string.h>

void fsd_state_init(FSDState* state, TeslaHWVersion hw) {
    memset(state, 0, sizeof(FSDState));
    state->hw_version = hw;
    if(hw == TeslaHW_HW4)
        state->speed_profile = 4;
    else if(hw == TeslaHW_Legacy)
        state->speed_profile = 1;
    else
        state->speed_profile = 2;
}

void fsd_set_bit(CANFRAME* frame, int bit, bool value) {
    if(bit < 0 || bit >= 64) return;
    int byte_idx = bit / 8;
    int bit_idx = bit % 8;
    uint8_t mask = (uint8_t)(1U << bit_idx);
    if(value) {
        frame->buffer[byte_idx] |= mask;
    } else {
        frame->buffer[byte_idx] &= (uint8_t)(~mask);
    }
}

uint8_t fsd_read_mux_id(const CANFRAME* frame) {
    return frame->buffer[0] & 0x07;
}

bool fsd_is_selected_in_ui(const CANFRAME* frame, bool force_fsd) {
    if(force_fsd) return true;
    if(frame->data_lenght < 5) return false;
    return (frame->buffer[4] >> 6) & 0x01;
}

TeslaHWVersion fsd_detect_hw_version(const CANFRAME* frame) {
    if(frame->canId != CAN_ID_GTW_CAR_CONFIG) return TeslaHW_Unknown;
    uint8_t das_hw = (frame->buffer[0] >> 6) & 0x03;
    switch(das_hw) {
    case 2:  return TeslaHW_HW3;
    case 3:  return TeslaHW_HW4;
    default: return TeslaHW_Unknown;
    }
}

// --- HW3/HW4 handlers ---

void fsd_handle_follow_distance(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 6) return;
    uint8_t fd = (frame->buffer[5] & 0xE0) >> 5;

    if(state->hw_version == TeslaHW_HW3) {
        switch(fd) {
        case 1: state->speed_profile = 2; break;
        case 2: state->speed_profile = 1; break;
        case 3: state->speed_profile = 0; break;
        default: break;
        }
    } else {
        switch(fd) {
        case 1: state->speed_profile = 3; break;
        case 2: state->speed_profile = 2; break;
        case 3: state->speed_profile = 1; break;
        case 4: state->speed_profile = 0; break;
        case 5: state->speed_profile = 4; break;
        default: break;
        }
    }
}

bool fsd_handle_autopilot_frame(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    uint8_t mux = fsd_read_mux_id(frame);
    bool fsd_ui = fsd_is_selected_in_ui(frame, state->force_fsd);
    bool modified = false;

    if(mux == 0) state->fsd_enabled = fsd_ui;

    if(state->hw_version == TeslaHW_HW3) {
        if(mux == 0 && state->fsd_enabled) {
            int raw = (int)((frame->buffer[3] >> 1) & 0x3F) - 30;
            int offset = raw * 5;
            if(offset < 0) offset = 0;
            if(offset > 100) offset = 100;
            state->speed_offset = offset;

            fsd_set_bit(frame, 46, true);
            frame->buffer[6] &= ~0x06;
            frame->buffer[6] |= (uint8_t)((state->speed_profile & 0x03) << 1);
            modified = true;
        }
        if(mux == 1) {
            fsd_set_bit(frame, 19, false);
            state->nag_suppressed = true;
            modified = true;
        }
        if(mux == 2 && state->fsd_enabled) {
            frame->buffer[0] &= ~0xC0;
            frame->buffer[1] &= ~0x3F;
            frame->buffer[0] |= (uint8_t)((state->speed_offset & 0x03) << 6);
            frame->buffer[1] |= (uint8_t)(state->speed_offset >> 2);
            modified = true;
        }
    } else {
        // HW4
        if(mux == 0 && state->fsd_enabled) {
            fsd_set_bit(frame, 46, true);
            fsd_set_bit(frame, 60, true);
            if(state->emergency_vehicle_detect) {
                fsd_set_bit(frame, 59, true);
            }
            modified = true;
        }
        if(mux == 1) {
            fsd_set_bit(frame, 19, false);
            fsd_set_bit(frame, 47, true);
            state->nag_suppressed = true;
            modified = true;
        }
        if(mux == 2) {
            frame->buffer[7] &= ~(0x07 << 4);
            frame->buffer[7] |= (uint8_t)((state->speed_profile & 0x07) << 4);
            modified = true;
        }
    }

    if(modified) state->frames_modified++;
    return modified;
}

// --- Legacy handler ---

void fsd_handle_legacy_stalk(FSDState* state, const CANFRAME* frame) {
    if(frame->data_lenght < 2) return;
    uint8_t pos = frame->buffer[1] >> 5;
    if(pos <= 1)
        state->speed_profile = 2;
    else if(pos == 2)
        state->speed_profile = 1;
    else
        state->speed_profile = 0;
}

bool fsd_handle_legacy_autopilot(FSDState* state, CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    uint8_t mux = fsd_read_mux_id(frame);
    bool fsd_ui = fsd_is_selected_in_ui(frame, state->force_fsd);
    bool modified = false;

    if(mux == 0) state->fsd_enabled = fsd_ui;

    if(mux == 0 && state->fsd_enabled) {
        fsd_set_bit(frame, 46, true);
        frame->buffer[6] &= ~0x06;
        frame->buffer[6] |= (uint8_t)((state->speed_profile & 0x03) << 1);
        modified = true;
    }
    if(mux == 1) {
        fsd_set_bit(frame, 19, false);
        state->nag_suppressed = true;
        modified = true;
    }

    if(modified) state->frames_modified++;
    return modified;
}

// --- ISA speed chime suppression ---

bool fsd_handle_isa_speed_chime(CANFRAME* frame) {
    if(frame->data_lenght < 8) return false;
    frame->buffer[1] |= 0x20;
    // recalculate checksum: sum bytes 0-6 + CAN ID bytes
    uint8_t sum = 0;
    for(int i = 0; i < 7; i++)
        sum += frame->buffer[i];
    sum += (CAN_ID_ISA_SPEED & 0xFF) + (CAN_ID_ISA_SPEED >> 8);
    frame->buffer[7] = sum & 0xFF;
    return true;
}
