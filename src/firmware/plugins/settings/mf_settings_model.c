#include "mf_settings_model.h"

#include "../../trainer_lesson.h"

#include <string.h>

bool mf_settings_snapshot_normalize(MfSettingsSnapshot* snapshot) {
    bool changed = false;

    if(snapshot == NULL) return false;
    if(snapshot->local_wpm < 10U) { snapshot->local_wpm = 10U; changed = true; }
    if(snapshot->local_wpm > 30U) { snapshot->local_wpm = 30U; changed = true; }
    if(snapshot->lesson < 1U) { snapshot->lesson = 1U; changed = true; }
    if(snapshot->lesson > morse_trainer_lesson_count()) {
        snapshot->lesson = (uint8_t)morse_trainer_lesson_count();
        changed = true;
    }
    if(snapshot->farnsworth_wpm < 1U) { snapshot->farnsworth_wpm = 1U; changed = true; }
    if(snapshot->farnsworth_wpm > snapshot->local_wpm) {
        snapshot->farnsworth_wpm = (uint8_t)snapshot->local_wpm;
        changed = true;
    }
    if(snapshot->rx_callsigns_length > 5U) {
        snapshot->rx_callsigns_length = 5U;
        changed = true;
    }
    if(snapshot->rx_callsigns_wpm < 10U) {
        snapshot->rx_callsigns_wpm = 10U;
        changed = true;
    }
    if(snapshot->rx_callsigns_wpm > 30U) {
        snapshot->rx_callsigns_wpm = 30U;
        changed = true;
    }
    if(snapshot->rx_callsigns_farnsworth_wpm < 1U) {
        snapshot->rx_callsigns_farnsworth_wpm = 1U;
        changed = true;
    }
    if(snapshot->rx_callsigns_farnsworth_wpm > snapshot->rx_callsigns_wpm) {
        snapshot->rx_callsigns_farnsworth_wpm = snapshot->rx_callsigns_wpm;
        changed = true;
    }
    return changed;
}

uint8_t mf_settings_row_count(uint8_t entry, const MfSettingsSnapshot* snapshot) {
    if(snapshot == NULL) return 0U;
    switch(entry) {
    case MfSettingsEntryKeying: return 4U;
    case MfSettingsEntryAudio: return snapshot->audio_path == 2U ? 3U : 4U;
    case MfSettingsEntryListening: return 8U;
    case MfSettingsEntryStraight: return 3U;
    case MfSettingsEntryTxGroups: return 1U;
    case MfSettingsEntryRxCallsigns: return 3U;
    case MfSettingsEntryGpio: return 4U;
    case MfSettingsEntryUsb: return 4U;
    default: return 0U;
    }
}

bool mf_settings_parse_custom_names(
    char* scratch,
    size_t scratch_size,
    MfSettingsCustomNames* names) {
    char* line;

    if(scratch == NULL || scratch_size == 0U || names == NULL) return false;
    memset(names, 0, sizeof(*names));
    scratch[scratch_size - 1U] = '\0';
    line = scratch;
    while(*line != '\0' && names->count < MF_SETTINGS_CUSTOM_SET_CAP) {
        char* next = strchr(line, '\n');
        char* separator;

        if(next != NULL) *next++ = '\0';
        separator = strchr(line, '=');
        if(separator != NULL) {
            *separator++ = '\0';
            if(line[0] != '\0' && separator[0] != '\0') {
                strncpy(
                    names->names[names->count], line, MF_SETTINGS_CUSTOM_NAME_CAP - 1U);
                names->count++;
            }
        }
        if(next == NULL) break;
        line = next;
    }
    return names->count != 0U;
}
