#include "display.h"
#include "test.h"

static const Mode display_test_offering[] = {
    ModeMain,
    ModeVoice,
    ModeOptions,
    ModeNoLink,
    ModeVoiceCut,
    ModeOptionsNoLink,
};

static void test_lit_while_the_link_is_held_or_offered(void) {
    for(size_t i = 0; i < sizeof(display_test_offering) / sizeof(display_test_offering[0]); i++) {
        assert(display_light(display_test_offering[i], true) == DisplayLightOn);
    }
}

static void test_left_to_the_device_with_backlight_auto(void) {
    for(size_t i = 0; i < sizeof(display_test_offering) / sizeof(display_test_offering[0]); i++) {
        assert(display_light(display_test_offering[i], false) == DisplayLightAuto);
    }
}

static void test_dark_once_the_offer_stops(void) {
    assert(display_light(ModeStopped, true) == DisplayLightOff);
    assert(display_light(ModeStopped, false) == DisplayLightOff);
}

int main(void) {
    test_run(test_lit_while_the_link_is_held_or_offered);
    test_run(test_left_to_the_device_with_backlight_auto);
    test_run(test_dark_once_the_offer_stops);
    return 0;
}
