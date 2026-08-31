/*
 * SmartTAG Color 2.6 (type 1626) geometry and page policy.
 * Glass is landscape 296x152. The image header is 152x296 (short x long).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define TAGTINKER_TYPE_SMARTAG_COLOR_26 1626
#define TAGTINKER_COLOR26_WIRE_W 152U
#define TAGTINKER_COLOR26_WIRE_H 296U
#define TAGTINKER_COLOR26_GLASS_W 296U
#define TAGTINKER_COLOR26_GLASS_H 152U

static inline bool tagtinker_type_needs_wh_swap(uint16_t type_code) {
    return type_code == TAGTINKER_TYPE_SMARTAG_COLOR_26;
}

static inline bool tagtinker_type_uses_ui_page(uint16_t type_code) {
    return tagtinker_type_needs_wh_swap(type_code);
}

/* Store-used Color 2.6 tags keep the barcode on page 1. Page 2 is the image
 * slot. Unspecified pages (0 or 1) map there. Explicit 2-7 are left alone. */
static inline uint8_t tagtinker_color26_resolve_page(uint8_t page) {
    if(page <= 1U) return 2U;
    if(page > 7U) return 7U;
    return page;
}

/* proto (px, py) on the 152x296 wire canvas -> glass (bx, by) 296x152. */
static inline void tagtinker_color26_proto_to_glass(
    uint16_t proto_w,
    uint16_t px,
    uint16_t py,
    uint16_t* bx,
    uint16_t* by) {
    *bx = py;
    *by = (uint16_t)(proto_w - 1U - px);
}
