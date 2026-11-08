#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "../BoardConfig.h"
#include "../core/Status.h"
#include "../core/Types.h"

namespace io {

class FlipperProtocol {
 public:
  FlipperProtocol();

  void begin(Stream &stream);
  bool pollPacket(core::FlipperPacket &out);

  bool sendPacket(const core::FlipperPacket &packet);
  bool sendResponse(uint8_t cmd_id,
                    uint8_t seq,
                    core::StatusCode status,
                    const uint8_t *payload = nullptr,
                    uint16_t payload_len = 0);
  bool sendEvent(uint8_t event_id, const uint8_t *payload, uint16_t payload_len);

  static uint16_t crc16CcittFalse(const uint8_t *data, size_t len);
  // Upper bound on bytes written for a packet with `payload_len` bytes of payload
  // (header + CRC, COBS overhead and the 0x00 delimiter).
  static uint16_t maxEncodedSize(uint16_t payload_len) { return (uint16_t)(payload_len + 8 + 4); }
  static constexpr uint8_t kProtocolVersion = 1;
  // Frames discarded because of framing, length, CRC or version errors.
  uint32_t rxErrors() const { return rx_errors_; }

 private:
  bool parseDecodedPacket(const uint8_t *buf, uint16_t len, core::FlipperPacket &out) const;

  static bool cobsEncode(const uint8_t *input,
                         uint16_t input_len,
                         uint8_t *output,
                         uint16_t output_max,
                         uint16_t &output_len);
  static bool cobsDecode(const uint8_t *input,
                         uint16_t input_len,
                         uint8_t *output,
                         uint16_t output_max,
                         uint16_t &output_len);

  Stream *stream_;
  uint8_t rx_encoded_[BoardConfig::MAX_FLIPPER_PAYLOAD + 32];
  uint16_t rx_encoded_len_;
  uint32_t rx_errors_;
};

} // namespace io
