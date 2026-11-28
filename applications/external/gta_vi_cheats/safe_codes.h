#pragma once
#include <stddef.h>

typedef enum {
    GtaSafeMission,
    GtaSafeOperation,
    GtaSafeDoor,
    GtaSafePuzzle,
} GtaSafeKind;

typedef enum {
    GtaSafeTested, /* Explicitly tested by a guide */
    GtaSafeCorroborated, /* Multiple guides */
    GtaSafeReported, /* Limited evidence */
    GtaSafeVariable, /* May change per save or retry */
    GtaSafeConflicting, /* Sources disagree */
} GtaSafeStatus;

typedef struct {
    const char* name;
    const char* region;
    const char* code; /* Kept as text to preserve zeros and non-digit passwords */
    const char* where;
    const char* notes;
    GtaSafeKind kind;
    GtaSafeStatus status;
} GtaSafeCode;

extern const GtaSafeCode gta_safe_codes[];
extern const size_t gta_safe_code_count;
