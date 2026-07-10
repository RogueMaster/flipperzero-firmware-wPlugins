#include "include/domain/version_info.h"

#include <stddef.h>

static bool parse_uint_segment(const char *start, const char *end, int *out) {
    if (start == end) {
        return false; // empty segment, e.g. "1..3" or ".2.3"
    }
    int value = 0;
    for (const char *p = start; p < end; p++) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        value = value * 10 + (*p - '0');
    }
    *out = value;
    return true;
}

bool gurpil_version_parse(const char *version, int *major, int *minor, int *patch) {
    if (version == NULL || major == NULL || minor == NULL || patch == NULL) {
        return false;
    }

    const char *first_dot = NULL;
    const char *second_dot = NULL;
    for (const char *p = version; *p != '\0'; p++) {
        if (*p == '.') {
            if (first_dot == NULL) {
                first_dot = p;
            } else if (second_dot == NULL) {
                second_dot = p;
            } else {
                return false; // more than two dots
            }
        }
    }
    if (first_dot == NULL || second_dot == NULL) {
        return false; // needs exactly three dot-separated segments
    }

    const char *end = version;
    while (*end != '\0') {
        end++;
    }

    return parse_uint_segment(version, first_dot, major) &&
           parse_uint_segment(first_dot + 1, second_dot, minor) &&
           parse_uint_segment(second_dot + 1, end, patch);
}
