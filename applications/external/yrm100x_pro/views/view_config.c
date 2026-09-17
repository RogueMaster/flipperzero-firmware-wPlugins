#include "view_config.h"
#include "../helpers/uhf_reader_settings.h"

typedef enum {
    UHFConfigItemConnection = 0,
    UHFConfigItemAboutAntenna,
    UHFConfigItemPower,
    UHFConfigItemBaud,
    UHFConfigItemRegion,
    UHFConfigItemDefaultAp,
    UHFConfigItemSaveOnWrite,
    UHFConfigItemSession,
    UHFConfigItemTarget,
    UHFConfigItemFullAfterStop,
    UHFConfigItemAutoSaveMulti,
    UHFConfigItemTagProfile,
    UHFConfigItemFullAttempts,
    UHFConfigItemCloneAttempts,
    UHFConfigItemForeverDelay,
    UHFConfigItemSound,
    UHFConfigItemVibration,
    UHFConfigItemCount,
} UHFConfigItemIndex;

_Static_assert(UHFConfigItemCount == 17U, "Update Configure preallocation");

#define UHF_READER_MANUAL_CONNECT_ATTEMPTS  3U
#define UHF_READER_STARTUP_CONNECT_ATTEMPTS 2U
#define UHF_READER_STARTUP_RETRY_DELAY_MS   1000U

static bool uhf_reader_silent_connect = false;
static uint8_t uhf_reader_connect_attempt_limit = UHF_READER_MANUAL_CONNECT_ATTEMPTS;

static void uhf_reader_settings_snapshot(const UHFReaderApp* App, UHFReaderSettings* settings) {
    settings->reader_profile_initialized = App->SettingsInitialized;
    settings->save_on_write_index = App->SettingSavingIndex;
    settings->region_index = App->SettingRegionIndex;
    settings->power_dbm = App->SettingPowerIndex;
    settings->session_index = App->SettingSessionIndex;
    settings->target_index = App->SettingTargetIndex;
    settings->default_access_password = bytes_to_uint32(App->ApTempBuffer, 4);
    settings->multi_full_dump_index = App->SettingMultiFullIndex;
    settings->auto_save_multi_index = App->SettingAutoSaveMultiIndex;
    settings->full_dump_attempts = App->SettingFullDumpAttempts;
    settings->clone_attempts = App->SettingCloneAttempts;
    settings->forever_delay_seconds = App->SettingForeverDelaySeconds;
    settings->sound_enabled = App->SettingSoundIndex != 0U;
    settings->vibration_enabled = App->SettingVibrationIndex != 0U;
    settings->tag_profile_index = App->SettingTagProfileIndex;
}

static bool uhf_reader_settings_save_current(UHFReaderApp* App) {
    UHFReaderSettings settings;
    uhf_reader_settings_snapshot(App, &settings);
    if(!uhf_reader_settings_save(App->TagStorage, &settings)) {
        FURI_LOG_E(TAG, "Failed to save reader settings");
        return false;
    }
    return true;
}

static uint8_t uhf_reader_region_index(WorkingRegion region) {
    switch(region) {
    case WR_US:
        return USA_REGION;
    case WR_EU:
        return EU_REGION;
    case WR_KOREA:
        return KOREA_REGION;
    case WR_CHINA_800:
        return CHINA_800_REGION;
    case WR_CHINA_900:
        return CHINA_900_REGION;
    default:
        return USA_REGION;
    }
}

static bool uhf_reader_settings_adopt_yrm100x(UHFReaderApp* App) {
    WorkingRegion region;
    uint16_t power_raw = 0;
    uint8_t session = 0;
    uint8_t target = 0;
    if(!m100_get_working_region(App->YRM100XWorker->module, &region) ||
       !m100_get_transmitting_power(App->YRM100XWorker->module, &power_raw) ||
       !m100_get_query_params(App->YRM100XWorker->module, &session, &target) ||
       !m100_set_select_mode(App->YRM100XWorker->module, 0x02)) {
        return false;
    }

    uint32_t power_dbm = (power_raw + 50U) / 100U;
    if(power_dbm < UHF_READER_MIN_POWER_DBM || power_dbm > UHF_READER_MAX_POWER_DBM) {
        return false;
    }
    App->SettingRegionIndex = uhf_reader_region_index(region);
    App->UHFRegionType = App->SettingRegionIndex;
    App->SettingPowerIndex = (uint8_t)power_dbm;
    App->SettingSessionIndex = session;
    App->SettingTargetIndex = target;
    App->SettingsInitialized = true;
    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);
    if(!uhf_reader_settings_save_current(App)) {
        App->SettingsInitialized = false;
        return false;
    }
    return true;
}

