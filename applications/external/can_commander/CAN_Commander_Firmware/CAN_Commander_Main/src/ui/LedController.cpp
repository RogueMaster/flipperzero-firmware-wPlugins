#include "LedController.h"

namespace ui {

LedController::LedController()
#if BOARDCFG_NEOPIXEL_ENABLED
    : pixels_(BoardConfig::NEOPIXEL_COUNT, BoardConfig::NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800)
    , active_(false)
    ,
#else
    : active_(false)
    ,
#endif
    error_(false)
    , wifi_ap_up_(false)
    , wifi_client_connected_(false)
    , brightness_(5)
    , boot_until_ms_(0)
    , activity_color_(0)
    , activity_until_ms_(0)
    , last_led0_(0xFFFFFFFF)
    , last_led1_(0xFFFFFFFF) {
}

uint32_t LedController::scaledColor(uint8_t r, uint8_t g, uint8_t b) const {
#if BOARDCFG_NEOPIXEL_ENABLED
    return pixels_.Color(
        (uint8_t)((uint16_t)r * brightness_ / 10),
        (uint8_t)((uint16_t)g * brightness_ / 10),
        (uint8_t)((uint16_t)b * brightness_ / 10));
#else
    (void)r;
    (void)g;
    (void)b;
    return 0;
#endif
}

uint32_t LedController::colorIdle() const {
    return scaledColor(18, 18, 18);
}

uint32_t LedController::colorActive() const {
    return scaledColor(0, 48, 0);
}

uint32_t LedController::colorError() const {
    return scaledColor(64, 0, 0);
}

uint32_t LedController::colorWifiAp() const {
    return scaledColor(0, 0, 48);
}

void LedController::begin() {
#if BOARDCFG_NEOPIXEL_ENABLED
    pixels_.begin();
    pixels_.clear();
    pixels_.show();
#endif

    active_ = false;
    error_ = false;
    boot_until_ms_ = millis() + 700;
    activity_color_ = scaledColor(48, 24, 0); // amber boot pulse
    activity_until_ms_ = millis() + 220;
}

void LedController::setToolActive(bool active) {
    active_ = active;
}

void LedController::setError(bool error) {
    error_ = error;
}

void LedController::setWifiApUp(bool up) {
    wifi_ap_up_ = up;
}

void LedController::setWifiClientConnected(bool connected) {
    wifi_client_connected_ = connected;
}

void LedController::setBrightness(uint8_t level) {
    if(level < 1) level = 1;
    if(level > 10) level = 10;
    brightness_ = level;
    // Force redraw on next tick
    last_led0_ = 0xFFFFFFFF;
    last_led1_ = 0xFFFFFFFF;
}

void LedController::noteRx() {
    activity_color_ = scaledColor(0, 0, 64);
    activity_until_ms_ = millis() + 70;
}

void LedController::noteTx() {
    activity_color_ = scaledColor(64, 0, 0);
    activity_until_ms_ = millis() + 70;
}

void LedController::noteDrop() {
    activity_color_ = scaledColor(64, 48, 0);
    activity_until_ms_ = millis() + 150;
}

void LedController::tick() {
#if !BOARDCFG_NEOPIXEL_ENABLED
    return;
#else
    uint32_t led0 = 0;
    uint32_t led1 = 0;

    const uint32_t now = millis();

    if(error_) {
        led0 = colorError();
        if((now / 500) % 2 == 0) {
            led0 = 0;
        }
    } else if(now < boot_until_ms_) {
        led0 = scaledColor(36, 18, 0);
    } else if(active_ && wifi_client_connected_) {
        // Tool active + Wi-Fi client: alternate blue/green
        led0 = ((now / 600) % 2 == 0) ? colorActive() : colorWifiAp();
    } else if(active_) {
        led0 = colorActive();
    } else if(wifi_client_connected_) {
        // No tool, client connected: blink blue
        led0 = ((now / 600) % 2 == 0) ? colorWifiAp() : (uint32_t)0;
    } else if(wifi_ap_up_) {
        // AP up, no client: solid blue
        led0 = colorWifiAp();
    } else {
        led0 = colorIdle();
    }

    if(now < activity_until_ms_) {
        led1 = activity_color_;
    }

    if(BoardConfig::NEOPIXEL_COUNT == 0) {
        return;
    }

    if(last_led0_ != led0 || last_led1_ != led1) {
        pixels_.setPixelColor(0, led0);
        if(BoardConfig::NEOPIXEL_COUNT > 1) {
            pixels_.setPixelColor(1, led1);
        }
        pixels_.show();
        last_led0_ = led0;
        last_led1_ = led1;
    }
#endif
}

} // namespace ui
