#include "CustomInjectTool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

static bool isValidFieldRange(uint32_t start_bit, uint32_t bit_len) {
    if(bit_len == 0 || bit_len > 64 || start_bit > 63) {
        return false;
    }
    return (start_bit + bit_len) <= 64;
}

CustomInjectTool::CustomInjectTool()
    : default_bus_(core::BusId::CAN0) {
    memset(slots_, 0, sizeof(slots_));
    memset(&pending_, 0, sizeof(pending_));
}

bool CustomInjectTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::CAN0;
    if(!parseBusArg(args, core::BusId::CAN0, bus) || !requireBusAvailable(host, id(), bus)) {
        return false;
    }
    default_bus_ = bus;
    memset(slots_, 0, sizeof(slots_));
    memset(&pending_, 0, sizeof(pending_));
    return true;
}

void CustomInjectTool::stop(ToolHost& host) {
    (void)host;
    pending_.active = false;
}

bool CustomInjectTool::parseSlot(const char* args, uint8_t& slot_idx) const {
    uint32_t slot = 0;
    if(!core::parse::parseU32(args, "slot", slot, 10)) {
        return false;
    }
    if(slot < 1 || slot > kMaxSignals) {
        return false;
    }
    slot_idx = (uint8_t)(slot - 1);
    return true;
}

bool CustomInjectTool::parseU64Arg(const char* args, const char* key, uint64_t& out, bool& present)
    const {
    present = false;
    if(!core::parse::hasKey(args, key)) {
        return true;
    }
    uint64_t parsed = 0;
    if(!core::parse::parseU64(args, key, parsed, 0)) {
        return false;
    }
    out = parsed;
    present = true;
    return true;
}

bool CustomInjectTool::parseBytesArg(
    const char* args,
    const char* key,
    uint8_t out[8],
    bool& present) const {
    present = false;
    if(!core::parse::hasKey(args, key)) {
        return true;
    }

    char hex[17] = {0};
    if(!core::parse::parseString(args, key, hex, sizeof(hex))) {
        return false;
    }

    uint8_t parsed[8] = {0};
    uint8_t parsed_len = 0;
    if(!core::parse::parseHexBytes(hex, parsed, 8, parsed_len)) {
        return false;
    }

    memcpy(out, parsed, 8);
    present = true;
    return true;
}

bool CustomInjectTool::parseNameArg(const char* args, char* out, size_t out_len, bool& present)
    const {
    present = false;
    if(out == nullptr || out_len == 0) {
        return false;
    }
    out[0] = '\0';

    // Names are display-only labels, so an over-long name is shortened rather than rejected.
    present = core::parse::parseStringTruncate(args, "name", out, out_len);
    return true;
}

bool CustomInjectTool::parseFieldSpec(
    const char* args,
    const char* start_key,
    const char* len_key,
    const char* value_key,
    uint8_t& start_bit,
    uint8_t& bit_len,
    uint64_t& value,
    bool& present) const {
    present = false;

    const bool start_present = core::parse::hasKey(args, start_key);
    const bool len_present = core::parse::hasKey(args, len_key);
    const bool value_present = core::parse::hasKey(args, value_key);

    if(!start_present && !len_present && !value_present) {
        return true;
    }
    if(!start_present || !len_present || !value_present) {
        return false;
    }

    uint32_t start_u32 = 0;
    uint32_t len_u32 = 0;
    uint64_t value_u64 = 0;
    if(!core::parse::parseU32(args, start_key, start_u32, 10) ||
       !core::parse::parseU32(args, len_key, len_u32, 10) ||
       !core::parse::parseU64(args, value_key, value_u64, 0)) {
        return false;
    }
    if(!isValidFieldRange(start_u32, len_u32)) {
        return false;
    }

    if(len_u32 < 64) {
        const uint64_t max_value = (1ULL << len_u32) - 1ULL;
        if(value_u64 > max_value) {
            return false;
        }
    }

    start_bit = (uint8_t)start_u32;
    bit_len = (uint8_t)len_u32;
    value = value_u64;
    present = true;
    return true;
}

