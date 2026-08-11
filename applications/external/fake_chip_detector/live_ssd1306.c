#include "live_ssd1306.h"

#include <furi.h>
#include <string.h>
#include <stdio.h>

// From the Solomon Systech SSD1306 datasheet (rev 1.1, April 2008) and its
// application note (rev 0.4, January 2009), cross-checked against the Sino
// Wealth SH1106 preliminary datasheet (V0.1) so the same bytes are safe on the
// part that is usually substituted for it.
//
// THIS TEST NEVER DECLARES A PASS, and that is deliberate. A display has no
// readback: every byte below is acknowledged by a controller whose panel may
// be stone dead, so "it accepted the commands" is not evidence of anything.
// The proof is that the user watches the panel light up. The app would rather
// hand that judgement over than award a pass it cannot justify.

// Section 8.1.5.2: the byte after the address is a control byte, Co in bit 7
// and D/C# in bit 6. With both clear, every byte to the STOP is a command.
#define SSD1306_CTRL_COMMAND_STREAM 0x00

// The fundamental command table, same datasheet. AE puts the panel to sleep
// and AF wakes it; A4 resumes showing the contents of display RAM and A5
// forces every pixel on regardless of what is in RAM.
//
// The blink below uses AE/AF and not A5, deliberately: A5 lights every pixel,
// which is the peak-current case described at ssd1306_wake, and on long jumper
// wires that can brown the panel out and imitate the very failure being
// tested for.
#define SSD1306_DISPLAY_OFF      0xAE
#define SSD1306_DISPLAY_ON       0xAF
#define SSD1306_DISPLAY_FROM_RAM 0xA4
#define SSD1306_DISPLAY_ALL_ON   0xA5

// Half a second each way: slow enough to read as deliberate blinking rather
// than flicker, fast enough that the whole test takes a few seconds.
#define SSD1306_BLINK_MS 500

// Every pixel lit at full contrast is the peak current this module will ever
// draw. On long jumper wires that can brown the panel out and imitate exactly
// the failure being tested for, so the contrast is left at its reset value
// rather than pushed up.
static const uint8_t ssd1306_wake[] = {
    SSD1306_CTRL_COMMAND_STREAM,
    SSD1306_DISPLAY_OFF, // start from a known state
    0xA8,
    0x3F, // multiplex ratio 64, for a 128x64 panel
    0xD3,
    0x00, // display offset zero
    0x40, // display start line zero
    0x81,
    0x7F, // contrast at its reset value
    0xA6, // normal, in case something left it inverted
    // The charge pump. It is not in the datasheet's command tables at all —
    // only in the application note — and it is the single most common reason a
    // perfectly good module stays black: without it the OLED driver block has
    // no supply on a board with no external Vcc, while every byte still gets
    // acknowledged. The note requires it before display-on.
    0x8D,
    0x14,
    SSD1306_DISPLAY_ON,
    // Lights every pixel regardless of what is in display RAM. That matters:
    // a reset does not clear the RAM, so anything that renders RAM would show
    // undefined content. This command is the only one whose result does not
    // depend on state we cannot see.
    SSD1306_DISPLAY_ALL_ON,
};

// Deliberately absent: the 0x20/0x21/0x22 addressing commands. They do not
// exist on the SH1106, where the 0x7F argument of the column command would be
// read as "set display start line to 63" and shift the whole image — the
// classic garbled-SH1106 symptom. Nothing here needs them.

