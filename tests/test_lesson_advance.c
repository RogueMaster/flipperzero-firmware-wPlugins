#include <assert.h>
#include <stdio.h>

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

static bool qualified(const MorseTrainer* trainer, bool standard_lesson) {
    return standard_lesson && morse_trainer_session_completed(trainer) &&
           morse_trainer_lesson(trainer) < morse_trainer_lesson_count() &&
           morse_trainer_session_letter_percent(trainer) >= 95U &&
           morse_trainer_session_total(trainer) >= 10U;
}

int main(void) {
    MorseTrainer trainer = completed_session(1U, 10U, 96U);

    CHECK(qualified(&trainer, true));
    trainer.session_letter_hits = 95U;
    CHECK(qualified(&trainer, true));
    trainer.session_letter_hits = 94U;
    CHECK(!qualified(&trainer, true));
    trainer.session_letter_hits = 96U;
    trainer.session_groups = 9U;
    trainer.session_index = 9U;
    trainer.session_scored_groups = 9U;
    CHECK(!qualified(&trainer, true));
    trainer = completed_session((uint8_t)morse_trainer_lesson_count(), 10U, 100U);
    CHECK(!qualified(&trainer, true));
    trainer = completed_session(1U, 10U, 100U);
    CHECK(!qualified(&trainer, false));
    trainer.session_active = true;
    CHECK(!qualified(&trainer, true));

    printf("test_lesson_advance: %u checks passed\n", checks);
    return 0;
}
