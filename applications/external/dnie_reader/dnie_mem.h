#pragma once

// Reservas de memoria que SI pueden fallar.
//
// En el Flipper, malloc/calloc/realloc no devuelven NULL: si no hay memoria, el sistema se
// cuelga con "out of memory". Estas versiones miran antes el heap y devuelven NULL, asi las
// comprobaciones if(!p) del codigo funcionan de verdad.
//
// DNIE_ON_FLIPPER lo define application.fam; en las pruebas de PC es un malloc normal.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef DNIE_ON_FLIPPER
#include <furi.h>

#define DNIE_MEM_BLOCK_MARGIN 256 // cabecera del bloque y algo de holgura
#define DNIE_MEM_HEAP_RESERVE 2048 // lo que dejamos siempre libre para el sistema

static inline bool dnie_mem_ok(size_t bytes) {
    return memmgr_heap_get_max_free_block() >= bytes + DNIE_MEM_BLOCK_MARGIN &&
           memmgr_get_free_heap() >= bytes + DNIE_MEM_HEAP_RESERVE;
}
#else
static inline bool dnie_mem_ok(size_t bytes) {
    (void)bytes;
    return true;
}
#endif

static inline void* dnie_malloc(size_t bytes) {
    return dnie_mem_ok(bytes) ? malloc(bytes) : NULL;
}

static inline void* dnie_calloc(size_t count, size_t size) {
    if(size && count > SIZE_MAX / size) return NULL;
    return dnie_mem_ok(count * size) ? calloc(count, size) : NULL;
}

// Para encoger un buffer: en el Flipper realloc reserva un bloque nuevo y copia.
// Si devuelve NULL, el bloque original sigue siendo valido.
static inline void* dnie_realloc(void* ptr, size_t bytes) {
    return dnie_mem_ok(bytes) ? realloc(ptr, bytes) : NULL;
}
