#include "mf_passive_policy.h"

#include <stdio.h>
#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#include <storage/storage.h>

#define MF_PASSIVE_SETTINGS_PATH      APP_DATA_PATH("passive.bin")
#define MF_PASSIVE_SETTINGS_TEMP_PATH APP_DATA_PATH("passive.tmp")
#endif
#define MF_PASSIVE_SETTINGS_MAGIC    0x4D465053UL
#define MF_PASSIVE_SETTINGS_VERSION  2U
#define MF_PASSIVE_SELECTED_ROW_MASK 0x7FU
#define MF_PASSIVE_TRANSMIT_FM_FLAG  0x80U

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t mode;
    uint8_t length;
    uint8_t lesson;
    uint16_t dit_ms;
    uint8_t farnsworth_wpm;
    uint8_t vibrate;
    uint8_t answer_delay_s;
    uint8_t repeat_after_answer;
    uint8_t selected_row;
    uint8_t courtesy_delay_half_s;
} MfPassiveSettingsRecord;

_Static_assert(sizeof(MfPassiveSettingsRecord) == 16U, "passive settings record size changed");

static const char mf_passive_teaching_order[] = "KMURESNAPTLWI.JZFOY,VG5/Q92H38B?47C1D60X";
static const char* const mf_passive_length_labels[] = {
    "",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "4-5",
    "5-6",
    "4-6",
};

void mf_passive_settings_length_bounds(
    uint8_t selection,
    uint8_t mode,
    uint8_t* min_length,
    uint8_t* max_length) {
    uint8_t min = mode ? 1U : 4U;
    uint8_t max = min;

    if(selection >= min && selection <= 6U) {
        min = selection;
        max = selection;
    } else if(selection == 7U) {
        min = 4U;
        max = 5U;
    } else if(selection == 8U) {
        min = 5U;
        max = 6U;
    } else if(selection == 9U) {
        min = 4U;
        max = 6U;
    }
    if(min_length != NULL) *min_length = min;
    if(max_length != NULL) *max_length = max;
}

const char* mf_passive_settings_length_label(uint8_t selection) {
    if(selection >= 1U && selection <= 9U) return mf_passive_length_labels[selection];
    return "?";
}

size_t mf_passive_settings_lesson_count(void) {
    /* Like Listening, lesson 1 introduces the first two characters. */
    return sizeof(mf_passive_teaching_order) - 2U;
}

uint8_t mf_passive_settings_lesson_charset_len(uint8_t lesson) {
    size_t count = mf_passive_settings_lesson_count();

    if(lesson < 1U) lesson = 1U;
    if(lesson > count) lesson = (uint8_t)count;
    return (uint8_t)(lesson + 1U);
}

uint8_t mf_passive_settings_wpm(const MfPassiveSettingsModel* model) {
    uint16_t dit;
    uint8_t wpm;

    if(model == NULL || model->dit_ms == 0U) return 25U;
    dit = model->dit_ms;
    wpm = (uint8_t)((1200U + dit / 2U) / dit);
    if(wpm < 10U) return 10U;
    if(wpm > 30U) return 30U;
    return wpm;
}

void mf_passive_settings_normalize(MfPassiveSettingsModel* model) {
    uint8_t wpm;
    if(model == NULL) return;
    if(model->mode > 1U) model->mode = 0U;
    if(model->length < (model->mode ? 1U : 4U) || model->length > 9U)
        model->length = model->mode ? 1U : 4U;
    if(model->lesson == 0U) model->lesson = 1U;
    if(model->lesson > mf_passive_settings_lesson_count())
        model->lesson = (uint8_t)mf_passive_settings_lesson_count();
    if(model->dit_ms == 0U) model->dit_ms = 48U;
    wpm = mf_passive_settings_wpm(model);
    if(model->farnsworth_wpm == 0U || model->farnsworth_wpm > wpm) model->farnsworth_wpm = wpm;
    model->vibrate = model->vibrate ? 1U : 0U;
    if(model->answer_delay_s < 1U) model->answer_delay_s = 1U;
    if(model->answer_delay_s > 5U) model->answer_delay_s = 5U;
    model->repeat_after_answer = model->repeat_after_answer ? 1U : 0U;
    if(model->courtesy_delay_half_s > 10U) model->courtesy_delay_half_s = 2U;
    if(model->selected_row > 9U) model->selected_row = 0U;
    model->transmit_fm = model->transmit_fm ? 1U : 0U;
}

const char* mf_passive_settings_lesson_charset(void) {
    return mf_passive_teaching_order;
}

