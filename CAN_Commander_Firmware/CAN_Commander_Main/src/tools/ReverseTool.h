#pragma once

#include "ToolBase.h"

namespace tools {

class ReverseTool : public ToolBase {
 public:
  ReverseTool();

  core::ToolId id() const override { return core::ToolId::REVERSE; }
  const char *name() const override { return "reverse"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  bool configure(const char *args, ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  static constexpr uint16_t kMaxIds = 256;

  enum class Phase : uint8_t {
    CALIBRATION = 0,
    MONITORING = 1,
    DONE = 2,
    READ = 3,
  };

  struct Entry {
    bool used;
    bool disqualified;
    core::BusId bus;
    uint32_t id;
    bool ext;
    uint8_t dlc;
    uint8_t data[8];
    bool byte_seen[8];
    bool byte_disq[8];
    bool byte_changed_since_mark[8];
  };

  static constexpr uint16_t kOutputReserve = 16;

  void resetAuto(ToolHost &host);
  void continueCalibrationSummary(ToolHost &host);
  void excludePreActionChanges(ToolHost &host);
  bool enterReadMode(const char *args, ToolHost &host);
  int findEntry(core::BusId bus, uint32_t id, bool ext) const;
  int allocEntry(core::BusId bus, uint32_t id, bool ext, uint8_t dlc);
  bool parseByteMask(const char *bytes, bool mask[8]);

  core::BusId bus_;
  Phase phase_;
  uint32_t phase_start_ms_;
  uint32_t calibration_ms_;
  uint32_t monitoring_ms_;

  Entry entries_[kMaxIds];

  uint32_t read_id_;
  bool read_ext_;
  bool read_all_bytes_;
  bool read_mask_[8];
  bool emit_calibration_summary_;
  bool table_full_reported_;
  bool summary_active_;
  uint16_t summary_cursor_;
};

} // namespace tools
