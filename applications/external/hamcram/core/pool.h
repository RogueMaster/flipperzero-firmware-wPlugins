/**
 * Question pool reader.
 *
 * A pool is one ".hcp" file on the SD card (see docs/pool-format.md), made
 * from an official NCVEC pool by tools/hampool.py. Only the small lookup
 * tables are kept in RAM; question text is read one record at a time.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define POOL_FORMAT_VERSION 1
#define POOL_MAX_RECORD     1536
#define POOL_MAX_QUESTIONS  1000
#define POOL_ANSWERS        4

/** The question refers to a figure that the app cannot display */
#define POOL_FLAG_FIGURE   0x01
/** The last answer is "All/None of these choices" and must stay last */
#define POOL_FLAG_PIN_LAST 0x02

typedef struct __attribute__((packed)) {
    char magic[4];
    uint8_t version;
    uint8_t element; /**< FCC exam element: 2, 3 or 4 */
    uint8_t num_subelements;
    uint8_t num_groups;
    uint16_t num_questions;
    uint8_t exam_questions; /**< One per group */
    uint8_t pass_correct;
    uint32_t valid_from; /**< YYYYMMDD */
    uint32_t valid_to; /**< YYYYMMDD */
    char name[16];
    char years[12];
    uint16_t max_record;
    uint8_t reserved[14];
} PoolHeader;

typedef struct __attribute__((packed)) {
    char id[3];
    uint8_t first_group;
    uint8_t num_groups;
    uint8_t reserved;
    uint16_t first_question;
    uint16_t num_questions;
    char title[38];
} PoolSubelement;

typedef struct __attribute__((packed)) {
    char id[4];
    uint16_t first_question;
    uint8_t num_questions;
    uint8_t subelement;
} PoolGroup;

typedef struct __attribute__((packed)) {
    uint32_t offset;
    uint16_t code; /**< Stable identity of the question, used by saved progress */
    uint8_t flags;
    uint8_t group;
} PoolIndex;

/** One question. Strings point into the pool's buffer and stay valid until
 * the next pool_read_question() call. */
typedef struct {
    char id[6];
    uint8_t correct;
    uint8_t flags;
    uint8_t group;
    const char* ref;
    const char* question;
    const char* answers[POOL_ANSWERS];
} PoolQuestion;

typedef struct {
    File* file;
    uint32_t file_size;
    PoolHeader header;
    PoolSubelement* subelements;
    PoolGroup* groups;
    PoolIndex* index;
    char* text;
} Pool;

/** Read and validate just the header, e.g. to list the installed pools */
bool pool_read_header(Storage* storage, const char* path, PoolHeader* header);

/** Open a pool. Returns NULL if the file is missing or malformed. */
Pool* pool_open(Storage* storage, const char* path);

void pool_close(Pool* pool);

/** Load question `index` (0-based, pool order) */
bool pool_read_question(Pool* pool, uint16_t index, PoolQuestion* question);
