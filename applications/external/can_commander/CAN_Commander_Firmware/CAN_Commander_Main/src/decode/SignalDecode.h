#pragma once

#include "../core/Types.h"

namespace decode {

// True when a field of `bit_len` bits starting at `start_bit` (DBC numbering for the
// given byte order) lies entirely inside an 8-byte payload.
bool fitsInPayload(core::ByteOrder order, uint8_t start_bit, uint8_t bit_len);

bool extractRawSignal(const core::CanFrame &frame, const core::DbcSignalDef &signal, int64_t &raw_out);
bool decodeSignal(const core::CanFrame &frame, const core::DbcSignalDef &signal, core::DbcDecodedValue &out);

} // namespace decode
