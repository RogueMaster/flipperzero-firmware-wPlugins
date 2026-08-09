/*
 * rc_bits_from_dump against real decoder output shapes.
 *
 * The bit length shown on the capture screen and in the per-press ledger comes
 * out of free text, so the parser has to be right about where a number ends
 * and where "bit" is just part of another word.
 *
 *   make -C test
 */
#include "../helpers/rc_parse.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

static void expect(const char* dump, uint16_t want, const char* why) {
    checks++;
    uint16_t got = rc_bits_from_dump(dump);
    if(got != want) {
        failures++;
        printf("  FAIL %-28s got %u, want %u  [%s]\n", why, got, want, dump ? dump : "(null)");
    }
}

int main(void) {
    printf("== rc_bits_from_dump ==\n");

    /* The shapes the built-in decoders actually emit. */
    expect("KeeLoq 64bit\r\nKey:1234567890ABCDEF\r\nFix:0FA1\r\n", 64, "keeloq");
    expect("Princeton 24bit\r\nKey:0000000000ABCDEF\r\nTe:400us\r\n", 24, "princeton");
    expect("CAME 12bit\r\nKey:0000000000000ABC\r\n", 12, "came");
    expect("Nice FloR-S 52bit\r\nKey:00A1B2C3D4E5F607\r\n", 52, "nice flor-s");
    expect("Security+ 2.0 62bit\r\nKey:00\r\n", 62, "security+ 2.0");

    /* Nothing usable in there. */
    expect("", 0, "empty string");
    expect(NULL, 0, "null pointer");
    expect("RAW\r\n", 0, "no bit token");
    expect("bit\r\n", 0, "bare token, no digits");
    expect("Unknown bit count\r\n", 0, "token with no leading digits");

    /* "bit" inside another word must be skipped, and the real count still found. */
    expect("Arbitrary 8bit\r\n", 8, "bit inside a word, then real");
    expect("Arbitrary rate\r\n", 0, "bit inside a word, nothing real");

    /* Values we refuse to believe. */
    expect("Proto 0bit\r\n", 0, "zero rejected");
    expect("Proto 512bit\r\n", 512, "upper bound accepted");
    expect("Proto 513bit\r\n", 0, "just over the bound rejected");
    expect("Proto 99999bit\r\n", 0, "absurd value rejected");

    /* The first plausible count wins, later ones are ignored. */
    expect("Proto 24bit\r\nSomething 48bit\r\n", 24, "first count wins");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
