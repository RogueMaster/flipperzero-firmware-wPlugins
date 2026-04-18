#include "include/app/impostor_app.h"
#include "include/app/role_card_text.h"
#include "include/domain/game_limits.h"
#include "include/domain/game_rules.h"
#include "include/domain/player_roster.h"
#include "include/domain/session.h"
#include "include/domain/word_bank.h"
#include "include/i18n/strings.h"
#include "include/infrastructure/saved_games.h"
#include "include/infrastructure/settings_storage.h"
#include "include/platform/random_port.h"
#include "include/ui/card_view.h"
#include "include/ui/pass_view.h"
#include "include/version.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char s_text_input_header[64];

static SavedGameRecord s_saved_rec_scratch;

#define PRESET_IDX_ADD 10u
#define PRESET_IDX_REMOVE 11u
#define PRESET_IDX_IMPOSTORS 12u
#define PRESET_IDX_START 13u
#define PRESET_IDX_DELETE_SAVED 14u
#define REMOVE_IDX_BASE 500u
#define IMPOSTOR_NO_SLOT 0xFFu

typedef enum {
  AppViewHome = 0,
  AppViewGames,
  AppViewSettings,
  AppViewLang,
  AppViewCredits,
  AppViewNameInput,
  AppViewNameMenu,
  AppViewManagePlayer,
  AppViewImpostor,
  AppViewReady,
  AppViewPass,
  AppViewCard,
  AppViewBegin,
  AppViewSavedPreset,
  AppViewRemovePlayer,
} AppView;

typedef enum {
  NameInputNone = 0,
  NameInputSessionTitle,
  NameInputPlayerNew,
  NameInputPlayerEdit,
  NameInputPreset,
} NameInputMode;

typedef struct {
  ViewDispatcher *vd;
  Gui *gui;
  Submenu *home;
  Submenu *games;
  Submenu *settings;
  Submenu *lang;
  Submenu *name_menu;
  Submenu *manage_menu;
  Submenu *impostor_menu;
  Submenu *preset_menu;
  Submenu *remove_menu;
  Widget *credits;
  Widget *ready;
  Widget *begin;
  TextInput *textin;
  PassView *pass;
  CardView *card;
  AppView current;
  GameSession session;
  GameSetup pending;
  uint8_t name_count;
  char name_buf[IMPOSTOR_MAX_NAME_LEN];
  char game_title[IMPOSTOR_MAX_SESSION_TITLE_LEN];
  char big_buf[320];
  char card_buf[256];
  bool save_on_play;
  SavedGameRecord saved_list[IMPOSTOR_MAX_SAVED_GAMES];
  uint8_t saved_count;
  bool games_wizard;
  uint8_t editing_saved_slot;
  bool impostor_pick_for_preset;
  uint8_t name_input_mode;
  uint8_t manage_player_idx;
} ImpostorApp;

static void app_switch(ImpostorApp *app, AppView v) {
  app->current = v;
  view_dispatcher_switch_to_view(app->vd, v);
}

static void app_rebuild_home(ImpostorApp *app);
static void app_rebuild_games(ImpostorApp *app);
static void app_rebuild_settings(ImpostorApp *app);
static void app_rebuild_lang(ImpostorApp *app);
static void app_rebuild_name_menu(ImpostorApp *app);
static void app_rebuild_impostor(ImpostorApp *app);
static void app_build_credits(ImpostorApp *app);
static void app_build_ready(ImpostorApp *app);
static void app_build_begin(ImpostorApp *app);
static void app_open_name_input(ImpostorApp *app);
static void app_open_pass(ImpostorApp *app);
static void app_show_card(ImpostorApp *app);

static void app_rebuild_preset_menu(ImpostorApp *app);
static void app_rebuild_remove_menu(ImpostorApp *app);
static void app_rebuild_manage_menu(ImpostorApp *app);
static void app_start_from_preset(ImpostorApp *app);
static bool app_session_begin_new_round(ImpostorApp *app);
static void app_sync_locale_to_all_surfaces(ImpostorApp *app);

static bool impostor_submenu_choice_valid(uint32_t index, uint8_t max_k) {
  return index > 0u && index <= (uint32_t)max_k;
}

static void app_clamp_preset_impostors(ImpostorApp *app) {
  if (app->name_count < IMPOSTOR_MIN_PLAYERS) {
    return;
  }
  const uint8_t max_k = game_rules_max_impostors(app->name_count);
  if (max_k == 0) {
    return;
  }
  if (app->pending.impostor_count < 1) {
    app->pending.impostor_count = 1;
  }
  if (app->pending.impostor_count > max_k) {
    app->pending.impostor_count = max_k;
  }
}

