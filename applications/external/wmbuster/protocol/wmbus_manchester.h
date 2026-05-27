#pragma once
/*
 * Manchester (G.E. Thomas, IEEE 802.3) decoder for wM-Bus S-mode.
 * EN 13757-4 §6.2.1.2 uses '01' = 1 and '10' = 0 (G.E. Thomas).
 *
 * Some implementations use the inverse; we accept both and return the one
 * with fewer transition errors.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    WmbusManchesterIeee802 = 0, /* 01->1, 10->0  (preferred) */
    WmbusManchesterThomas  = 1, /* inverse        */
} WmbusManchesterPolarity;

/* Decode `chip_bits` chip-level bits packed MSB-first into `chips`.
 * Output up to chip_bits/2 data bytes packed MSB-first into `out`.
 * Returns number of *bits* decoded (always even). `*errors` counts invalid
 * 11/00 pairs (which the decoder treats as 0 with an error tick). */
size_t wmbus_manchester_decode(
    const uint8_t* chips,
    size_t chip_bits,
    WmbusManchesterPolarity pol,
    uint8_t* out,
    size_t out_capacity_bits,
    size_t* errors);
