#include "SignalDecode.h"

#include <string.h>

namespace decode {

// Intel (little-endian, DBC "@1"): `start_bit` is the least significant bit, numbered
// byte * 8 + bit with bit 0 the LSB of each byte. Bits ascend through the payload.
static bool extractIntelBits(const core::CanFrame &frame, uint8_t start_bit, uint8_t bit_len, uint64_t &raw) {
  if (bit_len == 0 || bit_len > 64) {
    return false;
  }
  if ((uint16_t)start_bit + (uint16_t)bit_len > (uint16_t)frame.dlc * 8U) {
    return false;
  }

  uint64_t blob = 0;
  for (uint8_t i = 0; i < frame.dlc; i++) {
    blob |= ((uint64_t)frame.data[i]) << (8U * i);
  }

  if (bit_len == 64) {
    raw = blob >> start_bit;
  } else {
    const uint64_t mask = ((uint64_t)1 << bit_len) - 1U;
    raw = (blob >> start_bit) & mask;
  }
  return true;
}

// Motorola (big-endian, DBC "@0"): `start_bit` is the most significant bit, using the
// same byte * 8 + bit numbering. Bits run toward the LSB of each byte, then continue
// at bit 7 of the next byte (the DBC "sawtooth" layout).
static bool extractMotorolaBits(const core::CanFrame &frame, uint8_t start_bit, uint8_t bit_len, uint64_t &raw) {
  if (bit_len == 0 || bit_len > 64) {
    return false;
  }

  int bit_index = start_bit;
  uint64_t value = 0;

  for (uint8_t i = 0; i < bit_len; i++) {
    const int byte_index = bit_index / 8;
    const int bit_in_byte = bit_index % 8;
    if (byte_index < 0 || byte_index >= frame.dlc) {
      return false;
    }

    const uint8_t bit = (frame.data[byte_index] >> bit_in_byte) & 0x01;
    value = (value << 1) | bit;

    if (bit_in_byte == 0) {
      bit_index += 15;
    } else {
      bit_index -= 1;
    }
  }

  raw = value;
  return true;
}

static bool extractBits(const core::CanFrame &frame,
                        core::ByteOrder order,
                        uint8_t start_bit,
                        uint8_t bit_len,
                        uint64_t &raw) {
  if (order == core::ByteOrder::INTEL) {
    return extractIntelBits(frame, start_bit, bit_len, raw);
  }
  return extractMotorolaBits(frame, start_bit, bit_len, raw);
}

bool fitsInPayload(core::ByteOrder order, uint8_t start_bit, uint8_t bit_len) {
  core::CanFrame full{};
  full.dlc = 8;
  uint64_t unused = 0;
  return extractBits(full, order, start_bit, bit_len, unused);
}

// The multiplexer field uses the same byte order as the signal it selects. The
// descriptor carries a single order, and a DBC message's mux and signals normally
// share it.
static bool muxMatches(const core::CanFrame &frame, const core::DbcSignalDef &signal) {
  if (!signal.mux_enabled) {
    return true;
  }
  uint64_t mux = 0;
  if (!extractBits(frame, signal.order, signal.mux_start_bit, signal.mux_bit_len, mux)) {
    return false;
  }
  return mux == signal.mux_value;
}

bool extractRawSignal(const core::CanFrame &frame, const core::DbcSignalDef &signal, int64_t &raw_out) {
  uint64_t raw_u = 0;
  if (!extractBits(frame, signal.order, signal.start_bit, signal.bit_len, raw_u)) {
    return false;
  }

  if (signal.sign == core::SignMode::SIGNED && signal.bit_len < 64) {
    const uint64_t sign_bit = (uint64_t)1 << (signal.bit_len - 1);
    if ((raw_u & sign_bit) != 0) {
      const uint64_t extend_mask = ~(((uint64_t)1 << signal.bit_len) - 1U);
      raw_u |= extend_mask;
    }
  }

  // Unsigned values keep their bit pattern; callers reinterpret as uint64_t.
  raw_out = (int64_t)raw_u;
  return true;
}

bool decodeSignal(const core::CanFrame &frame, const core::DbcSignalDef &signal, core::DbcDecodedValue &out) {
  if (frame.rtr) {
    return false;
  }
  if (!muxMatches(frame, signal)) {
    return false;
  }

  int64_t raw = 0;
  if (!extractRawSignal(frame, signal, raw)) {
    return false;
  }

  const float raw_value = (signal.sign == core::SignMode::SIGNED) ? (float)raw : (float)(uint64_t)raw;

  out.sid = signal.sid;
  out.bus = frame.bus;
  out.frame_id = frame.id;
  out.raw = raw;
  out.value = (raw_value * signal.factor) + signal.offset;
  // DBC uses [0|0] to mean "no range specified".
  const bool no_range = (signal.min == 0.0f && signal.max == 0.0f);
  out.in_range = no_range || (out.value >= signal.min && out.value <= signal.max);
  strncpy(out.unit, signal.unit, sizeof(out.unit) - 1);
  out.unit[sizeof(out.unit) - 1] = '\0';
  return true;
}

} // namespace decode
