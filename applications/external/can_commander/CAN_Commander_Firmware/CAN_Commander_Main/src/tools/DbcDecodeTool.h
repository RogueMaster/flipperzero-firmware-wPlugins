#pragma once

#include "ToolBase.h"

namespace tools {

class DbcDecodeTool : public ToolBase {
public:
    DbcDecodeTool();

    core::ToolId id() const override {
        return core::ToolId::DBC_DECODE;
    }
    const char* name() const override {
        return "dbc_decode";
    }

    bool start(const char* args, ToolHost& host) override;
    void stop(ToolHost& host) override;
    void onFrame(const core::CanFrame& frame, ToolHost& host) override;
    void tick(ToolHost& host) override;
    void describe(char* out, size_t out_len) const override;

private:
    core::BusId bus_;
};

} // namespace tools
