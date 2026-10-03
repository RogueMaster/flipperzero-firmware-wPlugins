#include <stdio.h>
#include <string.h>

#include "plugins/ardf/mf_ardf_settings.h"

static unsigned checks;
#define CHECK(x)                                                                    \
    do {                                                                            \
        checks++;                                                                   \
        if(!(x)) {                                                                  \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
            return 1;                                                               \
        }                                                                           \
    } while(0)

static bool normalized(const char* input, const char* expected) {
    char output[MF_ARDF_CUSTOM_CAPACITY + 1U];
    mf_ardf_normalize_custom(output, sizeof(output), input);
    return strcmp(output, expected) == 0;
}

int main(void) {
    static const uint16_t expected_intervals[] = {
        3U,   5U,   8U,   10U,  12U,  15U,  20U,  24U,  30U,  45U,  60U,  75U,  90U,  105U,
        120U, 180U, 240U, 300U, 360U, 420U, 480U, 540U, 600U, 660U, 720U, 780U, 840U, 900U,
    };
    MfArdfSettings defaults;
    MfArdfSettings loaded;
    MfArdfConfigFile file;
    uint8_t short_data[4] = {0};
    unsigned i;
    mf_ardf_settings_defaults(&defaults);
    CHECK(mf_ardf_settings_valid(&defaults));
    CHECK(defaults.mode == MfArdfModeCustom);
    CHECK(defaults.modulation == MfArdfModulationCw);
    CHECK(defaults.message == MfArdfMessage1);
    CHECK(strcmp(defaults.custom, "FOX") == 0);
    CHECK(defaults.interval_index == MF_ARDF_DEFAULT_INTERVAL_INDEX);
    CHECK(mf_ardf_interval_seconds(defaults.interval_index) == 60U);
    CHECK(defaults.wpm == 10U);

    mf_ardf_settings_encode(&defaults, &file);
    CHECK(mf_ardf_settings_decode(&loaded, &file, sizeof(file)));
    CHECK(memcmp(&defaults, &loaded, sizeof(defaults)) == 0);
    CHECK(!mf_ardf_settings_decode(&loaded, short_data, sizeof(short_data)));
    CHECK(memcmp(&defaults, &loaded, sizeof(defaults)) == 0);
    file.version++;
    CHECK(!mf_ardf_settings_decode(&loaded, &file, sizeof(file)));
    file.version = MF_ARDF_CONFIG_VERSION;
    file.settings.wpm = 31U;
    CHECK(!mf_ardf_settings_decode(&loaded, &file, sizeof(file)));
    CHECK(memcmp(&defaults, &loaded, sizeof(defaults)) == 0);

    CHECK(normalized("fox", "FOX"));
    CHECK(normalized("a_b", "A B"));
    CHECK(normalized(" a___b ", "A B"));
    CHECK(normalized("a!b?2", "AB2"));
    CHECK(normalized("abcdef", "ABCDE"));
    CHECK(normalized("___", ""));
    CHECK(MF_ARDF_INTERVAL_COUNT == sizeof(expected_intervals) / sizeof(expected_intervals[0]));
    for(i = 0U; i < MF_ARDF_INTERVAL_COUNT; i++) {
        CHECK(mf_ardf_interval_seconds(i) == expected_intervals[i]);
        CHECK(mf_ardf_interval_label(i)[0] != '\0');
    }
    CHECK(strcmp(mf_ardf_interval_label(0U), "3 s") == 0);
    CHECK(strcmp(mf_ardf_interval_label(10U), "60 s") == 0);
    CHECK(strcmp(mf_ardf_interval_label(15U), "3 min") == 0);
    CHECK(strcmp(mf_ardf_interval_label(27U), "15 min") == 0);
    printf("test_ardf_settings: %u checks passed\n", checks);
    return 0;
}
