#include "../tpms_app_i.h"
#include "../tpms_save.h"
#include "../protocols/proto6/custom_presets.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

enum TPMSSettingIndex {
    TPMSSettingIndexFrequency = 0,
    TPMSSettingIndexHopping,
    TPMSSettingIndexModulation,
    TPMSSettingIndexRadio,
    TPMSSettingIndexAutosave,
    TPMSSettingIndexLock,
    TPMSSettingIndexRelearn,
};

static const char* const modulation_text[] = {
    "AUTO",
    "FSK",
    "OOK",
    "GFSK",
};

static const char* const autosave_text[] = {
    "OFF",
    "ON",
};

static uint8_t tpms_config_frequency_index(const TPMSApp* app, uint32_t frequency) {
    for(uint8_t i = 0U; i < subghz_setting_get_frequency_count(app->setting); i++) {
        if(frequency == subghz_setting_get_frequency(app->setting, i)) return i;
    }
    return subghz_setting_get_frequency_default_index(app->setting);
}

static uint8_t tpms_config_modulation_index(const TPMSApp* app, const char* preset_name) {
    if(app && app->receiver_modulation_auto) return 0U;
    if(preset_name && strcmp(preset_name, "TPMS FSK") == 0) return 1U;
    if(preset_name && strcmp(preset_name, "TPMS OOK") == 0) return 2U;
    if(preset_name && strcmp(preset_name, "TPMS GFSK") == 0) return 3U;
    return 1U;
}

static void tpms_config_frequency_text(VariableItem* item, uint32_t frequency) {
    char text[12];
    snprintf(
        text,
        sizeof(text),
        "%lu.%02lu",
        (unsigned long)(frequency / 1000000U),
        (unsigned long)((frequency % 1000000U) / 10000U));
    variable_item_set_current_value_text(item, text);
}

static void tpms_config_set_frequency(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    app->txrx->preset->frequency = subghz_setting_get_frequency(app->setting, index);
    tpms_config_frequency_text(item, app->txrx->preset->frequency);
}

static void tpms_config_set_modulation(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(modulation_text)) index = 1U;

    /* AUTO in normal Odczyt rotates only modulation. It intentionally keeps
       the frequency selected in the row above and is not the default. */
    app->receiver_modulation_auto = index == 0U;

    uint8_t physical_index = index == 0U ? 1U : index;
    const char* name = "TPMS FSK";
    uint8_t* data = (uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
    size_t size = protoview_subghz_tpms1_fsk_async_regs_size;
    Proto6Mode mode = Proto6ModeFSK;

    if(physical_index == 2U) {
        name = "TPMS OOK";
        data = (uint8_t*)protoview_subghz_tpms2_ook_async_regs;
        size = protoview_subghz_tpms2_ook_async_regs_size;
        mode = Proto6ModeOOK;
    } else if(physical_index == 3U) {
        name = "TPMS GFSK";
        data = (uint8_t*)protoview_subghz_tpms3_gfsk_async_regs;
        size = protoview_subghz_tpms3_gfsk_async_regs_size;
    }

    app->receiver_modulation_auto_index = (uint8_t)(physical_index - 1U);
    tpms_preset_init(app, name, app->txrx->preset->frequency, data, size);
    proto6_bridge_set_mode(app->txrx->proto6, mode);
    variable_item_set_current_value_text(item, modulation_text[index]);
}

static void tpms_config_set_radio(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    const uint8_t index = variable_item_get_current_value_index(item);
    const TPMSRadioMode mode = (TPMSRadioMode)index;

    (void)tpms_radio_select(app, mode);
    char text[12];
    tpms_radio_get_setting_text(app, text, sizeof(text));
    variable_item_set_current_value_text(item, text);
}

static void tpms_config_set_autosave(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    const bool enabled = variable_item_get_current_value_index(item) != 0U;

    if(tpms_autosave_set_enabled(app, enabled)) {
        variable_item_set_current_value_text(item, enabled ? "ON" : "OFF");
        notification_message(app->notifications, &sequence_blink_green_10);
    } else {
        /* Keep the displayed value consistent with the last setting that was
           successfully saved. A red blink means the SD write failed. */
        const uint8_t previous_index = app->autosave_enabled ? 1U : 0U;
        variable_item_set_current_value_index(item, previous_index);
        variable_item_set_current_value_text(item, autosave_text[previous_index]);
        notification_message(app->notifications, &sequence_blink_red_10);
    }
}

