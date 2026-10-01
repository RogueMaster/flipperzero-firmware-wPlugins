#include "protocol_bf_probe.h"

#include "protocols_common.h"
#include "psa.h"
#include "renault_v1.h"

#include <furi.h>

#define HITAG2_BF_KEY_FIELD "Hitag2 Key"
#define HITAG2_BF_RECOVERED "Recovered"
#define HITAG2_BF_KEY_SIZE  6U

bool protopirate_bf_probe_psa_needs_bruteforce(FlipperFormat* ff) {
    if(!ff) {
        return false;
    }
    flipper_format_rewind(ff);
    if(pp_verify_protocol_name(ff, PSA_PROTOCOL_NAME) != SubGhzProtocolStatusOk ||
       !flipper_format_key_exist(ff, FF_KEY)) {
        return false;
    }

    // A serial is only written once the key has been recovered.
    uint32_t serial = 0;
    flipper_format_rewind(ff);
    return !flipper_format_read_uint32(ff, FF_SERIAL, &serial, 1);
}

bool protopirate_bf_probe_hitag2_needs_bruteforce(FlipperFormat* ff) {
    if(!ff) {
        return false;
    }
    flipper_format_rewind(ff);
    if(pp_verify_protocol_name(ff, RENAULT_PROTOCOL_V1_NAME) != SubGhzProtocolStatusOk ||
       !flipper_format_key_exist(ff, FF_KEY)) {
        return false;
    }

    // Older captures store Recovered as hex, newer ones as uint32.
    uint8_t recovered = 0;
    flipper_format_rewind(ff);
    if(!flipper_format_read_hex(ff, HITAG2_BF_RECOVERED, &recovered, 1)) {
        uint32_t recovered_u32 = 0;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, HITAG2_BF_RECOVERED, &recovered_u32, 1)) {
            recovered = (uint8_t)recovered_u32;
        }
    }
    if(recovered != 0) {
        return false;
    }

    // A stored non-zero key leaves nothing to search for.
    uint8_t stored_key[HITAG2_BF_KEY_SIZE] = {0};
    flipper_format_rewind(ff);
    if(flipper_format_read_hex(ff, HITAG2_BF_KEY_FIELD, stored_key, sizeof(stored_key))) {
        for(size_t i = 0; i < sizeof(stored_key); i++) {
            if(stored_key[i]) {
                return false;
            }
        }
    }

    return true;
}

bool protopirate_bf_probe_needs_bruteforce(FlipperFormat* ff) {
    return protopirate_bf_probe_psa_needs_bruteforce(ff) ||
           protopirate_bf_probe_hitag2_needs_bruteforce(ff);
}
