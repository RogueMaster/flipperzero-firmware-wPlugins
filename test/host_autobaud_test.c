/* Host test for the real autobaud engine.
 *
 * The detector is the whole product, and it is the one part that cannot be
 * eyeballed on a screenshot. So this synthesises an 8N1 waveform at a known
 * rate, drives the actual interrupt handler edge by edge through a stubbed
 * HAL, and asks the real fit what rate it thinks it saw.
 *
 * No hardware involved, but every line under test is shipped code: autobaud.c
 * and baud_table.c are compiled here exactly as they are for the Flipper.
 *
 *   make -C test
 */

#include "helpers/autobaud.h"
#include <stm32wbxx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------- the HAL ---- */

bool hermes_test_level = true;
GpioExtiCallback hermes_test_isr = NULL;
void* hermes_test_isr_ctx = NULL;

const GpioPin gpio_usart_rx = {0};
const GpioPin gpio_usart_tx = {0};
const GpioPin gpio_ext_pc0 = {0};
const GpioPin gpio_ext_pc1 = {0};

static DWT_Type dwt_storage;
static CoreDebug_Type coredebug_storage;
DWT_Type* const DWT = &dwt_storage;
CoreDebug_Type* const CoreDebug = &coredebug_storage;

void furi_hal_gpio_init(const GpioPin* p, GpioMode m, GpioPull pu, GpioSpeed s) {
    (void)p;
    (void)m;
    (void)pu;
    (void)s;
}

void furi_hal_gpio_add_int_callback(const GpioPin* p, GpioExtiCallback cb, void* ctx) {
    (void)p;
    hermes_test_isr = cb;
    hermes_test_isr_ctx = ctx;
}

void furi_hal_gpio_remove_int_callback(const GpioPin* p) {
    (void)p;
    hermes_test_isr = NULL;
}

uint32_t furi_hal_cortex_instructions_per_microsecond(void) {
    return 64; // the Flipper's 64 MHz core
}

static FuriHalSerialHandle* const fake_handle = (FuriHalSerialHandle*)0xD00Du;

FuriHalSerialHandle* furi_hal_serial_control_acquire(FuriHalSerialId id) {
    (void)id;
    return fake_handle;
}
void furi_hal_serial_control_release(FuriHalSerialHandle* h) {
    (void)h;
}
void furi_hal_serial_configure_framing(
    FuriHalSerialHandle* h,
    FuriHalSerialDataBits d,
    FuriHalSerialParity p,
    FuriHalSerialStopBits s) {
    (void)h;
    (void)d;
    (void)p;
    (void)s;
}

/* ---------------------------------------------------------- the signal ---- */

#define CPU_HZ 64000000.0

static uint32_t rng_state = 12345;
static uint32_t rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

/** Play a byte stream onto the fake pin at `baud`, firing the real ISR on every
 *  level change.
 *
 * @param jitter    spread of interrupt latency, in CPU cycles. A *constant*
 *                  latency would cancel out in the deltas, so only the spread
 *                  is worth modelling.
 * @param idle_bits gap left between frames.
 */
static void play(const char* text, uint32_t baud, uint32_t jitter, uint32_t idle_bits) {
    const double cycles_per_bit = CPU_HZ / (double)baud;
    double t = 1000.0; // arbitrary start offset
    bool level = true; // UART idles high

    for(const char* p = text; *p; p++) {
        int bits[10];
        int n = 0;
        bits[n++] = 0; // start bit
        for(int i = 0; i < 8; i++) {
            bits[n++] = (*p >> i) & 1; // LSB first
        }
        bits[n++] = 1; // stop bit

        for(int i = 0; i < n; i++) {
            const bool want = bits[i] != 0;
            if(want != level) {
                level = want;
                uint32_t at = (uint32_t)t;
                if(jitter) at += rng() % jitter;

                DWT->CYCCNT = at;
                hermes_test_level = level;
                if(hermes_test_isr) hermes_test_isr(hermes_test_isr_ctx);
            }
            t += cycles_per_bit;
        }
        t += cycles_per_bit * (double)idle_bits;
    }
}

static const char* const BOOT_LOG =
    "U-Boot 2019.04\r\nHit any key to stop autoboot: 0\r\nroot@openwrt:/# ";

/* ------------------------------------------------------------- the test --- */

static int failures = 0;

