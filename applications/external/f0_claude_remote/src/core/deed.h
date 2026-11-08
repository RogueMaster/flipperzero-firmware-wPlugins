#pragma once

#include "action.h"

typedef enum {
    DeedNone,
    DeedDictation,
    DeedClear,
    DeedMAX,
} Deed;

Deed deed_earned(Action action);
