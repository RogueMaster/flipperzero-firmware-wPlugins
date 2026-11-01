#pragma once

#include "definitions.h"
#include "register.h"

class Address {
public:
    Address(u16 location) : addr(location) {}
    explicit Address(const RegisterPair& from) : addr(from.value()) {}
    explicit Address(const WordRegister& from) : addr(from.value()) {}

    auto value() const -> u16 { return addr; }

    auto in_range(Address low, Address high) const -> bool {
        return low.value() <= value() && value() <= high.value();
    }

    auto operator==(u16 other) const -> bool { return addr == other; }
    auto operator+(uint other) const -> Address {
        return Address(static_cast<u16>(addr + other));
    }
    auto operator-(uint other) const -> Address {
        return Address(static_cast<u16>(addr - other));
    }

private:
    u16 addr = 0x0;
};
