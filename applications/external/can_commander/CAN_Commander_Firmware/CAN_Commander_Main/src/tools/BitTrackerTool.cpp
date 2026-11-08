#include "BitTrackerTool.h"

#include <stdio.h>
#include <string.h>

#include "ToolUtils.h"
#include "../core/ParseUtils.h"

namespace tools {

BitTrackerTool::BitTrackerTool()
    : bus_(core::BusId::CAN0), id_filter_(0), ext_filter_set_(false), ext_filter_(false) {
  memset(have_last_, 0, sizeof(have_last_));
  memset(last_, 0, sizeof(last_));
}

bool BitTrackerTool::start(const char *args, ToolHost &host) {
  core::BusId bus = core::BusId::CAN0;
  uint32_t id = 0;
  bool ext = false;
  const bool ext_set = core::parse::hasKey(args, "ext");
  if (!parseBusArg(args, core::BusId::CAN0, bus) || !core::parse::parseU32(args, "id", id, 16) ||
      !core::parse::parseOptBool01(args, "ext", ext) || id > core::kMaxExtCanId) {
    return false;
  }
  if (!requireBusAvailable(host, this->id(), bus)) {
    return false;
  }
  bus_ = bus;
  id_filter_ = id;
  ext_filter_set_ = ext_set;
  ext_filter_ = ext;
  memset(have_last_, 0, sizeof(have_last_));
  memset(last_, 0, sizeof(last_));
  return true;
}

void BitTrackerTool::stop(ToolHost &host) {
  (void)host;
}

bool BitTrackerTool::frameDifferent(const core::CanFrame &frame) const {
  const uint8_t b = busIndex(frame.bus);
  const uint8_t e = frame.ext ? 1 : 0;
  if (!have_last_[b][e]) {
    return true;
  }
  const core::CanFrame &last = last_[b][e];
  if (frame.dlc != last.dlc) {
    return true;
  }
  for (uint8_t i = 0; i < frame.dlc; i++) {
    if (frame.data[i] != last.data[i]) {
      return true;
    }
  }
  return false;
}

void BitTrackerTool::updateLast(const core::CanFrame &frame) {
  const uint8_t b = busIndex(frame.bus);
  const uint8_t e = frame.ext ? 1 : 0;
  last_[b][e] = frame;
  have_last_[b][e] = true;
}

void BitTrackerTool::byteToBinary(uint8_t v, char *out8) const {
  for (int i = 7; i >= 0; i--) {
    out8[7 - i] = ((v >> i) & 0x01) ? '1' : '0';
  }
  out8[8] = '\0';
}

void BitTrackerTool::onFrame(const core::CanFrame &frame, ToolHost &host) {
  if (!busMatches(bus_, frame.bus)) {
    return;
  }
  if (frame.id != id_filter_ || frame.rtr) {
    return;
  }
  if (ext_filter_set_ && frame.ext != ext_filter_) {
    return;
  }

  if (!frameDifferent(frame)) {
    return;
  }

  char msg[128];
  int n = snprintf(msg,
                   sizeof(msg),
                   "id=0x%lX bits:",
                   (unsigned long)frame.id);
  size_t pos = n > 0 ? (size_t)n : 0;
  for (uint8_t i = 0; i < frame.dlc && pos + 16 < sizeof(msg); i++) {
    char bits[9];
    byteToBinary(frame.data[i], bits);
    int w = snprintf(msg + pos, sizeof(msg) - pos, " b%u=%s", i, bits);
    if (w <= 0) {
      break;
    }
    pos += (size_t)w;
  }

  host.emitToolEvent(id(), core::ToolEventCode::DELTA, msg);
  updateLast(frame);
}

void BitTrackerTool::tick(ToolHost &host) {
  (void)host;
}

void BitTrackerTool::describe(char *out, size_t out_len) const {
  snprintf(out,
           out_len,
           "bittrack bus=%s id=0x%lX",
           core::busName(bus_),
           (unsigned long)id_filter_);
}

} // namespace tools
