#pragma once

#include "ToolBase.h"

namespace tools {

class ObdPidTool : public ToolBase {
 public:
  ObdPidTool();

  core::ToolId id() const override { return core::ToolId::OBD_PID; }
  const char *name() const override { return "obd_pid"; }

  bool start(const char *args, ToolHost &host) override;
  void stop(ToolHost &host) override;
  void onFrame(const core::CanFrame &frame, ToolHost &host) override;
  void tick(ToolHost &host) override;
  bool configure(const char *args, ToolHost &host) override;
  void describe(char *out, size_t out_len) const override;

 private:
  enum class RequestKind : uint8_t {
    MODE01_PID = 0,
    VIN = 1,
    DTC_STORED = 2,
    DTC_PENDING = 3,
    DTC_PERMANENT = 4,
    DTC_CLEAR = 5,
    DTC_ALL = 6,
  };

  struct IsoTpRx {
    bool active;
    uint32_t ecu_id;
    uint16_t total_len;
    uint16_t collected_len;
    uint8_t next_seq;
    uint8_t payload[96];
  };

  // OBD response IDs 0x7E8-0x7EF; one reassembly buffer per responding ECU.
  static constexpr uint32_t kFirstResponseId = 0x7E8;
  static constexpr uint8_t kMaxResponders = 8;
  static constexpr uint8_t kMaxSendFailures = 5;
  static constexpr uint32_t kSendRetryMs = 300;
  static constexpr uint32_t kMaxIntervalMs = 86400000UL;

  bool sendCurrentRequest(ToolHost &host, uint32_t now_ms);
  bool sendFlowControl(const core::CanFrame &first_frame, ToolHost &host);
  void resetIsoTp();
  IsoTpRx *isoTpFor(uint32_t ecu_id);
  void onSingleFrame(const core::CanFrame &frame, ToolHost &host);
  void onFirstFrame(const core::CanFrame &frame, ToolHost &host);
  void onConsecutiveFrame(const core::CanFrame &frame, ToolHost &host);
  void processPayload(const uint8_t *payload, uint16_t payload_len, const core::CanFrame &frame, ToolHost &host);
  void advanceDtcAll(ToolHost &host, bool due_to_timeout);

  void emitParsedMode01(const uint8_t *payload, uint16_t payload_len, ToolHost &host);
  void emitParsedVin(const uint8_t *payload, uint16_t payload_len, ToolHost &host);
  void emitParsedDtc(const uint8_t *payload, uint16_t payload_len, uint32_t ecu_resp_id, ToolHost &host);
  static void formatDtcCode(uint8_t hi, uint8_t lo, char *out, size_t out_len);
  const char *dtcLabelForCurrentService() const;
  const char *requestName() const;

  core::BusId bus_;
  RequestKind request_kind_;
  uint8_t pid_;
  uint8_t service_;
  uint8_t expected_resp_service_;
  uint32_t request_id_;
  bool one_shot_;
  bool request_sent_;
  bool awaiting_response_;
  uint32_t interval_ms_;
  uint32_t next_send_ms_;
  uint32_t request_sent_ms_;
  uint8_t dtc_all_index_;
  bool dtc_all_running_;
  bool dtc_any_response_;
  uint32_t dtc_last_response_ms_;
  uint16_t dtc_responder_mask_;
  uint8_t send_failures_;

  IsoTpRx isotp_rx_[kMaxResponders];
};

} // namespace tools
