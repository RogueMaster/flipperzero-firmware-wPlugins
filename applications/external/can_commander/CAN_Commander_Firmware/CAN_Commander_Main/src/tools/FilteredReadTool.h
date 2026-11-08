#pragma once

#include "ToolBase.h"

namespace tools {

class FilteredReadTool : public ToolBase {
public:
    FilteredReadTool();

    core::ToolId id() const override {
        return core::ToolId::FILTERED;
    }
    const char* name() const override {
        return "filtered";
    }

    bool start(const char* args, ToolHost& host) override;
    void stop(ToolHost& host) override;
    void onFrame(const core::CanFrame& frame, ToolHost& host) override;
    void tick(ToolHost& host) override;
    void describe(char* out, size_t out_len) const override;

private:
    core::BusId bus_;
    uint32_t mask_;
    uint32_t filter_;
    bool ext_match_;
    bool ext_;
    bool ascii_;
};

} // namespace tools