static void check(const char* name, uint32_t truth, uint32_t jitter, uint32_t idle_bits) {
    Autobaud* ab = autobaud_alloc();
    autobaud_start(ab, HermesPortUsart);
    play(BOOT_LOG, truth, jitter, idle_bits);

    AutobaudResult r;
    autobaud_analyze(ab, &r);
    autobaud_stop(ab);

    const bool ok = (r.verdict == AutobaudVerdictOk) && (r.candidate_count > 0) &&
                    (r.candidates[0].baud == truth);
    if(!ok) failures++;

    printf(
        "  %-26s truth=%-7u got=%-7u raw=%-7u fit=%3u%% conf=%3u  %s\n",
        name,
        truth,
        (r.candidate_count > 0) ? r.candidates[0].baud : 0,
        r.baud_raw,
        r.fit_percent,
        (r.candidate_count > 0) ? r.candidates[0].confidence : 0,
        ok ? "ok" : "*** FAIL ***");

    autobaud_free(ab);
}

/** Binary traffic, not a console: the fit must not care what the bytes mean. */
static void check_binary(const char* name, uint32_t truth) {
    Autobaud* ab = autobaud_alloc();
    autobaud_start(ab, HermesPortUsart);

    char blob[64];
    for(size_t i = 0; i < sizeof(blob) - 1; i++) {
        blob[i] = (char)((rng() & 0xFF) | 0x01); // never NUL, that would end the string
    }
    blob[sizeof(blob) - 1] = '\0';
    play(blob, truth, 40, 2);

    AutobaudResult r;
    autobaud_analyze(ab, &r);
    autobaud_stop(ab);

    const bool ok = (r.verdict == AutobaudVerdictOk) && (r.candidate_count > 0) &&
                    (r.candidates[0].baud == truth);
    if(!ok) failures++;
    printf(
        "  %-26s truth=%-7u got=%-7u fit=%3u%% conf=%3u  %s\n",
        name,
        truth,
        (r.candidate_count > 0) ? r.candidates[0].baud : 0,
        r.fit_percent,
        (r.candidate_count > 0) ? r.candidates[0].confidence : 0,
        ok ? "ok" : "*** FAIL ***");
    autobaud_free(ab);
}

/** A rate nobody standard uses. Naming a neighbour confidently would be a lie;
 *  declining, or answering with visible doubt, is the honest outcome. */
static void check_nonstandard(uint32_t truth) {
    Autobaud* ab = autobaud_alloc();
    autobaud_start(ab, HermesPortUsart);
    play(BOOT_LOG, truth, 0, 3);

    AutobaudResult r;
    autobaud_analyze(ab, &r);
    autobaud_stop(ab);

    const bool lied = (r.verdict == AutobaudVerdictOk) && (r.candidate_count > 0) &&
                      (r.candidates[0].confidence > 50);
    if(lied) failures++;
    printf(
        "  %-26s truth=%-7u got=%-7u conf=%3u  %s\n",
        "non-standard rate",
        truth,
        (r.candidate_count > 0) ? r.candidates[0].baud : 0,
        (r.candidate_count > 0) ? r.candidates[0].confidence : 0,
        lied ? "*** FAIL (confident lie) ***" : "ok (declines or doubts)");
    autobaud_free(ab);
}

static void check_silent(void) {
    Autobaud* ab = autobaud_alloc();
    autobaud_start(ab, HermesPortUsart);

    AutobaudResult r;
    autobaud_analyze(ab, &r);
    autobaud_stop(ab);

    const bool ok = (r.verdict == AutobaudVerdictSilent);
    if(!ok) failures++;
    printf("  %-26s %s\n", "silent line", ok ? "ok (reports silence)" : "*** FAIL ***");
    autobaud_free(ab);
}

int main(void) {
    printf("\nclean signal\n");
    check("1200", 1200, 0, 3);
    check("9600", 9600, 0, 3);
    check("19200", 19200, 0, 3);
    check("38400", 38400, 0, 3);
    check("57600", 57600, 0, 3);
    check("74880 (esp8266 boot)", 74880, 0, 3);
    check("115200", 115200, 0, 3);

    printf("\ninterrupt jitter\n");
    check("9600  +100cy", 9600, 100, 3);
    check("57600  +100cy", 57600, 100, 3);
    check("115200 +100cy", 115200, 100, 3);
    check("115200 +200cy", 115200, 200, 3);

    printf("\nlong idle gaps between frames\n");
    check("9600   idle=40 bits", 9600, 50, 40);
    check("115200 idle=200 bits", 115200, 50, 200);

    printf("\nharmonic trap (2x the truth must not win)\n");
    check("57600 not 115200", 57600, 40, 3);
    check("19200 not 38400", 19200, 40, 3);
    check("9600 not 19200", 9600, 40, 3);

    printf("\nbinary payload, not text\n");
    check_binary("random bytes @ 9600", 9600);
    check_binary("random bytes @ 115200", 115200);

    printf("\ndegenerate\n");
    check_nonstandard(100000);
    check_silent();

    printf(
        "\n%s (%d failure%s)\n\n",
        failures ? "FAILED" : "ALL PASS",
        failures,
        failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
