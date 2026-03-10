#include "uhf_status_label.h"

#include <stdio.h>
#include <string.h>

static size_t seader_uhf_append_family(
    char* out,
    size_t out_size,
    size_t pos,
    bool* wrote_any,
    const char* name,
    bool key_present) {
    if(*wrote_any) {
        pos += (size_t)snprintf(out + pos, out_size - pos, "/");
    } else {
        pos += (size_t)snprintf(out + pos, out_size - pos, "UHF: ");
        *wrote_any = true;
    }

    pos += (size_t)snprintf(out + pos, out_size - pos, "%s", name);
    if(!key_present) {
        pos += (size_t)snprintf(out + pos, out_size - pos, " [no key]");
    }
    return pos;
}

void seader_uhf_status_label_format(
    bool has_monza4qt,
    bool monza4qt_key_present,
    bool has_higgs3,
    bool higgs3_key_present,
    char* out,
    size_t out_size) {
    bool wrote_any = false;
    size_t pos = 0U;

    if(!out || out_size == 0U) {
        return;
    }

    out[0] = '\0';

    if(has_monza4qt) {
        pos = seader_uhf_append_family(
            out, out_size, pos, &wrote_any, "Monza 4QT", monza4qt_key_present);
    }
    if(has_higgs3) {
        pos = seader_uhf_append_family(
            out, out_size, pos, &wrote_any, "Higgs 3", higgs3_key_present);
    }

    if(!wrote_any) {
        snprintf(out, out_size, "UHF: none");
    } else if(pos >= out_size) {
        out[out_size - 1U] = '\0';
    }
}
