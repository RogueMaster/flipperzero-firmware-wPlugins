#include <inttypes.h>

#include "minesweeper.h"
#include "scenes/minesweeper_scene.h"
#include "helpers/mine_sweeper_storage.h"
#include "helpers/mine_sweeper_config.h"

typedef enum {
    MineSweeperSettingsScreenDifficultyTypeEasy,
    MineSweeperSettingsScreenDifficultyTypeMedium,
    MineSweeperSettingsScreenDifficultyTypeHard,
    MineSweeperSettingsScreenDifficultyTypeNum,
} MineSweeperSettingsScreenDifficultyType;

typedef enum {
    MineSweeperSettingsScreenEventDifficultyChange,
    MineSweeperSettingsScreenEventWidthChange,
    MineSweeperSettingsScreenEventHeightChange,
    MineSweeperSettingsScreenEventSolvableChange,
    MineSweeperSettingsScreenEventInfoChange,
    MineSweeperSettingsScreenEventFeedbackChange,
    MineSweeperSettingsScreenEventWrapChange,
} MineSweeperSettingsScreenEvent;

static const char* settings_screen_difficulty_text[MineSweeperSettingsScreenDifficultyTypeNum] = {
    "Easy",
    "Medium",
    "Hard",
};

static const char* settings_screen_verifier_text[2] = {
    "False",
    "True",
};

static void minesweeper_scene_settings_screen_set_difficulty(VariableItem* item) {
    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);
    app->settings_draft.difficulty_item = item;

    uint8_t index = variable_item_get_current_value_index(app->settings_draft.difficulty_item);

    app->settings_draft.difficulty = index;

    variable_item_set_current_value_text(
        app->settings_draft.difficulty_item, settings_screen_difficulty_text[index]);

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventDifficultyChange);
}

static void minesweeper_scene_settings_screen_set_width(VariableItem* item) {
    char source[5];
    uint8_t index = 0;

    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);
    app->settings_draft.width_item = item;

    index = variable_item_get_current_value_index(app->settings_draft.width_item);
    app->settings_draft.board_width = index + 16;

    snprintf(source, 5, "%" PRIu8, index + 16);
    source[4] = '\0';

    furi_string_set_strn(app->settings_draft.width_str, source, 5);

    variable_item_set_current_value_text(
        app->settings_draft.width_item, furi_string_get_cstr(app->settings_draft.width_str));

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventWidthChange);
}

static void minesweeper_scene_settings_screen_set_height(VariableItem* item) {
    char source[5];
    uint8_t index = 0;

    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);
    app->settings_draft.height_item = item;

    index = variable_item_get_current_value_index(app->settings_draft.height_item);
    app->settings_draft.board_height = index + 7;

    snprintf(source, 5, "%" PRIu8, index + 7);
    source[4] = '\0';

    furi_string_set_strn(app->settings_draft.height_str, source, 5);

    variable_item_set_current_value_text(
        app->settings_draft.height_item, furi_string_get_cstr(app->settings_draft.height_str));

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventHeightChange);
}

static void minesweeper_scene_settings_screen_set_solvable(VariableItem* item) {
    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);

    uint8_t index = variable_item_get_current_value_index(app->settings_draft.solvable_item);

    app->settings_draft.ensure_solvable_board = (index == 1) ? true : false;

    variable_item_set_current_value_text(item, settings_screen_verifier_text[index]);

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventSolvableChange);
}

static void minesweeper_scene_settings_screen_set_feedback(VariableItem* item) {
    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);

    uint8_t value = variable_item_get_current_value_index(item);

    app->feedback_enabled = value;

    variable_item_set_current_value_text(item, ((value) ? "Enabled" : "Disabled"));

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventFeedbackChange);
}

static void minesweeper_scene_settings_screen_set_wrap(VariableItem* item) {
    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);

    uint8_t value = variable_item_get_current_value_index(item);

    app->wrap_enabled = value;

    variable_item_set_current_value_text(item, ((value) ? "Enabled" : "Disabled"));

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventWrapChange);
}

static void minesweeper_scene_settings_screen_set_info(VariableItem* item) {
    furi_assert(item);

    MineSweeperApp* app = variable_item_get_context(item);

    view_dispatcher_send_custom_event(
        app->view_dispatcher, MineSweeperSettingsScreenEventInfoChange);
}

