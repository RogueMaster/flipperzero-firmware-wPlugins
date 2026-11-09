#include "feedback.h"

bool feedback_vibrates(Action action) {
    switch(action) {
    case ActionNone:
    case ActionVoiceStop:
    case ActionExit:
    case ActionGiveUp:
    case ActionStartOver:
    case ActionUseUsb:
    case ActionUseBluetooth:
        return false;
    default:
        return true;
    }
}

/* Arrows come in series, and a flash longer than the gap between taps would merge them. */
uint32_t feedback_flash_ms(Action action) {
    switch(action) {
    case ActionArrowUp:
    case ActionArrowDown:
        return 100;
    case ActionEnter:
    case ActionEsc:
    case ActionEffortHigh:
    case ActionEffortMax:
    case ActionClear:
        return 400;
    default:
        return 0;
    }
}
