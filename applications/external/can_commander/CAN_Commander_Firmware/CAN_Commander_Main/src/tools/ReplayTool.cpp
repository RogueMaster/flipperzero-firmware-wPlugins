#include "ReplayTool.h"

#include <stdio.h>
#include <string.h>

#include "ToolUtils.h"

namespace tools {

ReplayTool::ReplayTool()
    : bus_(core::BusId::CAN0)
    , target_id_(0)
    , target_ext_(false)
    , state_(State::IDLE)
    , frame_count_(0)
    , last_record_ts_us_(0)
    , replay_index_(0)
    , replay_base_us_(0)
    , replay_offset_us_(0)
    , replay_next_due_us_(0)
    , replay_remaining_(0)
    , replay_speed_pct_(100)
    , replay_fail_streak_(0) {
    memset(frames_, 0, sizeof(frames_));
}

bool ReplayTool::start(const char* args, ToolHost& host) {
    core::BusId bus = core::BusId::CAN0;
    if(!parseBusArg(args, core::BusId::CAN0, bus)) {
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, "invalid bus (use can0 or can1)");
        return false;
    }
    if(bus == core::BusId::BOTH) {
        host.emitToolEvent(
            id(), core::ToolEventCode::ERROR, "bus=both not supported (use can0 or can1)");
        return false;
    }
    if(!requireBusAvailable(host, id(), bus)) {
        return false;
    }

    uint32_t target_id = 0;
    if(!core::parse::parseU32(args, "id", target_id, 16)) {
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, "missing or invalid id=<hex>");
        return false;
    }

    bool target_ext = false;
    if(!core::parse::parseOptBool01(args, "ext", target_ext) ||
       !core::isValidCanId(target_id, target_ext)) {
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, "id out of range for frame format");
        return false;
    }

    bus_ = bus;
    target_id_ = target_id;
    target_ext_ = target_ext;

    state_ = State::IDLE;
    frame_count_ = 0;
    last_record_ts_us_ = 0;
    replay_index_ = 0;
    replay_remaining_ = 0;
    replay_speed_pct_ = 100;
    replay_fail_streak_ = 0;
    memset(frames_, 0, sizeof(frames_));

    char msg[96];
    snprintf(
        msg,
        sizeof(msg),
        "ready bus=%s id=0x%lX %s (max %u frames)",
        core::busName(bus_),
        (unsigned long)target_id_,
        target_ext_ ? "ext" : "std",
        (unsigned)kMaxFrames);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

void ReplayTool::stop(ToolHost& host) {
    (void)host;
    state_ = State::IDLE;
}

void ReplayTool::onFrame(const core::CanFrame& frame, ToolHost& host) {
    if(state_ != State::RECORDING) {
        return;
    }

    if(!busMatches(bus_, frame.bus)) {
        return;
    }
    if(frame.id != target_id_ || frame.ext != target_ext_) {
        return;
    }

    if(frame_count_ >= kMaxFrames) {
        state_ = State::IDLE;
        char msg[64];
        snprintf(
            msg,
            sizeof(msg),
            "buffer full (%u frames) recording auto-stopped",
            (unsigned)frame_count_);
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, msg);
        return;
    }

    // Gaps come from the capture timestamps taken when each frame was read from the
    // controller, not from when the tool happened to process it.
    const uint32_t delta_us = (frame_count_ == 0) ? 0 :
                                                    (uint32_t)(frame.ts_us - last_record_ts_us_);

    frames_[frame_count_].delta_us = delta_us;
    frames_[frame_count_].dlc = frame.dlc;
    frames_[frame_count_].rtr = frame.rtr;
    memcpy(frames_[frame_count_].data, frame.data, 8);
    frame_count_++;
    last_record_ts_us_ = frame.ts_us;
}

void ReplayTool::tick(ToolHost& host) {
    if(state_ != State::REPLAYING) {
        return;
    }

    const uint32_t now_us = micros();
    if((int32_t)(now_us - replay_next_due_us_) < 0) {
        return;
    }
    // If the loop ran late, shift the timeline instead of bursting to catch up, so
    // the gaps between replayed frames stay as recorded.
    const uint32_t late_us = now_us - replay_next_due_us_;
    if(late_us > kMaxLatenessUs) {
        replay_base_us_ += late_us;
    }

    core::CanFrame frame{};
    frame.ts_ms = host.nowMs();
    frame.bus = bus_;
    frame.id = target_id_;
    frame.ext = target_ext_;
    frame.rtr = frames_[replay_index_].rtr;
    frame.dlc = frames_[replay_index_].dlc;
    memcpy(frame.data, frames_[replay_index_].data, 8);

    if(!host.sendCanFrame(frame)) {
        replay_fail_streak_++;
        if(replay_fail_streak_ == 1) {
            host.emitToolEvent(id(), core::ToolEventCode::WARNING, "replay send failed, retrying");
        } else if(replay_fail_streak_ >= 32) {
            state_ = State::IDLE;
            host.emitToolEvent(
                id(), core::ToolEventCode::ERROR, "replay aborted after repeated send failures");
            return;
        }
        replay_base_us_ += kRetryDelayUs;
        replay_next_due_us_ = now_us + kRetryDelayUs;
        return;
    }
    replay_fail_streak_ = 0;

    replay_index_++;

    if(replay_index_ >= frame_count_) {
        replay_remaining_--;
        if(replay_remaining_ == 0) {
            state_ = State::IDLE;
            host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "replay complete");
            return;
        }
        // Next loop starts a fresh timeline from now.
        replay_index_ = 0;
        replay_base_us_ = now_us;
        replay_offset_us_ = 0;
        replay_next_due_us_ = now_us;
        return;
    }

    const uint32_t raw_delta_us = frames_[replay_index_].delta_us;
    uint32_t scaled_delta_us = (uint32_t)(((uint64_t)raw_delta_us * 100ULL) / replay_speed_pct_);
    if(scaled_delta_us == 0 && raw_delta_us > 0) {
        scaled_delta_us = 1;
    }
    replay_offset_us_ += scaled_delta_us;
    replay_next_due_us_ = replay_base_us_ + replay_offset_us_;
}

