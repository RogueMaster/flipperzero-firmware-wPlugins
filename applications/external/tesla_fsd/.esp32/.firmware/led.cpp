#include "led.h"
#include "config.h"
#include <Arduino.h>

#if !defined(BOARD_TTGO_DISPLAY)
#include <Adafruit_NeoPixel.h>

// Smart LED for M5Stack, Lilygo T-CAN485, etc.
static Adafruit_NeoPixel g_strip(1, PIN_LED, NEO_GRB + NEO_KHZ800);
#endif

void led_init() {
#if defined(BOARD_TTGO_DISPLAY)
    if (PIN_LED >= 0) {
        pinMode(PIN_LED, OUTPUT);
        digitalWrite(PIN_LED, HIGH); // Off (active-LOW)
    }
#else
    g_strip.begin();
    g_strip.setBrightness(25);  // keep dim — the ATOM LED is very bright
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
#else
    uint32_t c;
    if (color == LED_SLEEP) {
        g_strip.setBrightness(5);
        g_strip.setPixelColor(0, g_strip.Color(255, 255, 255));  // dim white
        g_strip.show();
        g_strip.setBrightness(25);
        return;
    }
    if (color == LED_WHITE) {
        g_strip.setBrightness(255);
        g_strip.setPixelColor(0, g_strip.Color(255, 255, 255));
        g_strip.show();
        g_strip.setBrightness(25);
        return;
    }
    switch (color) {
        case LED_BLUE:   c = g_strip.Color(  0,   0, 255); break;
        case LED_GREEN:  c = g_strip.Color(  0, 255,   0); break;
        case LED_YELLOW: c = g_strip.Color(255, 200,   0); break;
        case LED_RED:    c = g_strip.Color(255,   0,   0); break;
        default:         c = 0;                             break;
    }
    g_strip.setPixelColor(0, c);
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
