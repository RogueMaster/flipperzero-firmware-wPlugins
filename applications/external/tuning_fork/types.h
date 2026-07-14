#include <stdint.h>
#include "constants.h"

typedef struct {
    char label[MAX_LABEL_LENGTH];
    float frequency;
} NOTE;

typedef struct {
    char label[MAX_LABEL_LENGTH];
    NOTE notes[MAX_NOTES_PER_TUNING];
    uint8_t notes_count;
} TUNING;

typedef struct {
    char label[MAX_LABEL_LENGTH];
    TUNING* tunings;
    uint8_t tunings_count;
} VARIATION;

typedef struct {
    char label[MAX_LABEL_LENGTH];
    VARIATION* variations;
    uint8_t variations_count;
} INSTRUMENT;
