#pragma once

#include "BitTrackerTool.h"
#include "CustomInjectTool.h"
#include "DbcDecodeTool.h"
#include "FilteredReadTool.h"
#include "ObdPidTool.h"
#include "ReadAllTool.h"
#include "ReverseTool.h"
#include "SpeedTestTool.h"
#include "ToolBase.h"
#include "UniqueIdsTool.h"
#include "ValueTrackerTool.h"
#include "ReplayTool.h"
#include "WriteTool.h"

namespace tools {

class ToolManager {
public:
    ToolManager();

    bool startById(core::ToolId id, const char* args, ToolHost& host);
    bool startByName(const char* name, const char* args, ToolHost& host, core::ToolId& started_id);
    bool stop(ToolHost& host);
    bool configureActive(const char* args, ToolHost& host);

    void onFrame(const core::CanFrame& frame, ToolHost& host);
    void tick(ToolHost& host);

    bool isActive() const;
    core::ToolId activeId() const;
    const char* activeName() const;
    void describeActive(char* out, size_t out_len) const;

    static core::ToolId parseToolIdName(const char* name);
    static const char* toolName(core::ToolId id);

private:
    ToolBase* find(core::ToolId id);
    const ToolBase* findConst(core::ToolId id) const;

    core::ToolId active_id_;

    ReadAllTool read_all_;
    FilteredReadTool filtered_;
    WriteTool write_;
    SpeedTestTool speed_;
    ValueTrackerTool valtrack_;
    UniqueIdsTool unique_ids_;
    BitTrackerTool bittrack_;
    ReverseTool reverse_;
    ObdPidTool obd_pid_;
    DbcDecodeTool dbc_decode_;
    CustomInjectTool custom_inject_;
    ReplayTool replay_;
};

} // namespace tools
