#include "FilteredReadTool.h"

#include <stdio.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

FilteredReadTool::FilteredReadTool()
    : bus_(core::BusId::BOTH)
    , mask_(0x7FF)
    , filter_(0)
    , ext_match_(false)
    , ext_(false)
    , ascii_(false) {
}

bool FilteredReadTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::BOTH;
    uint32_t mask = 0x7FF;
    uint32_t filter = 0;
    bool ext_match = false;
    bool ext = false;
    bool ascii = false;

    if(!parseBusArg(args, core::BusId::BOTH, bus) ||
       !core::parse::parseOptU32(args, "mask", mask, 16) ||
       !core::parse::parseOptU32(args, "filter", filter, 16) ||
       !core::parse::parseOptBool01(args, "ext_match", ext_match) ||
       !core::parse::parseOptBool01(args, "ext", ext) ||
       !core::parse::parseOptBool01(args, "ascii", ascii)) {
        return false;
    }

    if(!requireBusAvailable(host, id(), bus)) {
        return false;
    }
    bus_ = bus;
    mask_ = mask;
    filter_ = filter;
    ext_match_ = ext_match;
    ext_ = ext;
    ascii_ = ascii;
    return true;
}

void FilteredReadTool::stop(ToolHost& host) {
    (void)host;
}

void FilteredReadTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    if(!busMatches(bus_, frame.bus)) {
        return;
    }
    if(ext_match_ && frame.ext != ext_) {
        return;
    }
    if((frame.id & mask_) != (filter_ & mask_)) {
        return;
    }
    host.emitCanFrameEvent(frame, ascii_);
}

void FilteredReadTool::tick(ToolHost& host) {
    (void)host;
}

void FilteredReadTool::describe(char* out, size_t out_len) const {
    snprintf(
        out,
        out_len,
        "filtered bus=%s mask=0x%lX filter=0x%lX ext_match=%u ext=%u ascii=%u",
        core::busName(bus_),
        (unsigned long)mask_,
        (unsigned long)filter_,
        ext_match_ ? 1 : 0,
        ext_ ? 1 : 0,
        ascii_ ? 1 : 0);
}

} // namespace tools
