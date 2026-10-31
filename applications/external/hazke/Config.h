#pragma once
#include <stdint.h>

namespace Config {
// Release version: update here only. The title screen reads it.
constexpr const char* VersionTag = "v1.0";

constexpr int ScreenW = 128;
constexpr int ScreenH = 64;

// 3D view on top, one-line HUD strip underneath.
constexpr int ViewX = 0;
constexpr int ViewY = 0;
constexpr int ViewW = 128;
constexpr int ViewH = 54;
constexpr int HudY = 55;

// ~60 fps. Text menus redraw every MenuFrameDiv-th frame, or at once on
// a button press: font drawing is the costly part of a menu frame.
constexpr uint32_t FrameMs = 16;
constexpr uint32_t MenuFrameDiv = 3;

// Holding BACK this long in flight opens the menu; a shorter tap steps
// the throttle.
constexpr uint32_t BackHoldMs = 400;

// Two OK presses this close together fire a missile.
constexpr uint32_t DoubleClickMs = 300;
}

// Set to 1 to build a capture version that records the screen to the SD
// card for screenshots and clips (see Capture.h). Release builds leave it 0.
#ifndef HAZKE_CAPTURE
#define HAZKE_CAPTURE 0
#endif