static bool uhf_reader_settings_apply_yrm100x(UHFReaderApp* App) {
    WorkingRegion expected_region = WORKING_REGIONS[App->SettingRegionIndex];
    WorkingRegion actual_region;
    uint16_t expected_power = (uint16_t)App->SettingPowerIndex * 100U;
    uint16_t actual_power = 0;
    uint8_t actual_session = 0;
    uint8_t actual_target = 0;

    if(!m100_set_working_region(App->YRM100XWorker->module, expected_region) ||
       !m100_get_working_region(App->YRM100XWorker->module, &actual_region) ||
       actual_region != expected_region ||
       !m100_set_transmitting_power(App->YRM100XWorker->module, expected_power) ||
       !m100_get_transmitting_power(App->YRM100XWorker->module, &actual_power) ||
       actual_power != expected_power ||
       !m100_set_query_params(
           App->YRM100XWorker->module, App->SettingSessionIndex, App->SettingTargetIndex) ||
       !m100_get_query_params(App->YRM100XWorker->module, &actual_session, &actual_target) ||
       actual_session != App->SettingSessionIndex || actual_target != App->SettingTargetIndex ||
       !m100_set_select_mode(App->YRM100XWorker->module, 0x02)) {
        return false;
    }

    App->UHFRegionType = App->SettingRegionIndex;
    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);
    return true;
}

/**
 * @brief      Callback for returning to the configuration screen.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
*/
static void locked_popup_back_callback(void* context) {
    UHFReaderApp* app = context;
    view_dispatcher_switch_to_view(app->ViewDispatcher, UHFReaderViewConfigure);
}

/**
 * @brief      Shows a notification when the reader is not connected
 * @details    This function shows a notification when the reader is not connected and the user tries to change a setting.
 * @param      App  The UHFReaderApp - used to allocate app variables and views.
*/
static void show_locked_notification(UHFReaderApp* App) {
    uhf_reader_show_antenna_not_connected(App, UHFReaderViewConfigure);
}

static void show_command_error_notification(UHFReaderApp* App) {
    uhf_notify_error(App);

    uhf_reader_ensure_lock_view(App);

    Popup* popup = App->LockPopup;
    popup_reset(popup);
    popup_set_header(popup, "Command\nFailed!", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_WarningDolphin_45x42);

    popup_enable_timeout(popup);
    popup_set_timeout(popup, 2000);
    popup_set_context(popup, App);
    popup_set_callback(popup, locked_popup_back_callback);

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewLockPopup);
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t uhf_reader_navigation_config_submenu_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

