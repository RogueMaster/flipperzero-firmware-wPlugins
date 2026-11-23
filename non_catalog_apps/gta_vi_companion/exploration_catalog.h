#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct GtaExploreSection GtaExploreSection;

struct GtaExploreSection {
    const char* title;
    const char* asset;
    const char* checked_label;
    uint16_t game_total;
    const GtaExploreSection* children;
    size_t child_count;
};

extern const GtaExploreSection gta_explore_sections[];
extern const size_t gta_explore_section_count;
