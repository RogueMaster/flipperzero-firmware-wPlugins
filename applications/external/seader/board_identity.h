#pragma once

#include <stdbool.h>

typedef enum {
    SeaderBoardClassUnknown = 0,
    SeaderBoardClassNone,
    SeaderBoardClassSamOnly,
    SeaderBoardClassUhfCarrier,
} SeaderBoardClass;

SeaderBoardClass seader_board_classify(bool pa4_high, bool pc1_high, bool pc0_high);
bool seader_board_class_supports_uhf(SeaderBoardClass board_class);
