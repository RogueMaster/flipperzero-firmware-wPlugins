#pragma once

#include "iso14443_3a.h"
#include <toolbox/bit_buffer.h>

typedef struct Iso14443_3aListener Iso14443_3aListener;

typedef enum {
    Iso14443_3aListenerEventTypeFieldOff = 0,
    Iso14443_3aListenerEventTypeHalted = 1,
    Iso14443_3aListenerEventTypeReceivedStandardFrame = 2,
    Iso14443_3aListenerEventTypeReceivedData = 3,
} Iso14443_3aListenerEventType;

typedef struct {
    BitBuffer *buffer;
} Iso14443_3aListenerEventData;

typedef struct {
    Iso14443_3aListenerEventType type;
    Iso14443_3aListenerEventData *data;
} Iso14443_3aListenerEvent;
