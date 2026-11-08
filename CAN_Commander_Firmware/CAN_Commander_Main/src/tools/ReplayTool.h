#pragma once

#include "ToolBase.h"

namespace tools {

class ReplayTool : public ToolBase {
 public:
  ReplayTool();

  core::ToolId id() const override { return core::ToolId::REPLAY; }
  const char *name() const override { return "replay"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  bool configure(const char *args, ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  static constexpr uint16_t kMaxFrames = 384;
  static constexpr uint32_t kRetryDelayUs = 1000;
  // Lateness beyond this shifts the playback timeline rather than bursting.
  static constexpr uint32_t kMaxLatenessUs = 2000;

  enum class State : uint8_t {
    IDLE = 0,
    RECORDING = 1,
    REPLAYING = 2,
  };

  struct RecordedFrame {
    uint32_t delta_us;
    uint8_t dlc;
    bool rtr;
    uint8_t data[8];
  };

  bool handleRecord(ToolHost &host);
  bool handleStop(ToolHost &host);
  bool handleReplay(const char *args, ToolHost &host);
  bool handleStatus(ToolHost &host) const;
  bool handleClear(ToolHost &host);

  core::BusId bus_;
  uint32_t target_id_;
  bool target_ext_;

  State state_;
  RecordedFrame frames_[kMaxFrames];
  uint16_t frame_count_;

  uint32_t last_record_ts_us_;

  // Playback follows the recording's timeline: frame i is due at
  // replay_base_us_ + replay_offset_us_ (sum of scaled gaps up to i).
  uint16_t replay_index_;
  uint32_t replay_base_us_;
  uint32_t replay_offset_us_;
  uint32_t replay_next_due_us_;
  uint32_t replay_remaining_;
  uint16_t replay_speed_pct_;
  uint16_t replay_fail_streak_;
};

} // namespace tools
