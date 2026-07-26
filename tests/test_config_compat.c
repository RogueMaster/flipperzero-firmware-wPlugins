#include "morse_flipper_config_test.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    MorseFlipperListeningSettings source = {
        .local_dit_ms = 57U, .lesson = 37U, .group_size = 8U, .session_groups = 29U,
        .custom_set_idx = 7U, .input_source = 2U, .farnsworth_wpm = 19U, .answer_timeout_s = 9U,
        .group_pause_s = 14U};
    MorseFlipperListeningSettings loaded = {0};
    uint8_t bytes[632] = {0};

    mf_config_test_save(&source, bytes);
    assert(bytes[0] == 1U && bytes[4] == 37U && bytes[5] == 8U && bytes[6] == 29U);
    assert(bytes[7] == 2U && bytes[8] == 57U && bytes[9] == 0U && bytes[15] == 7U);
    assert(bytes[20] == 19U && bytes[21] == 9U && bytes[22] == 14U);
    assert(mf_config_test_load(bytes, &loaded));
    assert(memcmp(&source, &loaded, sizeof(source)) == 0);

    bytes[0] = 0U;
    assert(!mf_config_test_load(bytes, &loaded));
    bytes[0] = 1U;
    bytes[4] = 0U; bytes[5] = 99U; bytes[6] = 1U; bytes[7] = 9U; bytes[8] = 0U; bytes[15] = 9U;
    bytes[20] = 31U; bytes[21] = 2U; bytes[22] = 16U;
    assert(mf_config_test_load(bytes, &loaded));
    assert(loaded.local_dit_ms == 100U && loaded.lesson == 1U && loaded.group_size == 1U);
    assert(loaded.session_groups == 3U && loaded.custom_set_idx == 0U && loaded.input_source == 0U);
    assert(loaded.farnsworth_wpm == 12U && loaded.answer_timeout_s == 6U && loaded.group_pause_s == 3U);
    puts("test_config_compat: passed");
    return 0;
}
