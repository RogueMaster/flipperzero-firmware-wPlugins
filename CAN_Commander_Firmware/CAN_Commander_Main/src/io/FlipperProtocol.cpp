#include "FlipperProtocol.h"

#include <string.h>

namespace io {

FlipperProtocol::FlipperProtocol() : stream_(nullptr), rx_encoded_len_(0), rx_errors_(0) {
  memset(rx_encoded_, 0, sizeof(rx_encoded_));
}

void FlipperProtocol::begin(Stream &stream) {
  stream_ = &stream;
  rx_encoded_len_ = 0;
}

uint16_t FlipperProtocol::crc16CcittFalse(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x8000) != 0) {
        crc = (uint16_t)((crc << 1) ^ 0x1021);
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}

bool FlipperProtocol::cobsEncode(const uint8_t *input,
                                 uint16_t input_len,
                                 uint8_t *output,
                                 uint16_t output_max,
                                 uint16_t &output_len) {
  if (input == nullptr || output == nullptr || output_max == 0) {
    return false;
  }

  uint16_t read_index = 0;
  uint16_t write_index = 1;
  uint16_t code_index = 0;
  uint8_t code = 1;

  while (read_index < input_len) {
    if (input[read_index] == 0) {
      if (code_index >= output_max) {
        return false;
      }
      output[code_index] = code;
      code = 1;
      code_index = write_index;
      write_index++;
      if (write_index > output_max) {
        return false;
      }
      read_index++;
    } else {
      if (write_index >= output_max) {
        return false;
      }
      output[write_index++] = input[read_index++];
      code++;
      if (code == 0xFF) {
        if (code_index >= output_max) {
          return false;
        }
        output[code_index] = code;
        code = 1;
        code_index = write_index;
        write_index++;
        if (write_index > output_max) {
          return false;
        }
      }
    }
  }

  if (code_index >= output_max) {
    return false;
  }
  output[code_index] = code;
  output_len = write_index;
  return true;
}

bool FlipperProtocol::cobsDecode(const uint8_t *input,
                                 uint16_t input_len,
                                 uint8_t *output,
                                 uint16_t output_max,
                                 uint16_t &output_len) {
  if (input == nullptr || output == nullptr || input_len == 0) {
    return false;
  }

  uint16_t read_index = 0;
  uint16_t write_index = 0;

  while (read_index < input_len) {
    uint8_t code = input[read_index];
    if (code == 0) {
      return false;
    }
    read_index++;

    for (uint8_t i = 1; i < code; i++) {
      if (read_index >= input_len || write_index >= output_max) {
        return false;
      }
      output[write_index++] = input[read_index++];
    }

    if (code < 0xFF && read_index < input_len) {
      if (write_index >= output_max) {
        return false;
      }
      output[write_index++] = 0;
    }
  }

  output_len = write_index;
  return true;
}

bool FlipperProtocol::parseDecodedPacket(const uint8_t *buf, uint16_t len, core::FlipperPacket &out) const {
  if (buf == nullptr || len < 8) {
    return false;
  }

  const uint16_t payload_len = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
  const uint16_t expected_len = (uint16_t)(6 + payload_len + 2);
  if (len != expected_len || payload_len > BoardConfig::MAX_FLIPPER_PAYLOAD) {
    return false;
  }

  const uint16_t packet_crc = (uint16_t)buf[6 + payload_len] | ((uint16_t)buf[6 + payload_len + 1] << 8);
  const uint16_t calc_crc = crc16CcittFalse(buf, 6 + payload_len);
  if (packet_crc != calc_crc) {
    return false;
  }
  // A different protocol version may lay out payloads differently; never act on it.
  if (buf[0] != kProtocolVersion) {
    return false;
  }

  out.ver = buf[0];
  out.kind = static_cast<core::FlipperPacketKind>(buf[1]);
  out.id = buf[2];
  out.seq = buf[3];
  out.len = payload_len;
  if (payload_len > 0) {
    memcpy(out.payload, &buf[6], payload_len);
  }
  return true;
}

