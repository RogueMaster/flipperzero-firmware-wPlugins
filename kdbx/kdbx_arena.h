#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct KDBXArena KDBXArena;

/* A non-NULL counter enables the budget, including an exhausted zero limit.
 * Pass NULL for an unbudgeted arena, or SIZE_MAX for counted unrestricted growth. */
KDBXArena* kdbx_arena_alloc(size_t chunk_size, size_t* committed_bytes, size_t commit_limit);
void kdbx_arena_free(KDBXArena* arena);
void kdbx_arena_set_budget(KDBXArena* arena, size_t* committed_bytes, size_t commit_limit);
void* kdbx_arena_alloc_block(KDBXArena* arena, size_t size, size_t alignment);
char* kdbx_arena_strdup(KDBXArena* arena, const char* value);
char* kdbx_arena_strdup_range(KDBXArena* arena, const char* value, size_t len);
size_t kdbx_arena_chunk_overhead_bytes(void);
size_t kdbx_arena_bytes(const KDBXArena* arena);
bool kdbx_arena_budget_failed(const KDBXArena* arena);
const char* kdbx_arena_failure_reason(const KDBXArena* arena);
size_t kdbx_arena_last_failed_size(const KDBXArena* arena);
size_t kdbx_arena_last_failed_committed(const KDBXArena* arena);
size_t kdbx_arena_last_failed_max_free_block(const KDBXArena* arena);

/* Diagnostics for the near-limit chunk fallback. fallback_chunk_count is the
 * number of chunks that were allocated at less than the configured chunk_size
 * because the default size would have exceeded commit_limit. smallest_payload
 * is the smallest payload size the fallback ladder accepted, or 0 if no
 * fallback ever fired. Useful in probe builds to confirm A2 is actually
 * helping and to bound the trade-off in chunk header overhead. */
size_t kdbx_arena_fallback_chunk_count(const KDBXArena* arena);
size_t kdbx_arena_fallback_smallest_payload(const KDBXArena* arena);