/**
 * @brief      Allocates the configuration menu
 * @details    This function allocates all views and variables related to the configuration menu.
 * @param      app  The UHFReaderApp - used to allocate app variables and views.
*/
void view_config_alloc(UHFReaderApp* App) {
    ap_menu_alloc(App);

    UHFReaderSettings settings;
    uhf_reader_settings_set_defaults(&settings);
    uhf_reader_settings_load(App->TagStorage, &settings);

    App->SettingsInitialized = settings.reader_profile_initialized;
    App->ApTempBuffer[0] = (uint8_t)(settings.default_access_password >> 24);
    App->ApTempBuffer[1] = (uint8_t)(settings.default_access_password >> 16);
    App->ApTempBuffer[2] = (uint8_t)(settings.default_access_password >> 8);
    App->ApTempBuffer[3] = (uint8_t)settings.default_access_password;

    App->VariableItemListConfig = variable_item_list_alloc();
    variable_item_list_reset(App->VariableItemListConfig);

    /*
     * Firmware initially reserves 16 VariableItem slots. Grow the backing
     * array before retaining pointers so adding later rows cannot invalidate
     * Connection and the other stored item pointers.
     */
    for(uint8_t i = 0U; i < UHFConfigItemCount; i++) {
        variable_item_list_add(App->VariableItemListConfig, "", 1U, NULL, NULL);
    }
    variable_item_list_reset(App->VariableItemListConfig);

    App->Setting1Values[0] = 1;
    App->Setting1Values[1] = 2;
    App->Setting1Names[0] = "Disconnect";
    App->Setting1Names[1] = "Connect";
    App->ReaderConnected = false;
    App->Setting1ConfigLabel = "Connection";

    App->Setting2ConfigLabel = "Power Level";

    App->SettingBaudValues[0] = 1;
    App->SettingBaudValues[1] = 2;
    App->SettingBaudValues[2] = 3;
    App->SettingBaudNames[0] = "9600";
    App->SettingBaudNames[1] = "115200";
    App->SettingBaudNames[2] = "384000";
    App->SettingBaudConfigLabel = "Baud Rate";
    App->UHFBaudRate = 115200;

    App->UHFModuleType = YRM100X_MODULE;

    App->SettingSavingValues[0] = 1;
    App->SettingSavingValues[1] = 2;
    App->SettingSavingNames[0] = "No";
    App->SettingSavingNames[1] = "Yes";
    App->SettingSavingConfigLabel = "Save on Write";
    App->SettingSavingIndex = settings.save_on_write_index;
    App->UHFSaveType = App->SettingSavingIndex == 1 ? YES_SAVE_ON_WRITE : NO_SAVE_ON_WRITE;

    App->SettingMultiFullNames[0] = "No";
    App->SettingMultiFullNames[1] = "Yes";
    App->SettingMultiFullConfigLabel = "Full After Stop";
    App->SettingMultiFullIndex = settings.multi_full_dump_index;

    App->SettingAutoSaveMultiNames[0] = "No";
    App->SettingAutoSaveMultiNames[1] = "Yes";
    App->SettingAutoSaveMultiConfigLabel = "Auto Save Multi";
    App->SettingAutoSaveMultiIndex = settings.auto_save_multi_index;

    App->SettingTagProfileNames[UHF_READER_TAG_PROFILE_GENERIC] = "Generic";
    App->SettingTagProfileNames[UHF_READER_TAG_PROFILE_MONZA4QT] = "imp. 4QT";
    App->SettingTagProfileConfigLabel = "Tag Profile";
    App->SettingTagProfileIndex = settings.tag_profile_index;

    App->SettingFullDumpAttemptsConfigLabel = "Full Attempts";
    App->SettingFullDumpAttempts = settings.full_dump_attempts;

    App->SettingCloneAttemptsConfigLabel = "Clone Attempts";
    App->SettingCloneAttempts = settings.clone_attempts;

    App->SettingForeverDelayConfigLabel = "Forever Delay";
    App->SettingForeverDelaySeconds = settings.forever_delay_seconds;

    App->SettingSoundNames[0] = "Off";
    App->SettingSoundNames[1] = "On";
    App->SettingSoundConfigLabel = "Sound";
    App->SettingSoundIndex = settings.sound_enabled ? 1U : 0U;

    App->SettingVibrationNames[0] = "Off";
    App->SettingVibrationNames[1] = "On";
    App->SettingVibrationConfigLabel = "Vibration";
    App->SettingVibrationIndex = settings.vibration_enabled ? 1U : 0U;

    App->SettingRegionValues[0] = 1;
    App->SettingRegionValues[1] = 2;
    App->SettingRegionValues[2] = 3;
    App->SettingRegionValues[3] = 4;
    App->SettingRegionValues[4] = 5;
    App->SettingRegionNames[0] = "USA";
    App->SettingRegionNames[1] = "EU";
    App->SettingRegionNames[2] = "Korea";
    App->SettingRegionNames[3] = "China 800";
    App->SettingRegionNames[4] = "China 900";
    App->SettingRegionConfigLabel = "Region";
    App->SettingRegionIndex = settings.region_index;
    App->UHFRegionType = App->SettingRegionIndex;

    App->SettingSessionNames[0] = "S0";
    App->SettingSessionNames[1] = "S1";
    App->SettingSessionNames[2] = "S2";
    App->SettingSessionNames[3] = "S3";
    App->SettingSessionConfigLabel = "Session";
    App->SettingSessionIndex = settings.session_index;

    App->SettingTargetNames[0] = "A";
    App->SettingTargetNames[1] = "B";
    App->SettingTargetConfigLabel = "Target";
    App->SettingTargetIndex = settings.target_index;

    App->ReadAccessPasswordLabel = strdup("Default AP");
    App->AccessPasswordPlaceHolder = strdup("Enter Access Password!");
    App->DefaultAccessPassword = malloc(9);
    snprintf(
        App->DefaultAccessPassword, 9, "%08lX", (unsigned long)settings.default_access_password);
    App->DefaultAccessPwdStr = furi_string_alloc_set(App->DefaultAccessPassword);

    /*
     * Lockable YRM100X settings:
     * 0 Power, 1 Baud, 2 Region, 3 Default AP, 4 Session, 5 Target.
     */
    App->num_items = 6;
    App->item_locks = malloc(sizeof(VariableItemLock) * App->num_items);
    for(size_t i = 0; i < App->num_items; i++) {
        App->item_locks[i].locked = true;
    }

    // 0 Connection
    App->ConnectionItem = variable_item_list_add(
        App->VariableItemListConfig,
        App->Setting1ConfigLabel,
        COUNT_OF(App->Setting1Values),
        uhf_reader_setting_1_change,
        App);
    App->Setting1Index = 0;
    variable_item_set_current_value_index(App->ConnectionItem, 0);
    variable_item_set_current_value_text(App->ConnectionItem, App->Setting1Names[0]);

    // 1 Reader identification command screen
    VariableItem* about_antenna_item =
        variable_item_list_add(App->VariableItemListConfig, "About Antenna", 1, NULL, NULL);
    variable_item_set_current_value_text(about_antenna_item, "Open");

    // 2 Power
    App->SettingPowerIndex = settings.power_dbm;
    App->Setting2Item = variable_item_list_add(
        App->VariableItemListConfig,
        App->Setting2ConfigLabel,
        27,
        uhf_reader_power_setting_change,
        App);
    variable_item_set_current_value_index(App->Setting2Item, App->SettingPowerIndex);
    variable_item_set_current_value_text(App->Setting2Item, "LOCKED");

    // 3 Baud
    App->BaudSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingBaudConfigLabel,
        COUNT_OF(App->SettingBaudValues),
        uhf_reader_baud_setting_change,
        App);
    App->SettingBaudIndex = 1;
    variable_item_set_current_value_index(App->BaudSelection, App->SettingBaudIndex);
    variable_item_set_current_value_text(App->BaudSelection, "LOCKED");

    // 4 Region
    App->RegionSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingRegionConfigLabel,
        COUNT_OF(App->SettingRegionValues),
        uhf_reader_region_setting_change,
        App);
    variable_item_set_current_value_index(App->RegionSelection, App->SettingRegionIndex);
    variable_item_set_current_value_text(App->RegionSelection, "LOCKED");

    // 5 Default Access Password
    App->SettingApPwdItem = variable_item_list_add(
        App->VariableItemListConfig, App->ReadAccessPasswordLabel, 1, NULL, NULL);
    variable_item_set_current_value_text(App->SettingApPwdItem, "LOCKED");

    // 6 Save on Write
    App->SavingSelectionItem = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingSavingConfigLabel,
        COUNT_OF(App->SettingSavingValues),
        uhf_reader_save_setting_change,
        App);
    variable_item_set_current_value_index(App->SavingSelectionItem, App->SettingSavingIndex);
    variable_item_set_current_value_text(
        App->SavingSelectionItem, App->SettingSavingNames[App->SettingSavingIndex]);

    // 7 Session
    App->SessionSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingSessionConfigLabel,
        COUNT_OF(App->SettingSessionNames),
        uhf_reader_session_setting_change,
        App);
    variable_item_set_current_value_index(App->SessionSelection, App->SettingSessionIndex);
    variable_item_set_current_value_text(App->SessionSelection, "LOCKED");

    // 8 Target
    App->TargetSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingTargetConfigLabel,
        COUNT_OF(App->SettingTargetNames),
        uhf_reader_target_setting_change,
        App);
    variable_item_set_current_value_index(App->TargetSelection, App->SettingTargetIndex);
    variable_item_set_current_value_text(App->TargetSelection, "LOCKED");

    // 9 Optional full acquisition after stopping normal fast Multi
    App->MultiFullDumpSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingMultiFullConfigLabel,
        2,
        uhf_reader_multi_full_setting_change,
        App);
    variable_item_set_current_value_index(App->MultiFullDumpSelection, App->SettingMultiFullIndex);
    variable_item_set_current_value_text(
        App->MultiFullDumpSelection, App->SettingMultiFullNames[App->SettingMultiFullIndex]);

    // 10 Automatic save after normal Multi.
    App->AutoSaveMultiSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingAutoSaveMultiConfigLabel,
        2,
        uhf_reader_auto_save_multi_setting_change,
        App);
    variable_item_set_current_value_index(
        App->AutoSaveMultiSelection, App->SettingAutoSaveMultiIndex);
    variable_item_set_current_value_text(
        App->AutoSaveMultiSelection,
        App->SettingAutoSaveMultiNames[App->SettingAutoSaveMultiIndex]);

    // 11 Software tag-memory interpretation profile.
    App->TagProfileSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingTagProfileConfigLabel,
        UHF_READER_TAG_PROFILE_COUNT,
        uhf_reader_tag_profile_setting_change,
        App);
    variable_item_set_current_value_index(App->TagProfileSelection, App->SettingTagProfileIndex);
    variable_item_set_current_value_text(
        App->TagProfileSelection, App->SettingTagProfileNames[App->SettingTagProfileIndex]);

    // 12 Maximum whole-card acquisition passes in Read Full(Multi).
    App->FullDumpAttemptsSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingFullDumpAttemptsConfigLabel,
        UHF_READER_FULL_DUMP_ATTEMPTS_MAX,
        uhf_reader_full_dump_attempts_setting_change,
        App);
    variable_item_set_current_value_index(
        App->FullDumpAttemptsSelection,
        App->SettingFullDumpAttempts - UHF_READER_FULL_DUMP_ATTEMPTS_MIN);

    char attempts_label[4];
    snprintf(
        attempts_label, sizeof(attempts_label), "%u", (unsigned int)App->SettingFullDumpAttempts);
    variable_item_set_current_value_text(App->FullDumpAttemptsSelection, attempts_label);

    // 13 Attempts for each retryable Clone read/write stage.
    App->CloneAttemptsSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingCloneAttemptsConfigLabel,
        UHF_READER_CLONE_ATTEMPTS_MAX - UHF_READER_CLONE_ATTEMPTS_MIN + 1U,
        uhf_reader_clone_attempts_setting_change,
        App);
    variable_item_set_current_value_index(
        App->CloneAttemptsSelection, App->SettingCloneAttempts - UHF_READER_CLONE_ATTEMPTS_MIN);

    char clone_attempts_label[4];
    snprintf(
        clone_attempts_label,
        sizeof(clone_attempts_label),
        "%u",
        (unsigned int)App->SettingCloneAttempts);
    variable_item_set_current_value_text(App->CloneAttemptsSelection, clone_attempts_label);

    // 14 Delay after each saved Read Forever capture.
    App->ForeverDelaySelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingForeverDelayConfigLabel,
        UHF_READER_FOREVER_DELAY_MAX_SEC,
        uhf_reader_forever_delay_setting_change,
        App);
    variable_item_set_current_value_index(
        App->ForeverDelaySelection,
        App->SettingForeverDelaySeconds - UHF_READER_FOREVER_DELAY_MIN_SEC);

    char forever_delay_label[6];
    snprintf(
        forever_delay_label,
        sizeof(forever_delay_label),
        "%us",
        (unsigned int)App->SettingForeverDelaySeconds);
    variable_item_set_current_value_text(App->ForeverDelaySelection, forever_delay_label);

    // 15 Application sound.
    App->SoundSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingSoundConfigLabel,
        2,
        uhf_reader_sound_setting_change,
        App);
    variable_item_set_current_value_index(App->SoundSelection, App->SettingSoundIndex);
    variable_item_set_current_value_text(
        App->SoundSelection, App->SettingSoundNames[App->SettingSoundIndex]);

    // 16 Application vibration.
    App->VibrationSelection = variable_item_list_add(
        App->VariableItemListConfig,
        App->SettingVibrationConfigLabel,
        2,
        uhf_reader_vibration_setting_change,
        App);
    variable_item_set_current_value_index(App->VibrationSelection, App->SettingVibrationIndex);
    variable_item_set_current_value_text(
        App->VibrationSelection, App->SettingVibrationNames[App->SettingVibrationIndex]);

    variable_item_list_set_enter_callback(
        App->VariableItemListConfig, uhf_reader_setting_item_clicked, App);

    App->DeleteAllDialog = dialog_ex_alloc();
    dialog_ex_set_header(
        App->DeleteAllDialog, "Delete ALL saved?", 64, 12, AlignCenter, AlignCenter);
    dialog_ex_set_text(
        App->DeleteAllDialog,
        "Saved list + auto dumps\nwill be erased.",
        64,
        31,
        AlignCenter,
        AlignCenter);
    dialog_ex_set_left_button_text(App->DeleteAllDialog, "Cancel");
    dialog_ex_set_right_button_text(App->DeleteAllDialog, "Delete");
    dialog_ex_set_context(App->DeleteAllDialog, App);
    dialog_ex_set_result_callback(App->DeleteAllDialog, uhf_reader_delete_all_confirm_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher,
        UHFReaderViewDeleteAllConfirm,
        dialog_ex_get_view(App->DeleteAllDialog));

    view_set_previous_callback(
        variable_item_list_get_view(App->VariableItemListConfig),
        uhf_reader_navigation_config_submenu_callback);
    view_dispatcher_add_view(
        App->ViewDispatcher,
        UHFReaderViewConfigure,
        variable_item_list_get_view(App->VariableItemListConfig));
}

