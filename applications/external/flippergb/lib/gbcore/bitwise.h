#pragma once

#include "definitions.h"

namespace bitwise {

inline auto compose_bits(const u8 high, const u8 low) -> u8 {
    return static_cast<u8>(high << 1 | low);
}

inline auto compose_nibbles(const u8 high, const u8 low) -> u8 {
    return static_cast<u8>((high << 4) | low);
}

inline auto compose_bytes(const u8 high, const u8 low) -> u16 {
    return static_cast<u16>((high << 8) | low);
}

inline auto check_bit(const u8 value, const u8 bit) -> bool { return (value & (1 << bit)) != 0; }

inline auto bit_value(const u8 value, const u8 bit) -> u8 { return (value >> bit) & 1; }

inline auto set_bit(const u8 value, const u8 bit) -> u8 {
    auto value_set = value | (1 << bit);
    return static_cast<u8>(value_set);
}

inline auto clear_bit(const u8 value, const u8 bit) -> u8 {
    auto value_cleared = value & ~(1 << bit);
    return static_cast<u8>(value_cleared);
}

inline auto set_bit_to(const u8 value, const u8 bit, bool bit_on) -> u8 {
    return bit_on
        ? set_bit(value, bit)
        : clear_bit(value, bit);
}

} // namespace bitwise