static void preset_menu_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index == PRESET_IDX_ADD) {
    if (app->name_count >= IMPOSTOR_MAX_PLAYERS) {
      return;
    }
    app->name_input_mode = NameInputPreset;
    app_open_name_input(app);
    return;
  }
  if (index == PRESET_IDX_REMOVE) {
    app_rebuild_remove_menu(app);
    app_switch(app, AppViewRemovePlayer);
    return;
  }
  if (index == PRESET_IDX_IMPOSTORS) {
    app->impostor_pick_for_preset = true;
    app_rebuild_impostor(app);
    app_switch(app, AppViewImpostor);
    return;
  }
  if (index == PRESET_IDX_START) {
    app_start_from_preset(app);
    return;
  }
  if (index == PRESET_IDX_DELETE_SAVED) {
    if (app->editing_saved_slot == IMPOSTOR_NO_SLOT ||
        app->editing_saved_slot >= app->saved_count) {
      return;
    }
    if (saved_games_delete_at(app->saved_list, &app->saved_count,
                              app->editing_saved_slot)) {
      app->editing_saved_slot = IMPOSTOR_NO_SLOT;
      app_rebuild_games(app);
      app_switch(app, AppViewGames);
    }
  }
}

static void remove_player_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index < REMOVE_IDX_BASE) {
    return;
  }
  const uint32_t i = index - REMOVE_IDX_BASE;
  if (i >= app->name_count || app->name_count <= IMPOSTOR_MIN_PLAYERS) {
    app_rebuild_preset_menu(app);
    app_switch(app, AppViewSavedPreset);
    return;
  }
  player_roster_remove_at(app->pending.names, &app->name_count, (uint8_t)i);
  app_clamp_preset_impostors(app);
  app_rebuild_preset_menu(app);
  app_switch(app, AppViewSavedPreset);
}

static void app_rebuild_preset_menu(ImpostorApp *app) {
  submenu_reset(app->preset_menu);
  char hdr[48];
  if (app->game_title[0] != '\0') {
    snprintf(hdr, sizeof(hdr), "%.20s\n%s: %u  %s: %u", app->game_title,
             impostor_str(ImpostorStrPresetPlayers), (unsigned)app->name_count,
             impostor_str(ImpostorStrImpostorPickHeader),
             (unsigned)app->pending.impostor_count);
  } else {
    snprintf(hdr, sizeof(hdr), "%s: %u  %s: %u",
             impostor_str(ImpostorStrPresetPlayers), (unsigned)app->name_count,
             impostor_str(ImpostorStrImpostorPickHeader),
             (unsigned)app->pending.impostor_count);
  }
  submenu_set_header(app->preset_menu, hdr);
  submenu_add_item(app->preset_menu, impostor_str(ImpostorStrPresetAdd),
                   PRESET_IDX_ADD, preset_menu_cb, app);
  if (app->name_count > IMPOSTOR_MIN_PLAYERS) {
    submenu_add_item(app->preset_menu, impostor_str(ImpostorStrPresetRemove),
                     PRESET_IDX_REMOVE, preset_menu_cb, app);
  }
  submenu_add_item(app->preset_menu, impostor_str(ImpostorStrPresetImpostors),
                   PRESET_IDX_IMPOSTORS, preset_menu_cb, app);
  submenu_add_item(app->preset_menu, impostor_str(ImpostorStrPresetStart),
                   PRESET_IDX_START, preset_menu_cb, app);
  if (app->editing_saved_slot != IMPOSTOR_NO_SLOT) {
    submenu_add_item(app->preset_menu,
                     impostor_str(ImpostorStrPresetDeleteSaved),
                     PRESET_IDX_DELETE_SAVED, preset_menu_cb, app);
  }
}

static void app_rebuild_remove_menu(ImpostorApp *app) {
  submenu_reset(app->remove_menu);
  submenu_set_header(app->remove_menu,
                     impostor_str(ImpostorStrPresetRemoveHeader));
  for (uint8_t i = 0; i < app->name_count; ++i) {
    submenu_add_item(app->remove_menu, app->pending.names[i],
                     REMOVE_IDX_BASE + (uint32_t)i, remove_player_cb, app);
  }
}

