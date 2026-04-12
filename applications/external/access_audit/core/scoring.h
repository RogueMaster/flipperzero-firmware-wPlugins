#pragma once

#include <stdint.h>
#include "observation.h"

typedef enum {
    SeverityInfo = 0,
    SeverityLow,
    SeverityMedium,
    SeverityHigh,
} Severity;

typedef struct {
    uint8_t score;
    uint8_t confidence;
    Severity max_severity;
} AuditScore;

AuditScore score_observation(const AccessObservation* obs);
const char* severity_to_string(Severity severity);
const char* card_type_to_string(CardType type);
