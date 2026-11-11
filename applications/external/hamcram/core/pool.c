#include "pool.h"

#include <furi.h>
#include <string.h>

/* Extra bytes in the text buffer: one terminator for each of the six strings */
#define POOL_FIELDS 6

typedef struct __attribute__((packed)) {
    char id[6];
    uint8_t correct;
    uint8_t flags;
    uint8_t group;
    uint8_t ref_len;
    uint16_t question_len;
    uint8_t answer_len[POOL_ANSWERS];
} PoolRecord;

_Static_assert(sizeof(PoolHeader) == 64, "pool header layout");
_Static_assert(sizeof(PoolSubelement) == 48, "pool subelement layout");
_Static_assert(sizeof(PoolGroup) == 8, "pool group layout");
_Static_assert(sizeof(PoolIndex) == 8, "pool index layout");
_Static_assert(sizeof(PoolRecord) == 16, "pool record layout");

static bool pool_header_valid(PoolHeader* header) {
    if(memcmp(header->magic, "HCP1", 4) != 0) return false;
    if(header->version != POOL_FORMAT_VERSION) return false;
    if(header->num_subelements == 0 || header->num_groups == 0) return false;
    if(header->num_subelements > header->num_groups) return false;
    if(header->num_questions < header->num_groups) return false;
    if(header->num_questions > POOL_MAX_QUESTIONS) return false;
    if(header->exam_questions != header->num_groups) return false;
    if(header->pass_correct == 0 || header->pass_correct > header->exam_questions) return false;
    if(header->max_record <= sizeof(PoolRecord) || header->max_record > POOL_MAX_RECORD)
        return false;
    header->name[sizeof(header->name) - 1] = '\0';
    header->years[sizeof(header->years) - 1] = '\0';
    return true;
}

static bool pool_read_exact(File* file, void* buffer, size_t size) {
    return storage_file_read(file, buffer, size) == size;
}

bool pool_read_header(Storage* storage, const char* path, PoolHeader* header) {
    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
              pool_read_exact(file, header, sizeof(PoolHeader)) && pool_header_valid(header);
    storage_file_close(file);
    storage_file_free(file);
    return ok;
}

/* Make sure the tables describe one consistent subelement > group > question
 * tree, so the rest of the app can index them without further checks. */
static bool pool_tables_valid(const Pool* pool) {
    const PoolHeader* header = &pool->header;
    uint32_t data_start = sizeof(PoolHeader) + sizeof(PoolSubelement) * header->num_subelements +
                          sizeof(PoolGroup) * header->num_groups +
                          sizeof(PoolIndex) * header->num_questions;
    uint16_t group = 0;
    uint16_t question = 0;

    for(uint8_t s = 0; s < header->num_subelements; s++) {
        PoolSubelement* sub = &pool->subelements[s];
        sub->id[sizeof(sub->id) - 1] = '\0';
        sub->title[sizeof(sub->title) - 1] = '\0';
        if(sub->num_groups == 0 || sub->first_group != group) return false;
        if(group + sub->num_groups > header->num_groups) return false;
        if(sub->first_question != question) return false;

        uint16_t sub_questions = 0;
        for(uint8_t g = 0; g < sub->num_groups; g++, group++) {
            PoolGroup* grp = &pool->groups[group];
            grp->id[sizeof(grp->id) - 1] = '\0';
            if(grp->subelement != s || grp->num_questions == 0) return false;
            if(grp->first_question != question) return false;
            if(question + grp->num_questions > header->num_questions) return false;
            for(uint8_t q = 0; q < grp->num_questions; q++, question++) {
                const PoolIndex* entry = &pool->index[question];
                if(entry->group != group) return false;
                if(entry->offset < data_start) return false;
                if(entry->offset > pool->file_size - sizeof(PoolRecord)) return false;
            }
            sub_questions += grp->num_questions;
        }
        if(sub->num_questions != sub_questions) return false;
    }
    return group == header->num_groups && question == header->num_questions;
}

