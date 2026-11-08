#include "ReverseTool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

ReverseTool::ReverseTool()
    : bus_(core::BusId::CAN0),
      phase_(Phase::CALIBRATION),
      phase_start_ms_(0),
      calibration_ms_(10000),
      monitoring_ms_(20000),
      read_id_(0),
      read_ext_(false),
      read_all_bytes_(true),
      emit_calibration_summary_(true),
      table_full_reported_(false),
      summary_active_(false),
      summary_cursor_(0) {
  memset(entries_, 0, sizeof(entries_));
  memset(read_mask_, 0, sizeof(read_mask_));
}

int ReverseTool::findEntry(core::BusId bus, uint32_t id, bool ext) const {
  for (uint16_t i = 0; i < kMaxIds; i++) {
    if (entries_[i].used && entries_[i].id == id && entries_[i].ext == ext && entries_[i].bus == bus) {
      return (int)i;
    }
  }
  return -1;
}

int ReverseTool::allocEntry(core::BusId bus, uint32_t id, bool ext, uint8_t dlc) {
  for (uint16_t i = 0; i < kMaxIds; i++) {
    if (!entries_[i].used) {
      entries_[i].used = true;
      entries_[i].disqualified = false;
      entries_[i].bus = bus;
      entries_[i].id = id;
      entries_[i].ext = ext;
      entries_[i].dlc = dlc;
      memset(entries_[i].data, 0, sizeof(entries_[i].data));
      memset(entries_[i].byte_seen, 0, sizeof(entries_[i].byte_seen));
      memset(entries_[i].byte_disq, 0, sizeof(entries_[i].byte_disq));
      memset(entries_[i].byte_changed_since_mark, 0, sizeof(entries_[i].byte_changed_since_mark));
      return (int)i;
    }
  }
  return -1;
}

bool ReverseTool::parseByteMask(const char *bytes, bool mask[8]) {
  if (bytes == nullptr || mask == nullptr) {
    return false;
  }
  memset(mask, 0, 8);

  if (strcmp(bytes, "all") == 0) {
    return true;
  }

  // Comma-separated byte indexes 0-7, e.g. "0,1,2". Anything else is invalid.
  bool any = false;
  bool expect_digit = true;
  for (const char *p = bytes; *p != '\0'; ++p) {
    if (expect_digit) {
      if (*p < '0' || *p > '7') {
        return false;
      }
      mask[*p - '0'] = true;
      any = true;
      expect_digit = false;
    } else {
      if (*p != ',') {
        return false;
      }
      expect_digit = true;
    }
  }
  return any && !expect_digit;
}

void ReverseTool::resetAuto(ToolHost &host) {
  memset(entries_, 0, sizeof(entries_));
  summary_active_ = false;
  table_full_reported_ = false;
  phase_ = Phase::CALIBRATION;
  phase_start_ms_ = host.nowMs();
  host.emitToolEvent(id(), core::ToolEventCode::INFO, "reverse calibration start");
}

// Emits the per-ID calibration summary a few lines per tick while the output queues
// have room, so long summaries are not cut short by queue overflow.
void ReverseTool::continueCalibrationSummary(ToolHost &host) {
  while (summary_cursor_ < kMaxIds) {
    if (host.outputHeadroom() <= kOutputReserve) {
      return;
    }
    const Entry &e = entries_[summary_cursor_++];
    if (!e.used || e.disqualified) {
      continue;
    }

    char line[128];
    int n = snprintf(line,
                     sizeof(line),
                     "id=0x%lX %s stable:",
                     (unsigned long)e.id,
                     e.ext ? "ext" : "std");
    size_t pos = n > 0 ? (size_t)n : 0;

    for (uint8_t b = 0; b < e.dlc && pos + 8 < sizeof(line); b++) {
      int w = 0;
      if (e.byte_disq[b]) {
        w = snprintf(line + pos, sizeof(line) - pos, " XX");
      } else {
        w = snprintf(line + pos, sizeof(line) - pos, " %02X", e.data[b]);
      }
      if (w <= 0) {
        break;
      }
      pos += (size_t)w;
    }

    host.emitToolEvent(id(), core::ToolEventCode::INFO, line);
  }
  summary_active_ = false;
}

void ReverseTool::excludePreActionChanges(ToolHost &host) {
  if (phase_ != Phase::MONITORING) {
    host.emitToolEvent(id(), core::ToolEventCode::WARNING, "reverse exclude_pre only valid during monitoring");
    return;
  }

  uint32_t excluded_bytes = 0;
  uint32_t newly_disqualified_ids = 0;

  for (uint16_t i = 0; i < kMaxIds; i++) {
    Entry &e = entries_[i];
    if (!e.used || e.disqualified) {
      continue;
    }

    bool any_stable_left = false;
    for (uint8_t b = 0; b < 8; b++) {
      if (b < e.dlc && e.byte_changed_since_mark[b] && !e.byte_disq[b]) {
        e.byte_disq[b] = true;
        excluded_bytes++;
      }

      e.byte_changed_since_mark[b] = false;
      if (b < e.dlc && !e.byte_disq[b]) {
        any_stable_left = true;
      }
    }

    if (!any_stable_left) {
      e.disqualified = true;
      newly_disqualified_ids++;
    }
  }

  char msg[128];
  snprintf(msg,
           sizeof(msg),
           "reverse exclude_pre applied bytes=%lu ids_disqualified=%lu",
           (unsigned long)excluded_bytes,
           (unsigned long)newly_disqualified_ids);
  host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);
}

