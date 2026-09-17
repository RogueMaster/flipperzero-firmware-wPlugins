#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "needs.h"
#include "rng_control.h"
#include "tuning.h"

void run_needs_tests(void) {
    // Hunger decays over time and bookkeeps last_update with remainder preserved
    rng_seed(42);
    struct GameState s = {0};
    game_state_init(&s, 0);
    uint32_t span = HU_DECAY_FREQ * 50 + 7; // 50 events + remainder
    advance_hunger(&s, span);
    CHECK(s.persistent.hunger < MAX_HU);                 // decayed
    CHECK(s.persistent.last_hunger_update == HU_DECAY_FREQ * 50); // remainder kept

    // Hunger hitting 0 docks care and flags starving
    rng_seed(1);
    game_state_init(&s, 0);
    s.persistent.hunger = 2;
    int32_t care0 = s.persistent.care_score;
    GameEventFlags f = advance_hunger(&s, HU_DECAY_FREQ * 200);
    CHECK(s.persistent.hunger == 0);
    CHECK(f & EVT_STARVING);
    CHECK(s.persistent.care_score == care0 - CARE_METER_ZERO);

    // Feed restores and rewards care when hungry; clamps at max
    game_state_init(&s, 0);
    s.persistent.hunger = 10;
    int32_t c = s.persistent.care_score;
    hunger_feed(&s);
    CHECK(s.persistent.hunger > 10);
    CHECK(s.persistent.care_score == c + CARE_FEED_HUNGRY);
    CHECK(s.display_state == DISP_EATING);
    s.persistent.hunger = MAX_HU;
    c = s.persistent.care_score;
    hunger_feed(&s); // overfeed
    CHECK(s.persistent.hunger == MAX_HU);
    CHECK(s.persistent.care_score == c - CARE_OVERFEED);

    // Health drains only when in a bad state (starving here) and can kill
    rng_seed(7);
    game_state_init(&s, 0);
    s.persistent.hunger = 0;       // starving -> health drains
    s.persistent.health = 3;
    f = advance_health(&s, HP_CHECK_FREQ * 300);
    CHECK(s.persistent.health == 0);
    CHECK(s.persistent.stage == DEAD);
    CHECK(f & EVT_DIED);

    // Health does NOT drain when healthy/fed/clean
    game_state_init(&s, 0); // hunger full, not sick, no poop
    advance_health(&s, HP_CHECK_FREQ * 300);
    CHECK(s.persistent.health == MAX_HP);
}
