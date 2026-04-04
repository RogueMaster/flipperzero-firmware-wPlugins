#include "session.h"
#include <string.h>
#include <stdint.h>

void session_init(ScanSession* session) {
    memset(session, 0, sizeof(*session));
}

bool session_append(ScanSession* session, const AccessObservation* obs, const AuditScore* score) {
    if(!session || !obs || !score) return false;
    if(session->count >= SESSION_MAX_ENTRIES) return false;

    /* Deduplicate by UID. Cards without a UID cannot be deduplicated and are
     * always appended. Cards with a UID are skipped if already present. */
    if(obs->uid_present && obs->uid_len > 0) {
        for(size_t i = 0; i < session->count; i++) {
            const AccessObservation* existing = &session->entries[i].obs;
            if(existing->uid_present && existing->uid_len == obs->uid_len &&
               memcmp(existing->uid, obs->uid, obs->uid_len) == 0) {
                return false;
            }
        }
    }

    session->entries[session->count].obs = *obs;
    session->entries[session->count].score = *score;
    session->count++;
    return true;
}

SessionSummary session_summarise(const ScanSession* session) {
    SessionSummary s = {0};
    s.most_common_type = CardTypeUnknown;

    if(!session || session->count == 0) return s;

    /* Count card types to find the most common. */
    uint8_t type_counts[32] = {0}; /* covers all CardType values */

    for(size_t i = 0; i < session->count; i++) {
        const SessionEntry* e = &session->entries[i];
        switch(e->score.max_severity) {
        case SeverityHigh:
            s.high++;
            break;
        case SeverityMedium:
            s.medium++;
            break;
        case SeverityLow:
            s.low++;
            break;
        default:
            s.secure++;
            break;
        }
        size_t t = (size_t)e->obs.card_type;
        if(t < sizeof(type_counts)) type_counts[t]++;
    }

    uint8_t best = 0;
    for(size_t t = 1; t < sizeof(type_counts); t++) {
        if(type_counts[t] > best) {
            best = type_counts[t];
            s.most_common_type = (CardType)t;
        }
    }

    return s;
}