bool ReplayTool::configure(const char* args, ToolHost& host) {
    char cmd[16] = {0};
    if(!core::parse::parseString(args, "cmd", cmd, sizeof(cmd))) {
        return false;
    }

    if(strcmp(cmd, "record") == 0) return handleRecord(host);
    if(strcmp(cmd, "stop") == 0) return handleStop(host);
    if(strcmp(cmd, "replay") == 0) return handleReplay(args, host);
    if(strcmp(cmd, "status") == 0) return handleStatus(host);
    if(strcmp(cmd, "clear") == 0) return handleClear(host);
    return false;
}

bool ReplayTool::handleRecord(ToolHost& host) {
    if(state_ == State::REPLAYING) {
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "stop replay before recording");
        return false;
    }

    state_ = State::RECORDING;
    frame_count_ = 0;
    last_record_ts_us_ = 0;

    char msg[64];
    snprintf(msg, sizeof(msg), "recording started for id=0x%lX", (unsigned long)target_id_);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool ReplayTool::handleStop(ToolHost& host) {
    if(state_ == State::RECORDING) {
        state_ = State::IDLE;
        char msg[64];
        snprintf(msg, sizeof(msg), "recording stopped frames=%u", (unsigned)frame_count_);
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
        return true;
    }

    if(state_ == State::REPLAYING) {
        state_ = State::IDLE;
        char msg[80];
        snprintf(
            msg,
            sizeof(msg),
            "replay stopped at frame %u/%u",
            (unsigned)replay_index_,
            (unsigned)frame_count_);
        host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
        return true;
    }

    host.emitToolEvent(id(), core::ToolEventCode::INFO, "nothing active to stop");
    return true;
}

bool ReplayTool::handleReplay(const char* args, ToolHost& host) {
    if(state_ == State::RECORDING) {
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "stop recording first");
        return false;
    }
    if(state_ == State::REPLAYING) {
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "replay already in progress");
        return false;
    }
    if(frame_count_ == 0) {
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "no frames recorded");
        return false;
    }

    uint32_t count = 1;
    uint32_t speed = 100;
    if(!core::parse::parseOptU32(args, "count", count, 10) || count == 0 ||
       !core::parse::parseOptU32(args, "speed", speed, 10) || speed < 10 || speed > 1000) {
        host.emitToolEvent(
            id(), core::ToolEventCode::WARNING, "invalid args (count>=1, speed=10-1000)");
        return false;
    }

    state_ = State::REPLAYING;
    replay_index_ = 0;
    replay_remaining_ = count;
    replay_speed_pct_ = (uint16_t)speed;
    replay_base_us_ = micros();
    replay_offset_us_ = 0;
    replay_next_due_us_ = replay_base_us_;
    replay_fail_streak_ = 0;

    char msg[80];
    snprintf(
        msg,
        sizeof(msg),
        "replay started frames=%u count=%lu speed=%u%%",
        (unsigned)frame_count_,
        (unsigned long)count,
        (unsigned)speed);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
    return true;
}

bool ReplayTool::handleStatus(ToolHost& host) const {
    const char* state_str = "idle";
    if(state_ == State::RECORDING)
        state_str = "recording";
    else if(state_ == State::REPLAYING)
        state_str = "replaying";

    char msg[128];
    snprintf(
        msg,
        sizeof(msg),
        "state=%s frames=%u/%u bus=%s id=0x%lX %s",
        state_str,
        (unsigned)frame_count_,
        (unsigned)kMaxFrames,
        core::busName(bus_),
        (unsigned long)target_id_,
        target_ext_ ? "ext" : "std");
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);

    if(state_ == State::REPLAYING) {
        char progress[80];
        snprintf(
            progress,
            sizeof(progress),
            "progress=%u/%u remaining_loops=%lu speed=%u%%",
            (unsigned)replay_index_,
            (unsigned)frame_count_,
            (unsigned long)replay_remaining_,
            (unsigned)replay_speed_pct_);
        host.emitToolEvent(id(), core::ToolEventCode::INFO, progress);
    }
    return true;
}

bool ReplayTool::handleClear(ToolHost& host) {
    if(state_ != State::IDLE) {
        host.emitToolEvent(id(), core::ToolEventCode::WARNING, "stop first before clearing");
        return false;
    }

    frame_count_ = 0;
    host.emitToolEvent(id(), core::ToolEventCode::INFO, "recording cleared");
    return true;
}

void ReplayTool::describe(char* out, size_t out_len) const {
    const char* state_str = "idle";
    if(state_ == State::RECORDING)
        state_str = "recording";
    else if(state_ == State::REPLAYING)
        state_str = "replaying";

    snprintf(
        out,
        out_len,
        "replay bus=%s id=0x%lX state=%s frames=%u/%u",
        core::busName(bus_),
        (unsigned long)target_id_,
        state_str,
        (unsigned)frame_count_,
        (unsigned)kMaxFrames);
}

} // namespace tools
