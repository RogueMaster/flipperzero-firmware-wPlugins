#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include "highscores.h"

void highscores_load(HighScoreList* list, const char* path) {
    list->count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    if(flipper_format_file_open_existing(file, path)) {
        char key[12];
        for(int i = 0; i < HIGHSCORES_COUNT; i++) {
            FuriString* name_str = furi_string_alloc();
            snprintf(key, sizeof(key), "Name%d", i);
            if(!flipper_format_read_string(file, key, name_str)) {
                furi_string_free(name_str);
                break;
            }
            snprintf(key, sizeof(key), "Score%d", i);
            int32_t score = 0;
            if(!flipper_format_read_int32(file, key, &score, 1)) {
                furi_string_free(name_str);
                break;
            }
            strncpy(list->entries[i].name, furi_string_get_cstr(name_str), HIGHSCORE_NAME_LEN);
            list->entries[i].name[HIGHSCORE_NAME_LEN] = '\0';
            list->entries[i].score = (int)score;
            list->count++;
            furi_string_free(name_str);
        }
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void highscores_save(const HighScoreList* list, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    storage_common_mkdir(storage, APP_DATA_PATH(""));

    if(flipper_format_file_open_always(file, path)) {
        flipper_format_write_header_cstr(file, "StrataHero Highscores", 1);
        char key[12];
        for(int i = 0; i < list->count; i++) {
            snprintf(key, sizeof(key), "Name%d", i);
            flipper_format_write_string_cstr(file, key, list->entries[i].name);
            snprintf(key, sizeof(key), "Score%d", i);
            int32_t score = (int32_t)list->entries[i].score;
            flipper_format_write_int32(file, key, &score, 1);
        }
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool highscores_qualifies(const HighScoreList* list, int score) {
    if(score <= 0) return false;
    if(list->count < HIGHSCORES_COUNT) return true;
    return score > list->entries[list->count - 1].score;
}

void highscores_insert(HighScoreList* list, const char* name, int score) {
    int pos = list->count;
    for(int i = 0; i < list->count; i++) {
        if(score > list->entries[i].score) {
            pos = i;
            break;
        }
    }

    int new_count = list->count < HIGHSCORES_COUNT ? list->count + 1 : HIGHSCORES_COUNT;
    for(int i = new_count - 1; i > pos; i--) {
        list->entries[i] = list->entries[i - 1];
    }

    strncpy(list->entries[pos].name, name, HIGHSCORE_NAME_LEN);
    list->entries[pos].name[HIGHSCORE_NAME_LEN] = '\0';
    list->entries[pos].score = score;
    list->count = new_count;
}
