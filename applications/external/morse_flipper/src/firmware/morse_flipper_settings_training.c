/*
 * Purpose: Handle LCWO, straight key trainer, and TX Groups settings rows.
 * Owns: training setting callbacks, clamps, labels, and persistence hooks.
 * Depends on: morse_flipper_app_i.h plus trainer and TX Groups modules.
 * Tests: trainer, straight key trainer, and TX Groups host tests cover models.
 */

#include "morse_flipper_app_i.h"

static uint8_t morse_flipper_trainer_menu_custom_count(const MorseFlipperApp* app) {
    if(app == NULL || !app->custom_sets_loaded || app->custom_sets == NULL) return 0U;
    return app->custom_sets->count;
}

static uint8_t morse_flipper_trainer_menu_custom_idx(const MorseFlipperApp* app) {
    uint8_t count = morse_flipper_trainer_menu_custom_count(app);
    uint8_t idx;

    if(app == NULL) return 0U;
    idx = app->trainer.custom_set_idx;
    if(idx == 0U || idx > count) return 0U;
    return idx;
}

static const char*
    morse_flipper_trainer_menu_custom_label(const MorseFlipperApp* app, uint8_t idx) {
    if(idx == 0U || app == NULL || app->custom_sets == NULL) return "lesson";
    if(idx > app->custom_sets->count) return "lesson";
    return app->custom_sets->sets[idx - 1U].name;
}

void morse_flipper_trainer_sync_farn_item(MorseFlipperApp* app) {
    VariableItem* it;
    char txt[4];
    uint8_t idx;
    uint8_t wpm;

    if(app == NULL) return;

    morse_flipper_clamp_trainer_settings(app);
    it = app->trainer_items[MorseFlipperTrainerSettingFarnsworth];
    if(it == NULL) return;

    wpm = morse_flipper_local_wpm(app);
    if(wpm == 0U) wpm = 1U;
    variable_item_set_values_count(it, wpm);
    idx = app->trainer_farnsworth_wpm > 0U ? (uint8_t)(app->trainer_farnsworth_wpm - 1U) : 0U;
    variable_item_set_current_value_index(it, idx);
    snprintf(txt, sizeof(txt), "%u", (unsigned)app->trainer_farnsworth_wpm);
    variable_item_set_current_value_text(it, txt);
}

void morse_flipper_trainer_menu_refresh(MorseFlipperApp* app) {
    VariableItem* it;
    char txt[16];
    uint8_t idx;

    if(app == NULL) return;

    morse_flipper_clamp_trainer_settings(app);

    it = app->trainer_items[MorseFlipperTrainerSettingLesson];
    if(it) {
        idx = morse_trainer_lesson(&app->trainer);
        idx = idx > 0U ? (uint8_t)(idx - 1U) : 0U;
        morse_trainer_lesson_label((uint8_t)(idx + 1U), txt, sizeof(txt));
        variable_item_set_current_value_index(it, idx);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingWpm];
    if(it) {
        idx = (uint8_t)(morse_flipper_local_wpm(app) - 10U);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 10U));
        variable_item_set_current_value_index(it, idx);
        variable_item_set_current_value_text(it, txt);
    }

    morse_flipper_trainer_sync_farn_item(app);

    it = app->trainer_items[MorseFlipperTrainerSettingAnswerTimeout];
    if(it) {
        idx = (uint8_t)(app->trainer_answer_timeout_s - MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->trainer_answer_timeout_s);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingGroupPause];
    if(it) {
        idx = (uint8_t)(app->trainer_group_pause_s - MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->trainer_group_pause_s);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingGroupSize];
    if(it) {
        idx = (uint8_t)(morse_trainer_group_size(&app->trainer) - 1U);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 1U));
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingGroups];
    if(it) {
        idx = (uint8_t)(morse_trainer_session_groups(&app->trainer) - 3U);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 3U));
        variable_item_set_current_value_text(it, txt);
    }

    it = app->trainer_items[MorseFlipperTrainerSettingChars];
    if(it) {
        idx = morse_flipper_trainer_menu_custom_idx(app);
        variable_item_set_current_value_index(it, idx);
        variable_item_set_current_value_text(
            it, morse_flipper_trainer_menu_custom_label(app, idx));
    }
}

