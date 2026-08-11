#include "meshcore_detect.h"

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <furi_hal_rtc.h>

#define TAG "MeshCoreDetect"

/* Block A, pin 12 = PA13 = SWDIO. Block B, pin 17 = PB14 = the 1-Wire pin. */
static const GpioPin* const detect_a = &gpio_swdio;
static const GpioPin* const detect_b = &gpio_ibutton;

bool meshcore_detect_conflicts_with_debug(void) {
    /* With Debug on, PA13/PA14 belong to the SWD peripheral. We still drive the
     * pin -- it may well work -- but we must not claim it as reliable. */
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug);
}

void meshcore_detect_init(void) {
    furi_hal_gpio_init(detect_a, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(detect_b, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);

    furi_hal_gpio_write(detect_a, true);
    furi_hal_gpio_write(detect_b, true);

    MeshCoreDetectState state = meshcore_detect_state();
    FURI_LOG_I(
        TAG,
        "detect: A(pin12)=%d B(pin17)=%d debug=%d",
        state.block_a_high,
        state.block_b_high,
        state.debug_mode);

    if(state.debug_mode) {
        FURI_LOG_W(TAG, "Debug mode owns PA13; pin 12 detect may be unreliable");
    }
    if(!state.block_a_high || !state.block_b_high) {
        /* A push-pull output that reads back low is being held down by
         * something -- a short, or the debug peripheral. */
        FURI_LOG_E(TAG, "a detect line did not go high");
    }
}

void meshcore_detect_deinit(void) {
    /* Back to analog, which is how the firmware leaves unused externals: it
     * stops driving, so a node rebooted after this sees LOW and comes up on
     * its own radio. Leaving them driven would strand the node in serial mode
     * with nothing on the other end. */
    furi_hal_gpio_write(detect_a, false);
    furi_hal_gpio_write(detect_b, false);
    furi_hal_gpio_init_simple(detect_a, GpioModeAnalog);
    furi_hal_gpio_init_simple(detect_b, GpioModeAnalog);
}

MeshCoreDetectState meshcore_detect_state(void) {
    MeshCoreDetectState state;
    /* Reading a push-pull output returns the actual pin level, not the value we
     * asked for, which is what makes this a check rather than an echo. */
    state.block_a_high = furi_hal_gpio_read(detect_a);
    state.block_b_high = furi_hal_gpio_read(detect_b);
    state.debug_mode = meshcore_detect_conflicts_with_debug();
    return state;
}