uint32_t uhf_reader_navigation_configure_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewConfigure;
}

void uhf_reader_setting_1_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(!App->ReaderConnected) {
        if(!uhf_reader_ensure_worker(App)) {
            UHF_E("CFG", "connect rejected: worker/UART unavailable");
            uhf_debug_flush();
            variable_item_set_current_value_index(Item, 0);
            variable_item_set_current_value_text(Item, App->Setting1Names[0]);
            App->Setting1Index = 0;
            if(!uhf_reader_silent_connect) {
                uhf_reader_show_antenna_not_connected(App, UHFReaderViewConfigure);
            }
            return;
        }

        bool connected = false;

        uint8_t max_attempts = uhf_reader_connect_attempt_limit;
        if(max_attempts == 0U) max_attempts = UHF_READER_MANUAL_CONNECT_ATTEMPTS;

        // M100/QM100 can consume the first command immediately after wake.
        for(uint8_t attempt = 0; attempt < max_attempts && !connected; attempt++) {
            UHF_I(
                "CFG",
                "connect attempt=%u/%u settings_initialized=%d",
                (unsigned int)(attempt + 1U),
                (unsigned int)max_attempts,
                App->SettingsInitialized ? 1 : 0);
            uhf_debug_flush();
            connected = App->SettingsInitialized ? uhf_reader_settings_apply_yrm100x(App) :
                                                   uhf_reader_settings_adopt_yrm100x(App);
            UHF_I(
                "CFG",
                "connect attempt=%u/%u result=%d",
                (unsigned int)(attempt + 1U),
                (unsigned int)max_attempts,
                connected ? 1 : 0);
        }

        if(!connected) {
            Index = 0;
            variable_item_set_current_value_index(Item, 0);
            if(!uhf_reader_silent_connect) {
                uhf_reader_show_antenna_not_connected(App, UHFReaderViewConfigure);
            }
        } else {
            App->ReaderConnected = true;
            Index = 1;

            for(size_t i = 0; i < App->num_items; i++) {
                App->item_locks[i].locked = false;
            }

            char power_label[8];
            snprintf(power_label, sizeof(power_label), "%ddBm", App->SettingPowerIndex);
            variable_item_set_current_value_index(App->Setting2Item, App->SettingPowerIndex);
            variable_item_set_current_value_text(App->Setting2Item, power_label);

            variable_item_set_current_value_index(App->BaudSelection, App->SettingBaudIndex);
            variable_item_set_current_value_text(
                App->BaudSelection, App->SettingBaudNames[App->SettingBaudIndex]);

            variable_item_set_current_value_index(App->RegionSelection, App->SettingRegionIndex);
            variable_item_set_current_value_text(
                App->RegionSelection, App->SettingRegionNames[App->SettingRegionIndex]);

            variable_item_set_current_value_text(
                App->SettingApPwdItem, furi_string_get_cstr(App->DefaultAccessPwdStr));

            variable_item_set_current_value_index(App->SessionSelection, App->SettingSessionIndex);
            variable_item_set_current_value_text(
                App->SessionSelection, App->SettingSessionNames[App->SettingSessionIndex]);

            variable_item_set_current_value_index(App->TargetSelection, App->SettingTargetIndex);
            variable_item_set_current_value_text(
                App->TargetSelection, App->SettingTargetNames[App->SettingTargetIndex]);
        }
    } else {
        App->ReaderConnected = false;
        Index = 0;

        for(size_t i = 0; i < App->num_items; i++) {
            App->item_locks[i].locked = true;
        }

        variable_item_set_current_value_text(App->Setting2Item, "LOCKED");
        variable_item_set_current_value_text(App->BaudSelection, "LOCKED");
        variable_item_set_current_value_text(App->RegionSelection, "LOCKED");
        variable_item_set_current_value_text(App->SettingApPwdItem, "LOCKED");
        variable_item_set_current_value_text(App->SessionSelection, "LOCKED");
        variable_item_set_current_value_text(App->TargetSelection, "LOCKED");
    }

    variable_item_set_current_value_index(Item, Index);
    variable_item_set_current_value_text(Item, App->Setting1Names[Index]);

    /*
     * Alpha33 made ViewWrite lazy. During auto-connect ViewWrite is expected
     * to be NULL, so never call any View API for it until it actually exists.
     *
     * Keep the application state authoritative so a later lazy allocation of
     * Write inherits the current connection state immediately.
     */
    App->Setting1Index = Index;

    if(App->ViewRead) {
        UHFReaderConfigModel* ModelRead = view_get_model(App->ViewRead);
        if(ModelRead) {
            ModelRead->Setting1Index = Index;
            if(ModelRead->Setting1Value) {
                furi_string_set(ModelRead->Setting1Value, App->Setting1Names[Index]);
            }
        }
    }

    if(App->ViewWrite) {
        UHFReaderWriteModel* ModelWrite = view_get_model(App->ViewWrite);
        if(ModelWrite) {
            ModelWrite->Setting1Index = Index;
            if(ModelWrite->Setting1Value) {
                furi_string_set(ModelWrite->Setting1Value, App->Setting1Names[Index]);
            }
        }

        UHF_T("CFG", "connection sync existing Write index=%u", (unsigned int)Index);
    } else {
        UHF_T("CFG", "connection sync deferred Write index=%u", (unsigned int)Index);
    }

    uhf_debug_flush();
}

