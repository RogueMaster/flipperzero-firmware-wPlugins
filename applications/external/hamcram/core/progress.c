#include "progress.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#define PROGRESS_PATH_MAX   96
#define PROGRESS_STAT_VALID (PROGRESS_WEIGHT_MASK | PROGRESS_LAST_CORRECT | PROGRESS_SEEN)

typedef struct __attribute__((packed)) {
    char magic[4];
    uint8_t element;
    uint8_t reserved;
    uint16_t count;
    uint32_t valid_from;
    uint16_t exams_taken;
    uint16_t exams_passed;
    uint8_t best_score;
    uint8_t last_score;
    uint8_t padding[2];
} ProgressFileHeader;

typedef struct __attribute__((packed)) {
    uint16_t code;
    uint8_t stat;
} ProgressFileEntry;

_Static_assert(sizeof(ProgressFileHeader) == 20, "progress header layout");
_Static_assert(sizeof(ProgressFileEntry) == 3, "progress entry layout");

Progress* progress_alloc(uint16_t count) {
    Progress* progress = malloc(sizeof(Progress));
    memset(progress, 0, sizeof(Progress));
    progress->count = count;
    progress->stats = malloc(count ? count : 1);
    memset(progress->stats, 0, count);
    return progress;
}

void progress_free(Progress* progress) {
    if(!progress) return;
    free(progress->stats);
    free(progress);
}

void progress_reset(Progress* progress) {
    memset(progress->stats, 0, progress->count);
    progress->exams_taken = 0;
    progress->exams_passed = 0;
    progress->best_score = 0;
    progress->last_score = 0;
    progress->dirty = true;
}

static bool
    progress_load_file(Progress* progress, File* file, const char* path, const Pool* pool) {
    ProgressFileHeader header;
    ProgressFileEntry* entries = NULL;
    bool ok = false;

    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        if(storage_file_read(file, &header, sizeof(header)) != sizeof(header)) break;
        if(memcmp(header.magic, "HCS1", 4) != 0) break;
        /* A save belongs to one edition of one pool. When a new edition
         * replaces it the question numbers are reused, so start over. */
        if(header.element != pool->header.element) break;
        if(header.valid_from != pool->header.valid_from) break;
        if(header.count > POOL_MAX_QUESTIONS) break;

        size_t size = sizeof(ProgressFileEntry) * header.count;
        entries = malloc(size ? size : 1);
        if(storage_file_read(file, entries, size) != size) break;

        /* Saved entries are normally in pool order, so try the next question
         * first and only search when the pool has changed. */
        uint16_t next = 0;
        for(uint16_t i = 0; i < header.count; i++) {
            uint16_t found = progress->count;
            if(next < progress->count && pool->index[next].code == entries[i].code) {
                found = next;
            } else {
                for(uint16_t q = 0; q < progress->count; q++) {
                    if(pool->index[q].code == entries[i].code) {
                        found = q;
                        break;
                    }
                }
            }
            if(found < progress->count) {
                progress->stats[found] = entries[i].stat & PROGRESS_STAT_VALID;
                next = found + 1;
            }
        }
        progress->exams_taken = header.exams_taken;
        progress->exams_passed = header.exams_passed;
        progress->best_score = header.best_score;
        progress->last_score = header.last_score;
        ok = true;
    } while(false);

    free(entries);
    storage_file_close(file);
    return ok;
}

bool progress_load(Progress* progress, Storage* storage, const char* path, const Pool* pool) {
    furi_check(progress->count == pool->header.num_questions);
    memset(progress->stats, 0, progress->count);
    progress->exams_taken = 0;
    progress->exams_passed = 0;
    progress->best_score = 0;
    progress->last_score = 0;
    progress->dirty = false;

    char temp_path[PROGRESS_PATH_MAX];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

    File* file = storage_file_alloc(storage);
    bool ok = progress_load_file(progress, file, path, pool);
    if(!ok) {
        /* A save interrupted between "remove old" and "rename new" */
        memset(progress->stats, 0, progress->count);
        ok = progress_load_file(progress, file, temp_path, pool);
        if(!ok) memset(progress->stats, 0, progress->count);
    }
    storage_file_free(file);
    return ok;
}

bool progress_save(Progress* progress, Storage* storage, const char* path, const Pool* pool) {
    furi_check(progress->count == pool->header.num_questions);

    size_t size = sizeof(ProgressFileHeader) + sizeof(ProgressFileEntry) * progress->count;
    uint8_t* buffer = malloc(size);
    ProgressFileHeader header = {
        .magic = {'H', 'C', 'S', '1'},
        .element = pool->header.element,
        .count = progress->count,
        .valid_from = pool->header.valid_from,
        .exams_taken = progress->exams_taken,
        .exams_passed = progress->exams_passed,
        .best_score = progress->best_score,
        .last_score = progress->last_score,
    };
    memcpy(buffer, &header, sizeof(header));
    ProgressFileEntry* entries = (ProgressFileEntry*)(buffer + sizeof(header));
    for(uint16_t q = 0; q < progress->count; q++) {
        entries[q].code = pool->index[q].code;
        entries[q].stat = progress->stats[q];
    }

    /* Write a new file first so a power cut cannot destroy the old save */
    char temp_path[PROGRESS_PATH_MAX];
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

    File* file = storage_file_alloc(storage);
    bool ok = storage_file_open(file, temp_path, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
              storage_file_write(file, buffer, size) == size;
    ok = storage_file_close(file) && ok;
    storage_file_free(file);
    free(buffer);

    if(ok) {
        storage_common_remove(storage, path);
        ok = storage_common_rename(storage, temp_path, path) == FSE_OK;
    }
    if(ok) progress->dirty = false;
    return ok;
}

void progress_record(Progress* progress, uint16_t question, bool correct) {
    if(question >= progress->count) return;
    uint8_t weight = progress->stats[question] & PROGRESS_WEIGHT_MASK;
    uint8_t stat = PROGRESS_SEEN;
    if(correct) {
        if(weight > 0) weight--;
        stat |= PROGRESS_LAST_CORRECT;
    } else {
        weight += PROGRESS_WEIGHT_MISS;
        if(weight > PROGRESS_WEIGHT_MAX) weight = PROGRESS_WEIGHT_MAX;
    }
    progress->stats[question] = stat | weight;
    progress->dirty = true;
}

void progress_record_exam(Progress* progress, uint8_t score, bool passed) {
    if(progress->exams_taken < UINT16_MAX) progress->exams_taken++;
    if(passed && progress->exams_passed < UINT16_MAX) progress->exams_passed++;
    if(score > progress->best_score) progress->best_score = score;
    progress->last_score = score;
    progress->dirty = true;
}

void progress_summarize(
    const Progress* progress,
    const Pool* pool,
    uint16_t first,
    uint16_t count,
    bool skip_figures,
    ProgressSummary* summary) {
    memset(summary, 0, sizeof(ProgressSummary));
    for(uint32_t q = first; q < (uint32_t)first + count && q < progress->count; q++) {
        if(skip_figures && (pool->index[q].flags & POOL_FLAG_FIGURE)) continue;
        uint8_t stat = progress->stats[q];
        summary->total++;
        if(stat & PROGRESS_SEEN) summary->seen++;
        if(stat & PROGRESS_LAST_CORRECT) summary->mastered++;
        if(stat & PROGRESS_WEIGHT_MASK) summary->review++;
    }
}
