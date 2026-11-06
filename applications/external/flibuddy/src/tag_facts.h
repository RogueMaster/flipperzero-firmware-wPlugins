/* What a Bambu Lab MIFARE Classic tag says by itself, read on the Flipper
 * for a result nobody answered (no app): the screens show it as they show
 * the host's `tag_info`. It mirrors the host's TagParser and LookupBuilder
 * (and tools/bridge/bambu_tag.py), field for field:
 *
 *   block 1  variant id (bytes 0-7, "A00-W1"), filament id (8-15, "GFA00")
 *   block 2  material ("PLA")
 *   block 4  detailed type ("PLA Basic"; the subtype is it without the material)
 *   block 5  RGBA (0-3), label weight uint16 LE g (4-5), diameter float32 LE mm (8-11)
 *   block 6  uint16 LE: drying temp, drying hours, bed type, bed temp,
 *            nozzle max, nozzle min (°C)
 *
 * Blocks of a sector that did not read whole are left out; text is the
 * printable ASCII up to the first NUL, trimmed (anything else: none);
 * zero numbers, and temperatures outside 1-999, are not on the tag. The
 * colour code and name come from Bambu's table (bambu_colors.h). NTAG
 * tags carry nothing the app reads.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "proto_decode.h"
#include "tag_dump.h"

typedef struct {
    ProtoTagInfo info; /* the fields the host would send in `tag_info` */
    uint16_t drying_temp; /* °C, 0 when not on the tag */
    uint16_t drying_hours;
    uint16_t bed_temp; /* °C */
} TagFacts;

/* Fills `facts` (cleared first); false when the tag says nothing: not a
 * MIFARE Classic, or none of blocks 1, 2, 4, 5 and 6 read. */
bool tag_facts_read(const TagDump* dump, TagFacts* facts);