void uhf_reader_power_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(App->item_locks[0].locked) {
        show_locked_notification(App);
        variable_item_set_current_value_index(Item, App->SettingPowerIndex);
        return;
    }

    if(Index < UHF_READER_MIN_POWER_DBM || Index > UHF_READER_MAX_POWER_DBM) {
        variable_item_set_current_value_index(Item, App->SettingPowerIndex);
        show_command_error_notification(App);
        return;
    }

    if(!m100_set_transmitting_power(App->YRM100XWorker->module, (uint16_t)Index * 100U)) {
        variable_item_set_current_value_index(Item, App->SettingPowerIndex);
        show_command_error_notification(App);
        return;
    }

    App->SettingPowerIndex = Index;

    char power_label[8];
    snprintf(power_label, sizeof(power_label), "%ddBm", Index);
    variable_item_set_current_value_text(Item, power_label);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_setting_6_text_updated(void* context) {
    UHFReaderApp* App = context;
    char value[9];

    snprintf(
        value,
        sizeof(value),
        "%02X%02X%02X%02X",
        App->ApTempBuffer[0],
        App->ApTempBuffer[1],
        App->ApTempBuffer[2],
        App->ApTempBuffer[3]);

    furi_string_set_str(App->DefaultAccessPwdStr, value);
    memcpy(App->DefaultAccessPassword, value, sizeof(value));

    with_view_model(
        App->ViewRead,
        UHFReaderConfigModel * Model,
        { furi_string_set_str(Model->SettingReadAp, value); },
        true);

    variable_item_set_current_value_text(App->SettingApPwdItem, value);
    App->YRM100XWorker->DefaultAP = bytes_to_uint32(App->ApTempBuffer, 4);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewConfigure);
}

