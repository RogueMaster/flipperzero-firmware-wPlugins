#include "pocketlab_i18n.h"

// UI strings per language. Russian/Spanish are only legible when the Universal
// font is active; stock GUI modules (Settings) render Latin only. Lab content is
// localised separately and is not part of this table.
static const char* const strings[PocketLabLangCount][PocketLabTextCount] = {
    [PocketLabLangEn] =
        {
            [PocketLabTextAppName] = "PocketLab",
            [PocketLabTextMenuStart] = "Labs",
            [PocketLabTextMenuQuiz] = "Quiz",
            [PocketLabTextMenuProgress] = "Profile",
            [PocketLabTextMenuSettings] = "Settings",
            [PocketLabTextMenuSettingsShort] = "Settings",
            [PocketLabTextMenuAbout] = "About",
            [PocketLabTextLabsTitle] = "Choose a lab",
            [PocketLabTextSettingsSound] = "Sound",
            [PocketLabTextSettingsLanguage] = "Language",
            [PocketLabTextSettingsLed] = "LED",
            [PocketLabTextSettingsVibro] = "Vibro",
            [PocketLabTextSettingsReset] = "Reset progress",
            [PocketLabTextOn] = "On",
            [PocketLabTextOff] = "Off",
            [PocketLabTextCorrect] = "Correct!",
            [PocketLabTextTryAgain] = "Try again",
            [PocketLabTextLabComplete] = "Lab complete!",
            [PocketLabTextStreak] = "Streak",
            [PocketLabTextLabsWord] = "Labs",
            [PocketLabTextBadges] = "Badges",
            [PocketLabTextLocked] = "(locked)",
            [PocketLabTextNoQuizzes] = "No quizzes yet",
            [PocketLabTextUnlockQuiz] = "Complete some labs to unlock the quiz.",
            [PocketLabTextQuizComplete] = "Quiz complete!",
            [PocketLabTextWrong] = "Wrong",
            [PocketLabTextAnswer] = "Answer",
            [PocketLabTextVersion] = "Version",
            [PocketLabTextAuthor] = "Author",
            [PocketLabTextBack] = "< back",
            [PocketLabTextEnjoying] = "Enjoying",
            [PocketLabTextScan1] = "Scan to buy",
            [PocketLabTextScan2] = "me a coffee",
            [PocketLabTextCoffeeHint] = "coffee >",
            [PocketLabTextResetTitle] = "Reset progress?",
            [PocketLabTextResetBody] = "All XP and badges will be lost.",
            [PocketLabTextCancel] = "Cancel",
            [PocketLabTextResetBtn] = "Reset",
            [PocketLabTextResetDone] = "Reset!",
            [PocketLabTextResetCleared] = "Progress cleared",
        },
    [PocketLabLangRu] =
        {
            [PocketLabTextAppName] = "PocketLab",
            [PocketLabTextMenuStart] = "Уроки",
            [PocketLabTextMenuQuiz] = "Квиз",
            [PocketLabTextMenuProgress] = "Профиль",
            [PocketLabTextMenuSettings] = "Настройки",
            [PocketLabTextMenuSettingsShort] = "Опции",
            [PocketLabTextMenuAbout] = "О программе",
            [PocketLabTextLabsTitle] = "Выберите урок",
            [PocketLabTextSettingsSound] = "Звук",
            [PocketLabTextSettingsLanguage] = "Язык",
            [PocketLabTextSettingsLed] = "Светодиод",
            [PocketLabTextSettingsVibro] = "Вибро",
            [PocketLabTextSettingsReset] = "Сбросить прогресс",
            [PocketLabTextOn] = "Вкл",
            [PocketLabTextOff] = "Выкл",
            [PocketLabTextCorrect] = "Верно!",
            [PocketLabTextTryAgain] = "Ещё раз",
            [PocketLabTextLabComplete] = "Урок пройден!",
            [PocketLabTextStreak] = "Страйк",
            [PocketLabTextLabsWord] = "Уроки",
            [PocketLabTextBadges] = "Награды",
            [PocketLabTextLocked] = "(закрыто)",
            [PocketLabTextNoQuizzes] = "Пока нет квизов",
            [PocketLabTextUnlockQuiz] = "Пройдите уроки, чтобы открыть квиз.",
            [PocketLabTextQuizComplete] = "Квиз пройден!",
            [PocketLabTextWrong] = "Неверно",
            [PocketLabTextAnswer] = "Ответ",
            [PocketLabTextVersion] = "Версия",
            [PocketLabTextAuthor] = "Автор",
            [PocketLabTextBack] = "< назад",
            [PocketLabTextEnjoying] = "Нравится",
            [PocketLabTextScan1] = "Отсканируй",
            [PocketLabTextScan2] = "на кофе",
            [PocketLabTextCoffeeHint] = "кофе >",
            [PocketLabTextResetTitle] = "Сбросить прогресс?",
            [PocketLabTextResetBody] = "Весь опыт и награды будут потеряны.",
            [PocketLabTextCancel] = "Отмена",
            [PocketLabTextResetBtn] = "Сброс",
            [PocketLabTextResetDone] = "Сброс!",
            [PocketLabTextResetCleared] = "Очищено",
        },
};

static PocketLabLang s_lang = PocketLabLangEn;

void pocketlab_i18n_set_lang(PocketLabLang lang) {
    s_lang = (lang < PocketLabLangCount) ? lang : PocketLabLangEn;
}

PocketLabLang pocketlab_i18n_get_lang(void) {
    return s_lang;
}

const char* pocketlab_text(PocketLabTextId id) {
    if(id >= PocketLabTextCount) {
        return "";
    }
    const char* value = strings[s_lang][id];
    if(!value) {
        value = strings[PocketLabLangEn][id]; // fall back to English
    }
    return value ? value : "";
}