bool CustomInjectTool::applyFieldToMaskValue(
    SignalSlot& slot,
    uint8_t start_bit,
    uint8_t bit_len,
    uint64_t value) const {
    if(!isValidFieldRange(start_bit, bit_len)) {
        return false;
    }

    for(uint8_t i = 0; i < bit_len; i++) {
        const uint8_t bit = (uint8_t)(start_bit + i);
        const uint8_t byte_index = (uint8_t)(bit / 8);
        const uint8_t bit_index = (uint8_t)(bit % 8);
        const uint8_t bit_mask = (uint8_t)(1u << bit_index);

        slot.mask[byte_index] |= bit_mask;
        if((value & (1ULL << i)) != 0ULL) {
            slot.value[byte_index] |= bit_mask;
        } else {
            slot.value[byte_index] &= (uint8_t)(~bit_mask);
        }
    }
    return true;
}

uint64_t
    CustomInjectTool::extractFieldLe(const uint8_t data[8], uint8_t start_bit, uint8_t bit_len) {
    if(data == nullptr || !isValidFieldRange(start_bit, bit_len)) {
        return 0;
    }

    uint64_t out = 0;
    for(uint8_t i = 0; i < bit_len; i++) {
        const uint8_t bit = (uint8_t)(start_bit + i);
        const uint8_t byte_index = (uint8_t)(bit / 8);
        const uint8_t bit_index = (uint8_t)(bit % 8);
        const uint8_t bit_mask = (uint8_t)(1u << bit_index);
        if((data[byte_index] & bit_mask) != 0) {
            out |= (1ULL << i);
        }
    }
    return out;
}

bool CustomInjectTool::muxMatches(const SignalSlot& slot, const core::CanFrame& frame) {
    if(!slot.mux_enabled) {
        return true;
    }
    // The mux field must be inside the bytes the frame actually carries; bytes past
    // the DLC are zero-filled storage, not received data.
    if((uint16_t)slot.mux_start + (uint16_t)slot.mux_len > (uint16_t)frame.dlc * 8U) {
        return false;
    }
    return extractFieldLe(frame.data, slot.mux_start, slot.mux_len) == slot.mux_value;
}

void CustomInjectTool::cancelPendingForSlot(uint8_t slot_idx, const char* reason, ToolHost& host) {
    if(!pending_.active || pending_.slot != slot_idx) {
        return;
    }
    pending_.active = false;
    char msg[96];
    snprintf(msg, sizeof(msg), "pending injection canceled (%s)", reason);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
}

void CustomInjectTool::clearSlot(uint8_t slot_idx) {
    if(slot_idx >= kMaxSignals) {
        return;
    }
    memset(&slots_[slot_idx], 0, sizeof(slots_[slot_idx]));
}

uint8_t CustomInjectTool::usedCount() const {
    uint8_t count = 0;
    for(uint8_t i = 0; i < kMaxSignals; i++) {
        if(slots_[i].used) {
            count++;
        }
    }
    return count;
}

static void
    formatMux(char* out, size_t out_len, bool enabled, uint8_t start, uint8_t len, uint64_t value) {
    if(!enabled) {
        snprintf(out, out_len, "off");
        return;
    }
    snprintf(
        out,
        out_len,
        "[start=%u len=%u val=0x%llX]",
        (unsigned)start,
        (unsigned)len,
        (unsigned long long)value);
}

