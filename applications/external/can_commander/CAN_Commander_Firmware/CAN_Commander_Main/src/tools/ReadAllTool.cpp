#include "ReadAllTool.h"

#include <stdio.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

ReadAllTool::ReadAllTool()
    : bus_(core::BusId::BOTH)
    , ascii_(false) {
}

bool ReadAllTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::BOTH;
    bool ascii = false;
    if(!parseBusArg(args, core::BusId::BOTH, bus) ||
       !core::parse::parseOptBool01(args, "ascii", ascii)) {
        return false;
    }
    if(!requireBusAvailable(host, id(), bus)) {
        return false;
    }
    bus_ = bus;
    ascii_ = ascii;
    return true;
}

void ReadAllTool::stop(ToolHost& host) {
    (void)host;
}

void ReadAllTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    if(!busMatches(bus_, frame.bus)) {
        return;
    }
    host.emitCanFrameEvent(frame, ascii_);
}

void ReadAllTool::tick(ToolHost& host) {
    (void)host;
}

void ReadAllTool::describe(char* out, size_t out_len) const {
    snprintf(out, out_len, "read_all bus=%s ascii=%u", core::busName(bus_), ascii_ ? 1 : 0);
}

} // namespace tools