bool ReverseTool::enterReadMode(const char *args, ToolHost &host) {
  uint32_t frame_id = 0;
  bool ext = false;
  bool all_bytes = true;
  bool mask[8] = {false};

  if (!core::parse::parseOptU32(args, "id", frame_id, 16) || !core::parse::parseOptBool01(args, "ext", ext) ||
      !core::isValidCanId(frame_id, ext)) {
    return false;
  }

  char bytes[32] = {0};
  if (!core::parse::parseOptString(args, "bytes", bytes, sizeof(bytes))) {
    return false;
  }
  if (bytes[0] != '\0' && strcmp(bytes, "all") != 0) {
    if (!parseByteMask(bytes, mask)) {
      return false;
    }
    all_bytes = false;
  }

  phase_ = Phase::READ;
  read_id_ = frame_id;
  read_ext_ = ext;
  read_all_bytes_ = all_bytes;
  memcpy(read_mask_, mask, sizeof(read_mask_));

  char msg[96];
  snprintf(msg,
           sizeof(msg),
           "reverse read mode id=0x%lX %s bytes=%s",
           (unsigned long)read_id_,
           read_ext_ ? "ext" : "std",
           read_all_bytes_ ? "all" : "mask");
  host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
  return true;
}

bool ReverseTool::start(const char *args, ToolHost &host) {
  core::BusId bus = core::BusId::CAN0;
  uint32_t calibration_ms = 10000;
  uint32_t monitoring_ms = 20000;
  bool summary = true;
  char mode[16] = {0};
  if (!parseBusArg(args, core::BusId::CAN0, bus) ||
      !core::parse::parseOptU32(args, "calibration_ms", calibration_ms, 10) ||
      !core::parse::parseOptU32(args, "monitor_ms", monitoring_ms, 10) ||
      !core::parse::parseOptBool01(args, "summary", summary) ||
      !core::parse::parseOptString(args, "mode", mode, sizeof(mode))) {
    return false;
  }
  if (mode[0] != '\0' && strcmp(mode, "read") != 0 && strcmp(mode, "auto") != 0) {
    return false;
  }

  if (!requireBusAvailable(host, id(), bus)) {
    return false;
  }
  // Read-mode arguments are validated before any state changes, so a rejected
  // start leaves a running reverse session untouched.
  if (strcmp(mode, "read") == 0 && !enterReadMode(args, host)) {
    return false;
  }

  bus_ = bus;
  calibration_ms_ = calibration_ms;
  monitoring_ms_ = monitoring_ms;
  emit_calibration_summary_ = summary;

  if (strcmp(mode, "read") != 0) {
    resetAuto(host);
  }
  return true;
}

void ReverseTool::stop(ToolHost &host) {
  (void)host;
}

bool ReverseTool::configure(const char *args, ToolHost &host) {
  uint32_t restart = 0;
  if (core::parse::parseU32(args, "restart", restart, 10) && restart == 1) {
    resetAuto(host);
    return true;
  }

  uint32_t exclude_pre = 0;
  if ((core::parse::parseU32(args, "exclude_pre", exclude_pre, 10) ||
       core::parse::parseU32(args, "exclude_pre_action", exclude_pre, 10)) &&
      exclude_pre == 1) {
    excludePreActionChanges(host);
    return true;
  }

  char mode[16] = {0};
  if (core::parse::parseString(args, "mode", mode, sizeof(mode))) {
    if (strcmp(mode, "read") == 0) {
      return enterReadMode(args, host);
    }
    if (strcmp(mode, "auto") == 0) {
      resetAuto(host);
      return true;
    }
  }
  return false;
}