void minesweeper_scene_settings_screen_on_enter(void* context) {
    furi_assert(context);

    MineSweeperApp* app = (MineSweeperApp*)context;
    VariableItemList* va = app->settings_screen;
    VariableItem* item;

    // If we are accessing the scene and have not changed the settings
    if(!app->is_settings_changed) {
        // Set temp setting buffer to current state
        app->settings_draft = app->settings_committed;
    }

    // Set Difficulty Item
    item = variable_item_list_add(
        va,
        "Difficulty",
        MineSweeperSettingsScreenDifficultyTypeNum,
        minesweeper_scene_settings_screen_set_difficulty,
        app);

    app->settings_draft.difficulty_item = item;

    variable_item_set_current_value_index(item, app->settings_draft.difficulty);

    variable_item_set_current_value_text(
        item, settings_screen_difficulty_text[app->settings_draft.difficulty]);

    // Set Width Item
    item = variable_item_list_add(
        va, "Board Width", 33 - 16, minesweeper_scene_settings_screen_set_width, app);

    app->settings_draft.width_item = item;

    variable_item_set_current_value_index(item, app->settings_draft.board_width - 16);

    char source[5];
    snprintf(source, 5, "%" PRIu8, app->settings_draft.board_width);
    source[4] = '\0';
    furi_string_set_strn(app->settings_draft.width_str, source, 5);

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->settings_draft.width_str));

    // Set Height Item
    item = variable_item_list_add(
        va, "Board Height", 33 - 7, minesweeper_scene_settings_screen_set_height, app);

    app->settings_draft.height_item = item;

    variable_item_set_current_value_index(item, app->settings_draft.board_height - 7);

    snprintf(source, 5, "%" PRIu8, app->settings_draft.board_height);
    source[4] = '\0';
    furi_string_set_strn(app->settings_draft.height_str, source, 5);

    variable_item_set_current_value_text(
        item, furi_string_get_cstr(app->settings_draft.height_str));

    // Set solvable item
    item = variable_item_list_add(
        va, "Ensure Solvable", 2, minesweeper_scene_settings_screen_set_solvable, app);

    app->settings_draft.solvable_item = item;

    uint8_t idx = (app->settings_draft.ensure_solvable_board) ? 1 : 0;

    variable_item_set_current_value_index(item, idx);

    variable_item_set_current_value_text(item, settings_screen_verifier_text[idx]);

    // Set feedback item
    item = variable_item_list_add(
        va, "Feedback", 2, minesweeper_scene_settings_screen_set_feedback, app);

    variable_item_set_current_value_index(item, app->feedback_enabled);

    variable_item_set_current_value_text(item, ((app->feedback_enabled) ? "Enabled" : "Disabled"));

    // Set wrap item
    item = variable_item_list_add(va, "Wrap", 2, minesweeper_scene_settings_screen_set_wrap, app);

    variable_item_set_current_value_index(item, app->wrap_enabled);

    variable_item_set_current_value_text(item, ((app->wrap_enabled) ? "Enabled" : "Disabled"));

    // Set info item
    item = variable_item_list_add(
        va, "Right For Info", 2, minesweeper_scene_settings_screen_set_info, app);

    variable_item_set_current_value_index(item, 0);

    variable_item_set_current_value_text(item, "-------");

    view_dispatcher_switch_to_view(app->view_dispatcher, MineSweeperSettingsView);
}

bool minesweeper_scene_settings_screen_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);

    MineSweeperApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Only these fields require reset confirmation; wrap/feedback are immediate-save.
        app->is_settings_changed =
            (app->settings_committed.board_width != app->settings_draft.board_width ||
             app->settings_committed.board_height != app->settings_draft.board_height ||
             app->settings_committed.difficulty != app->settings_draft.difficulty ||
             app->settings_committed.ensure_solvable_board !=
                 app->settings_draft.ensure_solvable_board);

        switch(event.event) {
        case MineSweeperSettingsScreenEventDifficultyChange:

            break;

        case MineSweeperSettingsScreenEventWidthChange:

            break;

        case MineSweeperSettingsScreenEventHeightChange:

            break;

        case MineSweeperSettingsScreenEventSolvableChange:

            break;

        case MineSweeperSettingsScreenEventInfoChange:

            scene_manager_next_scene(app->scene_manager, MineSweeperSceneInfoScreen);
            break;

        case MineSweeperSettingsScreenEventWrapChange:
            mine_sweeper_save_settings(app);
            {
                MineSweeperConfig config = app->game_state.config;
                config.wrap_enabled = app->wrap_enabled;
                if(minesweeper_engine_set_config(&app->game_state, &config) ==
                   MineSweeperResultInvalid) {
                    FURI_LOG_E(TAG, "Failed to apply wrap setting to engine");
                }
            }
            break;

        case MineSweeperSettingsScreenEventFeedbackChange:
            mine_sweeper_save_settings(app);
            break;

        default:
            break;
        };
        consumed = true;

    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->is_settings_changed) {
            // If there are changes in the width, height, or difficulty go to confirmation screen for restart

            scene_manager_next_scene(app->scene_manager, MineSweeperSceneConfirmationScreen);
        } else {
            // Otherwise just go back

            memset(&app->settings_draft, 0, sizeof(app->settings_draft));

            if(!scene_manager_search_and_switch_to_previous_scene(
                   app->scene_manager, MineSweeperSceneMenuScreen)) {
                FURI_LOG_W(TAG, "Settings back target not found, stopping app");

                scene_manager_stop(app->scene_manager);
                view_dispatcher_stop(app->view_dispatcher);
            }
        }

        consumed = true;
    }

    return consumed;
}

void minesweeper_scene_settings_screen_on_exit(void* context) {
    furi_assert(context);
    MineSweeperApp* app = (MineSweeperApp*)context;
    VariableItemList* va = app->settings_screen;

    variable_item_list_reset(va);
}
