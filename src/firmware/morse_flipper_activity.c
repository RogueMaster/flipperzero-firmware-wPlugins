#include "morse_flipper_activity.h"

#include "morse_flipper_progress.h"

#include <string.h>

#define MORSE_FLIPPER_ACTIVITY_DAY_NONE UINT16_MAX

void morse_flipper_activity_daily_reset(MorseFlipperActivityDaily* daily) {
    if(daily == NULL) return;
    *daily = (MorseFlipperActivityDaily){
        .day = MORSE_FLIPPER_ACTIVITY_DAY_NONE,
    };
}

bool morse_flipper_activity_daily_note(
    MorseFlipperActivityDaily* daily,
    uint16_t day,
    MorseFlipperActivityKind kind) {
    bool award = false;

    if(daily == NULL || day == MORSE_FLIPPER_ACTIVITY_DAY_NONE) return false;
    if(daily->day != day) {
        daily->day = day;
        daily->deed_awards = 0U;
        daily->correct_answers = 0U;
    }
    if(kind == MorseFlipperActivityListeningSession) {
        award = daily->deed_awards < 3U;
    } else if(kind == MorseFlipperActivityCorrectAnswer) {
        if(daily->correct_answers < 2U) {
            daily->correct_answers++;
        } else {
            daily->correct_answers = 0U;
            award = daily->deed_awards < 3U;
        }
    }
    if(award) daily->deed_awards++;
    return award;
}

#ifdef MORSE_FLIPPER_FAP
#include <applications/services/dolphin/dolphin.h>
#include <furi.h>
#include <storage/storage.h>

#define MORSE_FLIPPER_ACTIVITY_PATH      APP_DATA_PATH("activity.bin")
#define MORSE_FLIPPER_ACTIVITY_TEMP_PATH APP_DATA_PATH("activity.tmp")
#define MORSE_FLIPPER_ACTIVITY_MAGIC     0x4D464143UL
#define MORSE_FLIPPER_ACTIVITY_VERSION   1U

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t deed_awards;
    uint8_t correct_answers;
    uint8_t reserved;
    uint16_t day;
} MorseFlipperActivityRecord;

_Static_assert(sizeof(MorseFlipperActivityRecord) == 12U, "activity record size changed");

static void morse_flipper_activity_load(MorseFlipperActivityDaily* daily) {
    Storage* storage;
    File* file;
    MorseFlipperActivityRecord record = {0};
    bool loaded = false;

    morse_flipper_activity_daily_reset(daily);
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    for(uint8_t attempt = 0U; attempt < 2U && !loaded; attempt++) {
        const char* path =
            attempt == 0U ? MORSE_FLIPPER_ACTIVITY_PATH : MORSE_FLIPPER_ACTIVITY_TEMP_PATH;
        loaded = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                 storage_file_size(file) == sizeof(record) &&
                 storage_file_read(file, &record, sizeof(record)) == sizeof(record) &&
                 record.magic == MORSE_FLIPPER_ACTIVITY_MAGIC &&
                 record.version == MORSE_FLIPPER_ACTIVITY_VERSION;
        loaded = storage_file_close(file) && loaded;
    }
    if(loaded) {
        daily->day = record.day;
        daily->deed_awards = record.deed_awards > 3U ? 3U : record.deed_awards;
        daily->correct_answers = record.correct_answers > 2U ? 0U : record.correct_answers;
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static bool morse_flipper_activity_save(const MorseFlipperActivityDaily* daily) {
    Storage* storage;
    File* file;
    MorseFlipperActivityRecord record;
    bool saved;

    if(daily == NULL) return false;
    record = (MorseFlipperActivityRecord){
        .magic = MORSE_FLIPPER_ACTIVITY_MAGIC,
        .version = MORSE_FLIPPER_ACTIVITY_VERSION,
        .deed_awards = daily->deed_awards,
        .correct_answers = daily->correct_answers,
        .day = daily->day,
    };
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    storage_common_remove(storage, MORSE_FLIPPER_ACTIVITY_TEMP_PATH);
    saved =
        storage_file_open(file, MORSE_FLIPPER_ACTIVITY_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        storage_file_write(file, &record, sizeof(record)) == sizeof(record);
    saved = storage_file_close(file) && saved;
    if(saved)
        saved = storage_common_rename(
                    storage,
                    MORSE_FLIPPER_ACTIVITY_TEMP_PATH,
                    MORSE_FLIPPER_ACTIVITY_PATH) == FSE_OK;
    else
        storage_common_remove(storage, MORSE_FLIPPER_ACTIVITY_TEMP_PATH);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
}

static void morse_flipper_activity_note_deed(
    uint16_t practice_day,
    MorseFlipperActivityKind kind) {
    MorseFlipperActivityDaily daily;
    bool award;

    morse_flipper_activity_load(&daily);
    award = morse_flipper_activity_daily_note(&daily, practice_day, kind);
    if(!morse_flipper_activity_save(&daily)) return;
    if(award) dolphin_deed(DolphinDeedPluginGameWin);
}

void morse_flipper_activity_note_rx(bool correct_answer) {
    MorseFlipperProgress progress;
    uint16_t practice_day = MORSE_FLIPPER_PROGRESS_DAY_NONE;

    if(!morse_flipper_progress_today(&practice_day)) return;
    if(morse_flipper_progress_load(&progress)) {
        morse_flipper_progress_note_rx_activity(&progress, true, practice_day);
        morse_flipper_progress_save(&progress);
    }
    if(correct_answer)
        morse_flipper_activity_note_deed(
            practice_day, MorseFlipperActivityCorrectAnswer);
}

void morse_flipper_activity_note_listening_session(uint16_t practice_day) {
    if(practice_day == MORSE_FLIPPER_PROGRESS_DAY_NONE) return;
    morse_flipper_activity_note_deed(
        practice_day, MorseFlipperActivityListeningSession);
}
#else
void morse_flipper_activity_note_rx(bool correct_answer) {
    (void)correct_answer;
}

void morse_flipper_activity_note_listening_session(uint16_t practice_day) {
    (void)practice_day;
}
#endif
