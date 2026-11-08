#pragma once

#include "../BoardConfig.h"

#if BOARDCFG_NEOPIXEL_ENABLED
#include <Adafruit_NeoPixel.h>
#endif

namespace ui {

class LedController {
public:
    LedController();

    void begin();
    void setToolActive(bool active);
    void setError(bool error);
    void setWifiApUp(bool up);
    void setWifiClientConnected(bool connected);
    void setBrightness(uint8_t level);

    void noteRx();
    void noteTx();
    void noteDrop();

    void tick();

    bool isActive() const {
        return active_;
    }
    bool isError() const {
        return error_;
    }
    uint8_t brightness() const {
        return brightness_;
    }

private:
    uint32_t scaledColor(uint8_t r, uint8_t g, uint8_t b) const;
    uint32_t colorIdle() const;
    uint32_t colorActive() const;
    uint32_t colorError() const;
    uint32_t colorWifiAp() const;

#if BOARDCFG_NEOPIXEL_ENABLED
    Adafruit_NeoPixel pixels_;
#endif
    bool active_;
    bool error_;
    bool wifi_ap_up_;
    bool wifi_client_connected_;
    uint8_t brightness_;
    uint32_t boot_until_ms_;

    uint32_t activity_color_;
    uint32_t activity_until_ms_;

    uint32_t last_led0_;
    uint32_t last_led1_;
};

} // namespace ui