static void manage_menu_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index == 0u) {
    app->name_input_mode = NameInputPlayerEdit;
    app_open_name_input(app);
    return;
  }
  if (app->name_count > IMPOSTOR_MIN_PLAYERS) {
    if (index == 1u) {
      player_roster_remove_at(app->pending.names, &app->name_count,
                              app->manage_player_idx);
      app_rebuild_name_menu(app);
      app_switch(app, AppViewNameMenu);
      return;
    }
    if (index == 2u) {
      app_rebuild_name_menu(app);
      app_switch(app, AppViewNameMenu);
      return;
    }
  } else if (index == 1u) {
    app_rebuild_name_menu(app);
    app_switch(app, AppViewNameMenu);
  }
}

static void app_rebuild_manage_menu(ImpostorApp *app) {
  submenu_reset(app->manage_menu);
  char hdr[48];
  strlcpy(hdr, app->pending.names[app->manage_player_idx], sizeof(hdr));
  submenu_set_header(app->manage_menu, hdr);
  submenu_add_item(app->manage_menu, impostor_str(ImpostorStrManageEdit), 0u,
                   manage_menu_cb, app);
  if (app->name_count > IMPOSTOR_MIN_PLAYERS) {
    submenu_add_item(app->manage_menu, impostor_str(ImpostorStrManageDelete),
                     1u, manage_menu_cb, app);
    submenu_add_item(app->manage_menu, impostor_str(ImpostorStrManageBack), 2u,
                     manage_menu_cb, app);
  } else {
    submenu_add_item(app->manage_menu, impostor_str(ImpostorStrManageBack), 1u,
                     manage_menu_cb, app);
  }
}

static void app_start_from_preset(ImpostorApp *app) {
  if (app->name_count < IMPOSTOR_MIN_PLAYERS) {
    return;
  }
  app_clamp_preset_impostors(app);
  if (!game_rules_validate_impostor_count(app->name_count,
                                          app->pending.impostor_count)) {
    return;
  }
  app->pending.player_count = app->name_count;
  if (!app_session_begin_new_round(app)) {
    return;
  }
  if (app->editing_saved_slot != IMPOSTOR_NO_SLOT &&
      app->editing_saved_slot < app->saved_count) {
    saved_games_record_refresh_kept_meta(
        &s_saved_rec_scratch, &app->saved_list[app->editing_saved_slot],
        &app->session.setup);
    saved_games_replace_at(app->saved_list, &app->saved_count,
                           app->editing_saved_slot, &s_saved_rec_scratch);
  }
  app->save_on_play = false;
  app->games_wizard = false;
  app_build_ready(app);
  app_switch(app, AppViewReady);
}

static void home_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index == 0u) {
    saved_games_load(app->saved_list, &app->saved_count);
    app_rebuild_games(app);
    app_switch(app, AppViewGames);
    return;
  }
  if (index == 1u) {
    app_rebuild_settings(app);
    app_switch(app, AppViewSettings);
  }
}

static void games_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index == 0u) {
    memset(&app->pending, 0, sizeof(app->pending));
    app->name_count = 0;
    memset(app->game_title, 0, sizeof(app->game_title));
    app->editing_saved_slot = IMPOSTOR_NO_SLOT;
    app->save_on_play = true;
    app->games_wizard = true;
    app->name_input_mode = NameInputSessionTitle;
    app_open_name_input(app);
    return;
  }
  const uint32_t slot = index - 100u;
  if (slot >= app->saved_count) {
    return;
  }
  const SavedGameRecord *s = &app->saved_list[slot];
  app->editing_saved_slot = (uint8_t)slot;
  app->games_wizard = false;
  memcpy(app->pending.names, s->names, sizeof(app->pending.names));
  app->name_count = s->player_count;
  app->pending.player_count = s->player_count;
  app->pending.impostor_count = s->impostor_count;
  app->pending.word_index = 0;
  strlcpy(app->game_title, s->title, sizeof(app->game_title));
  app_clamp_preset_impostors(app);
  app->impostor_pick_for_preset = false;
  app->name_input_mode = NameInputNone;
  app->save_on_play = false;
  app_rebuild_preset_menu(app);
  app_switch(app, AppViewSavedPreset);
}

static void settings_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index == 0u) {
    app_rebuild_lang(app);
    app_switch(app, AppViewLang);
    return;
  }
  app_build_credits(app);
  app_switch(app, AppViewCredits);
}

static void lang_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  impostor_locale_set(index == 0u ? ImpostorLocaleEn : ImpostorLocaleEs);
  settings_save_locale(impostor_locale_get());
  app_sync_locale_to_all_surfaces(app);
  app_switch(app, AppViewSettings);
}

