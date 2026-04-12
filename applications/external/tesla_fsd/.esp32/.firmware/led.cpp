#include "led.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

// M5Stack ATOM Lite: single SK6812 (GRB order) on PIN_LED (GPIO27)
static Adafruit_NeoPixel g_strip(1, PIN_LED, NEO_GRB + NEO_KHZ800);

void led_init() {
    g_strip.begin();
    g_strip.setBrightness(25);  // keep dim — the ATOM LED is very bright
    g_strip.clear();
    g_strip.show();
}

void led_set(LedColor color) {
    uint32_t c;
    switch (color) {
        case LED_BLUE:   c = g_strip.Color(  0,   0, 255); break;
        case LED_GREEN:  c = g_strip.Color(  0, 255,   0); break;
        case LED_YELLOW: c = g_strip.Color(255, 200,   0); break;
        case LED_RED:    c = g_strip.Color(255,   0,   0); break;
        default:         c = 0;                             break;
    }
    g_strip.setPixelColor(0, c);
    g_strip.show();
}
