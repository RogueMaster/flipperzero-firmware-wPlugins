#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "game_logic.h"
#include "needs.h"
#include "states.h"
#include "rng_control.h"
#include "tuning.h"

static void put_down(struct GameState *s, uint32_t t) {
    game_state_init(s, t);
    s->persistent.stage = HATCHLING;
    s->persistent.stage_entered_timestamp = t;
    s->persistent.hunger = 80; s->persistent.happiness = 80; s->persistent.health = 100;
    s->persistent.last_hunger_update = t; s->persistent.last_happiness_update = t;
    s->persistent.last_health_update = t; s->persistent.last_poop_update = t;
    s->persistent.last_sick_update = t; s->persistent.last_sleep_update = t;
    s->persistent.last_attention_update = t;
}

void run_overnight_tests(void) {
    struct GameState s = {0};

    // A healthy pet put down at 20:00 must be ALIVE (and not gutted) at 07:00.
    rng_seed(2024);
    put_down(&s, 20*3600);
    advance_state(&s, 20*3600 + 11*3600); // -> 07:00 next day
    CHECK(s.persistent.stage != DEAD);
    CHECK(s.persistent.health > 40);

    // Poop alone must NOT drain health (it is a sickness risk, not a bleed).
    game_state_init(&s, 12*3600); // midday
    s.persistent.stage = WYRMLING;
    s.persistent.hunger = MAX_HU; s.persistent.sick = 0; s.persistent.poop = 2;
    s.persistent.health = MAX_HP; s.persistent.last_health_update = 12*3600;
    advance_health(&s, 12*3600 + HP_CHECK_FREQ*50);
    CHECK(s.persistent.health == MAX_HP);

    // Health drain from starvation PAUSES at night (asleep).
    rng_seed(1);
    game_state_init(&s, 23*3600);
    s.persistent.stage = WYRMLING;
    s.persistent.hunger = 0; s.persistent.health = MAX_HP;
    s.persistent.last_health_update = 23*3600;
    advance_health(&s, 23*3600 + HP_CHECK_FREQ*5); // stays within night
    CHECK(s.persistent.health == MAX_HP);

    // Hunger does not decay while asleep at night.
    rng_seed(1);
    game_state_init(&s, 23*3600);
    s.persistent.stage = WYRMLING;
    s.persistent.hunger = 50; s.persistent.last_hunger_update = 23*3600;
    advance_hunger(&s, 23*3600 + HU_DECAY_FREQ*8);
    CHECK(s.persistent.hunger == 50);

    // Sanity: it DOES still decay during the day.
    rng_seed(1);
    game_state_init(&s, 10*3600);
    s.persistent.stage = WYRMLING;
    s.persistent.hunger = 50; s.persistent.last_hunger_update = 10*3600;
    advance_hunger(&s, 10*3600 + HU_DECAY_FREQ*40);
    CHECK(s.persistent.hunger < 50);
}
