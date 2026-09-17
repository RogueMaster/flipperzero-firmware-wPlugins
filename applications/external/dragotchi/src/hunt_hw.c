#include "hunt_hw.h"
#include <furi.h>
#include <lib/subghz/devices/devices.h>
#include "tuning.h"

/* Real sub-GHz airwave sensing (receive-only, no TX -> region-limit safe).
 * Note: subghz_devices_begin() returns false from a plain FAP but the radio
 * still works, so its return is intentionally ignored (verified by spike).
 * Requires an adequate thread stack (see secondary_thread stack size). */
static const uint32_t BAND_FREQ[HUNT_BANDS] = {315000000u, 433920000u, 868350000u, 915000000u};

void hunt_sense(uint8_t *activity_out, uint8_t *band_out) {
    uint8_t best_band = 1;
    float best = -127.0f, sum = 0.0f;
    int counted = 0;

    subghz_devices_init();
    const SubGhzDevice *dev = subghz_devices_get_by_name("cc1101_int");
    if(dev) {
        subghz_devices_begin(dev); // returns false but the device is usable
        subghz_devices_reset(dev);
        subghz_devices_load_preset(dev, FuriHalSubGhzPresetOok650Async, NULL);
        for(uint8_t b = 0; b < HUNT_BANDS; b++) {
            if(!subghz_devices_is_frequency_valid(dev, BAND_FREQ[b])) continue;
            subghz_devices_set_frequency(dev, BAND_FREQ[b]);
            subghz_devices_flush_rx(dev);
            subghz_devices_set_rx(dev);
            float peak = -127.0f;
            for(int i = 0; i < 6; i++) {
                furi_delay_ms(2);
                float r = subghz_devices_get_rssi(dev);
                if(r > peak) peak = r;
            }
            subghz_devices_idle(dev);
            if(peak > best) { best = peak; best_band = b; }
            sum += peak;
            counted++;
        }
        subghz_devices_sleep(dev);
        subghz_devices_end(dev);
    }
    subghz_devices_deinit();

    // Normalise average RSSI (~ -110 quiet .. -30 loud dBm) to 0..100.
    float avg = counted ? (sum / counted) : -110.0f;
    int act = (int)((avg + 110.0f) * 100.0f / 80.0f);
    if(act < 0) act = 0;
    if(act > 100) act = 100;
    *activity_out = (uint8_t)act;
    *band_out = best_band;
}
