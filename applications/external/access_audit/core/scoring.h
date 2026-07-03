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

/* Underlying air-interface standard for a card type (e.g. "ISO 14443-4A" for
 * HID Seos, "ISO 15693" for iCLASS, "125 kHz" for LF). Returned alongside the
 * friendly name in outputs so the real protocol is always documented. NULL for
 * Unknown. */
const char* card_type_protocol(CardType type);

/* OWASP Risk Rating Methodology framing
 * (https://owasp.org/www-community/OWASP_Risk_Rating_Methodology): the score is
 * a LIKELIHOOD-of-compromise rating only — how easily the credential technology
 * can be cloned or its secret recovered. IMPACT (what the credential protects)
 * is assessed in engagement context, not by this tool. */

/* Likelihood band for a severity: HIGH / MODERATE / LOW / MINIMAL. */
const char* likelihood_label(Severity severity);

/* On-device verdict word for a severity: HIGH RISK / MODERATE / LOW RISK /
 * SECURE. Mirrors the result-screen label; shown in the report alongside the
 * likelihood band so the two never drift. */
const char* verdict_label(Severity severity);

/* OWASP "Ease of Exploit" likelihood factor for a credential:
 * trivial / moderate / hard / indeterminate. */
const char* ease_of_exploit(const AccessObservation* obs);
