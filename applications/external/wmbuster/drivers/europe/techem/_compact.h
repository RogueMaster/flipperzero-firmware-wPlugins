#pragma once
/* Shared helpers for Techem compact-frame drivers (CI 0xA0..0xA2).
 *
 * Layouts ported from wmbusmeters under GPL-3:
 *   _refs/wmbusmeters/src/driver_fhkvdataiii.cc   (HCA)
 *   _refs/wmbusmeters/src/driver_mkradio3.cc      (water with dates)
 *   _refs/wmbusmeters/src/driver_mkradio4.cc      (water without dates)
 *   _refs/wmbusmeters/src/driver_compact5.cc      (heat, kWh u24)
 *   _refs/wmbusmeters/src/driver_vario451.cc      (heat, GJ-scaled u16)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Hex-dump fallback used when a Techem payload is too short to
 * trust. Produces "<title>\n" followed by `Bytes <n>\n` and up to
 * 6 rows of 8-byte hex. */
size_t techem_hex_dump(const uint8_t* a, size_t l, char* o, size_t cap,
                       const char* title);

/* Decode an FHKV-III/IV HCA frame. `version` is the link-header
 * version byte and selects the temperature offset (offset 9 for
 * 0x69, offset 10 for 0x94). */
size_t techem_decode_fhkv(uint8_t version,
                          const uint8_t* a, size_t l,
                          char* o, size_t cap);

/* Decode an MK-Radio water frame. `with_dates` enables the
 * billing-period date fields (mkradio3 has them, mkradio4 ships
 * zero-filled date bytes that aren't worth showing). */
size_t techem_decode_water(bool with_dates,
                           const uint8_t* a, size_t l,
                           char* o, size_t cap);

/* Decode a compact5 heat meter frame (kWh, u24 prev/curr). */
size_t techem_decode_compact5(const uint8_t* a, size_t l,
                              char* o, size_t cap);

/* Decode a Vario 451 heat meter frame (GJ-scaled u16 prev/curr).
 * Rendered with both kWh and GJ figures. */
size_t techem_decode_vario(const uint8_t* a, size_t l,
                           char* o, size_t cap);

/* Decode a Techem smoke detector frame. The full alarm protocol
 * is not public; we surface status + flag bytes verbatim. */
size_t techem_decode_smoke(const uint8_t* a, size_t l,
                           char* o, size_t cap);
