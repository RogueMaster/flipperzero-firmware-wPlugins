#include "display.h"
#include "config.h"
#include <Arduino.h>

#if defined(BOARD_TTGO_DISPLAY)
#include <TFT_eSPI.h>
#include <SPI.h>

static TFT_eSPI g_tft = TFT_eSPI();
static bool     g_enabled = true;
static bool     g_needs_full_clear = true;
static bool     g_awake = true;
static uint8_t  g_brightness = 50;

#define TFT_BL_PWM_CHANNEL 0
#define TFT_BL_PWM_FREQ    5000
#define TFT_BL_PWM_RES     8

void display_init() {
    pinMode(PIN_LCD_POWER, OUTPUT);
    digitalWrite(PIN_LCD_POWER, HIGH);

    g_tft.init();
    g_tft.setRotation(1); // Landscape

    ledcSetup(TFT_BL_PWM_CHANNEL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RES);
    ledcAttachPin(PIN_LCD_BL, TFT_BL_PWM_CHANNEL);
    ledcWrite(TFT_BL_PWM_CHANNEL, (g_brightness * 255) / 100);

    g_tft.fillScreen(TFT_BLUE);
    delay(200);
    g_tft.fillScreen(TFT_NAVY);
    g_needs_full_clear = false;

    g_tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    g_tft.setTextSize(2); // Use large built-in font
    g_tft.setCursor(10, 5);
    g_tft.println("Tesla FSD Unlock");
    g_tft.drawLine(0, 25, 240, 25, TFT_WHITE);
}

void display_set_brightness(uint8_t percentage) {
    if (percentage > 100) percentage = 100;
    g_brightness = percentage;
    if (g_enabled && g_awake) {
        ledcWrite(TFT_BL_PWM_CHANNEL, (g_brightness * 255) / 100);
    }
}

void display_set_enabled(bool enabled) {
    if (g_enabled == enabled) return;
    g_enabled = enabled;
    if (enabled) {
        digitalWrite(PIN_LCD_POWER, HIGH);
        delay(10);
        g_tft.writecommand(0x11); // Wake
        delay(120);
        g_tft.writecommand(0x29); // Display on
        if (g_awake) {
            ledcWrite(TFT_BL_PWM_CHANNEL, (g_brightness * 255) / 100);
        }
        g_needs_full_clear = true;
    } else {
        ledcWrite(TFT_BL_PWM_CHANNEL, 0);
        g_tft.writecommand(0x28); // Display off
        g_tft.writecommand(0x10); // Sleep
        delay(5);
        digitalWrite(PIN_LCD_POWER, LOW);
    }
}

void display_wake() {
    if (g_awake || !g_enabled) return;
    g_awake = true;
    g_tft.writecommand(0x11); // Wake
    delay(120);
    g_tft.writecommand(0x29); // Display on
    ledcWrite(TFT_BL_PWM_CHANNEL, (g_brightness * 255) / 100);
    g_needs_full_clear = true;
}

void display_sleep() {
    if (!g_awake || !g_enabled) return;
    g_awake = false;
    ledcWrite(TFT_BL_PWM_CHANNEL, 0);
    g_tft.writecommand(0x28); // Display off
    g_tft.writecommand(0x10); // Sleep
}

bool display_is_awake() {
    return g_awake;
}

