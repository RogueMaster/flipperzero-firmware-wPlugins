#include "SpeedTestTool.h"

#include <stdio.h>

#include "ToolUtils.h"

namespace tools {

SpeedTestTool::SpeedTestTool() : bus_(core::BusId::BOTH), counter_(0), last_emit_ms_(0) {}

bool SpeedTestTool::start(const char *args, ToolHost &host) {
  core::BusId bus = core::BusId::BOTH;
  if (!parseBusArg(args, core::BusId::BOTH, bus) || !requireBusAvailable(host, id(), bus)) {
    return false;
  }
  bus_ = bus;
  counter_ = 0;
  last_emit_ms_ = host.nowMs();
  return true;
}

void SpeedTestTool::stop(ToolHost &host) {
  (void)host;
}

void SpeedTestTool::onFrame(const core::CanFrame &frame, ToolHost &host) {
  (void)host;
  if (busMatches(bus_, frame.bus)) {
    counter_++;
  }
}

void SpeedTestTool::tick(ToolHost &host) {
  const uint32_t now = host.nowMs();
  if ((uint32_t)(now - last_emit_ms_) < 1000) {
    return;
  }

  char msg[96];
  snprintf(msg,
           sizeof(msg),
           "speed bus=%s %lu msg/sec",
           core::busName(bus_),
           (unsigned long)counter_);
  host.emitToolEvent(id(), core::ToolEventCode::SUMMARY, msg);

  counter_ = 0;
  last_emit_ms_ = now;
}

void SpeedTestTool::describe(char *out, size_t out_len) const {
  snprintf(out,
           out_len,
           "speed bus=%s count=%lu",
           core::busName(bus_),
           (unsigned long)counter_);
}

} // namespace tools
