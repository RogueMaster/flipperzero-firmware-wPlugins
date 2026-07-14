#pragma once

#include <stdint.h>

namespace MyblSave {
constexpr int kStart = 0;
constexpr int kLevel = kStart + (int)sizeof(uint8_t);
constexpr int kCoins = kLevel + (int)sizeof(uint8_t);
constexpr int kCoinsHs = kCoins + (int)sizeof(uint8_t);
constexpr int kScore = kCoinsHs + (int)sizeof(uint8_t);
constexpr int kHScore = kScore + (int)sizeof(uint32_t);
constexpr int kEnd = kHScore + (int)sizeof(uint32_t);
constexpr int kSize = kEnd + (int)sizeof(uint8_t);

static_assert(kSize == 13, "Unexpected Mystic Balloon save size");
} // namespace MyblSave
