/*
 * CVSS v3.1 base metric implementation.
 *
 * Stores the supported base metrics and option codes, validates and mutates
 * base vectors, computes the official CVSS v3.1 base score and severity, and
 * formats scores, metric ranges, vectors, and short explanations for the UI.
 */
#include "cvss31.h"

#include <stdio.h>
#include <string.h>

static const Cvss31Metric cvss31_metrics[] = {
    {
        .title = "Attack Vector",
        .metric_code = "AV",
        .option_count = 4,
        .options = {{"Network", "N"}, {"Adjacent", "A"}, {"Local", "L"}, {"Physical", "P"}},
    },
    {
        .title = "Attack Complexity",
        .metric_code = "AC",
        .option_count = 2,
        .options = {{"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "Privileges Required",
        .metric_code = "PR",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "User Interaction",
        .metric_code = "UI",
        .option_count = 2,
        .options = {{"None", "N"}, {"Required", "R"}},
    },
    {
        .title = "Scope",
        .metric_code = "S",
        .option_count = 2,
        .options = {{"Unchanged", "U"}, {"Changed", "C"}},
    },
    {
        .title = "Confidentiality",
        .metric_code = "C",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "Integrity",
        .metric_code = "I",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "Availability",
        .metric_code = "A",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
};

void cvss31_base_vector_reset(Cvss31BaseVector* vector) {
    if(!vector) {
        return;
    }

    vector->values[Cvss31MetricAttackVector] = 0; /* AV:N */
    vector->values[Cvss31MetricAttackComplexity] = 0; /* AC:L */
    vector->values[Cvss31MetricPrivilegesRequired] = 1; /* PR:L */
    vector->values[Cvss31MetricUserInteraction] = 0; /* UI:N */
    vector->values[Cvss31MetricScope] = 0; /* S:U */
    vector->values[Cvss31MetricConfidentiality] = 2; /* C:H */
    vector->values[Cvss31MetricIntegrity] = 2; /* I:H */
    vector->values[Cvss31MetricAvailability] = 2; /* A:H */
}

bool cvss31_base_vector_set(Cvss31BaseVector* vector, Cvss31MetricId metric_id, uint8_t option) {
    const Cvss31Metric* metric = cvss31_metric_get(metric_id);

    if(!vector || !metric || option >= metric->option_count) {
        return false;
    }

    vector->values[metric_id] = option;
    return true;
}

uint8_t cvss31_base_vector_get(const Cvss31BaseVector* vector, Cvss31MetricId metric_id) {
    const Cvss31Metric* metric = cvss31_metric_get(metric_id);

    if(!vector || !metric || vector->values[metric_id] >= metric->option_count) {
        return 0;
    }

    return vector->values[metric_id];
}

bool cvss31_base_vector_is_valid(const Cvss31BaseVector* vector) {
    if(!vector) {
        return false;
    }

    for(uint8_t i = 0; i < CVSS31_METRIC_COUNT; i++) {
        const Cvss31MetricId metric_id = (Cvss31MetricId)i;
        const Cvss31Metric* metric = cvss31_metric_get(metric_id);

        if(!metric || vector->values[metric_id] >= metric->option_count) {
            return false;
        }
    }

    return true;
}

const Cvss31Metric* cvss31_metric_get(Cvss31MetricId metric_id) {
    return metric_id < Cvss31MetricCount ? &cvss31_metrics[metric_id] : NULL;
}

uint8_t cvss31_metric_count(void) {
    return CVSS31_METRIC_COUNT;
}

static float cvss31_pow15(float value) {
    float result = 1.0f;

    for(uint8_t i = 0; i < 15; i++) {
        result *= value;
    }

    return result;
}

static float cvss31_min(float a, float b) {
    return a < b ? a : b;
}

static void cvss31_append(char* buffer, size_t buffer_size, const char* text) {
    if(buffer_size == 0) {
        return;
    }

    const size_t used = strlen(buffer);

    if(used < buffer_size) {
        snprintf(buffer + used, buffer_size - used, "%s", text);
    }
}

static uint8_t cvss31_roundup_tenths(float input) {
    /* CVSS v3.1 requires rounding up to one decimal place, not nearest rounding. */
    uint32_t scaled = (uint32_t)(input * 100000.0f + 0.5f);

    if((scaled % 10000) == 0) {
        return scaled / 10000;
    }

    return (scaled / 10000) + 1;
}

Cvss31Score cvss31_base_score(const Cvss31BaseVector* vector) {
    /* Constants and formulas follow the official CVSS v3.1 Base Score specification. */
    static const float attack_vector_weights[] = {0.85f, 0.62f, 0.55f, 0.20f};
    static const float attack_complexity_weights[] = {0.77f, 0.44f};
    static const float user_interaction_weights[] = {0.85f, 0.62f};
    static const float impact_weights[] = {0.0f, 0.22f, 0.56f};
    static const float privilege_required_weights_unchanged[] = {0.85f, 0.62f, 0.27f};
    static const float privilege_required_weights_changed[] = {0.85f, 0.68f, 0.50f};

    if(!vector) {
        return (Cvss31Score){.tenths = 0, .severity = cvss31_severity(0)};
    }

    const uint8_t scope = cvss31_base_vector_get(vector, Cvss31MetricScope);
    const float av =
        attack_vector_weights[cvss31_base_vector_get(vector, Cvss31MetricAttackVector)];
    const float ac =
        attack_complexity_weights[cvss31_base_vector_get(vector, Cvss31MetricAttackComplexity)];
    const uint8_t privileges = cvss31_base_vector_get(vector, Cvss31MetricPrivilegesRequired);
    const float pr = scope == 0 ? privilege_required_weights_unchanged[privileges] :
                                  privilege_required_weights_changed[privileges];
    const float ui =
        user_interaction_weights[cvss31_base_vector_get(vector, Cvss31MetricUserInteraction)];
    const float c = impact_weights[cvss31_base_vector_get(vector, Cvss31MetricConfidentiality)];
    const float i = impact_weights[cvss31_base_vector_get(vector, Cvss31MetricIntegrity)];
    const float a = impact_weights[cvss31_base_vector_get(vector, Cvss31MetricAvailability)];

    const float iss = 1.0f - ((1.0f - c) * (1.0f - i) * (1.0f - a));
    const float impact = scope == 0 ? 6.42f * iss :
                                      7.52f * (iss - 0.029f) - 3.25f * cvss31_pow15(iss - 0.02f);

    if(impact <= 0.0f) {
        return (Cvss31Score){.tenths = 0, .severity = cvss31_severity(0)};
    }

    const float exploitability = 8.22f * av * ac * pr * ui;
    const float raw_score = scope == 0 ? impact + exploitability :
                                         1.08f * (impact + exploitability);
    const uint8_t score_tenths = cvss31_roundup_tenths(cvss31_min(raw_score, 10.0f));

    return (Cvss31Score){.tenths = score_tenths, .severity = cvss31_severity(score_tenths)};
}

const char* cvss31_severity(uint8_t score_tenths) {
    if(score_tenths == 0) return "NONE";
    if(score_tenths < 40) return "LOW";
    if(score_tenths < 70) return "MEDIUM";
    if(score_tenths < 90) return "HIGH";
    return "CRITICAL";
}

void cvss31_format_score(char* buffer, size_t buffer_size, uint8_t score_tenths) {
    if(!buffer || buffer_size == 0) {
        return;
    }

    snprintf(buffer, buffer_size, "%u.%u", score_tenths / 10, score_tenths % 10);
}

void cvss31_format_metric_line(
    const Cvss31BaseVector* vector,
    char* buffer,
    size_t buffer_size,
    uint8_t first,
    uint8_t last) {
    if(!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';

    if(!vector) {
        return;
    }

    if(first >= CVSS31_METRIC_COUNT || last >= CVSS31_METRIC_COUNT || first > last) {
        return;
    }

    for(uint8_t i = first; i <= last; i++) {
        const Cvss31MetricId metric_id = (Cvss31MetricId)i;
        const Cvss31Metric* metric = cvss31_metric_get(metric_id);
        const Cvss31Option* option = &metric->options[cvss31_base_vector_get(vector, metric_id)];

        char token[12];
        snprintf(token, sizeof(token), "%s:%s", metric->metric_code, option->code);

        if(i != first) {
            cvss31_append(buffer, buffer_size, " ");
        }

        cvss31_append(buffer, buffer_size, token);
    }
}

void cvss31_format_vector(const Cvss31BaseVector* vector, char* buffer, size_t buffer_size) {
    if(!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';

    if(!vector) {
        return;
    }

    snprintf(buffer, buffer_size, "%s", "CVSS:3.1/");

    for(uint8_t i = 0; i < CVSS31_METRIC_COUNT; i++) {
        const Cvss31MetricId metric_id = (Cvss31MetricId)i;
        const Cvss31Metric* metric = cvss31_metric_get(metric_id);
        const Cvss31Option* option = &metric->options[cvss31_base_vector_get(vector, metric_id)];

        char token[12];
        snprintf(token, sizeof(token), "%s:%s", metric->metric_code, option->code);

        if(i != 0) {
            cvss31_append(buffer, buffer_size, "/");
        }

        cvss31_append(buffer, buffer_size, token);
    }
}

const char* cvss31_explain_attack_vector(const Cvss31BaseVector* vector) {
    const uint8_t attack_vector = cvss31_base_vector_get(vector, Cvss31MetricAttackVector);

    if(attack_vector == 0) return "Network reachable";
    if(attack_vector == 1) return "Adjacent network";
    if(attack_vector == 2) return "Local access";
    return "Physical access";
}

const char* cvss31_explain_attack_complexity(const Cvss31BaseVector* vector) {
    return cvss31_base_vector_get(vector, Cvss31MetricAttackComplexity) == 0 ? "Low complexity" :
                                                                               "High complexity";
}

const char* cvss31_explain_privileges_required(const Cvss31BaseVector* vector) {
    const uint8_t privileges = cvss31_base_vector_get(vector, Cvss31MetricPrivilegesRequired);

    if(privileges == 0) return "No privileges";
    if(privileges == 1) return "Limited privileges";
    return "High privileges";
}

const char* cvss31_explain_impact(const Cvss31BaseVector* vector) {
    const uint8_t confidentiality = cvss31_base_vector_get(vector, Cvss31MetricConfidentiality);
    const uint8_t integrity = cvss31_base_vector_get(vector, Cvss31MetricIntegrity);
    const uint8_t availability = cvss31_base_vector_get(vector, Cvss31MetricAvailability);

    if(confidentiality == 2 || integrity == 2 || availability == 2) {
        return "High C/I/A impact";
    }

    if(confidentiality || integrity || availability) {
        return "Partial C/I/A impact";
    }

    return "No C/I/A impact";
}
