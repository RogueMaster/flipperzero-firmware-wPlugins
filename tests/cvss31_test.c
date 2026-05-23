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
        /* Real-world CVE examples provided by CVEalert.io. */
        {
            .name = "CVE-2026-41940 cPanel WHM auth bypass",
            .values = {0, 0, 0, 0, 0, 2, 2, 2},
            .expected_tenths = 98,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H",
        },
        {
            .name = "CVE-2025-54236 Adobe Commerce input validation",
            .values = {0, 0, 0, 0, 0, 2, 2, 0},
            .expected_tenths = 91,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:N",
        },
        {
            .name = "CVE-2025-32432 Craft CMS RCE",
            .values = {0, 0, 0, 0, 1, 2, 2, 2},
            .expected_tenths = 100,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:C/C:H/I:H/A:H",
        },
        {
            .name = "CVE-2025-68613 n8n expression RCE",
            .values = {0, 0, 1, 0, 0, 2, 2, 2},
            .expected_tenths = 88,
            .expected_severity = "HIGH",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:N/S:U/C:H/I:H/A:H",
        },
        {
            .name = "CVE-2025-29927 Next.js middleware auth bypass",
            .values = {0, 0, 0, 0, 0, 2, 2, 0},
            .expected_tenths = 91,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:N",
        },
        {
            .name = "CVE-2024-43044 Jenkins remoting file read",
            .values = {0, 0, 1, 0, 0, 2, 2, 2},
            .expected_tenths = 88,
            .expected_severity = "HIGH",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:N/S:U/C:H/I:H/A:H",
        },
        {
            .name = "CVE-2024-12539 Elasticsearch DLS bypass",
            .values = {0, 0, 1, 0, 0, 2, 0, 0},
            .expected_tenths = 65,
            .expected_severity = "MEDIUM",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:N/S:U/C:H/I:N/A:N",
        },
        /* Educational examples used by the app. */
        {
            .name = "example RCE",
            .values = {0, 0, 0, 0, 0, 2, 2, 2},
            .expected_tenths = 98,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H",
        },
        {
            .name = "example SQL injection",
            .values = {0, 0, 0, 0, 0, 2, 2, 1},
            .expected_tenths = 94,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:L",
        },
        {
            .name = "example auth bypass",
            .values = {0, 0, 0, 0, 0, 2, 2, 0},
            .expected_tenths = 91,
            .expected_severity = "CRITICAL",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:N",
        },
        {
            .name = "example SSRF",
            .values = {0, 0, 0, 0, 1, 2, 0, 0},
            .expected_tenths = 86,
            .expected_severity = "HIGH",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:C/C:H/I:N/A:N",
        },
        {
            .name = "example IDOR BOLA read",
            .values = {0, 0, 1, 0, 0, 2, 0, 0},
            .expected_tenths = 65,
            .expected_severity = "MEDIUM",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:N/S:U/C:H/I:N/A:N",
        },
        {
            .name = "example reflected XSS",
            .values = {0, 0, 0, 1, 1, 1, 1, 0},
            .expected_tenths = 61,
            .expected_severity = "MEDIUM",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:R/S:C/C:L/I:L/A:N",
        },
        {
            .name = "example stored XSS",
            .values = {0, 0, 1, 1, 1, 1, 1, 0},
            .expected_tenths = 54,
            .expected_severity = "MEDIUM",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:L/UI:R/S:C/C:L/I:L/A:N",
        },
        {
            .name = "example open redirect",
            .values = {0, 0, 0, 1, 0, 0, 1, 0},
            .expected_tenths = 43,
            .expected_severity = "MEDIUM",
            .expected_vector = "CVSS:3.1/AV:N/AC:L/PR:N/UI:R/S:U/C:N/I:L/A:N",
        },
        {
            .name = "example debug info leak",
            .values = {0, 1, 1, 0, 0, 1, 0, 0},
            .expected_tenths = 31,
            .expected_severity = "LOW",
            .expected_vector = "CVSS:3.1/AV:N/AC:H/PR:L/UI:N/S:U/C:L/I:N/A:N",
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
