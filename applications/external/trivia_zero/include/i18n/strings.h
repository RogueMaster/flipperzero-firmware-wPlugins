#pragma once

#include "include/domain/category.h" /* for Lang */

typedef enum {
    TzStrMenuChangeLang = 0,
    TzStrMenuCredits,
    TzStrMenuExit,
    TzStrFooterReveal,
    TzStrFooterNext,
    TzStrLangPickHeader,
    TzStrLangSpanish,
    TzStrLangEnglish,
    TzStrCreditsBody,
    TzStrCreditsRepoLine1,
    TzStrCreditsRepoLine2,
    TzStrCount, /* keep last */
} TzStrId;

Lang tz_locale_get(void);
void tz_locale_set(Lang locale);

const char *tz_str(TzStrId id);