void uhf_reader_save_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(Index > 1) Index = 1;
    App->SettingSavingIndex = Index;
    App->UHFSaveType = Index == 1 ? YES_SAVE_ON_WRITE : NO_SAVE_ON_WRITE;

    variable_item_set_current_value_text(Item, App->SettingSavingNames[Index]);
    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_baud_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(App->item_locks[1].locked) {
        show_locked_notification(App);
        return;
    }

    if(Index == 0) {
        App->UHFBaudRate = 9600;
    } else if(Index == 1) {
        App->UHFBaudRate = 115200;
    } else {
        App->UHFBaudRate = 384000;
    }

    variable_item_set_current_value_text(Item, App->SettingBaudNames[Index]);

    if(App->ReaderConnected) {
        m100_set_baudrate(App->YRM100XWorker->module, App->UHFBaudRate);
    }
}

void uhf_reader_region_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(App->item_locks[2].locked) {
        show_locked_notification(App);
        return;
    }
    if(!App->ReaderConnected || Index >= WORKING_REGIONS_COUNT) return;

    WorkingRegion region = WORKING_REGIONS[Index];
    if(!m100_set_working_region(App->YRM100XWorker->module, region)) {
        variable_item_set_current_value_index(Item, App->SettingRegionIndex);
        show_command_error_notification(App);
        return;
    }

    App->SettingRegionIndex = Index;
    App->UHFRegionType = Index;
    variable_item_set_current_value_text(Item, App->SettingRegionNames[Index]);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_session_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(App->item_locks[4].locked) {
        show_locked_notification(App);
        return;
    }

    if(!m100_set_query_params(App->YRM100XWorker->module, Index, App->SettingTargetIndex)) {
        variable_item_set_current_value_index(Item, App->SettingSessionIndex);
        variable_item_set_current_value_text(
            Item, App->SettingSessionNames[App->SettingSessionIndex]);
        show_command_error_notification(App);
        return;
    }

    App->SettingSessionIndex = Index;
    variable_item_set_current_value_text(Item, App->SettingSessionNames[Index]);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_target_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(App->item_locks[5].locked) {
        show_locked_notification(App);
        return;
    }

    if(!m100_set_query_params(App->YRM100XWorker->module, App->SettingSessionIndex, Index)) {
        variable_item_set_current_value_index(Item, App->SettingTargetIndex);
        variable_item_set_current_value_text(
            Item, App->SettingTargetNames[App->SettingTargetIndex]);
        show_command_error_notification(App);
        return;
    }

    App->SettingTargetIndex = Index;
    variable_item_set_current_value_text(Item, App->SettingTargetNames[Index]);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_multi_full_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(Index > 1) Index = 1;
    App->SettingMultiFullIndex = Index;
    variable_item_set_current_value_text(Item, App->SettingMultiFullNames[Index]);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_auto_save_multi_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    if(Index > 1) Index = 1;
    App->SettingAutoSaveMultiIndex = Index;
    variable_item_set_current_value_text(Item, App->SettingAutoSaveMultiNames[Index]);

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_sound_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t index = variable_item_get_current_value_index(Item);
    if(index > 1U) index = 1U;

    App->SettingSoundIndex = index;
    variable_item_set_current_value_text(Item, App->SettingSoundNames[index]);

    UHF_I("CFG", "Sound=%s", index ? "On" : "Off");
    uhf_debug_flush();

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_vibration_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t index = variable_item_get_current_value_index(Item);
    if(index > 1U) index = 1U;

    App->SettingVibrationIndex = index;
    variable_item_set_current_value_text(Item, App->SettingVibrationNames[index]);

    UHF_I("CFG", "Vibration=%s", index ? "On" : "Off");
    uhf_debug_flush();

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_tag_profile_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t index = variable_item_get_current_value_index(Item);

    if(index >= UHF_READER_TAG_PROFILE_COUNT) {
        index = UHF_READER_TAG_PROFILE_GENERIC;
    }

    App->SettingTagProfileIndex = index;
    variable_item_set_current_value_text(Item, App->SettingTagProfileNames[index]);

    if(App->YRM100XWorker) {
        App->YRM100XWorker->ReadProfile = index;
    }

    UHF_I("CFG", "Tag Profile=%s(%u)", App->SettingTagProfileNames[index], (unsigned int)index);
    uhf_debug_flush();

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_full_dump_attempts_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t Index = variable_item_get_current_value_index(Item);

    uint8_t attempts = (uint8_t)(Index + UHF_READER_FULL_DUMP_ATTEMPTS_MIN);

    if(attempts < UHF_READER_FULL_DUMP_ATTEMPTS_MIN ||
       attempts > UHF_READER_FULL_DUMP_ATTEMPTS_MAX) {
        attempts = UHF_READER_FULL_DUMP_ATTEMPTS_DEFAULT;
    }

    App->SettingFullDumpAttempts = attempts;

    // The worker is allocated after view_config_alloc(), so guard this pointer.
    if(App->YRM100XWorker) {
        App->YRM100XWorker->FullDumpMaxAttempts = attempts;
    }

    char attempts_label[4];
    snprintf(attempts_label, sizeof(attempts_label), "%u", (unsigned int)attempts);
    variable_item_set_current_value_text(Item, attempts_label);

    UHF_I("CFG", "Full Attempts changed=%u", (unsigned int)attempts);
    uhf_debug_flush();

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_clone_attempts_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t index = variable_item_get_current_value_index(Item);
    uint8_t attempts = (uint8_t)(index + UHF_READER_CLONE_ATTEMPTS_MIN);

    if(attempts < UHF_READER_CLONE_ATTEMPTS_MIN || attempts > UHF_READER_CLONE_ATTEMPTS_MAX) {
        attempts = UHF_READER_CLONE_ATTEMPTS_DEFAULT;
    }

    App->SettingCloneAttempts = attempts;
    if(App->YRM100XWorker) App->YRM100XWorker->CloneMaxAttempts = attempts;

    char label[4];
    snprintf(label, sizeof(label), "%u", (unsigned int)attempts);
    variable_item_set_current_value_text(Item, label);

    UHF_I("CFG", "Clone Attempts changed=%u", (unsigned int)attempts);
    uhf_debug_flush();

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_forever_delay_setting_change(VariableItem* Item) {
    UHFReaderApp* App = variable_item_get_context(Item);
    uint8_t index = variable_item_get_current_value_index(Item);

    uint8_t seconds = (uint8_t)(index + UHF_READER_FOREVER_DELAY_MIN_SEC);

    if(seconds < UHF_READER_FOREVER_DELAY_MIN_SEC || seconds > UHF_READER_FOREVER_DELAY_MAX_SEC) {
        seconds = UHF_READER_FOREVER_DELAY_DEFAULT_SEC;
    }

    App->SettingForeverDelaySeconds = seconds;

    if(App->YRM100XWorker) {
        App->YRM100XWorker->ForeverDelaySeconds = seconds;
    }

    char label[6];
    snprintf(label, sizeof(label), "%us", (unsigned int)seconds);
    variable_item_set_current_value_text(Item, label);

    UHF_I("CFG", "Forever Delay changed=%us", (unsigned int)seconds);
    uhf_debug_flush();

    if(!uhf_reader_settings_save_current(App)) show_command_error_notification(App);
}

