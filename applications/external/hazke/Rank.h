#pragma once
#include "Platform.h"
#include <stdint.h>
#include "GameState.h"
#include "Audio.h"

// Refit R24: rank ladder.
//
// Seven ranks gated by total kills. Thresholds match the README spec:
//   Harmless 0..7  -> Mostly Harmless 8..15 -> Poor 16..63 ->
//   Average 64..127 -> Competent 128..511 -> Dangerous 512..2559 ->
//   Deadly 2560+.
//
// `checkPromotion()` is called after every kill credit in Combat / Missile;
// if the player crossed a threshold, a short-lived `toast` timer is armed
// and the player's last-seen rank is bumped so we don't double-toast.
// SystemFlight reads the toast in `renderHUD` to splash a "PROMOTED"
// banner; Status reads `nameFor` / `colorFor` to paint the row.

namespace Rank {

constexpr int N = 7;

enum Id : uint8_t {
    Harmless = 0,
    MostlyHarmless,
    Poor,
    Average,
    Competent,
    Dangerous,
    Deadly,
};

// Threshold to enter each rank (kills required). Index N matches Id.
constexpr int Thresholds[N] = {
    0,
    8,
    16,
    64,
    128,
    512,
    2560,
};

inline const char* nameFor(Id r) {
    switch(r) {
    case Harmless:
        return "HARMLESS";
    case MostlyHarmless:
        return "MOSTLY HARMLESS";
    case Poor:
        return "POOR";
    case Average:
        return "AVERAGE";
    case Competent:
        return "COMPETENT";
    case Dangerous:
        return "DANGEROUS";
    case Deadly:
        return "DEADLY";
    }
    return "?";
}

// Color tier — cool / dim at the bottom, hot / bright at the top.
inline uint16_t colorFor(Id r) {
    switch(r) {
    case Harmless:
        return 0x6B4D; // dim grey
    case MostlyHarmless:
        return 0xAD55; // light grey
    case Poor:
        return 0xFFE0; // yellow
    case Average:
        return 0xFD20; // orange
    case Competent:
        return 0xF800; // red
    case Dangerous:
        return 0xF81F; // magenta
    case Deadly:
        return 0x07FF; // cyan (apex)
    }
    return 0xFFFF;
}

inline Id forKills(int kills) {
    if(kills < 0) kills = 0;
    Id r = Harmless;
    for(int i = N - 1; i >= 0; i--) {
        if(kills >= Thresholds[i]) {
            r = (Id)i;
            break;
        }
    }
    return r;
}

// --- Promotion toast state ---

inline float toast = 0.0f; // s remaining on the "PROMOTED" banner
inline uint8_t toastRank = Harmless;

inline void resetToast() {
    toast = 0.0f;
    toastRank = Harmless;
}

// Call after `game.kills` increments. If the player crossed into a higher
// rank than `game.lastSeenRank`, arms the toast and bumps the marker so
// each rank fires its banner exactly once.
inline void checkPromotion(GameState& g) {
    Id cur = forKills(g.kills);
    if((uint8_t)cur > g.lastSeenRank) {
        g.lastSeenRank = (uint8_t)cur;
        toast = 3.0f;
        toastRank = (uint8_t)cur;
        Audio::rankPromote(); // R28: fanfare on tier crossings
    }
}

inline void tickToast(float dt) {
    if(toast > 0.0f) {
        toast -= dt;
        if(toast < 0.0f) toast = 0.0f;
    }
}

} // namespace Rank
