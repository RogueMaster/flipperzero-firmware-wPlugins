#pragma once

#include "include/domain/category.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define QUESTION_MAX 320u
#define ANSWER_MAX 96u

typedef struct {
    uint32_t id;
    uint8_t category_id;
    char question[QUESTION_MAX];
    char answer[ANSWER_MAX];
} Question;

typedef struct {
    uint16_t version;
    uint32_t count;
} PackIdxHeader;

/* ---- Pure parsing ---- */

bool pack_parse_tsv_line(const char *line, size_t len, Question *out);

bool pack_idx_header_decode(const uint8_t *blob, size_t len, PackIdxHeader *out);

bool pack_idx_offset_at(const uint8_t *blob, size_t len, uint32_t count, uint32_t i,
                        uint32_t *offset_out);

/* ---- Furi I/O (hardware-only) ---- */

bool pack_open(Lang lang);
void pack_close(void);
uint32_t pack_count(void);
bool pack_get_by_id(uint32_t id, Question *out);