bool CustomInjectTool::handleAdd(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }

    // Build the new slot completely before touching the live one.
    SignalSlot slot;
    memset(&slot, 0, sizeof(slot));
    slot.used = true;

    if(!parseBusArg(args, default_bus_, slot.bus) ||
       !core::parse::parseU32(args, "id", slot.id, 16) ||
       !core::parse::parseOptBool01(args, "ext", slot.ext) ||
       !core::isValidCanId(slot.id, slot.ext) || !requireBusAvailable(host, id(), slot.bus)) {
        return false;
    }

    bool name_present = false;
    if(!parseNameArg(args, slot.name, sizeof(slot.name), name_present)) {
        return false;
    }

    bool mask_present = false;
    bool value_present = false;
    bool xor_present = false;
    if(!parseBytesArg(args, "mask", slot.mask, mask_present) ||
       !parseBytesArg(args, "value", slot.value, value_present) ||
       !parseBytesArg(args, "xor", slot.xor_mask, xor_present)) {
        return false;
    }

    bool mux_present = false;
    if(!parseFieldSpec(
           args,
           "mux_start",
           "mux_len",
           "mux_value",
           slot.mux_start,
           slot.mux_len,
           slot.mux_value,
           mux_present)) {
        return false;
    }
    slot.mux_enabled = mux_present;

    uint8_t sig_start = 0;
    uint8_t sig_len = 0;
    uint64_t sig_value = 0;
    bool sig_present = false;
    if(!parseFieldSpec(
           args, "sig_start", "sig_len", "sig_value", sig_start, sig_len, sig_value, sig_present)) {
        return false;
    }
    if(sig_present && !applyFieldToMaskValue(slot, sig_start, sig_len, sig_value)) {
        return false;
    }

    // Replacing a slot must not let an already scheduled sequence transmit the new frame.
    cancelPendingForSlot(slot_idx, "slot replaced", host);
    slots_[slot_idx] = slot;

    // Keep "slot=<n> tracking bus=... id=0x... <std|ext>" first: the Flipper dashboard
    // parses that prefix.
    char mux_text[48];
    formatMux(
        mux_text, sizeof(mux_text), slot.mux_enabled, slot.mux_start, slot.mux_len, slot.mux_value);
    char msg[200];
    snprintf(
        msg,
        sizeof(msg),
        "slot=%u tracking bus=%s id=0x%lX %s mux=%s name=%s",
        (unsigned)(slot_idx + 1),
        core::busName(slot.bus),
        (unsigned long)slot.id,
        slot.ext ? "ext" : "std",
        mux_text,
        slot.name[0] != '\0' ? slot.name : "-");
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleRemove(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }
    if(!slots_[slot_idx].used) {
        return false;
    }

    clearSlot(slot_idx);
    if(pending_.active && pending_.slot == slot_idx) {
        pending_.active = false;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "slot=%u removed", (unsigned)(slot_idx + 1));
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleClear(ToolHost& host) {
    memset(slots_, 0, sizeof(slots_));
    pending_.active = false;
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "all slots cleared");
    return true;
}

bool CustomInjectTool::handleList(ToolHost& host) const {
    char summary[64];
    snprintf(summary, sizeof(summary), "custom_inject slots_used=%u", (unsigned)usedCount());
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, summary);

    for(uint8_t i = 0; i < kMaxSignals; i++) {
        const SignalSlot& slot = slots_[i];
        if(!slot.used) {
            continue;
        }
        // Keep "slot=<n> bus=... id=0x... <std|ext> latest=<0|1>" first: the Flipper
        // dashboard parses that prefix.
        char mux_text[48];
        formatMux(
            mux_text,
            sizeof(mux_text),
            slot.mux_enabled,
            slot.mux_start,
            slot.mux_len,
            slot.mux_value);
        char msg[224];
        snprintf(
            msg,
            sizeof(msg),
            "slot=%u bus=%s id=0x%lX %s latest=%u mux=%s name=%s",
            (unsigned)(i + 1),
            core::busName(slot.bus),
            (unsigned long)slot.id,
            slot.ext ? "ext" : "std",
            slot.have_latest ? 1 : 0,
            mux_text,
            slot.name[0] != '\0' ? slot.name : "-");
        host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    }

    if(pending_.active) {
        char msg[96];
        snprintf(
            msg,
            sizeof(msg),
            "pending slot=%u remaining=%lu interval_ms=%lu",
            (unsigned)(pending_.slot + 1),
            (unsigned long)pending_.remaining,
            (unsigned long)pending_.interval_ms);
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
    }

    return true;
}