static void ssd1306_delay(const volatile bool* stop, uint32_t ms) {
    while(ms && !*stop) {
        uint32_t chunk = ms > 50 ? 50 : ms;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

// Long enough to read the sentence and pull a jumper, short enough not to be
// in the way of somebody who knows what they plugged in. Not a datasheet
// figure; there is nothing to cite.
#define SSD1306_BLIND_WARN_S 3

// A display has no readback at all, so this test can never confirm what it is
// talking to — not before writing, not after. And the app's own database puts
// a PCF8574A across 0x38-0x3F, which covers both OLED addresses: those command
// bytes would land on a GPIO expander as output-port writes, driving whatever
// is wired to its pins.
//
// There is no way to make that safe, so the honest thing is to say it out loud
// while the wire is still in the user's hand. Shown once per run, before the
// first byte goes out.
static void ssd1306_warn_blind(const LiveTestEnv* env) {
    for(uint8_t left = SSD1306_BLIND_WARN_S; left && !*env->stop; left--) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Cannot identify this part");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Unplug now if it is not");
        snprintf(st.lines[2], LIVE_TEST_LINE_LEN, "a display. Writing in %u", left);
        env->publish(env->ctx, &st);
        ssd1306_delay(env->stop, 1000);
    }
}

static bool ssd1306_command(const LiveTestI2c* i2c, uint8_t addr7, uint8_t command) {
    const uint8_t frame[2] = {SSD1306_CTRL_COMMAND_STREAM, command};
    return i2c->write_raw(addr7, frame, sizeof(frame), LIVE_TEST_TIMEOUT_MS);
}

static void ssd1306_run(const LiveTestEnv* env) {
    const uint8_t addr7 = env->addr7;
    const volatile bool* stop = env->stop;
    const LiveTestI2c* i2c = env->i2c;
    const LiveTestPublish publish = env->publish;
    void* const ctx = env->ctx;

    // Before the first write, not inside the loop: the warning is about this
    // address, which does not change, and repeating it after every retry would
    // train the user to press through it.
    ssd1306_warn_blind(env);

    while(!*stop) {
        LiveTestState st;
        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseStarting;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "Waking the panel");
        publish(ctx, &st);

        bool awake =
            i2c->write_raw(addr7, ssd1306_wake, sizeof(ssd1306_wake), LIVE_TEST_TIMEOUT_MS);
        bool lit = true;
        uint8_t errors = 0;

        while(awake && !*stop && errors < 3) {
            memset(&st, 0, sizeof(st));
            st.phase = LiveTestPhaseRunning;
            snprintf(
                st.lines[0],
                LIVE_TEST_LINE_LEN,
                "%s",
                lit ? "The panel is lit NOW" : "The panel is dark NOW");
            snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "Following along? Then it");
            snprintf(st.lines[2], LIVE_TEST_LINE_LEN, "works. Only you can say.");
            publish(ctx, &st);

            ssd1306_delay(stop, SSD1306_BLINK_MS);
            if(*stop) break;

            lit = !lit;
            if(!ssd1306_command(i2c, addr7, lit ? SSD1306_DISPLAY_ON : SSD1306_DISPLAY_OFF)) {
                errors++;
            } else {
                errors = 0;
            }
        }

        // Park what we started: back to rendering RAM, and dark. Leaving every
        // pixel burning after the user walks away would be rude to the panel
        // and to whatever battery is feeding it.
        if(awake) {
            ssd1306_command(i2c, addr7, SSD1306_DISPLAY_FROM_RAM);
            ssd1306_command(i2c, addr7, SSD1306_DISPLAY_OFF);
        }

        if(*stop) break;

        memset(&st, 0, sizeof(st));
        st.phase = LiveTestPhaseLost;
        snprintf(st.lines[0], LIVE_TEST_LINE_LEN, "It stopped accepting");
        snprintf(st.lines[1], LIVE_TEST_LINE_LEN, "commands. Check wiring.");
        publish(ctx, &st);
        ssd1306_delay(stop, LIVE_TEST_RETRY_MS);
    }
}

const LiveTest live_test_ssd1306 = {
    .chip = "SSD1306/SH1106",
    .title = "OLED test",
    .offer = "Make the screen blink",
    .addrs = {0x3C, 0x3D},
    .run = ssd1306_run,
    .draw = NULL,
};
