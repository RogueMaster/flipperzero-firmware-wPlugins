#pragma once

#include "ToolBase.h"

namespace tools {

class BitTrackerTool : public ToolBase {
 public:
  BitTrackerTool();

  core::ToolId id() const override { return core::ToolId::BITTRACK; }
  const char *name() const override { return "bittrack"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  // Previous frame per stream: [bus 0/1][std/ext], so the same ID on two buses or in
  // both formats is never compared across streams.
  static uint8_t busIndex(core::BusId bus) { return bus == core::BusId::CAN1 ? 1 : 0; }
  bool frameDifferent(const core::CanFrame &frame) const;
  void updateLast(const core::CanFrame &frame);
  void byteToBinary(uint8_t v, char *out8) const;

  core::BusId bus_;
  uint32_t id_filter_;
  bool ext_filter_set_;
  bool ext_filter_;
  bool have_last_[2][2];
  core::CanFrame last_[2][2];
};

} // namespace tools
