#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "morse_flipper_progress.h"

#define MORSE_FLIPPER_LESSON_ADVANCE_ELIGIBLE(                                      \
    standard, completed, debug_result, lesson, lesson_count, percent, groups)        \
    ((standard) && (completed) && !(debug_result) && (lesson) < (lesson_count) &&     \
     (percent) >= 95U && (groups) >= MORSE_FLIPPER_PROGRESS_MASTERY_GROUPS)
