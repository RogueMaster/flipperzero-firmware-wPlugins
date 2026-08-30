/* Host stand-in for the Flipper furi API.
 *
 * Covers only what the app's protocol and crypto files touch. Logging is
 * discarded; assertions abort, the way furi_check does on device.
 */
#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COUNT_OF
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* Logging is discarded, but the tag is consumed so app code that declares one
 * still compiles warning-free. */
#define FURI_LOG_T(tag, ...) ((void)(tag))
#define FURI_LOG_D(tag, ...) ((void)(tag))
#define FURI_LOG_I(tag, ...) ((void)(tag))
#define FURI_LOG_W(tag, ...) ((void)(tag))
#define FURI_LOG_E(tag, ...) ((void)(tag))

/* Enough of a string for the file parsing to compare a header with. */
typedef struct FuriString FuriString;

FuriString* furi_string_alloc(void);
void furi_string_free(FuriString* string);
void furi_string_set_str(FuriString* string, const char* str);
const char* furi_string_get_cstr(const FuriString* string);
int furi_string_cmp_str(const FuriString* string, const char* str);
bool furi_string_empty(const FuriString* string);

/* Allocation counting, so a test can see how much churn a long exchange makes.
 * Host only: the app sees plain malloc and free on the device. */
void* seos_test_malloc(size_t size);
void* seos_test_calloc(size_t count, size_t size);
void seos_test_free(void* ptr);
unsigned seos_test_allocation_count(void);
size_t seos_test_allocation_live(void);
void seos_test_allocation_reset(void);

#define malloc(size)        seos_test_malloc(size)
#define calloc(count, size) seos_test_calloc(count, size)
#define free(ptr)           seos_test_free(ptr)

#define furi_assert(expr) assert(expr)
#define furi_check(expr)  assert(expr)
#define furi_crash(msg)   abort()

typedef enum {
    FuriLogLevelDefault = 0,
    FuriLogLevelNone = 1,
    FuriLogLevelError = 2,
    FuriLogLevelWarn = 3,
    FuriLogLevelInfo = 4,
    FuriLogLevelDebug = 5,
    FuriLogLevelTrace = 6,
} FuriLogLevel;

/* Tests run with logging off, so the log helpers take their early exit. */
static inline FuriLogLevel furi_log_get_level(void) {
    return FuriLogLevelNone;
}
