#include "deed.h"

Deed deed_earned(Action action) {
    switch(action) {
    case ActionVoiceStart:
        return DeedDictation;
    case ActionClear:
        return DeedClear;
    default:
        return DeedNone;
    }
}
