#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/variable_item_list.h>

#include <subghz_bruteforcer_icons.h>

#include <assets_icons.h>

#include <dialogs/dialogs.h>

#include "subbrute.h"
#include "subbrute_device.h"
#include "subbrute_settings.h"
#include "helpers/subbrute_worker.h"
#include "views/subbrute_attack_view.h"
#include "views/subbrute_main_view.h"

#define SUB_BRUTE_FORCER_VERSION "Sub-GHz BruteForcer 4.3"

/**
 * @enum SubBruteView
 * @brief An enumeration representing the different views of the SubBrute application
 *
 * This enumeration represents the possible views that the SubBrute application can have. Each view
 * corresponds to a specific screen or UI element in the application.
 */
typedef enum {
    SubBruteViewNone, /**< Not used */
    SubBruteViewMain,
    SubBruteViewAttack,
    SubBruteViewTextInput,
    SubBruteViewPopup,
    SubBruteViewVarList,
} SubBruteView;

/**
 * @class SubBruteState
 * @brief Represents the state of a SubBrute application.
 *
 * This class contains the various elements and variables necessary for the functioning of a SubBrute application.
 */
struct SubBruteState {
    /** GUI elements */
    NotificationApp* notifications;
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    TextInput* text_input;
    Popup* popup;
    VariableItemList* var_list;
    DialogsApp* dialogs;

    char text_store[SUBBRUTE_MAX_LEN_NAME]; /**< Text store */
    FuriString* file_path;

    const SubGhzDevice* radio_device; /**< Radio device */

    /** Views */
    SubBruteMainView* view_main; /**< Main menu */
    SubBruteAttackView* view_attack; /**< View for attack and setup current protocol */
    SubBruteView current_view;

    SceneManager* scene_manager; /**< Scene manager */

    SubBruteDevice* device; /**< SubBruteDevice to get value of transmission */
    SubBruteWorker* worker; /**< SubBruteWorker worker for background job */
    SubBruteSettings* settings; /**< Last used settings */
};

/**
 * @brief Callback function for text input in the sub-brute module.
 *
 * This function serves as a callback for handling text input in the sub-brute module.
 * It should be registered as a callback function for text input events.
 *
 * @param context A pointer to additional context data (if any).
 *
 * @return None
 */
void subbrute_text_input_callback(void* context);

/**
 * @brief Callback function called when a popup window is closed.
 *
 * This function is called when a popup window is closed. It can be used to perform
 * any necessary cleanup or additional processing after the popup window is closed.
 *
 * @param context A void pointer to the context data associated with the popup window.
 *                It can be used to pass any additional information needed by the callback function.
 *
 * @return void
 */
void subbrute_popup_closed_callback(void* context);

/**
 * @brief Apply saved per-protocol extra settings (time delay, TE, PT2262 code) to worker.
 *
 * Must be called after worker was initialised for current attack.
 *
 * @param instance SubBruteState instance
 */
void subbrute_apply_extra_settings(SubBruteState* instance);

/**
 * @brief Store current worker extra settings for current attack and save settings file.
 *
 * @param instance SubBruteState instance
 */
void subbrute_store_extra_settings(SubBruteState* instance);
