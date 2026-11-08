#pragma once

#include "ToolBase.h"

namespace tools {

class UniqueIdsTool : public ToolBase {
 public:
  UniqueIdsTool();

  core::ToolId id() const override { return core::ToolId::UNIQUE_IDS; }
  const char *name() const override { return "unique_ids"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  bool configure(const char *args, ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  // The same ID on two buses is two entries.
  struct IdEntry {
    uint32_t id;
    core::BusId bus;
    bool ext;
    bool used;
  };

  // Keep this many output-queue entries free for other traffic while a snapshot
  // is being emitted.
  static constexpr uint16_t kOutputReserve = 16;

  bool contains(core::BusId bus, uint32_t id, bool ext) const;
  bool insert(core::BusId bus, uint32_t id, bool ext);
  void continueSnapshot(ToolHost &host);

  core::BusId bus_;
  IdEntry ids_[512];
  bool new_found_;
  uint32_t last_emit_ms_;
  bool table_full_reported_;
  bool snapshot_active_;
  uint16_t snapshot_cursor_;
};

} // namespace tools
