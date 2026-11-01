#pragma once

#include <cstdint>

using uint = unsigned int;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = uint16_t; /* kept as upstream (unused alias) */

struct Noncopyable {
    auto operator=(const Noncopyable&) -> Noncopyable& = delete;
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable() = default;
    ~Noncopyable() = default;
};

template <typename... T> inline void unused(T&&...) {}

/* Logging compiled out for the embedded target */
#define log_error(...) ((void)0)
#define log_warn(...) ((void)0)
#define log_info(...) ((void)0)
#define log_debug(...) ((void)0)
#define log_trace(...) ((void)0)
#define log_unimplemented(...) ((void)0)

/* Fatal errors: platform provides the handler (never returns) */
extern "C" [[noreturn]] void gb_fatal(const char* msg);
#define fatal_error(...) gb_fatal("gb core fatal error")

const uint GAMEBOY_WIDTH = 160;
const uint GAMEBOY_HEIGHT = 144;

const int CLOCK_RATE = 4194304;

/* Shades are plain bytes now: 0=White .. 3=Black */
using Shade = u8;
const Shade SHADE_WHITE = 0;
const Shade SHADE_LIGHT = 1;
const Shade SHADE_DARK = 2;
const Shade SHADE_BLACK = 3;

struct Palette {
    Shade color0 = 0;
    Shade color1 = 1;
    Shade color2 = 2;
    Shade color3 = 3;
};

class Cycles {
public:
    Cycles(uint nCycles) : cycles(nCycles) {}
    const uint cycles;
};
