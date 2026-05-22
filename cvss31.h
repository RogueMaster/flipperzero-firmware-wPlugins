#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
    Cvss31MetricAttackVector,
    Cvss31MetricAttackComplexity,
    Cvss31MetricPrivilegesRequired,
    Cvss31MetricUserInteraction,
    Cvss31MetricScope,
    Cvss31MetricConfidentiality,
    Cvss31MetricIntegrity,
    Cvss31MetricAvailability,
    Cvss31MetricCount,
} Cvss31MetricId;

#define CVSS31_METRIC_COUNT ((uint8_t)Cvss31MetricCount)

typedef struct {
    const char* label;
    const char* code;
} Cvss31Option;

typedef struct {
    const char* title;
    const char* metric_code;
    uint8_t option_count;
    Cvss31Option options[4];
} Cvss31Metric;

typedef struct {
    uint8_t values[CVSS31_METRIC_COUNT];
} Cvss31BaseVector;

typedef struct {
    uint8_t tenths;
    const char* severity;
} Cvss31Score;

void cvss31_base_vector_reset(Cvss31BaseVector* vector);
bool cvss31_base_vector_set(Cvss31BaseVector* vector, Cvss31MetricId metric_id, uint8_t option);
uint8_t cvss31_base_vector_get(const Cvss31BaseVector* vector, Cvss31MetricId metric_id);
bool cvss31_base_vector_is_valid(const Cvss31BaseVector* vector);
const Cvss31Metric* cvss31_metric_get(Cvss31MetricId metric_id);
uint8_t cvss31_metric_count(void);
Cvss31Score cvss31_base_score(const Cvss31BaseVector* vector);
const char* cvss31_severity(uint8_t score_tenths);
void cvss31_format_score(char* buffer, size_t buffer_size, uint8_t score_tenths);
void cvss31_format_metric_line(
    const Cvss31BaseVector* vector,
    char* buffer,
    size_t buffer_size,
    uint8_t first,
    uint8_t last);
void cvss31_format_vector(const Cvss31BaseVector* vector, char* buffer, size_t buffer_size);
const char* cvss31_explain_attack_vector(const Cvss31BaseVector* vector);
const char* cvss31_explain_attack_complexity(const Cvss31BaseVector* vector);
const char* cvss31_explain_privileges_required(const Cvss31BaseVector* vector);
const char* cvss31_explain_impact(const Cvss31BaseVector* vector);
