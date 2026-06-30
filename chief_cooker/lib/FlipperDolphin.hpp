#pragma once

#include <dolphin/dolphin.h>

class FlipperDolphin {
private:
public:
    static void Deed(DolphinDeed deed) {
        dolphin_deed(deed);
    }
};
