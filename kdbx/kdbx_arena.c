#include "kdbx_arena.h"
#include "memzero.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

/* When the default chunk allocation would push past commit_limit near the
 * end of model construction, retry with progressively smaller chunks down to
 * KDBX_ARENA_MIN_FALLBACK_PAYLOAD bytes. This buys back the heap that the
 * safety reserve is otherwise withholding without lowering the reserve
 * itself. The trade-off is more KDBXArenaChunk header overhead per kilobyte
 * of arena tail, which is acceptable in the failure window we are trying to
 * survive. Set to 0 to disable. */
#ifndef KDBX_ARENA_NEAR_LIMIT_FALLBACK
#define KDBX_ARENA_NEAR_LIMIT_FALLBACK 1
#endif

#define KDBX_ARENA_MIN_FALLBACK_PAYLOAD 256U

typedef struct KDBXArenaChunk {
    struct KDBXArenaChunk* next;
    size_t size;
    size_t used;
    uint8_t data[];
} KDBXArenaChunk;

struct KDBXArena {
    KDBXArenaChunk* head;
    KDBXArenaChunk* tail;
    size_t chunk_size;
    size_t total_bytes;
    size_t* committed_bytes;
    size_t commit_limit;
    bool budget_failed;
    const char* failure_reason;
    size_t last_failed_size;
    size_t last_failed_committed;
    size_t last_failed_max_free_block;
    size_t fallback_chunk_count;
    size_t fallback_smallest_payload;
};

static size_t kdbx_arena_aligned_offset(const KDBXArenaChunk* chunk, size_t alignment) {
    const size_t mask = alignment - 1U;
    /* The flexible array is not necessarily aligned like malloc's base. */
    const uintptr_t address = (uintptr_t)chunk->data + chunk->used;
    const size_t padding = (alignment - (address & mask)) & mask;
    return chunk->used + padding;
}

static void kdbx_arena_note_failure(KDBXArena* arena, const char* reason, size_t size) {
    furi_assert(arena);

    arena->budget_failed = true;
    arena->failure_reason = reason;
    arena->last_failed_size = size;
    arena->last_failed_committed = arena->committed_bytes != NULL ? *arena->committed_bytes : 0U;
    arena->last_failed_max_free_block = memmgr_heap_get_max_free_block();
}

static bool kdbx_arena_budget_reserve(KDBXArena* arena, size_t size) {
    furi_assert(arena);

    if(arena->committed_bytes != NULL) {
        if(*arena->committed_bytes > arena->commit_limit ||
           size > (arena->commit_limit - *arena->committed_bytes)) {
            kdbx_arena_note_failure(arena, "commit_limit", size);
            return false;
        }
    }

    if(memmgr_heap_get_max_free_block() < size) {
        kdbx_arena_note_failure(arena, "max_free_block", size);
        return false;
    }

    if(arena->committed_bytes != NULL && size > SIZE_MAX - *arena->committed_bytes) {
        kdbx_arena_note_failure(arena, "size_overflow", size);
        return false;
    }

    if(arena->committed_bytes != NULL) {
        *arena->committed_bytes += size;
    }

    return true;
}

static KDBXArenaChunk* kdbx_arena_chunk_alloc(KDBXArena* arena, size_t min_payload_size) {
    furi_assert(arena);

    /* Start at the larger of the configured chunk size and the caller's
     * minimum. If the resulting allocation would not fit the current
     * commit_limit, halve the payload (down to KDBX_ARENA_MIN_FALLBACK_PAYLOAD
     * but never below min_payload_size) and try again. This is the
     * near-limit fallback ladder; without it, a 1024-byte chunk request
     * fails hard even when the heap holds enough for a 256- or 512-byte
     * chunk. */
    const size_t requested_size = (min_payload_size > arena->chunk_size) ? min_payload_size :
                                                                           arena->chunk_size;
    if(requested_size > (SIZE_MAX - sizeof(KDBXArenaChunk))) {
        kdbx_arena_note_failure(arena, "size_overflow", requested_size);
        return NULL;
    }

    size_t payload_size = requested_size;
    const size_t floor = (min_payload_size > KDBX_ARENA_MIN_FALLBACK_PAYLOAD) ?
                             min_payload_size :
                             KDBX_ARENA_MIN_FALLBACK_PAYLOAD;

    while(true) {
        const size_t alloc_size = sizeof(KDBXArenaChunk) + payload_size;

        /* Clear any stale failure note left by a previous fallback step so
         * the caller only observes the final outcome. */
        arena->budget_failed = false;
        arena->failure_reason = NULL;
        arena->last_failed_size = 0U;
        arena->last_failed_committed = 0U;
        arena->last_failed_max_free_block = 0U;

        if(kdbx_arena_budget_reserve(arena, alloc_size)) {
            KDBXArenaChunk* chunk = malloc(alloc_size);
            if(chunk != NULL) {
                memset(chunk, 0, alloc_size);
                chunk->size = payload_size;

                if(arena->tail != NULL) {
                    arena->tail->next = chunk;
                } else {
                    arena->head = chunk;
                }
                arena->tail = chunk;
                arena->total_bytes += alloc_size;
                if(payload_size < requested_size) {
                    arena->fallback_chunk_count++;
                    if(arena->fallback_smallest_payload == 0U ||
                       payload_size < arena->fallback_smallest_payload) {
                        arena->fallback_smallest_payload = payload_size;
                    }
                }
                return chunk;
            }
            /* malloc failed even though budget said OK. Refund the reservation
             * and either fall back or fail terminally. */
            if(arena->committed_bytes != NULL && *arena->committed_bytes >= alloc_size) {
                *arena->committed_bytes -= alloc_size;
            }
            kdbx_arena_note_failure(arena, "malloc", alloc_size);
        }

#if KDBX_ARENA_NEAR_LIMIT_FALLBACK
        if(payload_size > floor) {
            const size_t halved = payload_size / 2U;
            payload_size = (halved >= floor) ? halved : floor;
            continue;
        }
#endif
        /* Already at the floor or fallback disabled; the failure note from
         * kdbx_arena_budget_reserve / malloc is the final outcome. */
        if(arena->failure_reason == NULL) {
            kdbx_arena_note_failure(arena, "commit_limit", min_payload_size);
        }
        return NULL;
    }
}

