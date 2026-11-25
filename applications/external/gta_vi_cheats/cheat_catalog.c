#include "cheat_catalog.h"
const size_t gta_cheat_count = 61;
const size_t gta_screenshot_count = 28;
const size_t gta_unverified_count = 24;
const char* gta_reported_sequence(const GtaCheat* cheat) {
    return cheat && cheat->single_source && *cheat->single_source ? cheat->single_source : NULL;
}
