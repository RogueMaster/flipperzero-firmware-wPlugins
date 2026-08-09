#include "progress.h"

#include <flipper_format/flipper_format.h>
#include <furi.h>
#include <storage/storage.h>

#define STATS_PATH    APP_DATA_PATH("stats.txt")
#define SETTINGS_PATH APP_DATA_PATH("settings.txt")

#define STATS_HEADER    "CIDR Challenge Stats"
#define SETTINGS_HEADER "CIDR Challenge Settings"
#define FILE_VERSION    1

#define MAX_LEVEL 30

typedef struct {
    uint32_t level;
    const char* title;
} LevelTitle;

static const LevelTitle level_titles[] = {
    {1, "Subnet Beginner"},
    {3, "Network Apprentice"},
    {5, "Network Student"},
    {8, "Subnet Engineer"},
    {10, "CIDR Master"},
    {15, "Packet Guru"},
    {20, "Routing Wizard"},
};

static FlipperFormat* open_for_read(Storage* storage, const char* path, const char* header) {
    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool valid = false;

    if(flipper_format_file_open_existing(file, path)) {
        FuriString* type = furi_string_alloc();
        uint32_t version = 0;

        valid = flipper_format_read_header(file, type, &version) &&
                furi_string_equal_str(type, header) && version == FILE_VERSION;

        furi_string_free(type);
    }

    if(!valid) {
        flipper_format_free(file);
        return NULL;
    }
    return file;
}

static FlipperFormat* open_for_write(Storage* storage, const char* path, const char* header) {
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(!flipper_format_file_open_always(file, path) ||
       !flipper_format_write_header_cstr(file, header, FILE_VERSION)) {
        flipper_format_free(file);
        return NULL;
    }
    return file;
}

static void read_value(FlipperFormat* file, const char* key, uint32_t* value) {
    flipper_format_rewind(file);
    flipper_format_read_uint32(file, key, value, 1);
}

void stats_reset(Stats* stats) {
    memset(stats, 0, sizeof(Stats));
}

void stats_load(Stats* stats) {
    stats_reset(stats);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = open_for_read(storage, STATS_PATH, STATS_HEADER);

    if(file) {
        read_value(file, "XP", &stats->xp);
        read_value(file, "Questions", &stats->questions);
        read_value(file, "Correct", &stats->correct);
        read_value(file, "BestStreak", &stats->best_streak);
        read_value(file, "FastestMs", &stats->fastest_ms);
        read_value(file, "Sessions", &stats->sessions);
        flipper_format_free(file);
    }

    furi_record_close(RECORD_STORAGE);

    if(stats->correct > stats->questions) stats->correct = stats->questions;
}

void stats_save(const Stats* stats) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = open_for_write(storage, STATS_PATH, STATS_HEADER);

    if(file) {
        flipper_format_write_uint32(file, "XP", &stats->xp, 1);
        flipper_format_write_uint32(file, "Questions", &stats->questions, 1);
        flipper_format_write_uint32(file, "Correct", &stats->correct, 1);
        flipper_format_write_uint32(file, "BestStreak", &stats->best_streak, 1);
        flipper_format_write_uint32(file, "FastestMs", &stats->fastest_ms, 1);
        flipper_format_write_uint32(file, "Sessions", &stats->sessions, 1);
        flipper_format_free(file);
    }

    furi_record_close(RECORD_STORAGE);
}

void settings_load(Settings* settings) {
    settings->difficulty = DifficultyBeginner;
    settings->exam_index = 0;
    settings->sound = true;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = open_for_read(storage, SETTINGS_PATH, SETTINGS_HEADER);

    if(file) {
        uint32_t difficulty = DifficultyBeginner;
        uint32_t exam_index = 0;
        uint32_t sound = 1;

        read_value(file, "Difficulty", &difficulty);
        read_value(file, "ExamIndex", &exam_index);
        read_value(file, "Sound", &sound);
        flipper_format_free(file);

        if(difficulty < DifficultyCount) settings->difficulty = (Difficulty)difficulty;
        if(exam_index < 3) settings->exam_index = (uint8_t)exam_index;
        settings->sound = sound != 0;
    }

    furi_record_close(RECORD_STORAGE);
}

void settings_save(const Settings* settings) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = open_for_write(storage, SETTINGS_PATH, SETTINGS_HEADER);

    if(file) {
        uint32_t difficulty = settings->difficulty;
        uint32_t exam_index = settings->exam_index;
        uint32_t sound = settings->sound ? 1 : 0;

        flipper_format_write_uint32(file, "Difficulty", &difficulty, 1);
        flipper_format_write_uint32(file, "ExamIndex", &exam_index, 1);
        flipper_format_write_uint32(file, "Sound", &sound, 1);
        flipper_format_free(file);
    }

    furi_record_close(RECORD_STORAGE);
}

uint32_t stats_level(uint32_t xp) {
    uint32_t level = xp / XP_PER_LEVEL + 1;
    return level > MAX_LEVEL ? MAX_LEVEL : level;
}

uint32_t stats_accuracy(const Stats* stats) {
    if(stats->questions == 0) return 0;
    return stats->correct * 100 / stats->questions;
}

const char* level_title(uint32_t level) {
    const char* title = level_titles[0].title;
    for(size_t i = 0; i < COUNT_OF(level_titles); i++) {
        if(level >= level_titles[i].level) title = level_titles[i].title;
    }
    return title;
}

const char* accuracy_rank(uint32_t accuracy) {
    if(accuracy >= 95) return "Routing Wizard";
    if(accuracy >= 85) return "CIDR Master";
    if(accuracy >= 70) return "Network Apprentice";
    if(accuracy >= 50) return "Subnet Student";
    return "Keep Training";
}
