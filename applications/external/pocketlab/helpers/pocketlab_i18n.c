#include "pocketlab_i18n.h"

static const char* const strings[PocketLabTextCount] = {
    [PocketLabTextAppName] = "PocketLab",
    [PocketLabTextMenuStart] = "Labs",
    [PocketLabTextMenuQuiz] = "Quiz",
    [PocketLabTextMenuProgress] = "Profile",
    [PocketLabTextMenuSettings] = "Settings",
    [PocketLabTextMenuAbout] = "About",
    [PocketLabTextLabsTitle] = "Choose a lab",
    [PocketLabTextSettingsSound] = "Sound",
    [PocketLabTextSettingsLed] = "LED",
    [PocketLabTextSettingsVibro] = "Vibro",
    [PocketLabTextSettingsReset] = "Reset progress",
    [PocketLabTextOn] = "On",
    [PocketLabTextOff] = "Off",
    [PocketLabTextCorrect] = "Correct!",
    [PocketLabTextTryAgain] = "Try again",
    [PocketLabTextLabComplete] = "Lab complete!",
};

const char* pocketlab_text(PocketLabTextId id) {
    if(id >= PocketLabTextCount) {
        return "";
    }
    const char* value = strings[id];
    return value ? value : "";
}
