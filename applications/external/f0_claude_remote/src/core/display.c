#include "display.h"

DisplayLight display_light(Mode mode, bool backlight) {
    switch(mode) {
    case ModeMain:
    case ModeVoice:
    case ModeOptions:
    case ModeNoLink:
    case ModeVoiceCut:
    case ModeOptionsNoLink:
        return backlight ? DisplayLightOn : DisplayLightAuto;
    case ModeStopped:
        break;
    }
    return DisplayLightOff;
}