void mf_passive_settings_lesson_label(uint8_t lesson, char* out, size_t out_size) {
    size_t count = mf_passive_settings_lesson_count();

    if(out == NULL || out_size == 0U) return;
    if(lesson < 1U) lesson = 1U;
    if(lesson > count) lesson = (uint8_t)count;
    if(lesson == 1U)
        snprintf(
            out, out_size, "1 - %c %c", mf_passive_teaching_order[0], mf_passive_teaching_order[1]);
    else
        snprintf(out, out_size, "%u - %c", (unsigned)lesson, mf_passive_teaching_order[lesson]);
}

static MfPassiveSettingsModel mf_passive_settings_default(void) {
    return (MfPassiveSettingsModel){
        .length = 4U,
        .lesson = 1U,
        .dit_ms = 48U,
        .farnsworth_wpm = 12U,
        .vibrate = 1U,
        .answer_delay_s = 3U,
        .courtesy_delay_half_s = 2U,
    };
}

void mf_passive_settings_load(MfPassiveSettingsModel* model) {
#ifdef MORSE_FLIPPER_FAP
    Storage* storage;
    File* file;
    MfPassiveSettingsRecord record = {0};
    bool loaded = false;

    if(model == NULL) return;
    *model = mf_passive_settings_default();
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    for(uint8_t attempt = 0U; attempt < 2U && !loaded; attempt++) {
        const char* path = attempt == 0U ? MF_PASSIVE_SETTINGS_PATH :
                                           MF_PASSIVE_SETTINGS_TEMP_PATH;
        loaded = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                 storage_file_size(file) == sizeof(record) &&
                 storage_file_read(file, &record, sizeof(record)) == sizeof(record) &&
                 record.magic == MF_PASSIVE_SETTINGS_MAGIC &&
                 (record.version == 1U || record.version == MF_PASSIVE_SETTINGS_VERSION);
        loaded = storage_file_close(file) && loaded;
    }
    if(loaded) {
        model->mode = record.mode;
        model->length = record.length;
        model->lesson = record.lesson;
        model->dit_ms = record.dit_ms;
        model->farnsworth_wpm = record.farnsworth_wpm;
        model->vibrate = record.vibrate;
        model->answer_delay_s = record.answer_delay_s;
        model->repeat_after_answer = record.repeat_after_answer;
        model->selected_row = record.selected_row & MF_PASSIVE_SELECTED_ROW_MASK;
        model->transmit_fm = (record.selected_row & MF_PASSIVE_TRANSMIT_FM_FLAG) != 0U;
        model->courtesy_delay_half_s = record.version >= 2U ? record.courtesy_delay_half_s : 2U;
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    mf_passive_settings_normalize(model);
#else
    if(model != NULL) *model = mf_passive_settings_default();
#endif
}

bool mf_passive_settings_save(const MfPassiveSettingsModel* model) {
#ifdef MORSE_FLIPPER_FAP
    Storage* storage;
    File* file;
    MfPassiveSettingsRecord record;
    MfPassiveSettingsModel normalized;
    bool saved;

    if(model == NULL) return false;
    normalized = *model;
    mf_passive_settings_normalize(&normalized);
    record = (MfPassiveSettingsRecord){
        .magic = MF_PASSIVE_SETTINGS_MAGIC,
        .version = MF_PASSIVE_SETTINGS_VERSION,
        .mode = normalized.mode,
        .length = normalized.length,
        .lesson = normalized.lesson,
        .dit_ms = normalized.dit_ms,
        .farnsworth_wpm = normalized.farnsworth_wpm,
        .vibrate = normalized.vibrate,
        .answer_delay_s = normalized.answer_delay_s,
        .repeat_after_answer = normalized.repeat_after_answer,
        .selected_row = normalized.selected_row |
                        (normalized.transmit_fm ? MF_PASSIVE_TRANSMIT_FM_FLAG : 0U),
        .courtesy_delay_half_s = normalized.courtesy_delay_half_s,
    };
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    storage_common_remove(storage, MF_PASSIVE_SETTINGS_TEMP_PATH);
    saved =
        storage_file_open(file, MF_PASSIVE_SETTINGS_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        storage_file_write(file, &record, sizeof(record)) == sizeof(record);
    saved = storage_file_close(file) && saved;
    if(saved)
        saved = storage_common_rename(
                    storage, MF_PASSIVE_SETTINGS_TEMP_PATH, MF_PASSIVE_SETTINGS_PATH) == FSE_OK;
    else
        storage_common_remove(storage, MF_PASSIVE_SETTINGS_TEMP_PATH);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
#else
    (void)model;
    return false;
#endif
}
