#pragma once

#include "iso14443_4a.h"
#include <toolbox/bit_buffer.h>

typedef struct Iso14443_4aListener Iso14443_4aListener;

typedef enum {
    Iso14443_4aListenerEventTypeHalted = 0,
    Iso14443_4aListenerEventTypeFieldOff = 1,
    Iso14443_4aListenerEventTypeReceivedData = 2,
} Iso14443_4aListenerEventType;

typedef struct {
    BitBuffer *buffer;
} Iso14443_4aListenerEventData;

typedef struct {
    Iso14443_4aListenerEventType type;
    Iso14443_4aListenerEventData *data;
} Iso14443_4aListenerEvent;
