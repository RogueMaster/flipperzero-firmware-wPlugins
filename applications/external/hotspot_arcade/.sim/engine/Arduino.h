// Minimal Arduino compatibility for building ha_games.h off-target.
// Scope is set by what the engine actually uses, nothing more:
//   String (.c_str() + concatenation), millis(), random(), strlcpy().
// String is backed by std::string rather than vendoring Arduino's WString,
// which is LGPL and not worth the licensing question for a dev tool.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

class String {
public:
    String() {
    }
    String(const char* s)
        : _s(s ? s : "") {
    }
    String(const std::string& s)
        : _s(s) {
    }
    String(char c)
        : _s(1, c) {
    }
    String(int v)
        : _s(std::to_string(v)) {
    }
    String(unsigned v)
        : _s(std::to_string(v)) {
    }
    String(long v)
        : _s(std::to_string(v)) {
    }
    String(unsigned long v)
        : _s(std::to_string(v)) {
    }

    const char* c_str() const {
        return _s.c_str();
    }
    size_t length() const {
        return _s.size();
    }
    const std::string& str() const {
        return _s;
    }
    void reserve(size_t n) {
        _s.reserve(n);
    }

    String& operator+=(const String& o) {
        _s += o._s;
        return *this;
    }
    friend String operator+(String a, const String& b) {
        a += b;
        return a;
    }

private:
    std::string _s;
};

// The caller owns the clock so tests can step time deterministically instead of
// sleeping. Defined in ha_sim.cpp, where ha_tick() sets it before each engine tick.
uint32_t millis();

inline long random(long howbig) {
    return howbig > 0 ? (long)(::rand() % howbig) : 0;
}
inline long random() {
    return (long)::rand();
}

// ESP-IDF's hardware RNG (esp_system.h), pulled in transitively via Arduino.h on
// real hardware. ha_games.h calls it directly for Fisher-Yates shuffles.
inline uint32_t esp_random() {
    return (uint32_t)::rand();
}

#ifndef strlcpy
inline size_t strlcpy(char* dst, const char* src, size_t n) {
    size_t len = std::strlen(src);
    if(n) {
        size_t c = len < n - 1 ? len : n - 1;
        std::memcpy(dst, src, c);
        dst[c] = '\0';
    }
    return len;
}
#endif