static void name_menu_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  if (index == 0u) {
    if (app->name_count >= IMPOSTOR_MAX_PLAYERS) {
      return;
    }
    app->name_input_mode = NameInputPlayerNew;
    app_open_name_input(app);
    return;
  }
  const uint32_t cont_idx = (uint32_t)app->name_count + 1u;
  if (index == cont_idx) {
    if (app->name_count < IMPOSTOR_MIN_PLAYERS) {
      return;
    }
    app_rebuild_impostor(app);
    app_switch(app, AppViewImpostor);
    return;
  }
  if (index <= (uint32_t)app->name_count) {
    app->manage_player_idx = (uint8_t)(index - 1u);
    app_rebuild_manage_menu(app);
    app_switch(app, AppViewManagePlayer);
  }
}

static void impostor_cb(void *context, uint32_t index) {
  ImpostorApp *app = context;
  const uint8_t max_k = game_rules_max_impostors(app->name_count);
  if (app->impostor_pick_for_preset) {
    if (!impostor_submenu_choice_valid(index, max_k)) {
      return;
    }
    app->impostor_pick_for_preset = false;
    app->pending.impostor_count = (uint8_t)index;
    app_clamp_preset_impostors(app);
    app_rebuild_preset_menu(app);
    app_switch(app, AppViewSavedPreset);
    return;
  }
  if (!impostor_submenu_choice_valid(index, max_k)) {
    return;
  }
  app->pending.player_count = app->name_count;
  app->pending.impostor_count = (uint8_t)index;
  if (!app_session_begin_new_round(app)) {
    return;
  }
  app->games_wizard = false;
  app_build_ready(app);
  app_switch(app, AppViewReady);
}

static void name_input_cb(void *context) {
  ImpostorApp *app = context;
  if (strlen(app->name_buf) == 0u) {
    return;
  }
  if (app->name_input_mode == NameInputPreset) {
    if (app->name_count >= IMPOSTOR_MAX_PLAYERS) {
      app->name_input_mode = NameInputNone;
      app_rebuild_preset_menu(app);
      app_switch(app, AppViewSavedPreset);
      return;
    }
    strlcpy(app->pending.names[app->name_count], app->name_buf,
            IMPOSTOR_MAX_NAME_LEN);
    app->name_count++;
    app->name_input_mode = NameInputNone;
    app_clamp_preset_impostors(app);
    app_rebuild_preset_menu(app);
    app_switch(app, AppViewSavedPreset);
    return;
  }
  if (app->name_input_mode == NameInputSessionTitle) {
    strlcpy(app->game_title, app->name_buf, sizeof(app->game_title));
    app->name_input_mode = NameInputPlayerNew;
    app_open_name_input(app);
    return;
  }
  if (app->name_input_mode == NameInputPlayerEdit) {
    strlcpy(app->pending.names[app->manage_player_idx], app->name_buf,
            IMPOSTOR_MAX_NAME_LEN);
    app->name_input_mode = NameInputNone;
    app_rebuild_name_menu(app);
    app_switch(app, AppViewNameMenu);
    return;
  }
  if (app->name_input_mode == NameInputPlayerNew) {
    if (app->name_count >= IMPOSTOR_MAX_PLAYERS) {
      app->name_input_mode = NameInputNone;
      app_rebuild_name_menu(app);
      app_switch(app, AppViewNameMenu);
      return;
    }
    strlcpy(app->pending.names[app->name_count], app->name_buf,
            IMPOSTOR_MAX_NAME_LEN);
    app->name_count++;
    app->name_input_mode = NameInputNone;
    app_rebuild_name_menu(app);
    app_switch(app, AppViewNameMenu);
    return;
  }
}

static void ready_cb(GuiButtonType button, InputType type, void *context) {
  UNUSED(button);
  if (type != InputTypeShort) {
    return;
  }
  ImpostorApp *app = context;
  if (app->save_on_play) {
    saved_games_record_from_setup(&s_saved_rec_scratch, &app->session.setup,
                                  furi_hal_rtc_get_timestamp(),
                                  app->game_title);
    saved_games_append(app->saved_list, &app->saved_count,
                       &s_saved_rec_scratch);
  }
  app_open_pass(app);
}

static void begin_cb(GuiButtonType button, InputType type, void *context) {
  UNUSED(button);
  if (type != InputTypeShort) {
    return;
  }
  ImpostorApp *app = context;
  app_rebuild_home(app);
  app_switch(app, AppViewHome);
}

static void pass_long_cb(void *ctx) {
  ImpostorApp *app = ctx;
  app_show_card(app);
}

