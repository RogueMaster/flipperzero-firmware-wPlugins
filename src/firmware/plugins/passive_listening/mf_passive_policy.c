#include "mf_passive_policy.h"

#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#include <storage/storage.h>

#define MF_PASSIVE_SETTINGS_PATH APP_DATA_PATH("passive.bin")
#endif
#define MF_PASSIVE_SETTINGS_MAGIC 0x4D465053UL
#define MF_PASSIVE_SETTINGS_VERSION 1U

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
} MfPassiveSettingsRecord;

static const char mf_passive_teaching_order[] =
    "KMURESNAPTLWI.JZFOY,VG5/Q92H38B?47C1D60X";

uint8_t mf_passive_settings_wpm(const MfPassiveSettingsModel* model) {
    uint16_t dit;
    uint8_t wpm;

    if(model == NULL || model->dit_ms == 0U) return 12U;
    dit = model->dit_ms;
    wpm = (uint8_t)((1200U + dit / 2U) / dit);
    if(wpm < 10U) return 10U;
    if(wpm > 30U) return 30U;
    return wpm;
}

void mf_passive_settings_normalize(MfPassiveSettingsModel* model) {
    uint8_t wpm;
    uint8_t min_length;

    if(model == NULL) return;
    if(model->mode > 1U) model->mode = 0U;
    min_length = model->mode ? 3U : 4U;
    if(model->length < min_length) model->length = min_length;
    if(model->length > 6U) model->length = 6U;
    if(model->lesson == 0U) model->lesson = 1U;
    if(model->lesson >= sizeof(mf_passive_teaching_order)) model->lesson = sizeof(mf_passive_teaching_order) - 1U;
    if(model->dit_ms == 0U) model->dit_ms = 100U;
    wpm = mf_passive_settings_wpm(model);
    if(model->farnsworth_wpm == 0U || model->farnsworth_wpm > wpm) model->farnsworth_wpm = wpm;
    model->vibrate = model->vibrate ? 1U : 0U;
    if(model->answer_delay_s < 1U) model->answer_delay_s = 1U;
    if(model->answer_delay_s > 5U) model->answer_delay_s = 5U;
    model->repeat_after_answer = model->repeat_after_answer ? 1U : 0U;
    if(model->selected_row > 7U) model->selected_row = 0U;
}

const char* mf_passive_settings_lesson_charset(void) {
    return mf_passive_teaching_order;
}

static MfPassiveSettingsModel mf_passive_settings_default(void) {
    return (MfPassiveSettingsModel){
        .length = 4U,
        .lesson = 1U,
        .dit_ms = 100U,
        .farnsworth_wpm = 12U,
        .vibrate = 1U,
        .answer_delay_s = 3U,
    };
}

void mf_passive_settings_load(MfPassiveSettingsModel* model) {
#ifdef MORSE_FLIPPER_FAP
    Storage* storage;
    File* file;
    MfPassiveSettingsRecord record = {0};

    if(model == NULL) return;
    *model = mf_passive_settings_default();
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    if(storage_file_open(file, MF_PASSIVE_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_size(file) == sizeof(record) &&
       storage_file_read(file, &record, sizeof(record)) == sizeof(record) &&
       record.magic == MF_PASSIVE_SETTINGS_MAGIC && record.version == MF_PASSIVE_SETTINGS_VERSION) {
        model->mode = record.mode;
        model->length = record.length;
        model->lesson = record.lesson;
        model->dit_ms = record.dit_ms;
        model->farnsworth_wpm = record.farnsworth_wpm;
        model->vibrate = record.vibrate;
        model->answer_delay_s = record.answer_delay_s;
        model->repeat_after_answer = record.repeat_after_answer;
        model->selected_row = record.selected_row;
    }
    storage_file_close(file);
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
    bool saved;

    if(model == NULL) return false;
    record = (MfPassiveSettingsRecord){
        .magic = MF_PASSIVE_SETTINGS_MAGIC,
        .version = MF_PASSIVE_SETTINGS_VERSION,
        .mode = model->mode,
        .length = model->length,
        .lesson = model->lesson,
        .dit_ms = model->dit_ms,
        .farnsworth_wpm = model->farnsworth_wpm,
        .vibrate = model->vibrate,
        .answer_delay_s = model->answer_delay_s,
        .repeat_after_answer = model->repeat_after_answer,
        .selected_row = model->selected_row,
    };
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    saved = storage_file_open(file, MF_PASSIVE_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
            storage_file_write(file, &record, sizeof(record)) == sizeof(record);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
#else
    (void)model;
    return false;
#endif
}
