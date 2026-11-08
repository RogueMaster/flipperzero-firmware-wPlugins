#pragma once

#include <initializer_list>
#include <string>
#include <vector>

#include "tools/ToolBase.h"

// Records everything a tool does through the ToolHost interface.
struct FakeHost : tools::ToolHost {
  uint32_t now = 1000;
  bool tx_ok = true;
  bool can1_available = true;
  decode::DbcSignalStore store;
  std::vector<core::CanFrame> tx;
  std::vector<std::string> events;
  std::vector<core::DbcDecodedValue> decoded;

  uint32_t nowMs() const override { return now; }
  bool sendCanFrame(const core::CanFrame &frame) override {
    tx.push_back(frame);
    return tx_ok;
  }
  void emitCanFrameEvent(const core::CanFrame &, bool) override {}
  void emitToolEvent(core::ToolId, core::ToolEventCode, const char *text) override {
    events.push_back(text ? text : "");
    if (queued < queue_capacity) {
      queued++;
    }
  }
  void emitDbcDecodeEvent(const core::DbcDecodedValue &value) override { decoded.push_back(value); }
  decode::DbcSignalStore &dbcStore() override { return store; }
  // Simulated output queue: each event occupies a slot until drainOutput().
  uint16_t queue_capacity = 0xFFFF;
  uint16_t queued = 0;
  uint16_t outputHeadroom() const override { return (uint16_t)(queue_capacity - queued); }
  void drainOutput() { queued = 0; }

  bool isBusAvailable(core::BusId bus) const override {
    return bus == core::BusId::CAN0 || bus == core::BusId::BOTH || (bus == core::BusId::CAN1 && can1_available);
  }

  bool anyEventContains(const char *needle) const {
    for (const auto &e : events) {
      if (e.find(needle) != std::string::npos) {
        return true;
      }
    }
    return false;
  }
};

inline core::CanFrame makeFrame(uint32_t id, std::initializer_list<uint8_t> data, core::BusId bus = core::BusId::CAN0) {
  core::CanFrame f{};
  f.bus = bus;
  f.id = id;
  f.dlc = (uint8_t)data.size();
  uint8_t i = 0;
  for (uint8_t b : data) {
    f.data[i++] = b;
  }
  return f;
}
