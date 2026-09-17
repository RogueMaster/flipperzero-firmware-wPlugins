#include "tests.h"
#include "test_util.h"
#include "game_structs.h"
#include <string.h>

/* The device save uses toolbox saved_struct over the raw PersistentGameState,
 * so the struct must stay plain-old-data. Guard a byte round-trip. */
void run_save_tests(void) {
    struct PersistentGameState p;
    memset(&p, 0, sizeof(p));
    p.stage = ADULT; p.alignment = ALIGN_WHITE; p.birth_timestamp = 12345;
    p.hunger = 77; p.happiness = 66; p.health = 55;
    p.poop = 2; p.sick = 1; p.care_score = 88; p.discipline = 73;
    p.lights_off = 1; p.attention_call = 1;

    unsigned char buf[sizeof(p)];
    memcpy(buf, &p, sizeof(p));
    struct PersistentGameState q;
    memset(&q, 0xAA, sizeof(q));
    memcpy(&q, buf, sizeof(q));

    CHECK(q.stage == ADULT);
    CHECK(q.alignment == ALIGN_WHITE);
    CHECK(q.birth_timestamp == 12345);
    CHECK(q.hunger == 77 && q.happiness == 66 && q.health == 55);
    CHECK(q.poop == 2 && q.sick == 1);
    CHECK(q.care_score == 88 && q.discipline == 73);
    CHECK(q.lights_off == 1 && q.attention_call == 1);
    CHECK(memcmp(&p, &q, sizeof(p)) == 0);
}