void ReverseTool::onFrame(const core::CanFrame &frame, ToolHost &host) {
  if (!busMatches(bus_, frame.bus)) {
    return;
  }

  if (phase_ == Phase::READ) {
    if (frame.id != read_id_ || frame.ext != read_ext_) {
      return;
    }

    char line[128];
    int n = snprintf(line, sizeof(line), "read id=0x%lX data:", (unsigned long)frame.id);
    size_t pos = n > 0 ? (size_t)n : 0;
    for (uint8_t i = 0; i < frame.dlc && pos + 8 < sizeof(line); i++) {
      if (!read_all_bytes_ && !read_mask_[i]) {
        continue;
      }
      int w = snprintf(line + pos, sizeof(line) - pos, " %02X", frame.data[i]);
      if (w <= 0) {
        break;
      }
      pos += (size_t)w;
    }
    host.emitToolEvent(id(), core::ToolEventCode::INFO, line);
    return;
  }

  int idx = findEntry(frame.bus, frame.id, frame.ext);
  bool is_new = false;
  if (idx < 0) {
    idx = allocEntry(frame.bus, frame.id, frame.ext, frame.dlc);
    if (idx < 0) {
      if (!table_full_reported_) {
        table_full_reported_ = true;
        host.emitToolEvent(id(), core::ToolEventCode::ERROR, "reverse table full; further new IDs not tracked");
      }
      return;
    }
    is_new = true;
  }

  Entry &e = entries_[idx];

  // An ID that first appears while monitoring has no calibration baseline. Report it
  // once as a new ID (not as eight "changes" from 00), take its payload as the
  // baseline, and mark its bytes as changed so exclude_pre treats it as pre-action.
  if (is_new && phase_ == Phase::MONITORING) {
    memcpy(e.data, frame.data, sizeof(e.data));
    for (uint8_t i = 0; i < 8; i++) {
      e.byte_seen[i] = i < frame.dlc;
      e.byte_changed_since_mark[i] = i < frame.dlc;
    }
    char msg[128];
    int n = snprintf(msg,
                     sizeof(msg),
                     "new id=0x%lX %s during monitoring data:",
                     (unsigned long)frame.id,
                     frame.ext ? "ext" : "std");
    size_t pos = n > 0 ? (size_t)n : 0;
    for (uint8_t i = 0; i < frame.dlc && pos + 4 < sizeof(msg); i++) {
      const int w = snprintf(msg + pos, sizeof(msg) - pos, " %02X", frame.data[i]);
      if (w <= 0) {
        break;
      }
      pos += (size_t)w;
    }
    host.emitToolEvent(id(), core::ToolEventCode::DELTA, msg);
    return;
  }
  if (e.disqualified) {
    return;
  }

  if (e.dlc != frame.dlc) {
    e.disqualified = true;
    return;
  }

  if (phase_ == Phase::CALIBRATION) {
    for (uint8_t i = 0; i < frame.dlc; i++) {
      if (e.byte_disq[i]) {
        continue;
      }
      if (!e.byte_seen[i]) {
        e.byte_seen[i] = true;
        e.data[i] = frame.data[i];
      } else if (e.data[i] != frame.data[i]) {
        e.byte_disq[i] = true;
      }
    }

    bool all_disq = true;
    for (uint8_t i = 0; i < frame.dlc; i++) {
      if (!e.byte_disq[i]) {
        all_disq = false;
        break;
      }
    }
    if (all_disq) {
      e.disqualified = true;
    }
    return;
  }

  if (phase_ == Phase::MONITORING) {
    for (uint8_t i = 0; i < frame.dlc; i++) {
      if (e.byte_disq[i]) {
        continue;
      }
      if (e.data[i] != frame.data[i]) {
        e.byte_changed_since_mark[i] = true;
        // The Flipper dashboard parses the "change id=0x.. bN" prefix.
        char msg[128];
        snprintf(msg,
                 sizeof(msg),
                 "change id=0x%lX b%u %02X->%02X%s%s",
                 (unsigned long)frame.id,
                 i,
                 e.data[i],
                 frame.data[i],
                 bus_ == core::BusId::BOTH ? " bus=" : "",
                 bus_ == core::BusId::BOTH ? core::busName(frame.bus) : "");
        host.emitToolEvent(id(), core::ToolEventCode::DELTA, msg);
        e.data[i] = frame.data[i];
      }
    }
  }
}

void ReverseTool::tick(ToolHost &host) {
  if (summary_active_) {
    continueCalibrationSummary(host);
  }
  if (phase_ == Phase::READ || phase_ == Phase::DONE) {
    return;
  }

  const uint32_t now = host.nowMs();
  if (phase_ == Phase::CALIBRATION && (uint32_t)(now - phase_start_ms_) >= calibration_ms_) {
    phase_ = Phase::MONITORING;
    phase_start_ms_ = now;
    for (uint16_t i = 0; i < kMaxIds; i++) {
      if (entries_[i].used) {
        memset(entries_[i].byte_changed_since_mark, 0, sizeof(entries_[i].byte_changed_since_mark));
      }
    }
    host.emitToolEvent(id(), core::ToolEventCode::INFO, "reverse monitoring start");
    if (emit_calibration_summary_) {
      host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "reverse calibration complete");
      summary_active_ = true;
      summary_cursor_ = 0;
      continueCalibrationSummary(host);
    }
    return;
  }

  if (phase_ == Phase::MONITORING && (uint32_t)(now - phase_start_ms_) >= monitoring_ms_) {
    phase_ = Phase::DONE;
    host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "reverse monitoring complete");
  }
}

void ReverseTool::describe(char *out, size_t out_len) const {
  const char *phase = "done";
  if (phase_ == Phase::CALIBRATION) {
    phase = "calibration";
  } else if (phase_ == Phase::MONITORING) {
    phase = "monitoring";
  } else if (phase_ == Phase::READ) {
    phase = "read";
  }

  snprintf(out,
           out_len,
           "reverse bus=%s phase=%s cal_ms=%lu mon_ms=%lu",
           core::busName(bus_),
           phase,
           (unsigned long)calibration_ms_,
           (unsigned long)monitoring_ms_);
}

} // namespace tools
