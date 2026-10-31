#pragma once
#include "Galaxy.h"
#include "GameState.h"

// Hyperspace mechanics.
//
// Jumps cost credits proportional to distance, charged in tenths-CR.
// Destinations are limited to direct gate neighbours — the chart's
// adjacency graph is the only path between systems.

namespace Hyperspace {

constexpr int CreditsPerLY = 100; // tenths-CR — 10.0 CR per LY of jump

// Tenths-CR cost to jump from `fromIdx` to `toIdx`.
inline int jumpCostTenths(int fromIdx, int toIdx) {
    float ly = Galaxy::distanceLY(fromIdx, toIdx);
    int cost = (int)(ly * (float)CreditsPerLY + 0.5f);
    if(cost < 10) cost = 10; // floor so a free-feeling jump still bills 1.0 CR
    return cost;
}

// A destination is reachable only if there's a direct gate link from
// the current system — no skipping the graph by paying more.
inline bool inJumpRange(int fromIdx, int toIdx) {
    return Galaxy::isGateNeighbor(fromIdx, toIdx);
}

inline bool canJump(const GameState& s, int fromIdx, int toIdx) {
    if(!inJumpRange(fromIdx, toIdx)) return false;
    return s.credits >= jumpCostTenths(fromIdx, toIdx);
}

} // namespace Hyperspace
