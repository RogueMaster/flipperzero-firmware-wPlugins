#include "score_data.h"
#include "game_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char fr_score_class_glyph(uint8_t class_id) {
    if(class_id == 1) return 'R';
    if(class_id == 2) return 'M';
    return 'W';
}

static uint8_t fr_score_class_id(char glyph) {
    if(glyph == 'R') return 1;
    if(glyph == 'M') return 2;
    return 0;
}

static bool fr_score_before(const ScoreEntry* a, const ScoreEntry* b) {
    if(a->has_orb != b->has_orb) return a->has_orb > b->has_orb;
    return a->gold > b->gold;
}

void fr_score_insert(ScoreEntry scores[SCORE_CAP], uint8_t* count, const ScoreEntry* entry) {
    uint8_t insert = 0;
    while(insert < *count && !fr_score_before(entry, &scores[insert])) insert++;
    if(insert < SCORE_CAP) {
        uint8_t end = *count < SCORE_CAP ? *count : (uint8_t)(SCORE_CAP - 1);
        for(uint8_t i = end; i > insert; i--) scores[i] = scores[i - 1];
        scores[insert] = *entry;
    }
    if(*count < SCORE_CAP) (*count)++;
}

void fr_score_format_main(const ScoreEntry* score, uint8_t rank, char* out, size_t out_size) {
    if(out_size == 0) return;
    snprintf(
        out,
        out_size,
        "%u. %c %c F%u L%u %ug",
        rank,
        score->has_orb ? '&' : ' ',
        fr_score_class_glyph(score->class_id),
        score->floor,
        score->level,
        score->gold);
}

void fr_score_format_log(const ScoreEntry* score, uint8_t rank, char* out, size_t out_size) {
    if(out_size == 0) return;
    const char* indent = rank >= 10 ? "    " : "   ";
    const char* log = score->log[0] != '\0' ? score->log : (score->victory ? "Escaped." : "Fell.");
    snprintf(out, out_size, "%s%s", indent, fr_last_log_phrases(log, 2));
}

bool fr_score_parse_main(const char* line, ScoreEntry* score) {
    unsigned floor = 0;
    unsigned level = 0;
    unsigned gold = 0;
    char* end = NULL;
    strtoul(line, &end, 10);
    if(end == line || *end != '.') return false;
    const char* cursor = end + 1;
    while(*cursor == ' ') cursor++;
    score->has_orb = 0;
    if(*cursor == '&') {
        score->has_orb = 1;
        cursor++;
        while(*cursor == ' ') cursor++;
    }
    char class_glyph = *cursor;
    if(class_glyph != 'W' && class_glyph != 'R' && class_glyph != 'M') return false;
    cursor++;
    if(sscanf(cursor, " F%u L%u %ug", &floor, &level, &gold) != 3) return false;
    score->class_id = fr_score_class_id(class_glyph);
    score->floor = (uint8_t)floor;
    score->level = (uint8_t)level;
    score->gold = (uint16_t)gold;
    score->victory = score->has_orb && floor >= 18 ? 1 : 0;
    return true;
}

void fr_score_parse_log(const char* line, ScoreEntry* score) {
    while(*line == ' ') line++;
    snprintf(score->log, sizeof(score->log), "%s", fr_last_log_phrases(line, 2));
    size_t len = strlen(score->log);
    while(len > 0 && (score->log[len - 1] == '\n' || score->log[len - 1] == '\r')) {
        score->log[--len] = '\0';
    }
}
