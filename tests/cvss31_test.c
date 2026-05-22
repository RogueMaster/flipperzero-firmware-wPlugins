/*
 * Standalone CVSS v3.1 scoring regression tests.
 *
 * Builds several known base vectors, verifies their calculated score and
 * severity, and checks that formatted vector strings match the expected
 * CVSS:3.1 notation. This can be compiled outside the Flipper firmware tree.
 */
#include "cvss31.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char* name;
    uint8_t values[CVSS31_METRIC_COUNT];
    uint8_t expected_tenths;
    const char* expected_severity;
    const char* expected_vector;
} Cvss31ReferenceCase;

static bool cvss31_test_set_vector(Cvss31BaseVector* vector, const uint8_t* values) {
    for(uint8_t i = 0; i < CVSS31_METRIC_COUNT; i++) {
        if(!cvss31_base_vector_set(vector, (Cvss31MetricId)i, values[i])) {
            return false;
        }
    }

    return true;
}

static bool cvss31_test_case(const Cvss31ReferenceCase* test_case) {
    Cvss31BaseVector vector;
    char vector_text[96];
    Cvss31Score score;

    cvss31_base_vector_reset(&vector);

    if(!cvss31_test_set_vector(&vector, test_case->values)) {
        printf("FAIL %s: rejected reference vector\n", test_case->name);
        return false;
    }

    score = cvss31_base_score(&vector);
    cvss31_format_vector(&vector, vector_text, sizeof(vector_text));

    if(score.tenths != test_case->expected_tenths) {
        printf(
            "FAIL %s: score %u.%u, expected %u.%u\n",
            test_case->name,
            score.tenths / 10,
            score.tenths % 10,
            test_case->expected_tenths / 10,
            test_case->expected_tenths % 10);
        return false;
    }

    if(strcmp(score.severity, test_case->expected_severity) != 0) {
        printf(
            "FAIL %s: severity %s, expected %s\n",
            test_case->name,
            score.severity,
            test_case->expected_severity);
        return false;
    }

    if(strcmp(vector_text, test_case->expected_vector) != 0) {
        printf(
            "FAIL %s: vector %s, expected %s\n",
            test_case->name,
            vector_text,
            test_case->expected_vector);
        return false;
    }

    return true;
}

int main(void) {
    static const Cvss31ReferenceCase cases[] = {
        {
            .name = "none",
            .values = {0, 0, 0, 0, 0, 0, 0, 0},
            .expected_tenths = 0,
            .expected_severity = "NONE",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:N/I:N/A:N",
        },
        {
            .name = "high",
            .values = {0, 0, 1, 0, 0, 2, 2, 2},
            .expected_tenths = 88,
            .expected_severity = "HIGH",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:N/S:U/C:H/I:H/A:H",
        },
        {
            .name = "critical",
            .values = {0, 0, 0, 0, 0, 2, 2, 2},
            .expected_tenths = 98,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H",
        },
        {
            .name = "scope changed critical",
            .values = {0, 0, 0, 0, 1, 2, 2, 2},
            .expected_tenths = 100,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:C/C:H/I:H/A:H",
        },
        {
            .name = "medium",
            .values = {0, 0, 1, 1, 0, 1, 1, 0},
            .expected_tenths = 46,
            .expected_severity = "MEDIUM",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:R/S:U/C:L/I:L/A:N",
        },
    };

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if(!cvss31_test_case(&cases[i])) {
            return 1;
        }
    }

    printf("PASS %u CVSS v3.1 reference cases\n", (unsigned)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
