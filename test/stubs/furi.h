#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#define UNUSED(x) (void)(x)
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#define FuriWaitForever 0xFFFFFFFFU
typedef enum { FuriStatusOk = 0, FuriStatusError = -1 } FuriStatus;
typedef enum { FuriMutexTypeNormal = 0, FuriMutexTypeRecursive } FuriMutexType;
typedef struct FuriMutex FuriMutex;
typedef struct FuriMessageQueue FuriMessageQueue;
uint32_t furi_get_tick(void);
uint32_t furi_ms_to_ticks(uint32_t ms);
void furi_delay_ms(uint32_t ms);
FuriMutex* furi_mutex_alloc(FuriMutexType type);
void furi_mutex_free(FuriMutex* m);
FuriStatus furi_mutex_acquire(FuriMutex* m, uint32_t timeout);
FuriStatus furi_mutex_release(FuriMutex* m);
FuriMessageQueue* furi_message_queue_alloc(uint32_t capacity, uint32_t size);
void furi_message_queue_free(FuriMessageQueue* q);
FuriStatus furi_message_queue_put(FuriMessageQueue* q, const void* msg, uint32_t timeout);
FuriStatus furi_message_queue_get(FuriMessageQueue* q, void* msg, uint32_t timeout);
void* furi_record_open(const char* name);
void furi_record_close(const char* name);
