/**
 * Tests hôte de l'encodeur POCSAG.
 *   gcc -Wall -Wextra -I. test/test_pocsag.c pocsag.c -o test_pocsag && ./test_pocsag
 *
 * Export audio pour validation par un décodeur tiers (multimon-ng) :
 *   ./test_pocsag --raw out.raw <baud> <ric> <n|a> "<message>" [inv]
 *   multimon-ng -t raw -a POCSAG512 -a POCSAG1200 -a POCSAG2400 out.raw
 */
#include "pocsag.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Décodeur minimal, indépendant de l'encodeur (sauf pocsag_bch pour la vérif. BCH). */
static void decode(const uint32_t* cw, size_t n, uint32_t* ric, char* msg, PocsagType type) {
    size_t slot = 0, len = 0;
    uint32_t bits = 0;
    int nbits = 0;
    bool in_msg = false;
    *ric = UINT32_MAX;
    for(size_t i = 0; i < n; i++) {
        uint32_t w = cw[i];
        assert(pocsag_bch(w) == w); /* BCH + parité valides sur chaque mot */
        if(w == POCSAG_SYNC) {
            assert(slot % 16 == 0);
            continue;
        }
        if(w == POCSAG_IDLE) {
            in_msg = false;
        } else if(!(w & 0x80000000u)) {
            *ric = (((w >> 13) & 0x3FFFFu) << 3) | (uint32_t)(slot % 16 / 2);
            in_msg = true;
        } else {
            assert(in_msg);
            uint32_t d = (w >> 11) & 0xFFFFFu;
            for(int b = 19; b >= 0; b--) {
                bits |= ((d >> b) & 1u) << nbits; /* LSB en premier */
                int sym = type == PocsagNumeric ? 4 : 7;
                if(++nbits == sym) {
                    msg[len++] = type == PocsagNumeric ? "0123456789*U -)("[bits] : (char)bits;
                    bits = 0;
                    nbits = 0;
                }
            }
        }
        slot++;
    }
    assert(slot % 16 == 0); /* lots complets */
    msg[len] = 0;
    /* retire le bourrage final */
    while(len && (msg[len - 1] == ' ' || msg[len - 1] == 0))
        msg[--len] = 0;
}

static void roundtrip(uint32_t ric, PocsagType type, const char* text) {
    uint32_t cw[POCSAG_MAX_WORDS], got_ric;
    char got[256];
    size_t n = pocsag_encode(ric, type == PocsagAlpha ? 3 : 0, type, text, cw, POCSAG_MAX_WORDS);
    assert(n > 0 && cw[0] == POCSAG_SYNC);
    decode(cw, n, &got_ric, got, type);
    assert(got_ric == ric);
    assert(strcmp(got, text) == 0);
}

static int write_raw(int argc, char** argv) {
    if(argc < 7) {
        fprintf(stderr, "usage: %s --raw out.raw <baud> <ric> <n|a> <msg> [inv]\n", argv[0]);
        return 1;
    }
    unsigned baud = (unsigned)atoi(argv[3]);
    uint32_t ric = (uint32_t)strtoul(argv[4], NULL, 10);
    PocsagType type = argv[5][0] == 'a' ? PocsagAlpha : PocsagNumeric;
    bool inv = argc > 7;
    uint32_t cw[POCSAG_MAX_WORDS];
    size_t n =
        pocsag_encode(ric, type == PocsagAlpha ? 3 : 0, type, argv[6], cw, POCSAG_MAX_WORDS);
    if(!n) {
        fprintf(stderr, "encodage impossible\n");
        return 1;
    }
    FILE* f = fopen(argv[2], "wb");
    if(!f) return 1;
    const unsigned rate = 22050;
    size_t total = POCSAG_PREAMBLE_BITS + n * 32, sample = 0;
    int16_t s = 0;
    for(unsigned i = 0; i < rate / 10; i++)
        fwrite(&s, 2, 1, f); /* 100 ms de silence */
    for(size_t k = 0; k < total; k++) {
        bool bit = k < POCSAG_PREAMBLE_BITS ? !(k & 1) :
                                              (cw[(k - POCSAG_PREAMBLE_BITS) / 32] >>
                                               (31 - (k - POCSAG_PREAMBLE_BITS) % 32)) &
                                                  1u;
        s = (bit ^ inv) ? -12000 : 12000;
        size_t end = (size_t)((uint64_t)(k + 1) * rate / baud); /* même accumulateur que le TX */
        for(; sample < end; sample++)
            fwrite(&s, 2, 1, f);
    }
    s = 0;
    for(unsigned i = 0; i < rate / 10; i++)
        fwrite(&s, 2, 1, f);
    fclose(f);
    printf("%zu mots, %zu bits, %.2f s\n", n, total, (double)total / baud);
    return 0;
}

int main(int argc, char** argv) {
    if(argc > 1 && strcmp(argv[1], "--raw") == 0) return write_raw(argc, argv);

    /* SYNC et IDLE sont des mots de code BCH valides : référence externe du calcul. */
    assert(pocsag_bch(POCSAG_SYNC) == POCSAG_SYNC);
    assert(pocsag_bch(POCSAG_IDLE) == POCSAG_IDLE);

    /* Position de l'adresse : trame ric % 8 -> mot 1 + 2*(ric%8). */
    uint32_t cw[POCSAG_MAX_WORDS];
    for(uint32_t ric = 1234560; ric < 1234568; ric++) {
        size_t n = pocsag_encode(ric, 0, PocsagNumeric, "1", cw, POCSAG_MAX_WORDS);
        assert(n == ((ric & 7) == 7 ? 34u : 17u)); /* trame 7 : l'IDLE final ouvre un 2e lot */
        for(uint32_t i = 1; i < 1 + 2 * (ric & 7); i++)
            assert(cw[i] == POCSAG_IDLE);
        uint32_t a = cw[1 + 2 * (ric & 7)];
        assert(!(a & 0x80000000u) && ((a >> 13) & 0x3FFFFu) == ric >> 3);
    }

    /* Vecteur numérique calculé à la main : "1234" -> 1=1000 2=0100 3=1100 4=0010, puis 1 espace (0011). */
    assert(pocsag_encode(8, 0, PocsagNumeric, "1234", cw, POCSAG_MAX_WORDS) == 17);
    assert(cw[2] == pocsag_bch(0x80000000u | (0x84C23u << 11)));

    roundtrip(0, PocsagNumeric, "0123456789*U -)(");
    roundtrip(POCSAG_RIC_MAX, PocsagNumeric, "555");
    roundtrip(1234567, PocsagAlpha, "Hello POCSAG!");
    roundtrip(
        7,
        PocsagAlpha,
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890");
    roundtrip(
        1,
        PocsagNumeric,
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890");

    /* Rejets */
    assert(pocsag_encode(POCSAG_RIC_MAX + 1, 0, PocsagNumeric, "1", cw, POCSAG_MAX_WORDS) == 0);
    assert(pocsag_encode(1, 4, PocsagNumeric, "1", cw, POCSAG_MAX_WORDS) == 0);
    assert(pocsag_encode(1, 0, PocsagNumeric, "12a", cw, POCSAG_MAX_WORDS) == 0);
    assert(pocsag_encode(1, 3, PocsagAlpha, "caf\xc3\xa9", cw, POCSAG_MAX_WORDS) == 0);
    assert(pocsag_encode(7, 3, PocsagAlpha, "Hello POCSAG!", cw, 10) == 0); /* buffer trop petit */

    puts("OK : tous les tests passent");
    return 0;
}
