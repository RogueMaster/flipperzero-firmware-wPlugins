#include "feedback.h"

bool feedback_vibrates(Action action) {
    return action != ActionNone && action != ActionVoiceStop && action != ActionExit;
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