static void card_ok_cb(void *ctx) {
  ImpostorApp *app = ctx;
  session_reveal_after_card_ok(&app->session);
  if (session_reveal_done(&app->session)) {
    session_pick_starter(&app->session, impostor_rng_u32, app);
    app_build_begin(app);
    app_switch(app, AppViewBegin);
  } else {
    app_open_pass(app);
  }
}

static void app_rebuild_home(ImpostorApp *app) {
  submenu_reset(app->home);
  submenu_set_header(app->home, "Impostor Game");
  submenu_add_item(app->home, impostor_str(ImpostorStrMenuPlay), 0, home_cb,
                   app);
  submenu_add_item(app->home, impostor_str(ImpostorStrMenuSettings), 1, home_cb,
                   app);
}

static void app_rebuild_games(ImpostorApp *app) {
  submenu_reset(app->games);
  submenu_set_header(app->games, impostor_str(ImpostorStrGameListHeader));
  submenu_add_item(app->games, impostor_str(ImpostorStrGameNew), 0, games_cb,
                   app);
  char label[48];
  for (uint8_t i = 0; i < app->saved_count; ++i) {
    if (app->saved_list[i].title[0] != '\0') {
      strlcpy(label, app->saved_list[i].title, sizeof(label));
    } else {
      snprintf(label, sizeof(label), "%s %u",
               impostor_str(ImpostorStrGameSavedPrefix), (unsigned)(i + 1u));
    }
    submenu_add_item(app->games, label, 100u + (uint32_t)i, games_cb, app);
  }
}

static void app_rebuild_settings(ImpostorApp *app) {
  submenu_reset(app->settings);
  submenu_set_header(app->settings, impostor_str(ImpostorStrMenuSettings));
  submenu_add_item(app->settings, impostor_str(ImpostorStrSettingsLanguage), 0,
                   settings_cb, app);
  submenu_add_item(app->settings, impostor_str(ImpostorStrSettingsCredits), 1,
                   settings_cb, app);
}

static void app_rebuild_lang(ImpostorApp *app) {
  submenu_reset(app->lang);
  submenu_set_header(app->lang, impostor_str(ImpostorStrSettingsLanguage));
  submenu_add_item(app->lang, impostor_str(ImpostorStrLangEnglish), 0, lang_cb,
                   app);
  submenu_add_item(app->lang, impostor_str(ImpostorStrLangSpanish), 1, lang_cb,
                   app);
}

static void app_rebuild_name_menu(ImpostorApp *app) {
  submenu_reset(app->name_menu);
  if (app->game_title[0] != '\0') {
    submenu_set_header(app->name_menu, app->game_title);
  } else {
    submenu_set_header(app->name_menu, impostor_str(ImpostorStrGameListHeader));
  }
  submenu_add_item(app->name_menu, impostor_str(ImpostorStrNameMenuAdd), 0u,
                   name_menu_cb, app);
  for (uint8_t i = 0; i < app->name_count; ++i) {
    char line[32];
    strlcpy(line, app->pending.names[i], sizeof(line));
    submenu_add_item(app->name_menu, line, (uint32_t)(i + 1u), name_menu_cb,
                     app);
  }
  if (app->name_count >= IMPOSTOR_MIN_PLAYERS) {
    submenu_add_item(app->name_menu, impostor_str(ImpostorStrNameMenuContinue),
                     (uint32_t)app->name_count + 1u, name_menu_cb, app);
  }
}

static void app_rebuild_impostor(ImpostorApp *app) {
  submenu_reset(app->impostor_menu);
  submenu_set_header(app->impostor_menu,
                     impostor_str(ImpostorStrImpostorPickHeader));
  const uint8_t max_k = game_rules_max_impostors(app->name_count);
  const uint8_t suggested = game_rules_suggested_impostors(app->name_count);
  for (uint8_t k = 1; k <= max_k; ++k) {
    char line[28];
    if (k == suggested) {
      snprintf(line, sizeof(line), "%u (*)", (unsigned)k);
    } else {
      snprintf(line, sizeof(line), "%u", (unsigned)k);
    }
    submenu_add_item(app->impostor_menu, line, (uint32_t)k, impostor_cb, app);
  }
  if (max_k > 0) {
    submenu_set_selected_item(app->impostor_menu, (uint32_t)suggested);
  }
}

