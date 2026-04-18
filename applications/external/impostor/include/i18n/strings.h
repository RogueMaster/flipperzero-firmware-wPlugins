#pragma once

typedef enum {
  ImpostorLocaleEn = 0,
  ImpostorLocaleEs = 1,
} ImpostorLocale;

typedef enum {
  ImpostorStrMenuPlay = 0,
  ImpostorStrMenuNewGame,
  ImpostorStrMenuSettings,
  ImpostorStrSettingsLanguage,
  ImpostorStrSettingsCredits,
  ImpostorStrLangEnglish,
  ImpostorStrLangSpanish,
  ImpostorStrCreditsTitle,
  ImpostorStrCreditsBody,
  ImpostorStrGameListHeader,
  ImpostorStrGameNew,
  ImpostorStrGameSavedPrefix,
  ImpostorStrNameInputHeader,
  ImpostorStrNameMenuAdd,
  ImpostorStrNameMenuContinue,
  ImpostorStrImpostorPickHeader,
  ImpostorStrReadyLine,
  ImpostorStrReadyPlay,
  ImpostorStrBeginTitle,
  ImpostorStrBeginRandomLabel,
  ImpostorStrPassTitle,
  ImpostorStrPassHold,
  ImpostorStrRoleImpostor,
  ImpostorStrRoleHintLabel,
  ImpostorStrRoleWordLabel,
  ImpostorStrRoleCardFooter,
  ImpostorStrPresetPlayers,
  ImpostorStrPresetAdd,
  ImpostorStrPresetRemove,
  ImpostorStrPresetRemoveHeader,
  ImpostorStrPresetImpostors,
  ImpostorStrPresetStart,
  ImpostorStrSessionTitleHeader,
  ImpostorStrPlayerWord,
  ImpostorStrEditPlayerHeader,
  ImpostorStrManageEdit,
  ImpostorStrManageDelete,
  ImpostorStrManageBack,
  ImpostorStrPresetDeleteSaved,
  ImpostorStrCreditsRepoLine1,
  ImpostorStrCreditsRepoLine2,
  ImpostorStrCount,
} ImpostorStrId;

ImpostorLocale impostor_locale_get(void);
void impostor_locale_set(ImpostorLocale locale);

const char *impostor_str(ImpostorStrId id);
