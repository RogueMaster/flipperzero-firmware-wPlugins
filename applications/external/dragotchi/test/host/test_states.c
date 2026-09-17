#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "states.h"
#include "rng_control.h"
#include "tuning.h"

void run_states_tests(void) {
    struct GameState s = {0};

    // --- Hygiene: poop appears over time; Clean clears + rewards ---
    rng_seed(3);
    game_state_init(&s, 0);
    s.persistent.stage = HATCHLING;
    advance_hygiene(&s, POOP_FREQ * 200);
    CHECK(s.persistent.poop > 0);
    CHECK(s.persistent.poop <= MAX_POOP);
    CHECK(s.persistent.poop_since != 0);
    int32_t c = s.persistent.care_score;
    CHECK(clean_poop(&s) == true);
    CHECK(s.persistent.poop == 0);
    CHECK(s.persistent.care_score == c + CARE_CLEAN);
    CHECK(clean_poop(&s) == false); // nothing to clean

    // --- Ignored poop docks care exactly once ---
    rng_seed(3);
    game_state_init(&s, 0);
    s.persistent.stage = HATCHLING;
    s.persistent.poop = 1; s.persistent.poop_since = 0; s.persistent.last_poop_update = 0;
    c = s.persistent.care_score;
    advance_hygiene(&s, POOP_TOLERANCE + POOP_FREQ);   // beyond tolerance
    CHECK(s.persistent.care_score == c - CARE_POOP_IGNORED);
    int32_t c2 = s.persistent.care_score;
    advance_hygiene(&s, POOP_TOLERANCE * 3);           // still dirty, no second dock
    CHECK(s.persistent.care_score == c2);

    // --- Sickness: dirtier/hungrier -> can get sick; cure works ---
    rng_seed(9);
    game_state_init(&s, 0);
    s.persistent.stage = WYRMLING;
    s.persistent.hunger = 0; s.persistent.poop = MAX_POOP; // max illness odds
    advance_sickness(&s, SICK_CHECK_FREQ * 100);
    CHECK(s.persistent.sick == 1);
    c = s.persistent.care_score;
    CHECK(cure_sickness(&s) == true);
    CHECK(s.persistent.sick == 0);
    CHECK(s.persistent.care_score == c + CARE_HEAL);
    CHECK(cure_sickness(&s) == false);

    // --- Sleep: night detection + lights ---
    CHECK(is_night(23 * 3600) == true);    // 23:00
    CHECK(is_night(3 * 3600) == true);     // 03:00
    CHECK(is_night(12 * 3600) == false);   // 12:00
    game_state_init(&s, 12 * 3600);
    s.persistent.stage = ADULT;
    CHECK(is_asleep(&s, 23 * 3600) == true);    // sleeps at night regardless of lights
    CHECK(is_asleep(&s, 12 * 3600) == false);   // awake during the day
    set_lights(&s, true, 23 * 3600);            // lights out is a care bonus (not required to sleep)

    // --- Kept awake at night docks care ---
    rng_seed(5);
    game_state_init(&s, 22 * 3600);
    s.persistent.stage = ADULT;
    s.persistent.lights_off = 0;                 // lights on at night
    s.persistent.last_sleep_update = 22 * 3600;
    c = s.persistent.care_score;
    advance_sleep(&s, 22 * 3600 + HP_CHECK_FREQ * 5);
    CHECK(s.persistent.care_score == c - CARE_SLEEP_DISTURBED);
}
