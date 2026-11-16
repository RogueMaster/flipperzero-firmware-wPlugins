#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../core/action.h"
#include "../core/link.h"
#include "../core/mode.h"
#include "../core/option.h"

typedef struct {
    Mode mode;
    Link link;
    bool legend;
    uint8_t charge;
    bool charging;
    Action flash;
    Action sent;
    bool options[OptionMAX];
    Option cursor;
} ViewState;
