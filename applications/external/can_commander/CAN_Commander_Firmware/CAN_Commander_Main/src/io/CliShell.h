#pragma once

#include <Arduino.h>

#include "../BoardConfig.h"
#include "../core/RingQueue.h"
#include "../core/Types.h"

namespace io {

class CliShell {
public:
    CliShell();

    void begin(Stream& stream);
    void poll();
    bool popLine(core::CliLine& line);
    // True once per line that was discarded for exceeding CLI_LINE_MAX.
    bool takeOverflow();

private:
    Stream* stream_;
    char line_buf_[BoardConfig::CLI_LINE_MAX];
    uint16_t line_len_;
    bool discarding_;
    bool overflow_pending_;
    core::RingQueue<core::CliLine, 8> queue_;
};

} // namespace io
