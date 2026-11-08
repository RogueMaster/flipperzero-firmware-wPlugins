#pragma once

#include "ToolBase.h"

namespace tools {

class SpeedTestTool : public ToolBase {
public:
    SpeedTestTool();

    core::ToolId id() const override {
        return core::ToolId::SPEED;
    }
    const char* name() const override {
        return "speed";
    }

    bool start(const char* args, ToolHost& host) override;
    void stop(ToolHost& host) override;
    void onFrame(const core::CanFrame& frame, ToolHost& host) override;
    void tick(ToolHost& host) override;
    void describe(char* out, size_t out_len) const override;

private:
    core::BusId bus_;
    uint32_t counter_;
    uint32_t last_emit_ms_;
};

} // namespace tools
