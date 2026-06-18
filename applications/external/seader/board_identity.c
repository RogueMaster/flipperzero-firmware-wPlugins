#include "board_identity.h"

SeaderBoardClass seader_board_classify(bool pa4_high, bool pc1_high, bool pc0_high) {
    if(pa4_high) {
        return SeaderBoardClassUhfCarrier;
    }

    if(pc1_high || pc0_high) {
        return SeaderBoardClassSamOnly;
    }

    return SeaderBoardClassNone;
}

bool seader_board_class_supports_uhf(SeaderBoardClass board_class) {
    return board_class == SeaderBoardClassUhfCarrier;
}
