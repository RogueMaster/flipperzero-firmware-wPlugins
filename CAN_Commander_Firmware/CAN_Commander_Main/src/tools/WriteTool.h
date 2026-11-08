#pragma once

#include "ToolBase.h"

namespace tools {

class WriteTool : public ToolBase {
 public:
  WriteTool();

  core::ToolId id() const override { return core::ToolId::WRITE; }
  const char *name() const override { return "write"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  // Deadlines are compared as signed 32-bit differences, so periods must stay well
  // below 2^31 ms.
  static constexpr uint32_t kMaxPeriodMs = 86400000UL;

  core::CanFrame tx_;
  uint32_t period_ms_;
  uint32_t remaining_;
  bool continuous_;
  uint32_t next_due_ms_;
  bool running_;
};

} // namespace tools