KDBXArena* kdbx_arena_alloc(size_t chunk_size, size_t* committed_bytes, size_t commit_limit) {
    if(chunk_size < 256U) {
        chunk_size = 256U;
    }

    KDBXArena* arena = malloc(sizeof(KDBXArena));
    if(arena == NULL) {
        return NULL;
    }

    memset(arena, 0, sizeof(*arena));
    arena->chunk_size = chunk_size;
    arena->committed_bytes = committed_bytes;
    arena->commit_limit = commit_limit;
    return arena;
}

void kdbx_arena_set_budget(KDBXArena* arena, size_t* committed_bytes, size_t commit_limit) {
    if(arena == NULL) {
        return;
    }

    arena->committed_bytes = committed_bytes;
    arena->commit_limit = commit_limit;
}

void kdbx_arena_free(KDBXArena* arena) {
    if(arena == NULL) {
        return;
    }

    KDBXArenaChunk* chunk = arena->head;
    while(chunk != NULL) {
        KDBXArenaChunk* next = chunk->next;
        memzero(chunk, sizeof(KDBXArenaChunk) + chunk->size);
        free(chunk);
        chunk = next;
    }

    memzero(arena, sizeof(*arena));
    free(arena);
}

void* kdbx_arena_alloc_block(KDBXArena* arena, size_t size, size_t alignment) {
    furi_assert(arena);

    if(size == 0U) {
        return NULL;
    }

    if(alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
        kdbx_arena_note_failure(arena, "invalid_alignment", alignment);
        return NULL;
    }

    if(size > SIZE_MAX - (alignment - 1U)) {
        kdbx_arena_note_failure(arena, "size_overflow", size);
        return NULL;
    }

    KDBXArenaChunk* chunk = arena->tail;
    size_t offset = 0U;

    if(chunk != NULL) {
        offset = kdbx_arena_aligned_offset(chunk, alignment);
        if(offset > chunk->size || size > (chunk->size - offset)) {
            chunk = NULL;
        }
    }

    if(chunk == NULL) {
        const size_t required = size + alignment - 1U;
        chunk = kdbx_arena_chunk_alloc(arena, required);
        if(chunk == NULL) {
            return NULL;
        }
        offset = kdbx_arena_aligned_offset(chunk, alignment);
    }

    if(offset > chunk->size || size > (chunk->size - offset)) {
        kdbx_arena_note_failure(arena, "chunk_bounds", size);
        return NULL;
    }

    void* result = &chunk->data[offset];
    memset(result, 0, size);
    chunk->used = offset + size;
    return result;
}

char* kdbx_arena_strdup_range(KDBXArena* arena, const char* value, size_t len) {
    furi_assert(arena);

    if(len == SIZE_MAX || (value == NULL && len != 0U)) {
        kdbx_arena_note_failure(arena, "invalid_string_size", len);
        return NULL;
    }

    char* copy = kdbx_arena_alloc_block(arena, len + 1U, sizeof(char));
    if(copy == NULL) {
        return NULL;
    }

    if(len != 0U) {
        memcpy(copy, value, len);
    }
    copy[len] = '\0';
    return copy;
}

char* kdbx_arena_strdup(KDBXArena* arena, const char* value) {
    if(value == NULL) {
        return NULL;
    }

    return kdbx_arena_strdup_range(arena, value, strlen(value));
}

size_t kdbx_arena_chunk_overhead_bytes(void) {
    return sizeof(KDBXArenaChunk);
}

size_t kdbx_arena_bytes(const KDBXArena* arena) {
    return arena != NULL ? arena->total_bytes : 0U;
}

bool kdbx_arena_budget_failed(const KDBXArena* arena) {
    return arena != NULL && arena->budget_failed;
}

const char* kdbx_arena_failure_reason(const KDBXArena* arena) {
    return (arena != NULL && arena->failure_reason != NULL) ? arena->failure_reason : "none";
}

size_t kdbx_arena_last_failed_size(const KDBXArena* arena) {
    return arena != NULL ? arena->last_failed_size : 0U;
}

size_t kdbx_arena_last_failed_committed(const KDBXArena* arena) {
    return arena != NULL ? arena->last_failed_committed : 0U;
}

size_t kdbx_arena_last_failed_max_free_block(const KDBXArena* arena) {
    return arena != NULL ? arena->last_failed_max_free_block : 0U;
}

size_t kdbx_arena_fallback_chunk_count(const KDBXArena* arena) {
    return arena != NULL ? arena->fallback_chunk_count : 0U;
}

size_t kdbx_arena_fallback_smallest_payload(const KDBXArena* arena) {
    return arena != NULL ? arena->fallback_smallest_payload : 0U;
}