static void app_build_credits(ImpostorApp *app) {
  widget_reset(app->credits);
  snprintf(app->big_buf, sizeof(app->big_buf), "%s%s",
           impostor_str(ImpostorStrCreditsBody), APP_VERSION);
  widget_add_text_scroll_element(app->credits, 0, 0, 128, 46, app->big_buf);
  widget_add_string_element(app->credits, 64, 48, AlignCenter, AlignTop,
                            FontSecondary,
                            impostor_str(ImpostorStrCreditsRepoLine1));
  widget_add_string_element(app->credits, 64, 56, AlignCenter, AlignTop,
                            FontSecondary,
                            impostor_str(ImpostorStrCreditsRepoLine2));
}

static void app_build_ready(ImpostorApp *app) {
  widget_reset(app->ready);
  widget_add_text_box_element(app->ready, 0, 0, 128, 40, AlignCenter,
                              AlignCenter, impostor_str(ImpostorStrReadyLine),
                              false);
  widget_add_button_element(app->ready, GuiButtonTypeCenter,
                            impostor_str(ImpostorStrReadyPlay), ready_cb, app);
}

static void app_build_begin(ImpostorApp *app) {
  widget_reset(app->begin);
  snprintf(app->big_buf, sizeof(app->big_buf), "\e#%s\e#\n\n%s\n%s",
           impostor_str(ImpostorStrBeginTitle),
           impostor_str(ImpostorStrBeginRandomLabel),
           app->session.setup.names[app->session.starter_idx]);
  widget_add_text_box_element(app->begin, 0, 0, 128, 48, AlignCenter,
                              AlignCenter, app->big_buf, false);
  widget_add_button_element(app->begin, GuiButtonTypeCenter, "OK", begin_cb,
                            app);
}

static void app_open_name_input(ImpostorApp *app) {
  memset(app->name_buf, 0, sizeof(app->name_buf));
  if (app->name_input_mode == NameInputPlayerEdit) {
    strlcpy(app->name_buf, app->pending.names[app->manage_player_idx],
            sizeof(app->name_buf));
  }
  text_input_reset(app->textin);
  const char *hdr = impostor_str(ImpostorStrNameInputHeader);
  if (app->name_input_mode == NameInputSessionTitle) {
    hdr = impostor_str(ImpostorStrSessionTitleHeader);
  } else if (app->name_input_mode == NameInputPlayerNew) {
    snprintf(s_text_input_header, sizeof(s_text_input_header), "%s %u",
             impostor_str(ImpostorStrNameInputHeader),
             (unsigned)(app->name_count + 1u));
    hdr = s_text_input_header;
  } else if (app->name_input_mode == NameInputPlayerEdit) {
    hdr = impostor_str(ImpostorStrEditPlayerHeader);
  } else if (app->name_input_mode == NameInputPreset) {
    hdr = impostor_str(ImpostorStrNameInputHeader);
  }
  text_input_set_header_text(app->textin, hdr);
  text_input_set_result_callback(app->textin, name_input_cb, app, app->name_buf,
                                 sizeof(app->name_buf), false);
  app_switch(app, AppViewNameInput);
}

static void app_open_pass(ImpostorApp *app) {
  const uint8_t slot = session_reveal_current_slot(&app->session);
  pass_view_set_ui(app->pass, app->session.setup.names[slot],
                   impostor_str(ImpostorStrPassTitle),
                   impostor_str(ImpostorStrPassHold));
  app_switch(app, AppViewPass);
}

static void app_show_card(ImpostorApp *app) {
  role_card_format(app->card_buf, sizeof(app->card_buf), &app->session);
  card_view_set_text(app->card, app->card_buf);
  app_switch(app, AppViewCard);
}

static bool app_session_begin_new_round(ImpostorApp *app) {
  app->pending.word_index =
      (uint16_t)(impostor_rng_u32(app) % (uint32_t)word_bank_word_count());
  return session_begin(&app->session, &app->pending, impostor_rng_u32, app);
}

static void app_sync_locale_to_all_surfaces(ImpostorApp *app) {
  app_rebuild_home(app);
  app_rebuild_games(app);
  app_rebuild_settings(app);
  app_rebuild_lang(app);
  app_rebuild_name_menu(app);
  app_rebuild_impostor(app);
  app_rebuild_preset_menu(app);
  app_rebuild_remove_menu(app);
  app_build_credits(app);
  app_build_ready(app);
}

