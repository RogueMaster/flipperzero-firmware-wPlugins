#pragma once

#include "ToolBase.h"

namespace tools {

class CustomInjectTool : public ToolBase {
 public:
  CustomInjectTool();

  core::ToolId id() const override { return core::ToolId::CUSTOM_INJECT; }
  const char *name() const override { return "custom_inject"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  bool configure(const char *args, ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  static constexpr uint8_t kMaxSignals = 5;
  // The Flipper app sends slot names up to 31 characters.
  static constexpr uint8_t kSlotNameMax = 32;
  // Deadlines are compared as signed 32-bit differences.
  static constexpr uint32_t kMaxIntervalMs = 86400000UL;

  struct SignalSlot {
    bool used;
    core::BusId bus;
    uint32_t id;
    bool ext;
    bool mux_enabled;
    uint8_t mux_start;
    uint8_t mux_len;
    uint64_t mux_value;
    bool have_latest;
    core::CanFrame latest;
    uint8_t mask[8];
    uint8_t value[8];
    uint8_t xor_mask[8];
    char name[kSlotNameMax];
  };

  struct PendingInjection {
    bool active;
    uint8_t slot;
    uint32_t remaining;
    uint32_t interval_ms;
    uint32_t next_due_ms;
  };

  core::BusId default_bus_;
  SignalSlot slots_[kMaxSignals];
  PendingInjection pending_;

  bool parseSlot(const char *args, uint8_t &slot_idx) const;
  bool parseU64Arg(const char *args, const char *key, uint64_t &out, bool &present) const;
  bool parseBytesArg(const char *args, const char *key, uint8_t out[8], bool &present) const;
  bool parseNameArg(const char *args, char *out, size_t out_len, bool &present) const;
  bool parseFieldSpec(const char *args,
                      const char *start_key,
                      const char *len_key,
                      const char *value_key,
                      uint8_t &start_bit,
                      uint8_t &bit_len,
                      uint64_t &value,
                      bool &present) const;
  bool applyFieldToMaskValue(SignalSlot &slot, uint8_t start_bit, uint8_t bit_len, uint64_t value) const;
  static uint64_t extractFieldLe(const uint8_t data[8], uint8_t start_bit, uint8_t bit_len);
  static bool muxMatches(const SignalSlot &slot, const core::CanFrame &frame);
  void cancelPendingForSlot(uint8_t slot_idx, const char *reason, ToolHost &host);
  void clearSlot(uint8_t slot_idx);
  uint8_t usedCount() const;

  bool handleAdd(const char *args, ToolHost &host);
  bool handleRemove(const char *args, ToolHost &host);
  bool handleClear(ToolHost &host);
  bool handleList(ToolHost &host) const;
  bool handleModify(const char *args, ToolHost &host);
  bool handleField(const char *args, ToolHost &host);
  bool handleBit(const char *args, ToolHost &host);
  bool handleClearBit(const char *args, ToolHost &host);
  bool handleInject(const char *args, ToolHost &host);
  bool handleCancel(ToolHost &host);

  // Returns nullptr on success, otherwise a short reason the slot cannot inject.
  const char *buildFrameForSlot(uint8_t slot_idx, uint32_t now_ms, core::CanFrame &out) const;
};

} // namespace tools
