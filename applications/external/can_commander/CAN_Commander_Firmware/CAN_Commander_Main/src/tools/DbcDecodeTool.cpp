#include "DbcDecodeTool.h"

#include <stdio.h>

#include "ToolUtils.h"
#include "../decode/SignalDecode.h"

namespace tools {

DbcDecodeTool::DbcDecodeTool()
    : bus_(core::BusId::BOTH) {
}

bool DbcDecodeTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::BOTH;
    if(!parseBusArg(args, core::BusId::BOTH, bus) || !requireBusAvailable(host, id(), bus)) {
        return false;
    }
    bus_ = bus;
    return true;
}

void DbcDecodeTool::stop(ToolHost& host) {
    (void)host;
}

void DbcDecodeTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    if(!busMatches(bus_, frame.bus) || frame.rtr) {
        return;
    }

    // Every stored descriptor for this ID is considered; mux selection happens in
    // decodeSignal, so many mux variants of one ID all remain reachable.
    const core::DbcSignalDef* matches[BoardConfig::MAX_DBC_SIGNALS] = {0};
    const uint16_t found =
        host.dbcStore().findMatches(frame, matches, BoardConfig::MAX_DBC_SIGNALS);
    const uint16_t limit = found > BoardConfig::MAX_DBC_SIGNALS ? BoardConfig::MAX_DBC_SIGNALS :
                                                                  found;

    for(uint16_t i = 0; i < limit; i++) {
        core::DbcDecodedValue value{};
        if(decode::decodeSignal(frame, *matches[i], value)) {
            host.emitDbcDecodeEvent(value);
        }
    }
}

void DbcDecodeTool::tick(ToolHost& host) {
    (void)host;
}

void DbcDecodeTool::describe(char* out, size_t out_len) const {
    snprintf(out, out_len, "dbc_decode bus=%s", core::busName(bus_));
}

} // namespace tools
