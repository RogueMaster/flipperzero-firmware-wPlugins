#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "care.h"
#include "tuning.h"

void run_model_tests(void) {
    struct GameState s = {0};
    game_state_init(&s, 1000);
    CHECK(s.persistent.stage == EGG);
    CHECK(s.persistent.alignment == ALIGN_NONE);
    CHECK(s.persistent.birth_timestamp == 1000);
    CHECK(s.persistent.hunger == MAX_HU);
    CHECK(s.persistent.happiness == MAX_HAPPINESS);
    CHECK(s.persistent.health == MAX_HP);
    CHECK(s.persistent.care_score == CARE_START);
    CHECK(s.persistent.discipline == DISCIPLINE_START);
    CHECK(s.persistent.poop == 0 && s.persistent.sick == 0);

    // care clamping
    struct PersistentGameState p = s.persistent;
    p.care_score = 98; care_reward(&p, 10); CHECK(p.care_score == CARE_MAX);
    p.care_score = 3;  care_penalty(&p, 10); CHECK(p.care_score == CARE_MIN);

    // care band thresholds
    CHECK(care_band(CARE_WHITE) == ALIGN_WHITE);
    CHECK(care_band(CARE_WHITE - 1) == ALIGN_GREY);
    CHECK(care_band(CARE_GREY) == ALIGN_GREY);
    CHECK(care_band(CARE_GREY - 1) == ALIGN_BLACK);
}
