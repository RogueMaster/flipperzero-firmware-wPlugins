#include "tests.h"
#include "test_util.h"
#include "game_model.h"
#include "discipline.h"
#include "rng_control.h"
#include "tuning.h"

void run_discipline_tests(void) {
    struct GameState s = {0};

    // Low discipline -> a call is raised over time (no real need present)
    rng_seed(11);
    game_state_init(&s, 0);
    s.persistent.stage = WYRMLING;
    s.persistent.discipline = 0;             // maximal call odds
    advance_attention(&s, ATTENTION_CALL_FREQ * 50);
    CHECK(s.persistent.attention_call == 1);

    // Correct scold during a call: +discipline, +care, clears the call
    int32_t care = s.persistent.care_score;
    uint32_t disc = s.persistent.discipline;
    CHECK(scold(&s) == true);
    CHECK(s.persistent.attention_call == 0);
    CHECK(s.persistent.discipline == disc + DISCIPLINE_SCOLD_GAIN);
    CHECK(s.persistent.care_score == care + CARE_SCOLD_CORRECT);

    // Scolding when there is a real need is a misread: -care
    game_state_init(&s, 0);
    s.persistent.stage = WYRMLING;
    s.persistent.sick = 1;                   // a real need
    s.persistent.attention_call = 1;         // even if calling
    care = s.persistent.care_score;
    CHECK(scold(&s) == false);
    CHECK(s.persistent.care_score == care - CARE_SCOLD_WRONG);

    // A pet with a real need does not raise a discipline call
    rng_seed(11);
    game_state_init(&s, 0);
    s.persistent.stage = WYRMLING;
    s.persistent.discipline = 0;
    s.persistent.hunger = 0;                 // real need
    advance_attention(&s, ATTENTION_CALL_FREQ * 50);
    CHECK(s.persistent.attention_call == 0);
}
