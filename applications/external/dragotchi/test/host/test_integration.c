#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "game_logic.h"
#include "states.h"
#include "rng_control.h"
#include "tuning.h"

/* Simulate raising a pet from egg to just-adult with a given care policy.
 * good_care=1 keeps every need topped up; good_care=0 neglects it. */
static void simulate(struct GameState *s, int good_care, uint32_t seed) {
    rng_seed(seed);
    game_state_init(s, 0);
    for(uint32_t now = 0; now <= AGE_ADULT + 120; now += 60) {
        advance_state(s, now);
        if(s->persistent.stage == DEAD) return;
        if(good_care) {
            if(s->persistent.hunger < 60) do_feed(s, now);
            if(s->persistent.happiness < 60) do_play(s, now);
            if(s->persistent.poop > 0) do_clean(s, now);
            if(s->persistent.sick) do_medicine(s, now);
            if(is_night(now) && !s->persistent.lights_off) do_lights(s, now);
            if(s->persistent.attention_call) do_scold(s, now);
        }
    }
}

void run_integration_tests(void) {
    struct GameState a = {0}, b = {0};

    // Good care -> reaches adulthood alive as a WHITE dragon with high care
    simulate(&a, 1, 777);
    CHECK(a.persistent.stage == ADULT);
    CHECK(a.persistent.alignment == ALIGN_WHITE);
    CHECK(a.persistent.care_score >= CARE_WHITE);

    // Determinism: same seed + same policy -> identical outcome
    simulate(&b, 1, 777);
    CHECK(a.persistent.stage == b.persistent.stage);
    CHECK(a.persistent.alignment == b.persistent.alignment);
    CHECK(a.persistent.care_score == b.persistent.care_score);

    // Neglect -> the pet dies (never reaches a happy adulthood)
    struct GameState n = {0};
    simulate(&n, 0, 777);
    CHECK(n.persistent.stage == DEAD);

    // Egg actually hatches and needs do not retro-decay across the egg period
    rng_seed(1);
    game_state_init(&a, 0);
    advance_state(&a, AGE_HATCHLING + 1);
    CHECK(a.persistent.stage == HATCHLING);
    CHECK(a.persistent.hunger == MAX_HU);   // resynced at hatch, no retro-decay
    CHECK(a.persistent.last_hunger_update >= AGE_HATCHLING);
}