void morse_flipper_trainer_lesson_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    char buf[16];

    morse_trainer_set_lesson(&app->trainer, (uint8_t)(idx + 1U));
    morse_trainer_lesson_label(morse_trainer_lesson(&app->trainer), buf, sizeof(buf));
    variable_item_set_current_value_text(item, buf);
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_wpm_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t w = (uint8_t)(10U + idx);

    morse_flipper_set_local_wpm(app, w);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_farnsworth_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->trainer_farnsworth_wpm = (uint8_t)(idx + 1U);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_answer_timeout_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);

    app->trainer_answer_timeout_s = (uint8_t)(MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S + i);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_group_pause_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);

    app->trainer_group_pause_s = (uint8_t)(MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S + i);
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_straight_menu_refresh(MorseFlipperApp* app) {
    VariableItem* it;
    char txt[4];
    uint8_t idx;

    if(app == NULL) return;

    morse_flipper_clamp_straight_settings(app);

    it = app->straight_cfg_items[0];
    if(it) {
        idx = (uint8_t)(morse_flipper_straight_wpm(app) - 10U);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)(idx + 10U));
        variable_item_set_current_value_text(it, txt);
    }

    it = app->straight_cfg_items[1];
    if(it) {
        idx = (uint8_t)(app->straight_answer_timeout_s - MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->straight_answer_timeout_s);
        variable_item_set_current_value_text(it, txt);
    }

    it = app->straight_cfg_items[2];
    if(it) {
        idx = (uint8_t)(app->straight_next_delay_s - MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S);
        variable_item_set_current_value_index(it, idx);
        snprintf(txt, sizeof(txt), "%u", (unsigned)app->straight_next_delay_s);
        variable_item_set_current_value_text(it, txt);
    }
}

static void morse_flipper_straight_wpm_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t w = (uint8_t)(10U + idx);

    morse_flipper_set_straight_wpm(app, w);
    morse_flipper_straight_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_straight_timeout_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->straight_answer_timeout_s = (uint8_t)(MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S + idx);
    morse_flipper_straight_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_straight_next_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    app->straight_next_delay_s = (uint8_t)(MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S + idx);
    morse_flipper_straight_menu_refresh(app);
    morse_flipper_save_config(app);
}

static void morse_flipper_tx_groups_difficulty_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx >= MorseFlipperTxgDifficultyCount) idx = MorseFlipperTxgDifficultyCompetition;
    app->txg_difficulty = idx;
    variable_item_set_current_value_text(item, morse_flipper_txg_difficulty_name(idx));
    morse_flipper_tx_group_set_range(
        &app->tx_group,
        morse_flipper_txg_range_low(app->txg_difficulty),
        morse_flipper_txg_range_high(app->txg_difficulty));
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_group_size_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    char txt[4];

    morse_trainer_set_group_size(&app->trainer, (uint8_t)(idx + 1U));
    snprintf(txt, sizeof(txt), "%u", (unsigned)morse_trainer_group_size(&app->trainer));
    variable_item_set_current_value_text(item, txt);
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_groups_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    char tmp[4];

    morse_trainer_set_session_groups(&app->trainer, (uint8_t)(idx + 3U));
    snprintf(tmp, sizeof(tmp), "%u", (unsigned)morse_trainer_session_groups(&app->trainer));
    variable_item_set_current_value_text(item, tmp);
    morse_flipper_save_config(app);
}

void morse_flipper_trainer_chars_changed(VariableItem* item) {
    MorseFlipperApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    if(idx > morse_flipper_trainer_menu_custom_count(app)) idx = 0U;
    app->trainer.custom_set_idx = idx;
    morse_flipper_apply_trainer_charset_choice(app);
    variable_item_set_current_value_text(item, morse_flipper_trainer_menu_custom_label(app, idx));
    morse_flipper_save_config(app);
}

void morse_flipper_scene_trainer_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneTrainer);
    uint8_t gs;
    uint8_t groups;
    uint8_t custom_count;
    bool dirty = false;

    gs = morse_trainer_group_size(&app->trainer);
    groups = morse_trainer_session_groups(&app->trainer);
    if(gs > 9U) {
        morse_trainer_set_group_size(&app->trainer, 9U);
        dirty = true;
    }
    if(groups < 3U) {
        morse_trainer_set_session_groups(&app->trainer, 3U);
        dirty = true;
    } else if(groups > 30U) {
        morse_trainer_set_session_groups(&app->trainer, 30U);
        dirty = true;
    }
    if(dirty) morse_flipper_save_config(app);

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    if(app->settings_list == NULL) return;
    morse_flipper_try_custom_sets_loaded(app);
    custom_count = app->custom_sets_loaded && app->custom_sets != NULL ? app->custom_sets->count :
                                                                         0U;
    variable_item_list_reset(app->settings_list);
    memset(app->trainer_items, 0, sizeof(app->trainer_items));
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list,
        "Lesson",
        (uint8_t)morse_trainer_lesson_count(),
        morse_flipper_trainer_lesson_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingLesson] = item;

    item = variable_item_list_add(
        app->settings_list, "WPM", 21U, morse_flipper_trainer_wpm_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingWpm] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Farnsworth",
        morse_flipper_local_wpm(app),
        morse_flipper_trainer_farnsworth_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingFarnsworth] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Answer timeout",
        (uint8_t)(MORSE_FLIPPER_TRAINER_TIMEOUT_MAX_S - MORSE_FLIPPER_TRAINER_TIMEOUT_MIN_S + 1U),
        morse_flipper_trainer_answer_timeout_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingAnswerTimeout] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Group pause",
        (uint8_t)(MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MAX_S -
                  MORSE_FLIPPER_TRAINER_GROUP_PAUSE_MIN_S + 1U),
        morse_flipper_trainer_group_pause_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingGroupPause] = item;

    item = variable_item_list_add(
        app->settings_list, "Group size", 9U, morse_flipper_trainer_group_size_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingGroupSize] = item;

    item = variable_item_list_add(
        app->settings_list, "Groups", 28U, morse_flipper_trainer_groups_changed, app);
    app->trainer_items[MorseFlipperTrainerSettingGroups] = item;

    item = variable_item_list_add(
        app->settings_list,
        "Chars",
        (uint8_t)(custom_count + 1U),
        morse_flipper_trainer_chars_changed,
        app);
    app->trainer_items[MorseFlipperTrainerSettingChars] = item;

    if((sel & 0xffU) > MorseFlipperTrainerSettingChars) sel = 0U;
    morse_flipper_trainer_menu_refresh(app);
    morse_flipper_settings_list_restore(app->settings_list, sel);
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTrainer);
}

