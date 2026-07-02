#pragma once

#include <stddef.h>
#include <stdint.h>

#define WIKIFLIP_APP_VERSION       FAP_VERSION
#define WIKIFLIP_SCREEN_WIDTH      128
#define WIKIFLIP_SCREEN_HEIGHT     64
#define WIKIFLIP_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

typedef enum {
    WikiFlipViewCategories = 0,
    WikiFlipViewTerms,
    WikiFlipViewSettings,
    WikiFlipViewDefinition,
} WikiFlipView;

typedef struct {
    const char* title;
    const char* definition;
} WikiFlipTerm;

typedef struct {
    const char* name;
    const WikiFlipTerm* terms;
    size_t term_count;
} WikiFlipCategory;

typedef struct WikiFlipApp WikiFlipApp;

int32_t wikiflip_app(void* p);
