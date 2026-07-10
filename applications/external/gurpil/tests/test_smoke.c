#include "include/domain/version_info.h"

#include <assert.h>
#include <stdio.h>

static void test_parses_valid_semver(void) {
    int major = -1, minor = -1, patch = -1;
    assert(gurpil_version_parse("0.1.0", &major, &minor, &patch));
    assert(major == 0 && minor == 1 && patch == 0);

    assert(gurpil_version_parse("12.34.5", &major, &minor, &patch));
    assert(major == 12 && minor == 34 && patch == 5);
}

static void test_rejects_malformed_input(void) {
    int major, minor, patch;
    assert(!gurpil_version_parse("abc", &major, &minor, &patch));
    assert(!gurpil_version_parse("1.2", &major, &minor, &patch));
    assert(!gurpil_version_parse("1.2.3.4", &major, &minor, &patch));
    assert(!gurpil_version_parse("1..3", &major, &minor, &patch));
    assert(!gurpil_version_parse(NULL, &major, &minor, &patch));
}

int main(void) {
    test_parses_valid_semver();
    printf("test_parses_valid_semver: PASS\n");

    test_rejects_malformed_input();
    printf("test_rejects_malformed_input: PASS\n");

    return 0;
}