void uhf_reader_delete_all_confirm_callback(DialogExResult result, void* context) {
    UHFReaderApp* App = context;

    if(result == DialogExResultRight) {
        if(uhf_delete_all_saved_data(App)) {
            uhf_notify_success(App);
        } else {
            uhf_notify_error(App);
        }
    }

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSaved);
}

void ap_menu_alloc(UHFReaderApp* App) {
    App->ApInput = byte_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSetReadAp, byte_input_get_view(App->ApInput));

    App->ApInputBufferSize = 4;
    App->ApTempBuffer = malloc(App->ApInputBufferSize);
}

void uhf_reader_setting_item_clicked(void* context, uint32_t index) {
    UHFReaderApp* App = context;

    if(index == UHFConfigItemAboutAntenna) {
        uhf_reader_show_antenna_info(App, false);
        return;
    }

    if(index == UHFConfigItemDefaultAp) {
        if(App->item_locks[3].locked) {
            show_locked_notification(App);
            return;
        }

        byte_input_set_header_text(App->ApInput, App->AccessPasswordPlaceHolder);
        byte_input_set_result_callback(
            App->ApInput,
            uhf_reader_setting_6_text_updated,
            NULL,
            App,
            App->ApTempBuffer,
            App->ApInputBufferSize);
        view_set_previous_callback(
            byte_input_get_view(App->ApInput), uhf_reader_navigation_configure_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSetReadAp);
        return;
    }
}

