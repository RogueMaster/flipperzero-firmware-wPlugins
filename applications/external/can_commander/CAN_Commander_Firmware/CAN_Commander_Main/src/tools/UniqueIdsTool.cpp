#include "UniqueIdsTool.h"

#include <stdio.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

UniqueIdsTool::UniqueIdsTool()
    : bus_(core::BusId::BOTH)
    , new_found_(false)
    , last_emit_ms_(0)
    , table_full_reported_(false)
    , snapshot_active_(false)
    , snapshot_cursor_(0) {
    memset(ids_, 0, sizeof(ids_));
}

bool UniqueIdsTool::contains(core::BusId bus, uint32_t id, bool ext) const {
    for(size_t i = 0; i < (sizeof(ids_) / sizeof(ids_[0])); i++) {
        if(ids_[i].used && ids_[i].id == id && ids_[i].ext == ext && ids_[i].bus == bus) {
            return true;
        }
    }
    return false;
}

bool UniqueIdsTool::insert(core::BusId bus, uint32_t id, bool ext) {
    if(contains(bus, id, ext)) {
        return true;
    }
    for(size_t i = 0; i < (sizeof(ids_) / sizeof(ids_[0])); i++) {
        if(!ids_[i].used) {
            ids_[i].used = true;
            ids_[i].id = id;
            ids_[i].bus = bus;
            ids_[i].ext = ext;
            return true;
        }
    }
    return false;
}

bool UniqueIdsTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::BOTH;
    if(!parseBusArg(args, core::BusId::BOTH, bus) || !requireBusAvailable(host, id(), bus)) {
        return false;
    }
    bus_ = bus;
    memset(ids_, 0, sizeof(ids_));
    new_found_ = false;
    last_emit_ms_ = 0;
    table_full_reported_ = false;
    snapshot_active_ = false;
    snapshot_cursor_ = 0;
    return true;
}

void UniqueIdsTool::stop(ToolHost& host) {
    (void)host;
}

void UniqueIdsTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    if(!busMatches(bus_, frame.bus)) {
        return;
    }

    if(!contains(frame.bus, frame.id, frame.ext)) {
        if(!insert(frame.bus, frame.id, frame.ext)) {
            if(!table_full_reported_) {
                table_full_reported_ = true;
                host.emitToolEvent(
                    id(),
                    core::ToolEventCode::ERROR,
                    "unique_ids table full; further new IDs not tracked");
            }
            return;
        }
        char msg[80];
        snprintf(
            msg,
            sizeof(msg),
            "new id bus=%s id=0x%lX %s",
            core::busName(frame.bus),
            (unsigned long)frame.id,
            frame.ext ? "ext" : "std");
        host.emitToolEvent(id(), core::ToolEventCode::DELTA, msg);
        new_found_ = true;
    }
}

// Emits the snapshot a few lines per tick, only while the output queues have room,
// so a full 512-entry table is never cut short by queue overflow.
void UniqueIdsTool::continueSnapshot(ToolHost& host) {
    const uint16_t total = (uint16_t)(sizeof(ids_) / sizeof(ids_[0]));
    while(snapshot_cursor_ < total) {
        if(host.outputHeadroom() <= kOutputReserve) {
            return;
        }
        const IdEntry& e = ids_[snapshot_cursor_++];
        if(!e.used) {
            continue;
        }
        char msg[64];
        snprintf(
            msg,
            sizeof(msg),
            "id=0x%lX %s bus=%s",
            (unsigned long)e.id,
            e.ext ? "ext" : "std",
            core::busName(e.bus));
        host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    }
    snapshot_active_ = false;
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "unique_id snapshot end");
}

void UniqueIdsTool::tick(ToolHost& host) {
    if(snapshot_active_) {
        continueSnapshot(host);
    }

    const uint32_t now = host.nowMs();
    if(!new_found_) {
        return;
    }
    if((uint32_t)(now - last_emit_ms_) < 5000) {
        return;
    }

    uint32_t count = 0;
    for(size_t i = 0; i < (sizeof(ids_) / sizeof(ids_[0])); i++) {
        if(ids_[i].used) {
            count++;
        }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "unique ids count=%lu", (unsigned long)count);
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
    new_found_ = false;
    last_emit_ms_ = now;
}

bool UniqueIdsTool::configure(const char* args, ToolHost& host) {
    uint32_t snapshot = 0;
    if(core::parse::parseU32(args, "snapshot", snapshot, 10) && snapshot == 1) {
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "unique_id snapshot start");
        snapshot_active_ = true;
        snapshot_cursor_ = 0;
        continueSnapshot(host);
        return true;
    }
    return false;
}

void UniqueIdsTool::describe(char* out, size_t out_len) const {
    snprintf(out, out_len, "unique_ids bus=%s", core::busName(bus_));
}

} // namespace tools
