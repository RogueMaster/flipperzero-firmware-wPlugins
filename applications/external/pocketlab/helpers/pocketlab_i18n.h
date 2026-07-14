#pragma once

typedef enum {
    PocketLabTextAppName,
    PocketLabTextMenuStart,
    PocketLabTextMenuQuiz,
    PocketLabTextMenuProgress,
    PocketLabTextMenuSettings,
    PocketLabTextMenuSettingsShort, // fits the narrow home tile (RU shortened)
    PocketLabTextMenuAbout,
    PocketLabTextLabsTitle,
    PocketLabTextSettingsSound,
    PocketLabTextSettingsLanguage,
    PocketLabTextSettingsLed,
    PocketLabTextSettingsVibro,
    PocketLabTextSettingsReset,
    PocketLabTextOn,
    PocketLabTextOff,
    PocketLabTextCorrect,
    PocketLabTextTryAgain,
    PocketLabTextLabComplete,
    // Profile / Badges chrome.
    PocketLabTextStreak,
    PocketLabTextLabsWord,
    PocketLabTextBadges,
    PocketLabTextLocked,
    // Quiz chrome.
    PocketLabTextNoQuizzes,
    PocketLabTextUnlockQuiz,
    PocketLabTextQuizComplete,
    PocketLabTextWrong,
    PocketLabTextAnswer,
    // About chrome.
    PocketLabTextVersion,
    PocketLabTextAuthor,
    PocketLabTextBack,
    PocketLabTextEnjoying,
    PocketLabTextScan1,
    PocketLabTextScan2,
    PocketLabTextCoffeeHint,
    // Reset confirmation page.
    PocketLabTextResetTitle,
    PocketLabTextResetBody,
    PocketLabTextCancel,
    PocketLabTextResetBtn,
    PocketLabTextResetDone,
    PocketLabTextResetCleared,
    PocketLabTextCount,
} PocketLabTextId;

typedef enum {
    PocketLabLangEn = 0,
    PocketLabLangRu = 1,
    PocketLabLangCount,
} PocketLabLang;

/** Select the active UI language. Call at startup and on the Language setting. */
void pocketlab_i18n_set_lang(PocketLabLang lang);

/** The active UI language. */
PocketLabLang pocketlab_i18n_get_lang(void);

/** Localised UI string for the active language (falls back to English). */
const char* pocketlab_text(PocketLabTextId id);
