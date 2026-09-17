#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "evolution.h"
#include "rng_control.h"
#include "tuning.h"

void run_evolution_tests(void) {
    struct GameState s = {0};

    // Stage advances at exact age thresholds
    game_state_init(&s, 0);
    check_evolution(&s, AGE_HATCHLING - 1); CHECK(s.persistent.stage == EGG);
    check_evolution(&s, AGE_HATCHLING);     CHECK(s.persistent.stage == HATCHLING);
    check_evolution(&s, AGE_WYRMLING);      CHECK(s.persistent.stage == WYRMLING);
    check_evolution(&s, AGE_DRAKE);         CHECK(s.persistent.stage == DRAKE);

    // Care determines alignment at Drake->Adult
    game_state_init(&s, 0); s.persistent.care_score = 80;
    check_evolution(&s, AGE_ADULT);
    CHECK(s.persistent.stage == ADULT);
    CHECK(s.persistent.alignment == ALIGN_WHITE);

    game_state_init(&s, 0); s.persistent.care_score = 50;
    check_evolution(&s, AGE_ADULT);
    CHECK(s.persistent.alignment == ALIGN_GREY);

    game_state_init(&s, 0); s.persistent.care_score = 10;
    check_evolution(&s, AGE_ADULT);
    CHECK(s.persistent.alignment == ALIGN_BLACK);

    // Immortality: impeccable care -> never dies of old age
    rng_seed(123);
    game_state_init(&s, 0); s.persistent.care_score = 95;
    check_evolution(&s, AGE_ADULT);
    s.persistent.last_oldage_update = AGE_ADULT;
    check_old_age(&s, AGE_ADULT + AGE_ELDER * 100); // huge span
    CHECK(s.persistent.stage == ADULT);             // still alive

    // Mid care -> eventually dies of old age past the elder age
    rng_seed(123);
    game_state_init(&s, 0); s.persistent.care_score = 45;
    check_evolution(&s, AGE_ADULT);
    s.persistent.last_oldage_update = s.persistent.birth_timestamp + AGE_ELDER;
    GameEventFlags f = check_old_age(&s, s.persistent.birth_timestamp + AGE_ELDER + OLD_AGE_CHECK_FREQ * 2000);
    CHECK(s.persistent.stage == DEAD);
    CHECK(f & EVT_DIED);

    // Not old enough yet -> no old-age death even at low care
    rng_seed(123);
    game_state_init(&s, 0); s.persistent.care_score = 5;
    check_evolution(&s, AGE_ADULT);
    check_old_age(&s, AGE_ADULT + 1); // just became adult, age < ELDER
    CHECK(s.persistent.stage == ADULT);
}
