#pragma once

#include <gui/gui.h>
#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Draw BunnyConnect logo
 * @param canvas Canvas to draw on
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_logo(Canvas* canvas, uint8_t x, uint8_t y);

/**
 * @brief Draw connection status
 * @param canvas Canvas to draw on
 * @param connected Connection status
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_connection_status(Canvas* canvas, bool connected, uint8_t x, uint8_t y);

/**
 * @brief Draw signal strength indicator
 * @param canvas Canvas to draw on
 * @param strength Signal strength (0-100%)
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_signal_strength(Canvas* canvas, uint8_t strength, uint8_t x, uint8_t y);

/**
 * @brief Draw loading animation
 * @param canvas Canvas to draw on
 * @param frame Animation frame number
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_loading_animation(Canvas* canvas, uint8_t frame, uint8_t x, uint8_t y);

/**
 * @brief Draw battery status indicator
 * @param canvas Canvas to draw on
 * @param percentage Battery percentage (0-100)
 * @param charging Whether battery is charging
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_battery_status(
    Canvas* canvas,
    uint8_t percentage,
    bool charging,
    uint8_t x,
    uint8_t y);

/**
 * @brief Draw menu item with optional selection highlight
 * @param canvas Canvas to draw on
 * @param text Text to display
 * @param selected Whether item is selected
 * @param x X coordinate
 * @param y Y coordinate
 */
void bunnyconnect_draw_menu_item(
    Canvas* canvas,
    const char* text,
    bool selected,
    uint8_t x,
    uint8_t y);

#ifdef __cplusplus
}
#endif