bool CustomInjectTool::handleModify(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }
    const SignalSlot& current = slots_[slot_idx];
    if(!current.used) {
        return false;
    }

    // Apply every change to a copy; commit only if the whole request is valid.
    SignalSlot next = current;

    bool name_present = false;
    char slot_name[kSlotNameMax] = {0};
    if(!parseNameArg(args, slot_name, sizeof(slot_name), name_present)) {
        return false;
    }
    if(name_present) {
        strncpy(next.name, slot_name, sizeof(next.name) - 1);
        next.name[sizeof(next.name) - 1] = '\0';
    }

    const bool name_clear_present = core::parse::hasKey(args, "name_clear");
    bool name_clear = false;
    if(!core::parse::parseOptBool01(args, "name_clear", name_clear)) {
        return false;
    }
    if(name_clear) {
        next.name[0] = '\0';
    }

    bool mask_present = false;
    bool value_present = false;
    bool xor_present = false;
    if(!parseBytesArg(args, "mask", next.mask, mask_present) ||
       !parseBytesArg(args, "value", next.value, value_present) ||
       !parseBytesArg(args, "xor", next.xor_mask, xor_present)) {
        return false;
    }

    const bool mux_toggle_present = core::parse::hasKey(args, "mux");
    bool mux_on = true;
    if(!core::parse::parseOptBool01(args, "mux", mux_on)) {
        return false;
    }
    if(mux_toggle_present && !mux_on) {
        next.mux_enabled = false;
        next.mux_start = 0;
        next.mux_len = 0;
        next.mux_value = 0;
    }

    uint8_t mux_start = 0;
    uint8_t mux_len = 0;
    uint64_t mux_value = 0;
    bool mux_present = false;
    if(!parseFieldSpec(
           args, "mux_start", "mux_len", "mux_value", mux_start, mux_len, mux_value, mux_present)) {
        return false;
    }
    if(mux_present) {
        next.mux_enabled = true;
        next.mux_start = mux_start;
        next.mux_len = mux_len;
        next.mux_value = mux_value;
    }

    uint8_t sig_start = 0;
    uint8_t sig_len = 0;
    uint64_t sig_value = 0;
    bool sig_present = false;
    if(!parseFieldSpec(
           args, "sig_start", "sig_len", "sig_value", sig_start, sig_len, sig_value, sig_present)) {
        return false;
    }
    if(sig_present && !applyFieldToMaskValue(next, sig_start, sig_len, sig_value)) {
        return false;
    }

    if(!mask_present && !value_present && !xor_present && !mux_present && !sig_present &&
       !mux_toggle_present && !name_present && !name_clear_present) {
        return false;
    }

    // A different mux rule selects different frames, so the cached frame no longer
    // qualifies. Drop it and wait for the next frame that matches the new rule.
    const bool mux_changed = next.mux_enabled != current.mux_enabled ||
                             (next.mux_enabled && (next.mux_start != current.mux_start ||
                                                   next.mux_len != current.mux_len ||
                                                   next.mux_value != current.mux_value));
    if(mux_changed) {
        next.have_latest = false;
        memset(&next.latest, 0, sizeof(next.latest));
        cancelPendingForSlot(slot_idx, "mux changed", host);
    }

    slots_[slot_idx] = next;

    char msg[168];
    snprintf(
        msg,
        sizeof(msg),
        "slot=%u modify updated (mask=%u value=%u xor=%u mux=%u sig=%u name=%u)",
        (unsigned)(slot_idx + 1),
        mask_present ? 1 : 0,
        value_present ? 1 : 0,
        xor_present ? 1 : 0,
        (mux_present || mux_toggle_present) ? 1 : 0,
        sig_present ? 1 : 0,
        (name_present || name_clear_present) ? 1 : 0);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleField(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }
    SignalSlot& slot = slots_[slot_idx];
    if(!slot.used) {
        return false;
    }

    uint8_t start_bit = 0;
    uint8_t bit_len = 0;
    uint64_t value = 0;
    bool present = false;
    if(!parseFieldSpec(args, "start", "len", "value", start_bit, bit_len, value, present) ||
       !present) {
        return false;
    }
    if(!applyFieldToMaskValue(slot, start_bit, bit_len, value)) {
        return false;
    }

    char msg[128];
    snprintf(
        msg,
        sizeof(msg),
        "slot=%u field set start=%u len=%u value=0x%llX",
        (unsigned)(slot_idx + 1),
        (unsigned)start_bit,
        (unsigned)bit_len,
        (unsigned long long)value);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleBit(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }
    SignalSlot& slot = slots_[slot_idx];
    if(!slot.used) {
        return false;
    }

    uint32_t bit = 0;
    uint32_t val = 0;
    if(!core::parse::parseU32(args, "bit", bit, 10) || bit > 63) {
        return false;
    }
    if(!core::parse::parseU32(args, "value", val, 10) || val > 1) {
        return false;
    }

    const uint8_t byte_index = (uint8_t)(bit / 8);
    const uint8_t bit_index = (uint8_t)(bit % 8);
    const uint8_t bit_mask = (uint8_t)(1u << bit_index);

    slot.mask[byte_index] |= bit_mask;
    if(val == 0) {
        slot.value[byte_index] &= (uint8_t)(~bit_mask);
    } else {
        slot.value[byte_index] |= bit_mask;
    }

    char msg[96];
    snprintf(
        msg,
        sizeof(msg),
        "slot=%u bit=%lu set to %lu",
        (unsigned)(slot_idx + 1),
        (unsigned long)bit,
        (unsigned long)val);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleClearBit(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }
    SignalSlot& slot = slots_[slot_idx];
    if(!slot.used) {
        return false;
    }

    uint32_t bit = 0;
    if(!core::parse::parseU32(args, "bit", bit, 10) || bit > 63) {
        return false;
    }

    const uint8_t byte_index = (uint8_t)(bit / 8);
    const uint8_t bit_index = (uint8_t)(bit % 8);
    const uint8_t bit_mask = (uint8_t)(1u << bit_index);

    slot.mask[byte_index] &= (uint8_t)(~bit_mask);
    slot.value[byte_index] &= (uint8_t)(~bit_mask);

    char msg[96];
    snprintf(
        msg,
        sizeof(msg),
        "slot=%u bit=%lu passthrough",
        (unsigned)(slot_idx + 1),
        (unsigned long)bit);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleInject(const char* args, ToolHost& host) {
    uint8_t slot_idx = 0;
    if(!parseSlot(args, slot_idx)) {
        return false;
    }

    uint32_t count = 1;
    uint32_t interval_ms = 0;
    if(!core::parse::parseOptU32(args, "count", count, 10) || count == 0 ||
       !core::parse::parseOptU32(args, "interval_ms", interval_ms, 10) ||
       interval_ms > kMaxIntervalMs) {
        return false;
    }

    core::CanFrame probe{};
    const char* problem = buildFrameForSlot(slot_idx, host.nowMs(), probe);
    if(problem != nullptr) {
        char msg[96];
        snprintf(msg, sizeof(msg), "inject rejected: %s", problem);
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, msg);
        return false;
    }

    pending_.active = true;
    pending_.slot = slot_idx;
    pending_.remaining = count;
    pending_.interval_ms = interval_ms;
    pending_.next_due_ms = host.nowMs();

    char msg[112];
    snprintf(
        msg,
        sizeof(msg),
        "inject scheduled slot=%u count=%lu interval_ms=%lu",
        (unsigned)(slot_idx + 1),
        (unsigned long)count,
        (unsigned long)interval_ms);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool CustomInjectTool::handleCancel(ToolHost& host) {
    if(!pending_.active) {
        return false;
    }
    pending_.active = false;
    host.emitToolEvent(id(), core::ToolEventCode::INFO, "pending injection canceled");
    return true;
}

