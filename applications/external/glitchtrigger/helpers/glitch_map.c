#include "glitch_map.h"

#include <furi.h>
#include <storage/storage.h>
#include <string.h>
#include <stdio.h>

#define GLITCH_DIR      "/ext/apps_data/glitch_trigger"
#define GLITCH_MAP_PATH GLITCH_DIR "/faultmap.csv"

static inline uint32_t map_index(const GlitchFaultMap* m, uint16_t col, uint16_t row) {
    return (uint32_t)row * m->cols + col;
}

void glitch_map_reset(
    GlitchFaultMap* m,
    uint16_t cols,
    uint16_t rows,
    bool is_2d,
    uint32_t w_from,
    uint32_t w_to,
    uint32_t d_from,
    uint32_t d_to) {
    furi_assert(m);
    if(cols < 1) cols = 1;
    if(cols > GLITCH_MAP_W) cols = GLITCH_MAP_W;
    if(rows < 1) rows = 1;
    if(rows > GLITCH_MAP_H) rows = GLITCH_MAP_H;
    memset(m->bits, 0, sizeof(m->bits));
    m->cols = cols;
    m->rows = rows;
    m->is_2d = is_2d;
    m->w_from = w_from;
    m->w_to = w_to;
    m->d_from = d_from;
    m->d_to = d_to;
    m->hits = 0;
}

void glitch_map_set(GlitchFaultMap* m, uint16_t col, uint16_t row) {
    furi_assert(m);
    if(m->cols == 0 || col >= m->cols || row >= m->rows) return;
    uint32_t idx = map_index(m, col, row);
    uint8_t mask = (uint8_t)(1u << (idx & 7));
    uint8_t* byte = &m->bits[idx >> 3];
    if(!(*byte & mask)) {
        *byte |= mask;
        m->hits++;
    }
}

bool glitch_map_get(const GlitchFaultMap* m, uint16_t col, uint16_t row) {
    if(m->cols == 0 || col >= m->cols || row >= m->rows) return false;
    uint32_t idx = map_index(m, col, row);
    return (m->bits[idx >> 3] >> (idx & 7)) & 1u;
}

uint32_t glitch_map_col_width(const GlitchFaultMap* m, uint16_t col) {
    if(m->cols <= 1) return m->w_from;
    return m->w_from + (uint32_t)(((uint64_t)(m->w_to - m->w_from) * col) / (m->cols - 1));
}

uint32_t glitch_map_row_delay(const GlitchFaultMap* m, uint16_t row) {
    if(m->rows <= 1) return m->d_from;
    return m->d_from + (uint32_t)(((uint64_t)(m->d_to - m->d_from) * row) / (m->rows - 1));
}

bool glitch_map_export(const GlitchFaultMap* m) {
    furi_assert(m);
    if(m->cols == 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, GLITCH_DIR);

    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, GLITCH_MAP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char hdr[128];
        int n = snprintf(
            hdr,
            sizeof(hdr),
            "# fault map %ux%u  width_ns %lu..%lu  delay_us %lu..%lu  hits %lu\n",
            m->cols,
            m->rows,
            (unsigned long)m->w_from,
            (unsigned long)m->w_to,
            (unsigned long)m->d_from,
            (unsigned long)m->d_to,
            (unsigned long)m->hits);
        if(n > 0) storage_file_write(file, hdr, (size_t)n);

        /* one line per delay row, cells comma-separated (1 = hit) */
        char line[GLITCH_MAP_W * 2 + 2];
        for(uint16_t row = 0; row < m->rows; row++) {
            size_t pos = 0;
            for(uint16_t col = 0; col < m->cols && pos + 2 < sizeof(line); col++) {
                line[pos++] = glitch_map_get(m, col, row) ? '1' : '0';
                line[pos++] = (col + 1 < m->cols) ? ',' : '\n';
            }
            storage_file_write(file, line, pos);
        }
        ok = true;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
