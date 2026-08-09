#pragma once

#include <stdbool.h>
#include <stdint.h>

#define QUIZ_ANSWER_COUNT 4
#define QUIZ_TEXT_SIZE    17
#define QUIZ_SUBJECT_SIZE 26

typedef enum {
    DifficultyBeginner,
    DifficultyIntermediate,
    DifficultyAdvanced,
    DifficultyCount,
} Difficulty;

typedef enum {
    QuestionNetwork,
    QuestionBroadcast,
    QuestionFirstHost,
    QuestionLastHost,
    QuestionHostCount,
    QuestionMaskToCidr,
    QuestionCidrForHosts,
    QuestionTypeCount,
} QuestionType;

typedef struct {
    QuestionType type;
    char subject[QUIZ_SUBJECT_SIZE];
    const char* prompt;
    char answers[QUIZ_ANSWER_COUNT][QUIZ_TEXT_SIZE];
    uint8_t correct;
} Question;

/** Build a random exercise for the given difficulty. */
void quiz_generate(Question* question, Difficulty difficulty);

const char* quiz_difficulty_name(Difficulty difficulty);
const char* quiz_difficulty_range(Difficulty difficulty);
