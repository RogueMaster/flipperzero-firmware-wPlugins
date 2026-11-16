#include "feedback.h"

bool feedback_vibrates(Action action, bool vibration) {
    switch(action) {
    case ActionEnter:
    case ActionArrowUp:
    case ActionArrowDown:
    case ActionArrowLeft:
    case ActionArrowRight:
    case ActionEsc:
    case ActionEffortHigh:
    case ActionEffortMax:
    case ActionClear:
    case ActionVoiceStart:
        return vibration;
    case ActionNone:
    case ActionVoiceStop:
    case ActionExit:
    case ActionGiveUp:
    case ActionStartOver:
        break;
    }
    return false;
}

/* Arrows come in series, and a flash longer than the gap between taps would merge them. */
uint32_t feedback_flash_ms(Action action) {
    switch(action) {
    case ActionArrowUp:
    case ActionArrowDown:
    case ActionArrowLeft:
    case ActionArrowRight:
        return 100;
    case ActionEnter:
    case ActionEsc:
    case ActionEffortHigh:
    case ActionEffortMax:
    case ActionClear:
        return 400;
    case ActionNone:
    case ActionVoiceStart:
    case ActionVoiceStop:
    case ActionExit:
    case ActionGiveUp:
    case ActionStartOver:
        break;
    }
    return 0;
}

/* The label names what flashed, so one list holds both. */
uint32_t feedback_sent_ms(Action action) {
    return feedback_flash_ms(action) > 0 ? 3000 : 0;
}
