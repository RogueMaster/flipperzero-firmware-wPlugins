#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UNUSED(x) ((void)(x))

typedef struct FuriSemaphore FuriSemaphore;
typedef struct FuriTimer FuriTimer;
typedef struct FuriThread FuriThread;
typedef struct FuriMutex FuriMutex;
typedef void *FuriThreadId;
typedef int32_t (*FuriThreadCallback)(void *context);

typedef enum {
    FuriThreadPriorityIdle = 0,
    FuriThreadPriorityInit = 4,
    FuriThreadPriorityLowest = 14,
    FuriThreadPriorityLow = 15,
    FuriThreadPriorityNormal = 16,
    FuriThreadPriorityHigh = 17,
    FuriThreadPriorityHighest = 18,
} FuriThreadPriority;

typedef enum {
    FuriTimerTypeOnce = 0,
    FuriTimerTypePeriodic = 1,
} FuriTimerType;

typedef void (*FuriTimerCallback)(void *context);

FuriThreadId furi_thread_get_current_id(void);
uint32_t furi_thread_get_stack_space(FuriThreadId thread_id);
FuriThread *furi_thread_alloc_ex(const char *name, size_t stack_size, FuriThreadCallback callback,
                                 void *context);
void furi_thread_set_appid(FuriThread *thread, const char *appid);
void furi_thread_set_priority(FuriThread *thread, FuriThreadPriority priority);
void furi_thread_start(FuriThread *thread);
void furi_thread_join(FuriThread *thread);
void furi_thread_free(FuriThread *thread);
void *furi_record_open(const char *name);
void furi_record_close(const char *name);
FuriTimer *furi_timer_alloc(FuriTimerCallback callback, FuriTimerType type, void *context);
void furi_timer_free(FuriTimer *timer);
void furi_timer_start(FuriTimer *timer, uint32_t timeout);
void furi_timer_stop(FuriTimer *timer);
void furi_delay_ms(uint32_t milliseconds);