static void tpms_config_build_list(TPMSApp* app);

static void tpms_config_enter_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(index == TPMSSettingIndexLock) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TPMSCustomEventSceneSettingLock);
    } else if(index == TPMSSettingIndexRelearn) {
        view_dispatcher_send_custom_event(app->view_dispatcher, TPMSCustomEventSceneOpenRelearn);
    }
}

static const char* tpms_config_relearn_text(const TPMSApp* app) {
    if(!app || app->relearn == TPMSRelearnOff) return "OFF";
    if(app->relearn_type == TPMSRelearnTypeEL50448) return "EL-50448";
    if(app->relearn_type == TPMSRelearnTypeFordEL50449) return "Ford";
    return "CW";
}

static void tpms_config_build_list(TPMSApp* app) {
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Frequency",
        subghz_setting_get_frequency_count(app->setting),
        tpms_config_set_frequency,
        app);
    const uint8_t frequency_index =
        tpms_config_frequency_index(app, app->txrx->preset->frequency);
    variable_item_set_current_value_index(item, frequency_index);
    tpms_config_frequency_text(item, app->txrx->preset->frequency);

    item = variable_item_list_add(app->variable_item_list, "Hopping", 1U, NULL, app);
    variable_item_set_current_value_text(item, "OFF");
    app->txrx->hopper_state = TPMSHopperStateOFF;

    item = variable_item_list_add(
        app->variable_item_list,
        "Modulation",
        COUNT_OF(modulation_text),
        tpms_config_set_modulation,
        app);
    const uint8_t modulation_index =
        tpms_config_modulation_index(app, furi_string_get_cstr(app->txrx->preset->name));
    variable_item_set_current_value_index(item, modulation_index);
    variable_item_set_current_value_text(item, modulation_text[modulation_index]);

    item = variable_item_list_add(app->variable_item_list, "Radio", 3U, tpms_config_set_radio, app);
    variable_item_set_current_value_index(item, (uint8_t)app->txrx->radio_mode);
    char radio_text[12];
    tpms_radio_get_setting_text(app, radio_text, sizeof(radio_text));
    variable_item_set_current_value_text(item, radio_text);

    /* TPMS 4.5 FIX2: this is the settings screen opened from the main menu,
       so the persisted Autosave switch must live here. */
    item = variable_item_list_add(
        app->variable_item_list,
        "Autosave RX",
        COUNT_OF(autosave_text),
        tpms_config_set_autosave,
        app);
    const uint8_t autosave_index = app->autosave_enabled ? 1U : 0U;
    variable_item_set_current_value_index(item, autosave_index);
    variable_item_set_current_value_text(item, autosave_text[autosave_index]);

    item = variable_item_list_add(app->variable_item_list, "Key lock", 1U, NULL, app);
    variable_item_set_current_value_text(item, app->lock == TPMSLockOn ? "ON" : "OFF");

    /* TPMS 3.8: Relearn is a normal, always-visible RX18 feature. */
    item = variable_item_list_add(app->variable_item_list, "Relearn", 1U, NULL, app);
    variable_item_set_current_value_text(item, tpms_config_relearn_text(app));

    variable_item_list_set_enter_callback(app->variable_item_list, tpms_config_enter_callback, app);
}

void tpms_scene_receiver_config_on_enter(void* context) {
    TPMSApp* app = context;
    tpms_config_build_list(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_receiver_config_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == TPMSCustomEventSceneSettingLock) {
            app->lock = TPMSLockOn;
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(event.event == TPMSCustomEventSceneOpenRelearn) {
            scene_manager_next_scene(app->scene_manager, TPMSSceneRelearn);
            return true;
        }
    }
    return false;
}

void tpms_scene_receiver_config_on_exit(void* context) {
    TPMSApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0U);
    variable_item_list_reset(app->variable_item_list);
}