bool uhf_reader_auto_connect(UHFReaderApp* App) {
    UHF_I("CFG", "auto_connect wrapper BEGIN");
    uhf_debug_flush();
    if(!App || !App->ConnectionItem) return false;

    if(App->ReaderConnected) return true;

    uhf_reader_silent_connect = true;
    uhf_reader_connect_attempt_limit = 1U;

    for(uint8_t attempt = 1U; attempt <= UHF_READER_STARTUP_CONNECT_ATTEMPTS; attempt++) {
        uhf_reader_show_antenna_connecting(App, attempt, UHF_READER_STARTUP_CONNECT_ATTEMPTS);

        UHF_I(
            "CFG",
            "startup connect attempt=%u/%u BEGIN",
            (unsigned int)attempt,
            (unsigned int)UHF_READER_STARTUP_CONNECT_ATTEMPTS);
        uhf_debug_flush();

        variable_item_set_current_value_index(App->ConnectionItem, 1U);
        uhf_reader_setting_1_change(App->ConnectionItem);

        UHF_I(
            "CFG",
            "startup connect attempt=%u/%u END connected=%d",
            (unsigned int)attempt,
            (unsigned int)UHF_READER_STARTUP_CONNECT_ATTEMPTS,
            App->ReaderConnected ? 1 : 0);
        uhf_debug_flush();

        if(App->ReaderConnected) break;
        if(attempt < UHF_READER_STARTUP_CONNECT_ATTEMPTS) {
            UHF_I(
                "CFG",
                "startup connect retry delay=%lums",
                (unsigned long)UHF_READER_STARTUP_RETRY_DELAY_MS);
            uhf_debug_flush();
            furi_delay_ms(UHF_READER_STARTUP_RETRY_DELAY_MS);
        }
    }

    uhf_reader_connect_attempt_limit = UHF_READER_MANUAL_CONNECT_ATTEMPTS;
    uhf_reader_silent_connect = false;

    UHF_I("CFG", "auto_connect wrapper END connected=%d", App->ReaderConnected ? 1 : 0);
    uhf_debug_flush();
    return App->ReaderConnected;
}

void view_config_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewDeleteAllConfirm);
    dialog_ex_free(App->DeleteAllDialog);
    App->DeleteAllDialog = NULL;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSetReadAp);
    byte_input_free(App->ApInput);
    App->ApInput = NULL;

    free(App->ApTempBuffer);
    App->ApTempBuffer = NULL;

    if(App->DefaultAccessPwdStr) {
        furi_string_free(App->DefaultAccessPwdStr);
        App->DefaultAccessPwdStr = NULL;
    }

    free(App->DefaultAccessPassword);
    App->DefaultAccessPassword = NULL;
    free(App->ReadAccessPasswordLabel);
    App->ReadAccessPasswordLabel = NULL;
    free(App->AccessPasswordPlaceHolder);
    App->AccessPasswordPlaceHolder = NULL;
    free(App->item_locks);
    App->item_locks = NULL;

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewConfigure);
    variable_item_list_free(App->VariableItemListConfig);
    App->VariableItemListConfig = NULL;
}
