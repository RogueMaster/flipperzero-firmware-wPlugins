#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCORE_CAP 10
#define SCORE_LOG_SIZE 40

typedef struct {
    uint8_t victory;
    uint8_t floor;
    uint8_t level;
    uint8_t has_orb;
    uint8_t class_id;
    uint16_t gold;
    char log[SCORE_LOG_SIZE];
} ScoreEntry;

char fr_score_class_glyph(uint8_t class_id);
void fr_score_insert(ScoreEntry scores[SCORE_CAP], uint8_t* count, const ScoreEntry* entry);
void fr_score_format_main(const ScoreEntry* score, uint8_t rank, char* out, size_t out_size);
void fr_score_format_log(const ScoreEntry* score, uint8_t rank, char* out, size_t out_size);
bool fr_score_parse_main(const char* line, ScoreEntry* score);
void fr_score_parse_log(const char* line, ScoreEntry* score);
