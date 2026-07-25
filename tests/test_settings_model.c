#include "mf_settings_model.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    MfSettingsSnapshot snapshot = {
        .local_wpm = 9U,
        .lesson = 0U,
        .farnsworth_wpm = 31U,
        .audio_path = 2U,
    };

    assert(mf_settings_snapshot_normalize(&snapshot));
    assert(snapshot.local_wpm == 10U);
    assert(snapshot.lesson == 1U);
    assert(snapshot.farnsworth_wpm == 10U);
    assert(mf_settings_row_count(MfSettingsEntryAudio, &snapshot) == 3U);
    snapshot.audio_path = 0U;
    assert(mf_settings_row_count(MfSettingsEntryAudio, &snapshot) == 4U);
    assert(mf_settings_row_count(MfSettingsEntryGpio, &snapshot) == 4U);
    char custom[] = "numbers=0123456789\nmore dits=EISH5AUVNDB\ninvalid\n=empty\n";
    MfSettingsCustomNames names;
    assert(mf_settings_parse_custom_names(custom, sizeof(custom), &names));
    assert(names.count == 2U);
    assert(names.names[0][0] == 'n');
    assert(names.names[1][0] == 'm');
    printf("test_settings_model: passed\n");
    return 0;
}
