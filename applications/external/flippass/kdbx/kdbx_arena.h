#pragma once

#include "kdbx_includes.h"

typedef struct KDBXArena KDBXArena;

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
