#pragma once

#include <furi.h>

#define HIGHSCORE_NAME_LEN 20
#define HIGHSCORES_COUNT 5

typedef struct {
    char name[HIGHSCORE_NAME_LEN + 1];
    int score;
} HighScoreEntry;

typedef struct {
    HighScoreEntry entries[HIGHSCORES_COUNT];
    int count;
} HighScoreList;

void highscores_load(HighScoreList* list, const char* path);
void highscores_save(const HighScoreList* list, const char* path);
bool highscores_qualifies(const HighScoreList* list, int score);
void highscores_insert(HighScoreList* list, const char* name, int score);
