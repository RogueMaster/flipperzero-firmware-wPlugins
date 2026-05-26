#include "include/infrastructure/pack_reader.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Builds the .idx blob in memory matching the .tsv lines below.
 * Returns the number of bytes used in `out`. */
static size_t build_idx_blob(uint8_t *out, size_t cap, const uint32_t *offsets, uint32_t count) {
    const size_t need = 10u + (size_t)count * 4u;
    if (cap < need)
        return 0u;
    out[0] = 'T';
    out[1] = 'R';
    out[2] = 'V';
    out[3] = 'I';
    out[4] = 0x01;
    out[5] = 0x00;
    out[6] = (uint8_t)(count & 0xFFu);
    out[7] = (uint8_t)((count >> 8) & 0xFFu);
    out[8] = (uint8_t)((count >> 16) & 0xFFu);
    out[9] = (uint8_t)((count >> 24) & 0xFFu);
    for (uint32_t i = 0u; i < count; ++i) {
        const size_t pos = 10u + (size_t)i * 4u;
        out[pos + 0] = (uint8_t)(offsets[i] & 0xFFu);
        out[pos + 1] = (uint8_t)((offsets[i] >> 8) & 0xFFu);
        out[pos + 2] = (uint8_t)((offsets[i] >> 16) & 0xFFu);
        out[pos + 3] = (uint8_t)((offsets[i] >> 24) & 0xFFu);
    }
    return need;
}

int main(void) {
    /* TSV lines */
    const char *tsv = "0\t1\tCapital of Spain?\tMadrid\n"
                      "1\t6\tFirst World Cup year?\t1930\n"
                      "2\t7\tWhat is H2O?\tWater\n";

    /* Compute offsets */
    uint32_t offsets[3];
    offsets[0] = 0u;
    offsets[1] = (uint32_t)(strchr(tsv, '\n') - tsv) + 1u;
    const char *second_nl = strchr(tsv + offsets[1], '\n');
    offsets[2] = (uint32_t)(second_nl - tsv) + 1u;

    uint8_t idx[64];
    const size_t idx_len = build_idx_blob(idx, sizeof(idx), offsets, 3u);
    assert(idx_len == 22u);

    /* Decode header */
    PackIdxHeader h;
    assert(pack_idx_header_decode(idx, idx_len, &h));
    assert(h.version == 1u && h.count == 3u);

    /* Lookup each offset and parse the line at that offset */
    for (uint32_t i = 0u; i < 3u; ++i) {
        uint32_t off;
        assert(pack_idx_offset_at(idx, idx_len, 3u, i, &off));
        const char *line = tsv + off;
        const char *line_end = strchr(line, '\n');
        assert(line_end != NULL);
        Question q;
        assert(pack_parse_tsv_line(line, (size_t)(line_end - line), &q));
        assert(q.id == i);
    }
    return 0;
}