const char* CustomInjectTool::buildFrameForSlot(
    uint8_t slot_idx,
    uint32_t now_ms,
    core::CanFrame& out) const {
    if(slot_idx >= kMaxSignals || !slots_[slot_idx].used) {
        return "slot not configured";
    }
    const SignalSlot& slot = slots_[slot_idx];
    if(!slot.have_latest) {
        return "no matching frame received yet";
    }

    // Re-check that the cached frame still satisfies the slot's current rule.
    const core::CanFrame& base = slot.latest;
    if(base.rtr || base.id != slot.id || base.ext != slot.ext || !muxMatches(slot, base)) {
        return "cached frame no longer matches slot";
    }

    // Modifications to bytes the frame does not carry would silently do nothing.
    for(uint8_t i = base.dlc; i < 8; i++) {
        if((slot.mask[i] | slot.xor_mask[i]) != 0) {
            return "modified bits beyond frame DLC";
        }
    }

    out = base;
    out.ts_ms = now_ms;

    for(uint8_t i = 0; i < 8; i++) {
        const uint8_t forced =
            (uint8_t)((out.data[i] & (uint8_t)(~slot.mask[i])) | (slot.value[i] & slot.mask[i]));
        out.data[i] = (uint8_t)(forced ^ slot.xor_mask[i]);
    }
    return nullptr;
}

