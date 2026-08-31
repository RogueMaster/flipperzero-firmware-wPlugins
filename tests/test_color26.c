#include "../protocol/tagtinker_color26.h"
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

static void expect_u8(const char* name, uint8_t got, uint8_t want) {
    if(got == want) return;
    fprintf(stderr, "FAIL %s: got %u want %u\n", name, got, want);
    failures++;
}

static void expect_xy(
    const char* name,
    uint16_t gx,
    uint16_t gy,
    uint16_t want_x,
    uint16_t want_y) {
    if(gx == want_x && gy == want_y) return;
    fprintf(stderr, "FAIL %s: got %u,%u want %u,%u\n", name, gx, gy, want_x, want_y);
    failures++;
}

int main(void) {
    expect_u8("page 0", tagtinker_color26_resolve_page(0), 2);
    expect_u8("page 1", tagtinker_color26_resolve_page(1), 2);
    expect_u8("page 2", tagtinker_color26_resolve_page(2), 2);
    expect_u8("page 3", tagtinker_color26_resolve_page(3), 3);
    expect_u8("page 7", tagtinker_color26_resolve_page(7), 7);
    expect_u8("page 8", tagtinker_color26_resolve_page(8), 7);
    expect_u8("page 9", tagtinker_color26_resolve_page(9), 7);

    /* 2x3 wire, 3x2 glass. bx = py, by = proto_w - 1 - px. */
    const uint16_t proto_w = 2;
    struct {
        uint16_t px, py, bx, by;
    } cases[] = {
        {0, 0, 0, 1},
        {1, 0, 0, 0},
        {0, 1, 1, 1},
        {1, 1, 1, 0},
        {0, 2, 2, 1},
        {1, 2, 2, 0},
    };
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint16_t bx = 99, by = 99;
        tagtinker_color26_proto_to_glass(proto_w, cases[i].px, cases[i].py, &bx, &by);
        char name[32];
        snprintf(name, sizeof(name), "map %u,%u", cases[i].px, cases[i].py);
        expect_xy(name, bx, by, cases[i].bx, cases[i].by);
    }

    if(!tagtinker_type_needs_wh_swap(TAGTINKER_TYPE_SMARTAG_COLOR_26)) {
        fprintf(stderr, "FAIL type 1626 should swap W/H\n");
        failures++;
    }
    if(tagtinker_type_needs_wh_swap(1627)) {
        fprintf(stderr, "FAIL type 1627 should not swap W/H\n");
        failures++;
    }

    if(failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
