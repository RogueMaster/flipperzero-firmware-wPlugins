/*
 * MD5 context type — struct definition only.
 * Exposed so that esp_loader_t can be fully defined in the public header,
 * enabling stack allocation.  The MD5 functions (MD5Init/Update/Final) are
 * kept in the private header md5_hash.h and are not part of the public API.
 *
 * Original implementation:
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 * Distributed under the BSD licence.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MD5Context {
    uint32_t buf[4];
    uint32_t bits[2];
    uint8_t  in[64];
};

#ifdef __cplusplus
}
#endif