bool CustomInjectTool::configure(const char* args, ToolHost& host) {
    char cmd[16] = {0};
    if(!core::parse::parseString(args, "cmd", cmd, sizeof(cmd))) {
        return false;
    }

    if(strcmp(cmd, "add") == 0) {
        return handleAdd(args, host);
    }
    if(strcmp(cmd, "remove") == 0) {
        return handleRemove(args, host);
    }
    if(strcmp(cmd, "clear") == 0) {
        return handleClear(host);
    }
    if(strcmp(cmd, "list") == 0) {
        return handleList(host);
    }
    if(strcmp(cmd, "modify") == 0) {
        return handleModify(args, host);
    }
    if(strcmp(cmd, "bit") == 0) {
        return handleBit(args, host);
    }
    if(strcmp(cmd, "field") == 0) {
        return handleField(args, host);
    }
    if(strcmp(cmd, "clearbit") == 0) {
        return handleClearBit(args, host);
    }
    if(strcmp(cmd, "inject") == 0) {
        return handleInject(args, host);
    }
    if(strcmp(cmd, "cancel") == 0) {
        return handleCancel(host);
    }
    return false;
}

void CustomInjectTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    (void)host;
    // Only data frames can serve as injection templates.
    if(frame.rtr) {
        return;
    }
    for(uint8_t i = 0; i < kMaxSignals; i++) {
        SignalSlot& slot = slots_[i];
        if(!slot.used) {
            continue;
        }
        if(!busMatches(slot.bus, frame.bus)) {
            continue;
        }
        if(slot.id != frame.id || slot.ext != frame.ext) {
            continue;
        }
        if(!muxMatches(slot, frame)) {
            continue;
        }

        slot.latest = frame;
        slot.have_latest = true;
    }
}

void CustomInjectTool::tick(ToolHost& host) {
    if(!pending_.active) {
        return;
    }

    const uint32_t now = host.nowMs();
    if((int32_t)(now - pending_.next_due_ms) < 0) {
        return;
    }

    core::CanFrame frame{};
    const char* problem = buildFrameForSlot(pending_.slot, now, frame);
    if(problem != nullptr) {
        pending_.active = false;
        char msg[96];
        snprintf(msg, sizeof(msg), "inject aborted: %s", problem);
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, msg);
        return;
    }

    const bool ok = host.sendCanFrame(frame);
    if(!ok) {
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, "inject send failed");
    } else {
        char msg[112];
        snprintf(
            msg,
            sizeof(msg),
            "inject tx slot=%u bus=%s id=0x%lX remaining=%lu",
            (unsigned)(pending_.slot + 1),
            core::busName(frame.bus),
            (unsigned long)frame.id,
            (unsigned long)(pending_.remaining > 0 ? (pending_.remaining - 1) : 0));
        host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    }

    if(pending_.remaining > 0) {
        pending_.remaining--;
    }

    if(pending_.remaining == 0) {
        pending_.active = false;
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "inject sequence complete");
        return;
    }

    pending_.next_due_ms = now + pending_.interval_ms;
}

void CustomInjectTool::describe(char* out, size_t out_len) const {
    if(pending_.active) {
        snprintf(
            out,
            out_len,
            "custom_inject slots=%u pending_slot=%u remaining=%lu interval_ms=%lu",
            (unsigned)usedCount(),
            (unsigned)(pending_.slot + 1),
            (unsigned long)pending_.remaining,
            (unsigned long)pending_.interval_ms);
        return;
    }

    snprintf(out, out_len, "custom_inject slots=%u pending_slot=none", (unsigned)usedCount());
}

} // namespace tools