void morse_flipper_scene_trainer_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneTrainer,
        morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
    memset(app->trainer_items, 0, sizeof(app->trainer_items));
    morse_flipper_unload_custom_sets(app);
}

void morse_flipper_scene_straight_cfg_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint32_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneStraightCfg);

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    variable_item_list_reset(app->settings_list);
    memset(app->straight_cfg_items, 0, sizeof(app->straight_cfg_items));
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list, "WPM", 21U, morse_flipper_straight_wpm_changed, app);
    app->straight_cfg_items[0] = item;
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "10");

    item = variable_item_list_add(
        app->settings_list,
        "Answer timeout",
        (uint8_t)(MORSE_FLIPPER_STRAIGHT_TIMEOUT_MAX_S - MORSE_FLIPPER_STRAIGHT_TIMEOUT_MIN_S +
                  1U),
        morse_flipper_straight_timeout_changed,
        app);
    app->straight_cfg_items[1] = item;
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "3");

    item = variable_item_list_add(
        app->settings_list,
        "Next delay",
        (uint8_t)(MORSE_FLIPPER_STRAIGHT_NEXT_MAX_S - MORSE_FLIPPER_STRAIGHT_NEXT_MIN_S + 1U),
        morse_flipper_straight_next_changed,
        app);
    app->straight_cfg_items[2] = item;
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "3");

    morse_flipper_straight_menu_refresh(app);
    if((sel & 0xffU) > 2U) sel = 0U;
    morse_flipper_settings_list_restore(app->settings_list, sel);
    morse_flipper_scene_enter_now(app, MorseFlipperSceneStraightCfg);
}

void morse_flipper_scene_straight_cfg_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneStraightCfg,
        morse_flipper_settings_list_state(app->settings_list));
    variable_item_list_reset(app->settings_list);
}

void morse_flipper_scene_tx_groups_cfg_on_enter(void* context) {
    MorseFlipperApp* app = context;
    VariableItem* item;
    uint8_t sel = scene_manager_get_scene_state(app->scene_manager, MorseFlipperSceneTxGroupsCfg);

    if(app->txg_difficulty >= MorseFlipperTxgDifficultyCount)
        app->txg_difficulty = MorseFlipperTxgDifficultyCompetition;

    morse_flipper_ensure_view(app, MorseFlipperViewSettings);
    variable_item_list_reset(app->settings_list);
    variable_item_list_set_enter_callback(
        app->settings_list, morse_flipper_settings_noop_enter, app);

    item = variable_item_list_add(
        app->settings_list,
        "Difficulty",
        MorseFlipperTxgDifficultyCount,
        morse_flipper_tx_groups_difficulty_changed,
        app);
    variable_item_set_current_value_index(item, app->txg_difficulty);
    variable_item_set_current_value_text(
        item, morse_flipper_txg_difficulty_name(app->txg_difficulty));

    if(sel > 0U) sel = 0U;
    variable_item_list_set_selected_item(app->settings_list, sel);
    morse_flipper_scene_enter_now(app, MorseFlipperSceneTxGroupsCfg);
}

bool morse_flipper_scene_tx_groups_cfg_on_event(void* context, SceneManagerEvent event) {
    MorseFlipperApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        morse_flipper_scene_back(app);
        return true;
    }

    return false;
}

void morse_flipper_scene_tx_groups_cfg_on_exit(void* context) {
    MorseFlipperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        MorseFlipperSceneTxGroupsCfg,
        variable_item_list_get_selected_item_index(app->settings_list));
    variable_item_list_reset(app->settings_list);
}
