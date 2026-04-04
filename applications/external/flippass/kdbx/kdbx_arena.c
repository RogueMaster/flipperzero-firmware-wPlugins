#include "kdbx_arena.h"

#include <stdlib.h>

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
};

static size_t kdbx_arena_align_up(size_t value, size_t alignment) {
    const size_t mask = alignment - 1U;
    return (value + mask) & ~mask;
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

    if(arena->committed_bytes != NULL && arena->commit_limit > 0U) {
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

    if(arena->committed_bytes != NULL) {
        *arena->committed_bytes += size;
    }

    return true;
}

static KDBXArenaChunk* kdbx_arena_chunk_alloc(KDBXArena* arena, size_t min_payload_size) {
    furi_assert(arena);

    const size_t payload_size =
        (min_payload_size > arena->chunk_size) ? min_payload_size : arena->chunk_size;
    if(payload_size > (SIZE_MAX - sizeof(KDBXArenaChunk))) {
        kdbx_arena_note_failure(arena, "size_overflow", payload_size);
        return NULL;
    }

    const size_t alloc_size = sizeof(KDBXArenaChunk) + payload_size;
    if(!kdbx_arena_budget_reserve(arena, alloc_size)) {
        return NULL;
    }

    KDBXArenaChunk* chunk = malloc(alloc_size);
    if(chunk == NULL) {
        kdbx_arena_note_failure(arena, "malloc", alloc_size);
        return NULL;
    }

    memset(chunk, 0, alloc_size);
    chunk->size = payload_size;

    if(arena->tail != NULL) {
        arena->tail->next = chunk;
    } else {
        arena->head = chunk;
    }
    arena->tail = chunk;
    arena->total_bytes += alloc_size;
    return chunk;
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

    if(alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    if((alignment & (alignment - 1U)) != 0U) {
        kdbx_arena_note_failure(arena, "invalid_alignment", alignment);
        return NULL;
    }

    KDBXArenaChunk* chunk = arena->tail;
    size_t offset = 0U;

    if(chunk != NULL) {
        offset = kdbx_arena_align_up(chunk->used, alignment);
        if(offset > chunk->size || size > (chunk->size - offset)) {
            chunk = NULL;
        }
    }

    if(chunk == NULL) {
        const size_t required = size + alignment;
        chunk = kdbx_arena_chunk_alloc(arena, required);
        if(chunk == NULL) {
            return NULL;
        }
        offset = kdbx_arena_align_up(chunk->used, alignment);
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

    char* copy = kdbx_arena_alloc_block(arena, len + 1U, sizeof(char));
    if(copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, len);
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
