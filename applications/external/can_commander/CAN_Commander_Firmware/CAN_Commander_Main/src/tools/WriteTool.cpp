#include "WriteTool.h"

#include <stdio.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

WriteTool::WriteTool()
    : period_ms_(0), remaining_(0), continuous_(false), next_due_ms_(0), running_(false) {
  memset(&tx_, 0, sizeof(tx_));
}

bool WriteTool::start(const char *args, ToolHost &host) {
  core::CanFrame tx{};
  tx.dlc = 8;

  if (!parseTxBusArg(args, core::BusId::CAN0, tx.bus) ||
      !core::parse::parseU32(args, "id", tx.id, 16) ||
      !core::parse::parseOptBool01(args, "ext", tx.ext) ||
      !core::parse::parseOptBool01(args, "rtr", tx.rtr)) {
    return false;
  }
  if (!requireBusAvailable(host, id(), tx.bus)) {
    return false;
  }
  if (!core::isValidCanId(tx.id, tx.ext)) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "id out of range for frame format");
    return false;
  }

  uint32_t dlc = 8;
  if (!core::parse::parseOptU32(args, "dlc", dlc, 10) || dlc > 8) {
    return false;
  }
  tx.dlc = (uint8_t)dlc;

  char data_hex[17] = {0};
  if (!core::parse::parseOptString(args, "data", data_hex, sizeof(data_hex))) {
    return false;
  }
  if (data_hex[0] != '\0') {
    uint8_t parsed_len = 0;
    if (!core::parse::parseHexBytes(data_hex, tx.data, 8, parsed_len)) {
      return false;
    }
    if (parsed_len > tx.dlc) {
      tx.dlc = parsed_len;
    }
  }

  // period_ms=0 (default) is a one-shot send. With a period, omitting count runs
  // continuously; an explicit count must be at least 1.
  uint32_t period_ms = 0;
  if (!core::parse::parseOptU32(args, "period_ms", period_ms, 10) || period_ms > kMaxPeriodMs) {
    return false;
  }
  const bool count_present = core::parse::hasKey(args, "count");
  uint32_t count = 1;
  if (!core::parse::parseOptU32(args, "count", count, 10) || count == 0) {
    return false;
  }
  if (period_ms == 0 && count > 1) {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "count>1 requires period_ms>0");
    return false;
  }

  tx_ = tx;
  period_ms_ = period_ms;
  continuous_ = (period_ms > 0 && !count_present);
  remaining_ = continuous_ ? 0 : count;
  next_due_ms_ = host.nowMs();
  running_ = true;
  return true;
}

void WriteTool::stop(ToolHost &host) {
  (void)host;
  running_ = false;
  remaining_ = 0;
  continuous_ = false;
}

void WriteTool::onFrame(const core::CanFrame &frame, ToolHost &host) {
  (void)frame;
  (void)host;
}

void WriteTool::tick(ToolHost &host) {
  if (!running_) {
    return;
  }
  const uint32_t now = host.nowMs();
  if ((int32_t)(now - next_due_ms_) < 0) {
    return;
  }

  core::CanFrame send_frame = tx_;
  send_frame.ts_ms = now;

  const bool ok = host.sendCanFrame(send_frame);
  if (ok) {
    char msg[96];
    snprintf(msg,
             sizeof(msg),
             "TX sent bus=%s id=0x%lX dlc=%u",
             core::busName(send_frame.bus),
             (unsigned long)send_frame.id,
             send_frame.dlc);
    host.emitToolEvent(id(), core::ToolEventCode::INFO, msg);
  } else {
    host.emitToolEvent(id(), core::ToolEventCode::ERROR, "TX failed");
  }

  if (!continuous_) {
    if (remaining_ > 0) {
      remaining_--;
    }
    if (remaining_ == 0) {
      running_ = false;
      host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, "write complete");
      return;
    }
  }

  next_due_ms_ = now + period_ms_;
}

void WriteTool::describe(char *out, size_t out_len) const {
  snprintf(out,
           out_len,
           "write bus=%s id=0x%lX period_ms=%lu remaining=%s%lu running=%u",
           core::busName(tx_.bus),
           (unsigned long)tx_.id,
           (unsigned long)period_ms_,
           continuous_ ? "continuous/" : "",
           (unsigned long)remaining_,
           running_ ? 1 : 0);
}

} // namespace tools
