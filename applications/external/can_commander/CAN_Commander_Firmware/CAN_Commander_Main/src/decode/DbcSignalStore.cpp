#include "DbcSignalStore.h"

#include <math.h>
#include <string.h>

#include "SignalDecode.h"

namespace decode {

DbcSignalStore::DbcSignalStore() {
  clear();
}

void DbcSignalStore::clear() {
  memset(signals_, 0, sizeof(signals_));
}

bool DbcSignalStore::isValid(const core::DbcSignalDef &signal) {
  if (!core::isValidBusId(signal.bus) || !core::isValidCanId(signal.id, signal.ext)) {
    return false;
  }
  if (signal.order != core::ByteOrder::INTEL && signal.order != core::ByteOrder::MOTOROLA) {
    return false;
  }
  if (signal.sign != core::SignMode::UNSIGNED && signal.sign != core::SignMode::SIGNED) {
    return false;
  }
  if (!fitsInPayload(signal.order, signal.start_bit, signal.bit_len)) {
    return false;
  }
  if (!isfinite(signal.factor) || !isfinite(signal.offset) || !isfinite(signal.min) || !isfinite(signal.max)) {
    return false;
  }
  if (signal.mux_enabled) {
    if (!fitsInPayload(signal.order, signal.mux_start_bit, signal.mux_bit_len)) {
      return false;
    }
    if (signal.mux_bit_len < 64) {
      const uint64_t max_mux = ((uint64_t)1 << signal.mux_bit_len) - 1U;
      if (signal.mux_value > max_mux) {
        return false;
      }
    }
  }
  return true;
}

bool DbcSignalStore::add(const core::DbcSignalDef &signal) {
  if (!isValid(signal)) {
    return false;
  }

  for (uint16_t i = 0; i < BoardConfig::MAX_DBC_SIGNALS; i++) {
    if (signals_[i].used && signals_[i].sid == signal.sid) {
      signals_[i] = signal;
      signals_[i].used = true;
      return true;
    }
  }

  for (uint16_t i = 0; i < BoardConfig::MAX_DBC_SIGNALS; i++) {
    if (!signals_[i].used) {
      signals_[i] = signal;
      signals_[i].used = true;
      return true;
    }
  }

  return false;
}

bool DbcSignalStore::removeBySid(uint16_t sid) {
  for (uint16_t i = 0; i < BoardConfig::MAX_DBC_SIGNALS; i++) {
    if (signals_[i].used && signals_[i].sid == sid) {
      memset(&signals_[i], 0, sizeof(signals_[i]));
      return true;
    }
  }
  return false;
}

uint16_t DbcSignalStore::count() const {
  uint16_t c = 0;
  for (uint16_t i = 0; i < BoardConfig::MAX_DBC_SIGNALS; i++) {
    if (signals_[i].used) {
      c++;
    }
  }
  return c;
}

bool DbcSignalStore::getByIndex(uint16_t index, core::DbcSignalDef &out) const {
  uint16_t seen = 0;
  for (uint16_t i = 0; i < BoardConfig::MAX_DBC_SIGNALS; i++) {
    if (!signals_[i].used) {
      continue;
    }
    if (seen == index) {
      out = signals_[i];
      return true;
    }
    seen++;
  }
  return false;
}

uint16_t DbcSignalStore::findMatches(const core::CanFrame &frame,
                                     const core::DbcSignalDef **out,
                                     uint16_t out_capacity) const {
  uint16_t matched = 0;
  for (uint16_t i = 0; i < BoardConfig::MAX_DBC_SIGNALS; i++) {
    const core::DbcSignalDef &s = signals_[i];
    if (!s.used) {
      continue;
    }
    if ((s.bus != core::BusId::BOTH && s.bus != frame.bus) || s.id != frame.id || s.ext != frame.ext) {
      continue;
    }
    if (matched < out_capacity && out != nullptr) {
      out[matched] = &signals_[i];
    }
    matched++;
  }
  return matched;
}

} // namespace decode