Pool* pool_open(Storage* storage, const char* path) {
    Pool* pool = malloc(sizeof(Pool));
    memset(pool, 0, sizeof(Pool));
    pool->file = storage_file_alloc(storage);

    bool ok = false;
    do {
        if(!storage_file_open(pool->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        uint64_t size = storage_file_size(pool->file);
        if(size < sizeof(PoolHeader) + sizeof(PoolRecord) || size > UINT32_MAX) break;
        pool->file_size = (uint32_t)size;
        if(!pool_read_exact(pool->file, &pool->header, sizeof(PoolHeader))) break;
        if(!pool_header_valid(&pool->header)) break;

        size_t subs_size = sizeof(PoolSubelement) * pool->header.num_subelements;
        size_t groups_size = sizeof(PoolGroup) * pool->header.num_groups;
        size_t index_size = sizeof(PoolIndex) * pool->header.num_questions;
        if(sizeof(PoolHeader) + subs_size + groups_size + index_size >= pool->file_size) break;

        pool->subelements = malloc(subs_size);
        pool->groups = malloc(groups_size);
        pool->index = malloc(index_size);
        pool->text = malloc(pool->header.max_record + POOL_FIELDS);
        if(!pool_read_exact(pool->file, pool->subelements, subs_size)) break;
        if(!pool_read_exact(pool->file, pool->groups, groups_size)) break;
        if(!pool_read_exact(pool->file, pool->index, index_size)) break;
        if(!pool_tables_valid(pool)) break;
        ok = true;
    } while(false);

    if(!ok) {
        pool_close(pool);
        pool = NULL;
    }
    return pool;
}

void pool_close(Pool* pool) {
    if(!pool) return;
    storage_file_close(pool->file);
    storage_file_free(pool->file);
    free(pool->subelements);
    free(pool->groups);
    free(pool->index);
    free(pool->text);
    free(pool);
}

bool pool_read_question(Pool* pool, uint16_t index, PoolQuestion* question) {
    furi_check(pool);
    furi_check(question);
    if(index >= pool->header.num_questions) return false;

    const PoolIndex* entry = &pool->index[index];
    PoolRecord record;
    if(!storage_file_seek(pool->file, entry->offset, true)) return false;
    if(!pool_read_exact(pool->file, &record, sizeof(record))) return false;

    size_t lengths[POOL_FIELDS] = {record.ref_len, record.question_len};
    size_t total = record.ref_len + record.question_len;
    for(size_t i = 0; i < POOL_ANSWERS; i++) {
        if(record.answer_len[i] == 0) return false;
        lengths[2 + i] = record.answer_len[i];
        total += record.answer_len[i];
    }
    if(record.correct >= POOL_ANSWERS || record.question_len == 0) return false;
    if(sizeof(record) + total > pool->header.max_record) return false;

    /* The strings are stored back to back. Read them into the tail of the
     * buffer, then slide each one forward to make room for its terminator. */
    char* source = pool->text + POOL_FIELDS;
    char* dest = pool->text;
    if(!pool_read_exact(pool->file, source, total)) return false;

    const char* fields[POOL_FIELDS];
    for(size_t i = 0; i < POOL_FIELDS; i++) {
        memmove(dest, source, lengths[i]);
        dest[lengths[i]] = '\0';
        fields[i] = dest;
        dest += lengths[i] + 1;
        source += lengths[i];
    }

    memcpy(question->id, record.id, sizeof(question->id));
    question->id[sizeof(question->id) - 1] = '\0';
    question->correct = record.correct;
    question->flags = entry->flags;
    question->group = entry->group;
    question->ref = fields[0];
    question->question = fields[1];
    for(size_t i = 0; i < POOL_ANSWERS; i++) {
        question->answers[i] = fields[2 + i];
    }
    return true;
}
