#pragma once

#include <stdint.h>

namespace core {

enum class StatusCode : uint8_t {
    OK = 0,
    BAD_CMD = 1,
    BAD_ARG = 2,
    BUS_ERR = 3,
    LOCKED = 4,
    NO_MEM = 5,
    CRC_FAIL = 6,
    NOT_ACTIVE = 7,
};

} // namespace core
