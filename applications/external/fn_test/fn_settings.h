/**
 * @file fn_settings.h
 * @brief Сохранение настроек приложения на SD-карту.
 *
 * Настройки лежат в /data/fn_test/settings.txt (внутри приложения — APP_DATA_PATH).
 * Формат простой: одна строка `lang=en` или `lang=ru`.
 */
#pragma once

#include "lib/fn/core/fn_lang.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Прочитать язык. @return false, если настройка ещё не сохранялась. */
bool fn_settings_load_lang(FnLang* lang);

/** Сохранить язык (создаёт каталог приложения при необходимости). */
void fn_settings_save_lang(FnLang lang);

#ifdef __cplusplus
}
#endif
