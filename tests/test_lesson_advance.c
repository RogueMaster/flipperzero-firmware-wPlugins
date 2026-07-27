#include <assert.h>
#include <stdio.h>

#include "morse_flipper_lesson_advance_policy.h"
#include "trainer.h"

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

static MorseTrainer completed_session(uint8_t lesson, uint8_t groups, uint16_t hits) {
    MorseTrainer trainer = {0};

    trainer.lesson = lesson;
    trainer.group_size = 5U;
    trainer.session_groups = groups;
    trainer.phase = MorseTrainerPhaseDone;
    trainer.session_index = groups;
    trainer.session_scored_groups = groups;
    trainer.session_letter_hits = hits;
    trainer.session_letter_total = 100U;
    return trainer;
}

static bool eligible(const MorseTrainer* trainer, bool standard_lesson, bool debug_result) {
    return MORSE_FLIPPER_LESSON_ADVANCE_ELIGIBLE(
        standard_lesson,
        morse_trainer_session_completed(trainer),
        debug_result,
        morse_trainer_lesson(trainer),
        morse_trainer_lesson_count(),
        morse_trainer_session_letter_percent(trainer),
        morse_trainer_session_total(trainer));
}

int main(void) {
    MorseTrainer trainer = completed_session(1U, 10U, 96U);

    CHECK(eligible(&trainer, true, false));
    trainer.session_letter_hits = 95U;
    CHECK(eligible(&trainer, true, false));
    trainer.session_letter_hits = 94U;
    CHECK(!eligible(&trainer, true, false));
    trainer.session_letter_hits = 96U;
    trainer.session_groups = 9U;
    trainer.session_index = 9U;
    trainer.session_scored_groups = 9U;
    CHECK(!eligible(&trainer, true, false));
    trainer = completed_session((uint8_t)morse_trainer_lesson_count(), 10U, 100U);
    CHECK(!eligible(&trainer, true, false));
    trainer = completed_session(1U, 10U, 100U);
    CHECK(!eligible(&trainer, false, false));
    CHECK(!eligible(&trainer, true, true));
    trainer.session_active = true;
    CHECK(!eligible(&trainer, true, false));

    printf("test_lesson_advance: %u checks passed\n", checks);
    return 0;
}
