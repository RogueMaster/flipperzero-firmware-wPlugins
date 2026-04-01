#include "sam_key_label.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool seader_sam_key_label_is_missing(const uint8_t* value, size_t value_len) {
    if(!value || value_len == 0U) {
        return true;
    }

    for(size_t i = 0; i < value_len; i++) {
        if(value[i] != 0x00U) {
            return false;
        }
    }

    return true;
}

void seader_sam_key_label_format(
    bool sam_present,
    SeaderSamKeyProbeStatus probe_status,
    const uint8_t* elite_ice_value,
    size_t elite_ice_value_len,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';

    if(!sam_present) {
        snprintf(out, out_size, "NO SAM");
        return;
    }

    if(probe_status == SeaderSamKeyProbeStatusUnknown) {
        snprintf(out, out_size, "SAM: Key Unknown");
        return;
    }

    if(probe_status == SeaderSamKeyProbeStatusProbeFailed) {
        snprintf(out, out_size, "SAM: Probe Failed");
        return;
    }

    if(probe_status == SeaderSamKeyProbeStatusVerifiedStandard) {
        snprintf(out, out_size, "SAM: Standard Key");
        return;
    }

    if(seader_sam_key_label_is_missing(elite_ice_value, elite_ice_value_len)) {
        snprintf(out, out_size, "SAM: Probe Failed");
        return;
    }

    size_t pos = 0U;
    pos += (size_t)snprintf(out + pos, out_size - pos, "SAM: ");
    if(pos >= out_size) {
        out[out_size - 1U] = '\0';
        return;
    }

    for(size_t i = 0; i < elite_ice_value_len && pos + 1U < out_size; i++) {
        unsigned char ch = elite_ice_value[i];
        out[pos++] = isprint(ch) ? (char)ch : '?';
    }

    out[pos] = '\0';
}
