#pragma once

enum PagerAction {
    UNKNOWN,
    RING,
    MUTE,
    DESYNC,
    TURN_OFF_ALL,

    PagerActionCount,
};

class PagerActions {
public:
    static const char* GetDescription(PagerAction action) {
        switch(action) {
        case UNKNOWN:
            return "?";
        case RING:
            return "RING";
        case MUTE:
            return "MUTE";
        case DESYNC:
            return "DESYNC_ALL";
        case TURN_OFF_ALL:
            return "ALL_OFF";
        default:
            return "";
        }
    }

    static bool IsPagerActionSpecial(PagerAction action) {
        switch(action) {
        case DESYNC:
        case TURN_OFF_ALL:
            return true;

        default:
            return false;
        }
    }
};
