#include "led.h"
#include "config.h"
#include <Arduino.h>

#if !defined(BOARD_TTGO_DISPLAY) && (PIN_LED >= 0)
#include <Adafruit_NeoPixel.h>

// LED_COUNT defaults to 1 (single SK6812, e.g. ATOM Lite). The ATOM Matrix env
// overrides it to 25 for the 5x5 grid; both share PIN_LED and the GRB SK6812
// driver. Original work by @maslyankov (PR #46).
#ifndef LED_COUNT
#define LED_COUNT 1
#endif

// 25 LEDs at full white pull far more current than a single LED, so the matrix
// runs dimmer to stay within the M5Stack USB regulator's budget.
#if LED_COUNT > 1
#define LED_BRIGHT_NORMAL 8
#define LED_BRIGHT_DIM    2
#define LED_BRIGHT_FULL   40
#else
#define LED_BRIGHT_NORMAL 25
#define LED_BRIGHT_DIM    5
#define LED_BRIGHT_FULL   255
#endif

// Smart LED(s) for M5Stack (Lite single / Matrix 5x5), Lilygo T-CAN485, etc.
static Adafruit_NeoPixel g_strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

static inline void led_fill(uint32_t c) {
    for (uint16_t i = 0; i < LED_COUNT; i++) g_strip.setPixelColor(i, c);
}
#endif

void led_init() {
#if defined(BOARD_TTGO_DISPLAY)
    if (PIN_LED >= 0) {
        pinMode(PIN_LED, OUTPUT);
        digitalWrite(PIN_LED, HIGH); // Off (active-LOW)
    }
#elif PIN_LED < 0
    return;
#else
    g_strip.begin();
    g_strip.setBrightness(LED_BRIGHT_NORMAL);  // keep dim — the ATOM LED is very bright
    g_strip.clear();
    g_strip.show();
#endif
}

void led_set(LedColor color) {
#if defined(BOARD_TTGO_DISPLAY)
    if (PIN_LED < 0) return;
    // Simple LED only supports ON/OFF.
    // We'll use LED_OFF/LED_SLEEP as OFF, others as ON.
    if (color == LED_OFF || color == LED_SLEEP) {
        digitalWrite(PIN_LED, HIGH); // Off
    } else {
        digitalWrite(PIN_LED, LOW);  // On
    }
#elif PIN_LED < 0
    (void)color;
    return;
#else
    uint32_t c;
    if (color == LED_SLEEP) {
        g_strip.setBrightness(LED_BRIGHT_DIM);
        led_fill(g_strip.Color(255, 255, 255));  // dim white
        g_strip.show();
        g_strip.setBrightness(LED_BRIGHT_NORMAL);
        return;
    }
    if (color == LED_WHITE) {
        g_strip.setBrightness(LED_BRIGHT_FULL);
        led_fill(g_strip.Color(255, 255, 255));
        g_strip.show();
        g_strip.setBrightness(LED_BRIGHT_NORMAL);
        return;
    }
    switch (color) {
        case LED_BLUE:   c = g_strip.Color(  0,   0, 255); break;
        case LED_GREEN:  c = g_strip.Color(  0, 255,   0); break;
        case LED_YELLOW: c = g_strip.Color(255, 200,   0); break;
        case LED_RED:    c = g_strip.Color(255,   0,   0); break;
        default:         c = 0;                             break;
    }
    led_fill(c);
    g_strip.show();
#endif
}

void led_factory_blink() {
    for (int i = 0; i < 3; i++) {
        led_set(LED_WHITE);
        delay(300);
        led_set(LED_OFF);
        delay(300);
    }
    led_set(LED_WHITE);  // stay lit to signal armed
}
