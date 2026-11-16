#include "deed.h"

Deed deed_earned(Action action) {
    switch(action) {
    case ActionVoiceStart:
        return DeedDictation;
    case ActionClear:
        return DeedClear;
    case ActionNone:
    case ActionEnter:
    case ActionArrowUp:
    case ActionArrowDown:
    case ActionArrowLeft:
    case ActionArrowRight:
    case ActionEsc:
    case ActionEffortHigh:
    case ActionEffortMax:
    case ActionVoiceStop:
    case ActionExit:
    case ActionGiveUp:
    case ActionStartOver:
        break;
    }
    return DeedNone;
}