static bool app_nav(void *context) {
  ImpostorApp *app = context;
  switch (app->current) {
  case AppViewHome:
    return false;
  case AppViewGames:
    app_switch(app, AppViewHome);
    return true;
  case AppViewSettings:
    app_switch(app, AppViewHome);
    return true;
  case AppViewLang:
    app_rebuild_settings(app);
    app_switch(app, AppViewSettings);
    return true;
  case AppViewCredits:
    app_rebuild_settings(app);
    app_switch(app, AppViewSettings);
    return true;
  case AppViewNameInput:
    if (app->name_input_mode == NameInputPreset) {
      app->name_input_mode = NameInputNone;
      app_rebuild_preset_menu(app);
      app_switch(app, AppViewSavedPreset);
      return true;
    }
    if (app->name_input_mode == NameInputSessionTitle) {
      app->name_input_mode = NameInputNone;
      app_switch(app, AppViewGames);
      return true;
    }
    if (app->name_input_mode == NameInputPlayerEdit) {
      app->name_input_mode = NameInputNone;
      app_rebuild_manage_menu(app);
      app_switch(app, AppViewManagePlayer);
      return true;
    }
    if (app->name_input_mode == NameInputPlayerNew && app->games_wizard) {
      app->name_input_mode = NameInputNone;
      if (app->name_count == 0u) {
        app_switch(app, AppViewGames);
      } else {
        app_rebuild_name_menu(app);
        app_switch(app, AppViewNameMenu);
      }
      return true;
    }
    app_switch(app, AppViewGames);
    return true;
  case AppViewNameMenu:
    app_switch(app, AppViewGames);
    return true;
  case AppViewManagePlayer:
    app_rebuild_name_menu(app);
    app_switch(app, AppViewNameMenu);
    return true;
  case AppViewImpostor:
    if (app->impostor_pick_for_preset) {
      app->impostor_pick_for_preset = false;
      app_rebuild_preset_menu(app);
      app_switch(app, AppViewSavedPreset);
      return true;
    }
    app_rebuild_name_menu(app);
    app_switch(app, AppViewNameMenu);
    return true;
  case AppViewSavedPreset:
    app->editing_saved_slot = IMPOSTOR_NO_SLOT;
    app->impostor_pick_for_preset = false;
    app->name_input_mode = NameInputNone;
    saved_games_load(app->saved_list, &app->saved_count);
    app_rebuild_games(app);
    app_switch(app, AppViewGames);
    return true;
  case AppViewRemovePlayer:
    app_rebuild_preset_menu(app);
    app_switch(app, AppViewSavedPreset);
    return true;
  case AppViewReady:
    app->editing_saved_slot = IMPOSTOR_NO_SLOT;
    app_switch(app, AppViewHome);
    return true;
  case AppViewPass:
  case AppViewCard:
    app_switch(app, AppViewReady);
    return true;
  case AppViewBegin:
    app_switch(app, AppViewHome);
    return true;
  default:
    return false;
  }
}

