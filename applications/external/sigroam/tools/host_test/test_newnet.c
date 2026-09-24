#include "sr_test.h"

#include "sr_newnet.h"
#include "sr_settings.h"

#include <string.h>

static void test_rate(void) {
    SrNewNetCtx c;

    memset(&c, 0, sizeof(c));
    CHECK(sr_newnet_eval(NULL, 1u, true, 0u) == false);

    /* First eval only syncs. */
    CHECK(sr_newnet_eval(&c, 0u, true, 0u) == false);
    CHECK(sr_newnet_eval(&c, 0u, true, 50u) == false);

    /* Two rises inside 2 s: one tick. The next rise at exactly 2000 ms fires. */
    CHECK(sr_newnet_eval(&c, 1u, true, 100u) == true);
    CHECK(sr_newnet_eval(&c, 2u, true, 100u + 1999u) == false);
    CHECK(sr_newnet_eval(&c, 3u, true, 100u + 2000u) == true);
    CHECK(sr_newnet_eval(&c, 3u, true, 100u + 2000u) == false);

    /* Unsigned wrap of the fire clock. */
    memset(&c, 0, sizeof(c));
    CHECK(sr_newnet_eval(&c, 5u, true, 0xFFFFFFF0u) == false);
    CHECK(sr_newnet_eval(&c, 6u, true, 0xFFFFFFF0u) == true);
    CHECK(sr_newnet_eval(&c, 7u, true, 100u) == false);
    CHECK(sr_newnet_eval(&c, 8u, true, 1984u) == true);

    /* Not running resyncs. Stop/start must not burst, and the next rise can. */
    memset(&c, 0, sizeof(c));
    CHECK(sr_newnet_eval(&c, 1u, true, 5000u) == false);
    CHECK(sr_newnet_eval(&c, 4u, false, 5100u) == false);
    CHECK(sr_newnet_eval(&c, 4u, true, 5200u) == false);
    CHECK(sr_newnet_eval(&c, 0u, true, 5300u) == false);
    CHECK(sr_newnet_eval(&c, 1u, true, 5300u) == true);

    /* A rise while stopped is not a tick, and does not fire on the resume sync. */
    memset(&c, 0, sizeof(c));
    CHECK(sr_newnet_eval(&c, 10u, true, 0u) == false);
    CHECK(sr_newnet_eval(&c, 10u, true, 10u) == false);
    CHECK(sr_newnet_eval(&c, 80u, false, 20u) == false);
    CHECK(sr_newnet_eval(&c, 80u, true, 30u) == false);
    CHECK(sr_newnet_eval(&c, 81u, true, 30u) == true);
}

static void test_newnet_settings(void) {
    SrSettings in;
    SrSettings out;
    SrSettingsParseStats st;
    char text[SR_SETTINGS_TEXT_MAX];
    size_t n;
    const char* old = "Filetype: SigRoam Settings\n"
                      "Version: 1\n"
                      "Baud: 57600\n"
                      "Source: marauder\n"
                      "Sound: 0\n"
                      "Vibro: 1\n"
                      "Backlight: 0\n"
                      "Stealth: 1\n"
                      "Debug: 1\n";

    sr_settings_defaults(&in);
    CHECK(in.newnet == true);

    in.newnet = false;
    in.baud = 9600u;
    n = sr_settings_serialize(&in, text, sizeof(text));
    CHECK(n > 0u);
    CHECK(n + 1u <= (size_t)SR_SETTINGS_TEXT_MAX);
    CHECK(sr_settings_parse(text, n, &out, &st) == true);
    CHECK(st.keys_unknown == 0u);
    CHECK(out.newnet == false);
    CHECK(out.baud == 9600u);
    CHECK(sr_settings_equal(&in, &out));

    in.newnet = true;
    n = sr_settings_serialize(&in, text, sizeof(text));
    CHECK(sr_settings_parse(text, n, &out, &st) == true);
    CHECK(out.newnet == true);
    CHECK(sr_settings_equal(&in, &out));

    CHECK(strstr(text, "NewNet: 1\n") != NULL);
    CHECK(strstr(text, "Version: 1\n") != NULL);

    memset(&out, 0, sizeof(out));
    CHECK(sr_settings_parse(old, strlen(old), &out, &st) == true);
    CHECK(out.newnet == true);
    CHECK(out.baud == 57600u);
    CHECK(out.source == SrSourceMarauder);
    CHECK(out.sound == false);
    CHECK(out.vibro == true);
    CHECK(out.backlight_always == false);
    CHECK(out.stealth == true);
    CHECK(out.debug_rows == true);

    /* Longest file still fits the existing buffer. */
    sr_settings_defaults(&in);
    in.baud = 230400u;
    in.source = SrSourceGhostesp;
    in.sound = true;
    in.vibro = true;
    in.backlight_always = true;
    in.stealth = true;
    in.debug_rows = true;
    in.newnet = true;
    n = sr_settings_serialize(&in, text, sizeof(text));
    CHECK(n > 0u);
    CHECK(n + 1u <= (size_t)SR_SETTINGS_TEXT_MAX);
}

int test_newnet_run(void) {
    sr_test_failures = 0;
    test_rate();
    test_newnet_settings();
    return sr_test_failures;
}
