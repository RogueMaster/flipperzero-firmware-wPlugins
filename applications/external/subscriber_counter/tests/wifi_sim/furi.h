/* Minimal host stand-ins for the furi APIs yt_wifi.c uses. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef struct {
    int unused;
} FuriMutex;
typedef enum {
    FuriMutexTypeNormal
} FuriMutexType;
#define FuriWaitForever 0xFFFFFFFFU
extern uint32_t sim_tick;
static inline uint32_t furi_get_tick(void) {
    return sim_tick;
}
static inline uint32_t furi_kernel_get_tick_frequency(void) {
    return 1000U;
}
static inline FuriMutex* furi_mutex_alloc(FuriMutexType t) {
    (void)t;
    static FuriMutex m;
    return &m;
}
static inline void furi_mutex_free(FuriMutex* m) {
    (void)m;
}
static inline int furi_mutex_acquire(FuriMutex* m, uint32_t t) {
    (void)m;
    (void)t;
    return 0;
}
static inline int furi_mutex_release(FuriMutex* m) {
    (void)m;
    return 0;
}
void* furi_record_open(const char* name);
void furi_record_close(const char* name);
size_t strlcpy(char* dst, const char* src, size_t size);
