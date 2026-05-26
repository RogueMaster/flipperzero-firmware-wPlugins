#include "include/infrastructure/pack_reader.h"
#include "include/data/embedded_pack.h"
#include <stdlib.h>
#include <string.h>

#define IDX_HEADER_SIZE 10u /* 4 magic + 2 version + 4 count */

static uint16_t le_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool pack_parse_tsv_line(const char *line, size_t len, Question *out) {
    if (!line || !out || len == 0u) {
        return false;
    }
    while (len > 0u && (line[len - 1u] == '\n' || line[len - 1u] == '\r')) {
        len--;
    }
    if (len == 0u) {
        return false;
    }

    size_t tabs[3];
    size_t found = 0u;
    for (size_t i = 0u; i < len && found < 3u; ++i) {
        if (line[i] == '\t') {
            tabs[found++] = i;
        }
    }
    if (found != 3u) {
        return false;
    }

    char idbuf[16];
    if (tabs[0] >= sizeof(idbuf))
        return false;
    memcpy(idbuf, line, tabs[0]);
    idbuf[tabs[0]] = '\0';
    char *end = NULL;
    const unsigned long id = strtoul(idbuf, &end, 10);
    if (end == idbuf || *end != '\0')
        return false;

    const size_t cat_start = tabs[0] + 1u;
    const size_t cat_len = tabs[1] - cat_start;
    char catbuf[8];
    if (cat_len >= sizeof(catbuf))
        return false;
    memcpy(catbuf, line + cat_start, cat_len);
    catbuf[cat_len] = '\0';
    const unsigned long cat = strtoul(catbuf, &end, 10);
    if (end == catbuf || *end != '\0' || cat < 1u || cat > 7u)
        return false;

    const size_t q_start = tabs[1] + 1u;
    const size_t q_len = tabs[2] - q_start;
    if (q_len >= QUESTION_MAX)
        return false;

    const size_t a_start = tabs[2] + 1u;
    const size_t a_len = len - a_start;
    if (a_len >= ANSWER_MAX)
        return false;

    out->id = (uint32_t)id;
    out->category_id = (uint8_t)cat;
    memcpy(out->question, line + q_start, q_len);
    out->question[q_len] = '\0';
    memcpy(out->answer, line + a_start, a_len);
    out->answer[a_len] = '\0';
    return true;
}

bool pack_idx_header_decode(const uint8_t *blob, size_t len, PackIdxHeader *out) {
    if (!blob || !out || len < IDX_HEADER_SIZE)
        return false;
    if (blob[0] != 'T' || blob[1] != 'R' || blob[2] != 'V' || blob[3] != 'I') {
        return false;
    }
    out->version = le_u16(blob + 4);
    out->count = le_u32(blob + 6);
    return true;
}

bool pack_idx_offset_at(const uint8_t *blob, size_t len, uint32_t count, uint32_t i,
                        uint32_t *offset_out) {
    if (!blob || !offset_out || i >= count)
        return false;
    const size_t need = (size_t)IDX_HEADER_SIZE + (size_t)count * 4u;
    if (len < need)
        return false;
    *offset_out = le_u32(blob + IDX_HEADER_SIZE + (size_t)i * 4u);
    return true;
}

/* ---- Embedded-pack runtime ---- */

static const uint8_t *s_idx = NULL;
static size_t s_idx_len = 0u;
static const char *s_tsv = NULL;
static size_t s_tsv_len = 0u;
static uint32_t s_count = 0u;

bool pack_open(Lang lang) {
    pack_close();
    if (lang == LangEs) {
        s_idx = trivia_es_idx;
        s_idx_len = trivia_es_idx_len;
        s_tsv = trivia_es_tsv;
        s_tsv_len = trivia_es_tsv_len;
    } else {
        s_idx = trivia_en_idx;
        s_idx_len = trivia_en_idx_len;
        s_tsv = trivia_en_tsv;
        s_tsv_len = trivia_en_tsv_len;
    }
    PackIdxHeader h;
    if (!pack_idx_header_decode(s_idx, s_idx_len, &h) || h.version != 1u) {
        pack_close();
        return false;
    }
    s_count = h.count;
    return true;
}

void pack_close(void) {
    s_idx = NULL;
    s_idx_len = 0u;
    s_tsv = NULL;
    s_tsv_len = 0u;
    s_count = 0u;
}

uint32_t pack_count(void) {
    return s_count;
}

bool pack_get_by_id(uint32_t id, Question *out) {
    if (!out || !s_idx || !s_tsv || id >= s_count)
        return false;

    uint32_t offset;
    if (!pack_idx_offset_at(s_idx, s_idx_len, s_count, id, &offset))
        return false;
    if ((size_t)offset >= s_tsv_len)
        return false;

    const char *line_start = s_tsv + offset;
    const size_t remaining = s_tsv_len - (size_t)offset;
    const char *line_end = memchr(line_start, '\n', remaining);
    const size_t line_len = line_end ? (size_t)(line_end - line_start) : remaining;

    return pack_parse_tsv_line(line_start, line_len, out);
}