int32_t impostor_app_run(void) {
  ImpostorApp *app = malloc(sizeof(ImpostorApp));
  if (!app) {
    return -1;
  }
  memset(app, 0, sizeof(*app));
  app->editing_saved_slot = IMPOSTOR_NO_SLOT;

  ImpostorLocale loc = ImpostorLocaleEn;
  settings_load_locale(&loc);
  impostor_locale_set(loc);

  app->gui = furi_record_open(RECORD_GUI);
  app->vd = view_dispatcher_alloc();
  view_dispatcher_attach_to_gui(app->vd, app->gui,
                                ViewDispatcherTypeFullscreen);
  view_dispatcher_set_event_callback_context(app->vd, app);
  view_dispatcher_set_navigation_event_callback(app->vd, app_nav);

  app->home = submenu_alloc();
  app->games = submenu_alloc();
  app->settings = submenu_alloc();
  app->lang = submenu_alloc();
  app->name_menu = submenu_alloc();
  app->manage_menu = submenu_alloc();
  app->impostor_menu = submenu_alloc();
  app->preset_menu = submenu_alloc();
  app->remove_menu = submenu_alloc();
  app->credits = widget_alloc();
  app->ready = widget_alloc();
  app->begin = widget_alloc();
  app->textin = text_input_alloc();
  app->pass = pass_view_alloc();
  app->card = card_view_alloc();

  if (!app->home || !app->games || !app->settings || !app->lang ||
      !app->name_menu || !app->manage_menu || !app->impostor_menu ||
      !app->preset_menu || !app->remove_menu || !app->credits || !app->ready ||
      !app->begin || !app->textin || !app->pass || !app->card) {
    goto fail;
  }

  pass_view_set_callback(app->pass, app, pass_long_cb);
  card_view_set_callback(app->card, app, card_ok_cb);

  app_rebuild_home(app);
  saved_games_load(app->saved_list, &app->saved_count);

  view_dispatcher_add_view(app->vd, AppViewHome, submenu_get_view(app->home));
  view_dispatcher_add_view(app->vd, AppViewGames, submenu_get_view(app->games));
  view_dispatcher_add_view(app->vd, AppViewSettings,
                           submenu_get_view(app->settings));
  view_dispatcher_add_view(app->vd, AppViewLang, submenu_get_view(app->lang));
  view_dispatcher_add_view(app->vd, AppViewCredits,
                           widget_get_view(app->credits));
  view_dispatcher_add_view(app->vd, AppViewNameInput,
                           text_input_get_view(app->textin));
  view_dispatcher_add_view(app->vd, AppViewNameMenu,
                           submenu_get_view(app->name_menu));
  view_dispatcher_add_view(app->vd, AppViewManagePlayer,
                           submenu_get_view(app->manage_menu));
  view_dispatcher_add_view(app->vd, AppViewImpostor,
                           submenu_get_view(app->impostor_menu));
  view_dispatcher_add_view(app->vd, AppViewSavedPreset,
                           submenu_get_view(app->preset_menu));
  view_dispatcher_add_view(app->vd, AppViewRemovePlayer,
                           submenu_get_view(app->remove_menu));
  view_dispatcher_add_view(app->vd, AppViewReady, widget_get_view(app->ready));
  view_dispatcher_add_view(app->vd, AppViewPass, pass_view_get_view(app->pass));
  view_dispatcher_add_view(app->vd, AppViewCard, card_view_get_view(app->card));
  view_dispatcher_add_view(app->vd, AppViewBegin, widget_get_view(app->begin));

  app_switch(app, AppViewHome);
  view_dispatcher_run(app->vd);

  view_dispatcher_remove_view(app->vd, AppViewBegin);
  view_dispatcher_remove_view(app->vd, AppViewCard);
  view_dispatcher_remove_view(app->vd, AppViewPass);
  view_dispatcher_remove_view(app->vd, AppViewReady);
  view_dispatcher_remove_view(app->vd, AppViewRemovePlayer);
  view_dispatcher_remove_view(app->vd, AppViewSavedPreset);
  view_dispatcher_remove_view(app->vd, AppViewImpostor);
  view_dispatcher_remove_view(app->vd, AppViewManagePlayer);
  view_dispatcher_remove_view(app->vd, AppViewNameMenu);
  view_dispatcher_remove_view(app->vd, AppViewNameInput);
  view_dispatcher_remove_view(app->vd, AppViewCredits);
  view_dispatcher_remove_view(app->vd, AppViewLang);
  view_dispatcher_remove_view(app->vd, AppViewSettings);
  view_dispatcher_remove_view(app->vd, AppViewGames);
  view_dispatcher_remove_view(app->vd, AppViewHome);

  card_view_free(app->card);
  pass_view_free(app->pass);
  text_input_free(app->textin);
  widget_free(app->begin);
  widget_free(app->ready);
  widget_free(app->credits);
  submenu_free(app->remove_menu);
  submenu_free(app->preset_menu);
  submenu_free(app->impostor_menu);
  submenu_free(app->manage_menu);
  submenu_free(app->name_menu);
  submenu_free(app->lang);
  submenu_free(app->settings);
  submenu_free(app->games);
  submenu_free(app->home);
  view_dispatcher_free(app->vd);
  furi_record_close(RECORD_GUI);
  free(app);
  return 0;

fail:
  if (app->card) {
    card_view_free(app->card);
  }
  if (app->pass) {
    pass_view_free(app->pass);
  }
  if (app->textin) {
    text_input_free(app->textin);
  }
  if (app->begin) {
    widget_free(app->begin);
  }
  if (app->ready) {
    widget_free(app->ready);
  }
  if (app->credits) {
    widget_free(app->credits);
  }
  if (app->remove_menu) {
    submenu_free(app->remove_menu);
  }
  if (app->preset_menu) {
    submenu_free(app->preset_menu);
  }
  if (app->impostor_menu) {
    submenu_free(app->impostor_menu);
  }
  if (app->manage_menu) {
    submenu_free(app->manage_menu);
  }
  if (app->name_menu) {
    submenu_free(app->name_menu);
  }
  if (app->lang) {
    submenu_free(app->lang);
  }
  if (app->settings) {
    submenu_free(app->settings);
  }
  if (app->games) {
    submenu_free(app->games);
  }
  if (app->home) {
    submenu_free(app->home);
  }
  if (app->vd) {
    view_dispatcher_free(app->vd);
  }
  if (app->gui) {
    furi_record_close(RECORD_GUI);
  }
  free(app);
  return -1;
}
