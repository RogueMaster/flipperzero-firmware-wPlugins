#include "credential_sio_label.h"

#include <stdio.h>

bool seader_sio_label_format(
    bool has_sio,
    bool is_picopass_sio_context,
    uint8_t sio_start_block,
    char* out,
    size_t out_size) {
    if(out && out_size > 0U) {
        out[0] = '\0';
    }

    if(!out || out_size == 0U || !has_sio) {
        return false;
    }

    if(!is_picopass_sio_context) {
        snprintf(out, out_size, "+SIO");
        return true;
    }

    /* Picopass/iClass-only SIO labeling. DESFire/other media do not use block-derived SR/SE labels. */
    switch(sio_start_block) {
    case 6:
        snprintf(out, out_size, "+SIO(SE)");
        return true;
    case 10:
        snprintf(out, out_size, "+SIO(SR)");
        return true;
    default:
        snprintf(out, out_size, "+SIO(?)");
        return true;
    }
}