bool FlipperProtocol::pollPacket(core::FlipperPacket &out) {
  if (stream_ == nullptr) {
    return false;
  }

  while (stream_->available() > 0) {
    int c = stream_->read();
    if (c < 0) {
      break;
    }

    const uint8_t b = static_cast<uint8_t>(c);
    if (b == 0) {
      if (rx_encoded_len_ == 0) {
        continue;
      }

      uint8_t decoded[BoardConfig::MAX_FLIPPER_PAYLOAD + 32];
      uint16_t decoded_len = 0;
      const bool ok = cobsDecode(rx_encoded_, rx_encoded_len_, decoded, sizeof(decoded), decoded_len) &&
                      parseDecodedPacket(decoded, decoded_len, out);
      rx_encoded_len_ = 0;
      if (ok) {
        return true;
      }
      rx_errors_++;
      continue;
    }

    if (rx_encoded_len_ < sizeof(rx_encoded_)) {
      rx_encoded_[rx_encoded_len_++] = b;
    } else {
      rx_encoded_len_ = 0;
      rx_errors_++;
    }
  }

  return false;
}

bool FlipperProtocol::sendPacket(const core::FlipperPacket &packet) {
  if (stream_ == nullptr || packet.len > BoardConfig::MAX_FLIPPER_PAYLOAD) {
    return false;
  }

  uint8_t raw[BoardConfig::MAX_FLIPPER_PAYLOAD + 32];
  raw[0] = packet.ver;
  raw[1] = static_cast<uint8_t>(packet.kind);
  raw[2] = packet.id;
  raw[3] = packet.seq;
  raw[4] = (uint8_t)(packet.len & 0xFF);
  raw[5] = (uint8_t)((packet.len >> 8) & 0xFF);
  if (packet.len > 0) {
    memcpy(&raw[6], packet.payload, packet.len);
  }

  const uint16_t crc = crc16CcittFalse(raw, 6 + packet.len);
  raw[6 + packet.len] = (uint8_t)(crc & 0xFF);
  raw[6 + packet.len + 1] = (uint8_t)((crc >> 8) & 0xFF);

  uint8_t encoded[BoardConfig::MAX_FLIPPER_PAYLOAD + 40];
  uint16_t encoded_len = 0;
  if (!cobsEncode(raw, (uint16_t)(6 + packet.len + 2), encoded, sizeof(encoded), encoded_len)) {
    return false;
  }

  stream_->write(encoded, encoded_len);
  stream_->write((uint8_t)0x00);
  return true;
}

bool FlipperProtocol::sendResponse(uint8_t cmd_id,
                                   uint8_t seq,
                                   core::StatusCode status,
                                   const uint8_t *payload,
                                   uint16_t payload_len) {
  if (payload_len > (BoardConfig::MAX_FLIPPER_PAYLOAD - 1)) {
    return false;
  }

  core::FlipperPacket pkt{};
  pkt.ver = kProtocolVersion;
  pkt.kind = core::FlipperPacketKind::RESP;
  pkt.id = cmd_id;
  pkt.seq = seq;
  pkt.len = (uint16_t)(payload_len + 1);
  pkt.payload[0] = static_cast<uint8_t>(status);
  if (payload_len > 0 && payload != nullptr) {
    memcpy(&pkt.payload[1], payload, payload_len);
  }
  return sendPacket(pkt);
}

bool FlipperProtocol::sendEvent(uint8_t event_id, const uint8_t *payload, uint16_t payload_len) {
  if (payload_len > BoardConfig::MAX_FLIPPER_PAYLOAD) {
    return false;
  }

  core::FlipperPacket pkt{};
  pkt.ver = kProtocolVersion;
  pkt.kind = core::FlipperPacketKind::EVENT;
  pkt.id = event_id;
  pkt.seq = 0;
  pkt.len = payload_len;
  if (payload_len > 0 && payload != nullptr) {
    memcpy(pkt.payload, payload, payload_len);
  }
  return sendPacket(pkt);
}

} // namespace io
