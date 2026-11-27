#include <cassert>
#include <cstdio>
#include "power_log.h"

// esp_reset_reason() in the pinned IDF 4.4 says "unknown" for an S3 reset
// from the USB host; the chip's RTC reason (0x15 / 0x16) names it.
static void test_refine() {
    assert(power_log_refine_reset(PWR_RST_UNKNOWN, PWR_S3_RTC_USB_UART, true) == PWR_RST_USB);
    assert(power_log_refine_reset(PWR_RST_UNKNOWN, PWR_S3_RTC_USB_JTAG, true) == PWR_RST_USB);
    // Other S3 reasons IDF 4.4 cannot name stay unknown (eFuse CRC, glitches).
    assert(power_log_refine_reset(PWR_RST_UNKNOWN, 0x13, true) == PWR_RST_UNKNOWN);
    assert(power_log_refine_reset(PWR_RST_UNKNOWN, 0x14, true) == PWR_RST_UNKNOWN);
    assert(power_log_refine_reset(PWR_RST_UNKNOWN, 0x17, true) == PWR_RST_UNKNOWN);
    // A reason esp_reset_reason() named is kept, whatever the RTC says.
    assert(
        power_log_refine_reset(PWR_RST_SOFTWARE, PWR_S3_RTC_USB_UART, true) == PWR_RST_SOFTWARE);
    assert(power_log_refine_reset(PWR_RST_POWERON, 0x01, true) == PWR_RST_POWERON);
    // The classic ESP32 numbers its RTC reasons differently: never refined.
    assert(power_log_refine_reset(PWR_RST_UNKNOWN, 0x15, false) == PWR_RST_UNKNOWN);
    // The dashboard's reset-name list is indexed by these codes.
    assert(PWR_RST_EXTERNAL == 7 && PWR_RST_USB == 8);
}

int main() {
    test_refine();
    puts("Power log tests passed: S3 USB reset refinement");
}
