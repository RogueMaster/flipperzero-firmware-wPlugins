#pragma once

#include "ToolBase.h"

namespace tools {

class ReadAllTool : public ToolBase {
 public:
  ReadAllTool();

  core::ToolId id() const override { return core::ToolId::READ_ALL; }
  const char *name() const override { return "read_all"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  core::BusId bus_;
  bool ascii_;
};

} // namespace tools
