#pragma once

#include "ToolBase.h"

namespace tools {

// Tracks byte changes and min/max for one frame stream.
//
// The stream is chosen by `id=`/`ext=` when given. Without an ID, strict mode
// (default) locks onto the first frame seen; strict=0 compares every frame with the
// previous one regardless of ID (legacy behavior, useful behind a bus filter).
class ValueTrackerTool : public ToolBase {
public:
    ValueTrackerTool();

    core::ToolId id() const override {
        return core::ToolId::VALTRACK;
    }
    const char* name() const override {
        return "valtrack";
    }

    bool start(const char* args, ToolHost& host) override;
    void stop(ToolHost& host) override;
    void onFrame(const core::CanFrame& frame, ToolHost& host) override;
    void tick(ToolHost& host) override;
    bool configure(const char* args, ToolHost& host) override;
    void describe(char* out, size_t out_len) const override;

private:
    void emitMinMax(ToolHost& host, bool emit_min, bool emit_max);
    void resetTracking(const core::CanFrame& frame);
    bool acceptFrame(const core::CanFrame& frame, ToolHost& host);

    core::BusId bus_;
    bool strict_;
    bool filter_by_id_;
    uint32_t filter_id_;
    bool filter_ext_;

    // Locked stream (bus + ID + format) once the first matching frame arrives.
    bool locked_;
    core::BusId lock_bus_;
    uint32_t lock_id_;
    bool lock_ext_;

    bool first_;
    uint8_t last_dlc_;
    uint8_t last_data_[8];
    uint8_t min_[8];
    uint8_t max_[8];
};

} // namespace tools
