#pragma once

#include "definitions.h"

/* Devirtualized registers: on the original design every register access was
 * a virtual call and every 1-byte register carried an 8-byte vtable pointer.
 * On a 64 MHz Cortex-M4 that overhead matters, so everything here is plain
 * inline code. The flag register masking (lower nibble always 0) is handled
 * by FlagRegister's shadowing set() and by RegisterPair's low_mask. */

class ByteRegister {
public:
    ByteRegister() = default;

    void set(u8 new_value) { val = new_value; }
    void reset() { val = 0; }
    auto value() const -> u8 { return val; }

    auto check_bit(u8 bit) const -> bool { return (val & (1 << bit)) != 0; }
    void set_bit_to(u8 bit, bool set) {
        if(set)
            val = static_cast<u8>(val | (1 << bit));
        else
            val = static_cast<u8>(val & ~(1 << bit));
    }

    void increment() { val++; }
    void decrement() { val--; }

    auto operator==(u8 other) const -> bool { return val == other; }

protected:
    u8 val = 0x0;
};

class FlagRegister : public ByteRegister {
public:
    FlagRegister() = default;

    /* lower nibble of F is always 0 */
    void set(u8 new_value) { val = static_cast<u8>(new_value & 0xF0); }

    void set_flag_zero(bool set) { set_bit_to(7, set); }
    void set_flag_subtract(bool set) { set_bit_to(6, set); }
    void set_flag_half_carry(bool set) { set_bit_to(5, set); }
    void set_flag_carry(bool set) { set_bit_to(4, set); }

    auto flag_zero() const -> bool { return check_bit(7); }
    auto flag_subtract() const -> bool { return check_bit(6); }
    auto flag_half_carry() const -> bool { return check_bit(5); }
    auto flag_carry() const -> bool { return check_bit(4); }

    auto flag_zero_value() const -> u8 { return static_cast<u8>((val >> 7) & 1); }
    auto flag_subtract_value() const -> u8 { return static_cast<u8>((val >> 6) & 1); }
    auto flag_half_carry_value() const -> u8 { return static_cast<u8>((val >> 5) & 1); }
    auto flag_carry_value() const -> u8 { return static_cast<u8>((val >> 4) & 1); }
};

class WordRegister {
public:
    WordRegister() = default;

    void set(u16 new_value) { val = new_value; }

    auto value() const -> u16 { return val; }

    auto low() const -> u8 { return static_cast<u8>(val); }
    auto high() const -> u8 { return static_cast<u8>(val >> 8); }

    void increment() { val++; }
    void decrement() { val--; }

private:
    u16 val = 0x0;
};

class RegisterPair {
public:
    /* mask_low is 0xF0 for AF (F's lower nibble reads/writes as 0) */
    RegisterPair(ByteRegister& high, ByteRegister& low, u8 mask_low = 0xFF)
        : low_byte(low)
        , high_byte(high)
        , low_mask(mask_low) {}

    void set(u16 word) {
        low_byte.set(static_cast<u8>(word & low_mask));
        high_byte.set(static_cast<u8>(word >> 8));
    }

    auto value() const -> u16 {
        return static_cast<u16>((high_byte.value() << 8) | (low_byte.value() & low_mask));
    }

    auto low() const -> u8 { return static_cast<u8>(low_byte.value() & low_mask); }
    auto high() const -> u8 { return high_byte.value(); }

    void increment() { set(static_cast<u16>(value() + 1)); }
    void decrement() { set(static_cast<u16>(value() - 1)); }

private:
    ByteRegister& low_byte;
    ByteRegister& high_byte;
    u8 low_mask;
};
