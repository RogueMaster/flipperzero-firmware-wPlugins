#include "cheat_catalog.h"
#include <string.h>
const size_t gta_cheat_count = 61;
const size_t gta_screenshot_count = 28;
const size_t gta_unverified_count = 24;
const char* gta_reported_sequence(const GtaCheat* cheat) {
    if(strcmp(cheat->name, "Pinhole") == 0) {
        return "Down Down Up Right Right Up Left Left Up Down";
    }
    return NULL;
}