void display_update(const FSDState *state) {
    if (!g_enabled || !g_awake) return;

    static uint32_t last_draw_ms = 0;
    uint32_t now = millis();
    if (now - last_draw_ms < 1000) return;
    last_draw_ms = now;

    if (g_needs_full_clear) {
        g_tft.fillScreen(TFT_NAVY);
        g_tft.setTextColor(TFT_YELLOW, TFT_NAVY);
        g_tft.setTextSize(2);
        g_tft.setCursor(10, 5);
        g_tft.println("Tesla FSD Unlock");
        g_tft.drawLine(0, 25, 240, 25, TFT_WHITE);
        g_needs_full_clear = false;
    }

    // Set text size and background for "flicker-free" overwriting
    g_tft.setTextSize(2);
    g_tft.setTextColor(TFT_WHITE, TFT_NAVY);

    // Position lines explicitly
    g_tft.setCursor(10, 35);
    const char *hw_str = (state->hw_version == TeslaHW_HW4) ? "HW4   " :
                         (state->hw_version == TeslaHW_HW3) ? "HW3   " :
                         (state->hw_version == TeslaHW_Legacy) ? "Legacy" : "Detect";
    g_tft.print("HW: "); g_tft.println(hw_str);

    // FPS next to HW row: HW field ends ~x=130, FPS:1500 (8ch*12=96px) fits to x=240.
    // Use %4u so 4-digit values render in a fixed-width slot without wrapping.
    static uint32_t last_rx = 0;
    uint32_t rx_delta = state->rx_count - last_rx;
    last_rx = state->rx_count;
    if (rx_delta > 9999) rx_delta = 9999;
    g_tft.setCursor(144, 35);
    g_tft.printf("FPS:%4u", rx_delta);

    g_tft.setCursor(10, 55);
    g_tft.print("Mode: ");
    if (state->op_mode == OpMode_Active) {
        g_tft.setTextColor(TFT_GREEN, TFT_NAVY);
        g_tft.println("ACTIVE ");
    } else {
        g_tft.setTextColor(TFT_CYAN, TFT_NAVY);
        g_tft.println("LISTEN ");
    }

    // NAG-killer indicator (right side of Mode row) — Active mode only.
    //   green  = enabled and currently suppressing nags
    //   yellow = enabled but no nag seen yet
    //   grey   = disabled
    // In Listen-Only we never TX so the indicator would be misleading; blank it.
    g_tft.setCursor(192, 55);
    if (state->op_mode == OpMode_Active) {
        if (state->nag_killer) {
            g_tft.setTextColor(state->nag_suppressed ? TFT_GREEN : TFT_YELLOW, TFT_NAVY);
        } else {
            g_tft.setTextColor(TFT_DARKGREY, TFT_NAVY);
        }
        g_tft.print("NAG");
    } else {
        // Erase the slot ("NAG" = 3 chars * 12 px = 36 px wide at text size 2)
        g_tft.fillRect(192, 55, 48, 16, TFT_NAVY);
    }

    g_tft.setTextColor(TFT_WHITE, TFT_NAVY);
    g_tft.setCursor(10, 75);
    g_tft.printf("RX: %-8lu", (unsigned long)state->rx_count);

    // TX = total frames the driver has put on the bus (nag echoes + AP mods + ISA + …),
    // NOT just patched autopilot frames. frames_modified would read 0 in HW=Legacy /
    // HW=Unknown deployments where only the nag killer fires.
    g_tft.setCursor(10, 95);
    g_tft.printf("TX: %-8lu", (unsigned long)state->tx_count);

    // Uptime bottom right
    uint32_t s = now / 1000;
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    uint32_t sc = s % 60;
    g_tft.setTextSize(1);
    g_tft.setTextColor(TFT_LIGHTGREY, TFT_NAVY);
    g_tft.setCursor(150, 115);
    g_tft.printf("%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)sc);

    // Heartbeat dot
    static bool heart = false;
    heart = !heart;
    g_tft.fillCircle(230, 10, 3, heart ? TFT_RED : TFT_DARKGREY);

    if (state->tesla_ota_in_progress) {
        g_tft.setCursor(10, 115);
        g_tft.setTextColor(TFT_RED, TFT_NAVY);
        g_tft.setTextSize(1);
        g_tft.print("OTA ACTIVE!");
    } else {
        // Clear OTA warning area with a blank space
        g_tft.setCursor(10, 115);
        g_tft.print("           ");
    }
}

#else
void display_init() {}
void display_update(const FSDState *) {}
void display_set_enabled(bool) {}
void display_wake() {}
void display_sleep() {}
void display_set_brightness(uint8_t) {}
bool display_is_awake() { return false; }
#endif
