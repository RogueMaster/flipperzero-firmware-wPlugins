#include "ValueTrackerTool.h"

#include <stdio.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

ValueTrackerTool::ValueTrackerTool()
    : bus_(core::BusId::BOTH)
    , strict_(true)
    , filter_by_id_(false)
    , filter_id_(0)
    , filter_ext_(false)
    , locked_(false)
    , lock_bus_(core::BusId::CAN0)
    , lock_id_(0)
    , lock_ext_(false)
    , first_(true)
    , last_dlc_(0) {
    memset(last_data_, 0, sizeof(last_data_));
    memset(min_, 0xFF, sizeof(min_));
    memset(max_, 0x00, sizeof(max_));
}

bool ValueTrackerTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::BOTH;
    bool strict = true;
    uint32_t filter_id = 0;
    bool filter_ext = false;
    const bool filter_by_id = core::parse::hasKey(args, "id");
    if(!parseBusArg(args, core::BusId::BOTH, bus) ||
       !core::parse::parseOptBool01(args, "strict", strict) ||
       !core::parse::parseOptU32(args, "id", filter_id, 16) ||
       !core::parse::parseOptBool01(args, "ext", filter_ext) ||
       (filter_by_id && !core::isValidCanId(filter_id, filter_ext))) {
        return false;
    }
    if(!requireBusAvailable(host, id(), bus)) {
        return false;
    }

    bus_ = bus;
    strict_ = strict;
    filter_by_id_ = filter_by_id;
    filter_id_ = filter_id;
    filter_ext_ = filter_ext;
    locked_ = false;
    first_ = true;
    last_dlc_ = 0;
    memset(last_data_, 0, sizeof(last_data_));
    memset(min_, 0xFF, sizeof(min_));
    memset(max_, 0x00, sizeof(max_));
    return true;
}

void ValueTrackerTool::stop(ToolHost& host) {
    (void)host;
}

void ValueTrackerTool::emitMinMax(ToolHost& host, bool emit_min, bool emit_max) {
    char line[128];
    if(emit_min) {
        snprintf(
            line,
            sizeof(line),
            "valtrack min: b0=%u b1=%u b2=%u b3=%u b4=%u b5=%u b6=%u b7=%u",
            min_[0],
            min_[1],
            min_[2],
            min_[3],
            min_[4],
            min_[5],
            min_[6],
            min_[7]);
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, line);
    }
    if(emit_max) {
        snprintf(
            line,
            sizeof(line),
            "valtrack max: b0=%u b1=%u b2=%u b3=%u b4=%u b5=%u b6=%u b7=%u",
            max_[0],
            max_[1],
            max_[2],
            max_[3],
            max_[4],
            max_[5],
            max_[6],
            max_[7]);
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, line);
    }
}

bool ValueTrackerTool::configure(const char* args, ToolHost& host) {
    char show[16] = {0};
    if(!core::parse::parseString(args, "show", show, sizeof(show))) {
        return false;
    }
    if(strcmp(show, "min") == 0) {
        emitMinMax(host, true, false);
        return true;
    }
    if(strcmp(show, "max") == 0) {
        emitMinMax(host, false, true);
        return true;
    }
    if(strcmp(show, "both") == 0) {
        emitMinMax(host, true, true);
        return true;
    }
    return false;
}

// Starts a fresh tracking window from `frame`: last value, min and max all restart.
void ValueTrackerTool::resetTracking(const core::CanFrame& frame) {
    last_dlc_ = frame.dlc;
    memcpy(last_data_, frame.data, sizeof(last_data_));
    memset(min_, 0xFF, sizeof(min_));
    memset(max_, 0x00, sizeof(max_));
    for(uint8_t i = 0; i < frame.dlc; i++) {
        min_[i] = frame.data[i];
        max_[i] = frame.data[i];
    }
    first_ = false;
}

bool ValueTrackerTool::acceptFrame(const core::CanFrame& frame, ToolHost& host) {
    if(!busMatches(bus_, frame.bus) || frame.rtr) {
        return false;
    }
    if(filter_by_id_ && (frame.id != filter_id_ || frame.ext != filter_ext_)) {
        return false;
    }
    if(!filter_by_id_ && !strict_) {
        return true; // legacy: every frame on the bus
    }

    // Lock onto one stream (with bus=both, the same ID on two buses is two streams).
    if(!locked_) {
        locked_ = true;
        lock_bus_ = frame.bus;
        lock_id_ = frame.id;
        lock_ext_ = frame.ext;
        if(!filter_by_id_) {
            char msg[80];
            snprintf(
                msg,
                sizeof(msg),
                "valtrack locked id=0x%lX %s bus=%s (use id= to choose)",
                (unsigned long)frame.id,
                frame.ext ? "ext" : "std",
                core::busName(frame.bus));
            host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
        }
        return true;
    }
    return frame.bus == lock_bus_ && frame.id == lock_id_ && frame.ext == lock_ext_;
}

void ValueTrackerTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    if(!acceptFrame(frame, host)) {
        return;
    }

    if(first_) {
        resetTracking(frame);
        return;
    }

    if(strict_ && frame.dlc != last_dlc_) {
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "ID or DLC changed; tracker reset");
        resetTracking(frame);
        return;
    }

    for(uint8_t i = 0; i < frame.dlc; i++) {
        if(frame.data[i] < min_[i]) {
            min_[i] = frame.data[i];
        }
        if(frame.data[i] > max_[i]) {
            max_[i] = frame.data[i];
        }
    }

    for(uint8_t i = 0; i < frame.dlc; i++) {
        if(last_data_[i] != frame.data[i]) {
            // The Flipper dashboard parses this exact format.
            char msg[128];
            snprintf(
                msg,
                sizeof(msg),
                "byte %u changed 0x%02X(%u)->0x%02X(%u)",
                i,
                last_data_[i],
                last_data_[i],
                frame.data[i],
                frame.data[i]);
            host.emitToolEvent(id(), core::ToolEventCode::DELTA, msg);
            last_data_[i] = frame.data[i];
        }
    }
    last_dlc_ = frame.dlc;
}

void ValueTrackerTool::tick(ToolHost& host) {
    (void)host;
}

void ValueTrackerTool::describe(char* out, size_t out_len) const {
    if(filter_by_id_ || locked_) {
        snprintf(
            out,
            out_len,
            "valtrack bus=%s strict=%u id=0x%lX %s%s",
            core::busName(bus_),
            strict_ ? 1 : 0,
            (unsigned long)(filter_by_id_ ? filter_id_ : lock_id_),
            (filter_by_id_ ? filter_ext_ : lock_ext_) ? "ext" : "std",
            (!filter_by_id_ && locked_) ? " (locked)" : "");
        return;
    }
    snprintf(
        out, out_len, "valtrack bus=%s strict=%u id=any", core::busName(bus_), strict_ ? 1 : 0);
}

} // namespace tools
