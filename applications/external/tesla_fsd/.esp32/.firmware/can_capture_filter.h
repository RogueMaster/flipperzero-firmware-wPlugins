#pragma once

#include "can_driver.h"
#include <string.h>

static constexpr uint8_t CAN_CAPTURE_MAX_IDS = 6;

inline const char *can_capture_mode_label(uint8_t count, uint8_t buses, bool scoped,
                                          CanBusId selected, const char *const *modes) {
    bool prefilter = false;
    bool software = false;
    for (uint8_t i = 0; i < buses && i < CAN_BUS_COUNT; i++) {
        if (scoped && i != selected) continue;
        if (strcmp(modes[i], "down") == 0) return "unavailable";
        if (strcmp(modes[i], "unknown") == 0) return "unknown";
        prefilter |= strcmp(modes[i], "prefilter") == 0;
        software |= strcmp(modes[i], "accept-all") == 0;
    }
    if (scoped && selected >= buses) return "unavailable";
    if (count == 0) return "all-id-decimated";
    if (software) return "software-filter";
    if (prefilter) return "multi-id-hwprefilter";
    return count == 1 ? "single-id-hwfilter" : "multi-id-hwfilter";
}

struct CanTwaiPrefilter {
    uint32_t code;
    uint32_t mask;
};

// TWAI mask bits set to one are don't-care. Retain only common ID bits;
// the stream's software filter enforces the exact requested list.
inline CanTwaiPrefilter can_capture_twai_prefilter(const uint32_t *ids, uint8_t count) {
    if (!ids || count == 0) return {0, 0xFFFFFFFFu};
    if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
    uint32_t ones = ids[0] & 0x7FFu;
    uint32_t zeros = (~ids[0]) & 0x7FFu;
    for (uint8_t i = 1; i < count; i++) {
        ones &= ids[i] & 0x7FFu;
        zeros &= (~ids[i]) & 0x7FFu;
    }
    return {ones << 21, ((~(ones | zeros) & 0x7FFu) << 21) | 0x001FFFFFu};
}

// Shared by the real MCP2515 driver and register-call tests. Restore the
// library reset() filter layout without resetting bitrate or controller state.
template <typename Controller>
bool can_capture_program_mcp_filters(Controller &mcp, const uint32_t *ids,
                                     uint8_t count, bool listen_only) {
    if (!ids) count = 0;
    if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
    bool capture = count != 0;
    bool ok = true;
    uint32_t mask = capture ? 0x7FFu : 0;
    ok &= mcp.setFilterMask(Controller::MASK0, !capture, mask) == Controller::ERROR_OK;
    ok &= mcp.setFilterMask(Controller::MASK1, !capture, mask) == Controller::ERROR_OK;
    const typename Controller::RXF filters[] = {
        Controller::RXF0, Controller::RXF1, Controller::RXF2,
        Controller::RXF3, Controller::RXF4, Controller::RXF5};
    for (uint8_t i = 0; i < CAN_CAPTURE_MAX_IDS; i++) {
        uint32_t id = capture ? ids[i < count ? i : 0] & 0x7FFu : 0;
        ok &= mcp.setFilter(filters[i], !capture && i == 1, id) == Controller::ERROR_OK;
    }
    ok &= (listen_only ? mcp.setListenOnlyMode() : mcp.setNormalMode()) == Controller::ERROR_OK;
    return ok;
}

class CanCaptureFilterSync {
    bool initialized_ = false;
    OpMode mode_ = OpMode_ListenOnly;
    bool available_[CAN_BUS_COUNT] = {};
    uint8_t counts_[CAN_BUS_COUNT] = {};
    uint32_t ids_[CAN_BUS_COUNT][CAN_CAPTURE_MAX_IDS] = {};

public:
    void update(CanDriver *const *drivers, const bool *available, uint8_t buses,
                OpMode mode, bool streaming, const uint32_t *ids, uint8_t count,
                bool scoped, CanBusId selected) {
        for (uint8_t i = 0; i < buses && i < CAN_BUS_COUNT; i++) {
            bool ready = available[i] && drivers[i];
            bool capture = mode == OpMode_ListenOnly && streaming && ids &&
                           count > 0 && count <= CAN_CAPTURE_MAX_IDS &&
                           (!scoped || i == selected);
            uint8_t target_count = capture ? count : 0;
            bool changed = !initialized_ || mode_ != mode || available_[i] != ready ||
                           counts_[i] != target_count;
            for (uint8_t j = 0; !changed && j < target_count; j++) {
                changed = ids_[i][j] != (ids[j] & 0x7FFu);
            }
            if (!changed) continue;
            if (ready) drivers[i]->setAcceptanceFilters(capture ? ids : nullptr, target_count);
            available_[i] = ready;
            counts_[i] = target_count;
            for (uint8_t j = 0; j < CAN_CAPTURE_MAX_IDS; j++) {
                ids_[i][j] = j < target_count ? ids[j] & 0x7FFu : 0;
            }
        }
        initialized_ = true;
        mode_ = mode;
    }
};
