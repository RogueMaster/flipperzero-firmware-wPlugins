#include "include/i18n/strings.h"

static ImpostorLocale g_locale = ImpostorLocaleEn;

static const char *const k_en[ImpostorStrCount] = {
    [ImpostorStrMenuPlay] = "Play",
    [ImpostorStrMenuNewGame] = "New game",
    [ImpostorStrMenuSettings] = "Settings",
    [ImpostorStrSettingsLanguage] = "Language",
    [ImpostorStrSettingsCredits] = "Credits",
    [ImpostorStrLangEnglish] = "English",
    [ImpostorStrLangSpanish] = "Spanish",
    [ImpostorStrCreditsTitle] = "Credits",
    [ImpostorStrCreditsBody] = "Impostor Game\n"
                               "Author: Endika\n",
    [ImpostorStrGameListHeader] = "Games",
    [ImpostorStrGameNew] = "Create new",
    [ImpostorStrGameSavedPrefix] = "Saved",
    [ImpostorStrNameInputHeader] = "Player name",
    [ImpostorStrNameMenuAdd] = "Add another player",
    [ImpostorStrNameMenuContinue] = "Choose impostors",
    [ImpostorStrImpostorPickHeader] = "Impostors",
    [ImpostorStrReadyLine] = "Roles are set.\nPass the device when asked.",
    [ImpostorStrReadyPlay] = "PLAY",
    [ImpostorStrBeginTitle] = "START",
    [ImpostorStrBeginRandomLabel] = "First speaker:",
    [ImpostorStrPassTitle] = "Pass device to:",
    [ImpostorStrPassHold] = "Hold OK to reveal",
    [ImpostorStrRoleImpostor] = "IMPOSTOR",
    [ImpostorStrRoleHintLabel] = "Hint:",
    [ImpostorStrRoleWordLabel] = "Word:",
    [ImpostorStrRoleCardFooter] = "Short OK = next",
    [ImpostorStrPresetPlayers] = "Players",
    [ImpostorStrPresetAdd] = "Add player",
    [ImpostorStrPresetRemove] = "Remove player",
    [ImpostorStrPresetRemoveHeader] = "Remove who?",
    [ImpostorStrPresetImpostors] = "Set impostors",
    [ImpostorStrPresetStart] = "START",
    [ImpostorStrSessionTitleHeader] = "Game name",
    [ImpostorStrPlayerWord] = "Player",
    [ImpostorStrEditPlayerHeader] = "Edit name",
    [ImpostorStrManageEdit] = "Edit",
    [ImpostorStrManageDelete] = "Remove",
    [ImpostorStrManageBack] = "Back",
    [ImpostorStrPresetDeleteSaved] = "Delete saved game",
    [ImpostorStrCreditsRepoLine1] = "https://github.com/endika/",
    [ImpostorStrCreditsRepoLine2] = "flipper-impostor-game",
};

static const char *const k_es[ImpostorStrCount] = {
    [ImpostorStrMenuPlay] = "Jugar",
    [ImpostorStrMenuNewGame] = "Nueva partida",
    [ImpostorStrMenuSettings] = "Ajustes",
    [ImpostorStrSettingsLanguage] = "Idioma",
    [ImpostorStrSettingsCredits] = "Creditos",
    [ImpostorStrLangEnglish] = "Ingles",
    [ImpostorStrLangSpanish] = "Espanol",
    [ImpostorStrCreditsTitle] = "Creditos",
    [ImpostorStrCreditsBody] = "Impostor Game\n"
                               "Autor: Endika\n",
    [ImpostorStrGameListHeader] = "Partidas",
    [ImpostorStrGameNew] = "Crear nueva",
    [ImpostorStrGameSavedPrefix] = "Guardada",
    [ImpostorStrNameInputHeader] = "Nombre del jugador",
    [ImpostorStrNameMenuAdd] = "Anadir jugador",
    [ImpostorStrNameMenuContinue] = "Elegir impostores",
    [ImpostorStrImpostorPickHeader] = "Impostores",
    [ImpostorStrReadyLine] = "Roles listos.\nPasa el dispositivo.",
    [ImpostorStrReadyPlay] = "JUGAR",
    [ImpostorStrBeginTitle] = "COMIENZA",
    [ImpostorStrBeginRandomLabel] = "Empieza:",
    [ImpostorStrPassTitle] = "Pasa el dispositivo a:",
    [ImpostorStrPassHold] = "Mantener OK",
    [ImpostorStrRoleImpostor] = "IMPOSTOR",
    [ImpostorStrRoleHintLabel] = "Pista:",
    [ImpostorStrRoleWordLabel] = "Palabra:",
    [ImpostorStrRoleCardFooter] = "OK corto = siguiente",
    [ImpostorStrPresetPlayers] = "Jugadores",
    [ImpostorStrPresetAdd] = "Anadir jugador",
    [ImpostorStrPresetRemove] = "Quitar jugador",
    [ImpostorStrPresetRemoveHeader] = "Quitar a",
    [ImpostorStrPresetImpostors] = "Impostores",
    [ImpostorStrPresetStart] = "EMPEZAR",
    [ImpostorStrSessionTitleHeader] = "Nombre de la partida",
    [ImpostorStrPlayerWord] = "Jugador",
    [ImpostorStrEditPlayerHeader] = "Editar nombre",
    [ImpostorStrManageEdit] = "Editar",
    [ImpostorStrManageDelete] = "Quitar",
    [ImpostorStrManageBack] = "Volver",
    [ImpostorStrPresetDeleteSaved] = "Borrar partida",
    [ImpostorStrCreditsRepoLine1] = "https://github.com/endika/",
    [ImpostorStrCreditsRepoLine2] = "flipper-impostor-game",
};

ImpostorLocale impostor_locale_get(void) { return g_locale; }

void impostor_locale_set(ImpostorLocale locale) {
  if (locale != ImpostorLocaleEn && locale != ImpostorLocaleEs) {
    g_locale = ImpostorLocaleEn;
    return;
  }
  g_locale = locale;
}

const char *impostor_str(ImpostorStrId id) {
  if ((unsigned)id >= (unsigned)ImpostorStrCount) {
    return "";
  }
  if (g_locale == ImpostorLocaleEs) {
    return k_es[id];
  }
  return k_en[id];
}
