#pragma once

#include <stdint.h>

#include "../BoardConfig.h"
#include "../core/Types.h"

namespace decode {

class DbcSignalStore {
 public:
  DbcSignalStore();

  // True when the descriptor describes a decodable signal: valid bus, ID, byte order,
  // sign, geometry inside 8 bytes (for both signal and mux) and finite scaling.
  static bool isValid(const core::DbcSignalDef &signal);

  void clear();
  // Adds or replaces (same sid) a descriptor. Returns false if it is invalid or the
  // store is full; call isValid() first to tell the two apart.
  bool add(const core::DbcSignalDef &signal);
  bool removeBySid(uint16_t sid);

  uint16_t count() const;
  bool getByIndex(uint16_t index, core::DbcSignalDef &out) const;

  uint16_t findMatches(const core::CanFrame &frame,
                       const core::DbcSignalDef **out,
                       uint16_t out_capacity) const;

 private:
  core::DbcSignalDef signals_[BoardConfig::MAX_DBC_SIGNALS];
};

} // namespace decode
