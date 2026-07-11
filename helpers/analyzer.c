#include "analyzer.h"
#include <string.h>

const char* rc_class_label(RcCodeClass cls) {
    switch(cls) {
    case RcCodeStatic:
        return "Fixed";
    case RcCodeDynamic:
        return "Rolling";
    default:
        return "Unknown";
    }
}

const char* rc_health_label(RcHealth h) {
    switch(h) {
    case RcHealthHealthy:
        return "HEALTHY";
    case RcHealthLikely:
        return "LIKELY OK";
    case RcHealthCaution:
        return "CAUTION";
    case RcHealthAtRisk:
        return "AT RISK";
    default:
        return "UNKNOWN";
    }
}

/* Count distinct fingerprints among the captures. */
static uint8_t rc_unique(const RcCapture* caps, uint8_t n) {
    uint8_t u = 0;
    for(uint8_t i = 0; i < n; i++) {
        bool seen = false;
        for(uint8_t j = 0; j < i; j++) {
            if(caps[j].fingerprint == caps[i].fingerprint) {
                seen = true;
                break;
            }
        }
        if(!seen) u++;
    }
    return u;
}

void rc_analyze(const RcCapture* caps, uint8_t n, RcVerdict* out) {
    memset(out, 0, sizeof(*out));
    out->presses = n;

    if(n == 0) {
        out->health = RcHealthUnknown;
        strcpy(out->letter, "?");
        strcpy(out->protocol, "No signal");
        out->cls = RcCodeUnknown;
        strcpy(out->headline, "Nothing decoded");
        strcpy(
            out->detail,
            "No remote was decoded on this band.\n\n"
            "- Hold the fob close to the Flipper\n"
            "- Check the frequency (315 US /\n  433.92 EU are most common)\n"
            "- If it still fails, switch the\n  modulation to FM in Settings.");
        out->meter = 0;
        out->unique = 0;
        return;
    }

    /* Representative protocol = the most recent capture (fobs are consistent). */
    const RcCapture* last = &caps[n - 1];
    strncpy(out->protocol, last->protocol, sizeof(out->protocol) - 1);
    out->cls = last->cls;
    out->unique = rc_unique(caps, n);

    if(out->cls == RcCodeDynamic) {
        if(n >= 2 && out->unique == n) {
            out->health = RcHealthHealthy;
            strcpy(out->letter, "A");
            strcpy(out->headline, "Rolling code confirmed");
            snprintf(
                out->detail,
                sizeof(out->detail),
                "%s is a rolling code, and every\n"
                "press produced a DIFFERENT parcel\n"
                "(%d presses, %d unique codes).\n\n"
                "A recorded press is useless to a\n"
                "replay attacker - the code has\n"
                "already rolled forward. This is\n"
                "how a healthy remote should behave.",
                out->protocol,
                out->presses,
                out->unique);
            out->meter = 100;
        } else if(n >= 2 && out->unique >= 2) {
            out->health = RcHealthLikely;
            strcpy(out->letter, "B");
            strcpy(out->headline, "Rolling, codes advancing");
            snprintf(
                out->detail,
                sizeof(out->detail),
                "%s is a rolling code and the\n"
                "parcel changed across presses\n"
                "(%d presses, %d unique codes).\n\n"
                "Some presses repeated - likely the\n"
                "same press captured twice. Press a\n"
                "few more times with a short pause\n"
                "to see a clean climb.",
                out->protocol,
                out->presses,
                out->unique);
            out->meter = 78;
        } else if(n >= 2 && out->unique == 1) {
            out->health = RcHealthCaution;
            strcpy(out->letter, "C");
            strcpy(out->headline, "Rolling, but code held still");
            snprintf(
                out->detail,
                sizeof(out->detail),
                "%s is a rolling protocol, but the\n"
                "SAME parcel came back on all %d\n"
                "presses.\n\n"
                "Usually just too-fast presses (the\n"
                "fob resends its last frame). Wait\n"
                "~1s between presses and retest. If\n"
                "it never changes, the fob may be\n"
                "faulty or a static clone.",
                out->protocol,
                out->presses);
            out->meter = 40;
        } else {
            /* single press of a dynamic protocol */
            out->health = RcHealthLikely;
            strcpy(out->letter, "B");
            strcpy(out->headline, "Rolling protocol detected");
            snprintf(
                out->detail,
                sizeof(out->detail),
                "%s is a rolling-code protocol.\n\n"
                "Only one press was captured, so the\n"
                "roll hasn't been observed yet. Press\n"
                "the remote 2-3 more times to confirm\n"
                "the code climbs each time.",
                out->protocol);
            out->meter = 68;
        }
    } else if(out->cls == RcCodeStatic) {
        out->health = RcHealthAtRisk;
        strcpy(out->letter, "F");
        strcpy(out->headline, "Fixed code - replayable");
        snprintf(
            out->detail,
            sizeof(out->detail),
            "%s is a STATIC fixed code. The same\n"
            "parcel is sent every single press\n"
            "(%d presses, %d unique code%s).\n\n"
            "Anyone who records one press can\n"
            "replay it later to trigger your\n"
            "remote. Consider upgrading to a\n"
            "rolling-code receiver where it\n"
            "guards something that matters.",
            out->protocol,
            out->presses,
            out->unique,
            out->unique == 1 ? "" : "s");
        out->meter = 8;
    } else {
        out->health = RcHealthUnknown;
        strcpy(out->letter, "?");
        strcpy(out->headline, "Signal seen, not classified");
        snprintf(
            out->detail,
            sizeof(out->detail),
            "A signal was decoded but its rolling/\n"
            "fixed nature couldn't be determined\n"
            "(protocol: %s).\n\n"
            "Try another modulation (AM/FM) or\n"
            "frequency in Settings and retest.",
            out->protocol);
        out->meter = 0;
    }
}
