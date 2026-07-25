#include "trainer_lesson.h"

#include "cw.h"

#include <stdio.h>
#include <string.h>

static const char morse_trainer_koch_order[] = "KMURESNAPTLWI.JZ=FOY,VG5/Q92H38B?47C1D60X";

size_t morse_trainer_lesson_count(void) {
    return sizeof(morse_trainer_koch_order) - 2U;
}

static uint8_t morse_trainer_lesson_normalize(uint8_t lesson) {
    uint8_t maximum = (uint8_t)morse_trainer_lesson_count();

    if(lesson < 1U) return 1U;
    return lesson > maximum ? maximum : lesson;
}

void morse_trainer_lesson_label(uint8_t lesson, char* out, size_t out_size) {
    lesson = morse_trainer_lesson_normalize(lesson);
    if(out == NULL || out_size == 0U) return;
    if(lesson == 1U) {
        snprintf(
            out,
            out_size,
            "1 - %c %c",
            morse_trainer_koch_order[0],
            morse_trainer_koch_order[1]);
    } else {
        snprintf(out, out_size, "%u - %c", (unsigned)lesson, morse_trainer_koch_order[lesson]);
    }
}

size_t morse_trainer_lesson_charset_copy(uint8_t lesson, char* out, size_t out_size) {
    size_t count;

    if(out == NULL || out_size == 0U) return 0U;
    lesson = morse_trainer_lesson_normalize(lesson);
    count = (size_t)lesson + 1U;
    if(count >= out_size) count = out_size - 1U;
    memcpy(out, morse_trainer_koch_order, count);
    out[count] = '\0';
    return count;
}

bool morse_trainer_char_morse_copy(char ch, char* out, size_t out_size) {
    uint8_t encoded;

    if(out == NULL || out_size == 0U) return false;
    out[0] = '\0';
    encoded = cw(ch);
    if(encoded == CW_INVALID) return false;
    cw_to_text(encoded, out, out_size);
    return out[0] != '\0';
}
