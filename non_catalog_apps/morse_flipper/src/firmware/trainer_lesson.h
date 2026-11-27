#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t morse_trainer_lesson_count(void);
void morse_trainer_lesson_label(uint8_t lesson, char* out, size_t out_size);
size_t morse_trainer_lesson_charset_copy(uint8_t lesson, char* out, size_t out_size);
bool morse_trainer_char_morse_copy(char ch, char* out, size_t out_size);
