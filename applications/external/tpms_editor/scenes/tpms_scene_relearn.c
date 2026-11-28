#include "../tpms_app_i.h"
#include <stdint.h>

/* TPMS 3.8: one Relearn configuration page for all LF wake-up methods.
   The selected LF profile is executed from the normal RX18 receiver.
   UHF protocol decoding is always automatic (RX18). */

typedef enum {
    TPMSRelearnModeOff = 0,
    TPMSRelearnModeCW,
    TPMSRelearnModeEL50448,
    TPMSRelearnModeFordEL50449,
    TPMSRelearnModeCount,
} TPMSRelearnMode;

static const char* const tpms_relearn_mode_text[TPMSRelearnModeCount] = {
    "OFF",
    "CW Relearn",
    "EL-50448",
    "EL-50449",
};

static const char* const tpms_relearn_cw_frequency_text[TPMSCWFrequencyCount] = {
    "125.0 kHz",
    "134.2 kHz",
};

static const char* const tpms_relearn_band_text[TPMSRelearnRXBandCount] = {
    "315.00 MHz",
    "433.92 MHz",
    "AUTO 315/433",
};

static const char* const tpms_relearn_mod_text[TPMSRelearnModCount] = {
    "AUTO",
    "FSK",
    "OOK",
    "GFSK",
};

static const char* const tpms_relearn_ford_profile_text[TPMSFordLFProfileCount] = {
    "Ford 5A5A",
    "VDO/FCA 615E",
    "AUTO Ford+VDO",
};

static const char* const tpms_relearn_repeat_text[TPMSRelearnRepeatCount] = {
    "MANUAL",
    "AUTO",
};

static const char* const tpms_relearn_rx_wait_text[TPMSRelearnRXWaitCount] = {
    "5 s",
    "10 s",
    "15 s",
};

static uint8_t tpms_relearn_mode_index(const TPMSApp* app) {
    if(!app || app->relearn == TPMSRelearnOff) return TPMSRelearnModeOff;
    if(app->relearn_type == TPMSRelearnTypeEL50448) return TPMSRelearnModeEL50448;
    if(app->relearn_type == TPMSRelearnTypeFordEL50449) return TPMSRelearnModeFordEL50449;
    return TPMSRelearnModeCW;
}

static void tpms_relearn_mode_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSRelearnModeCount) index = TPMSRelearnModeOff;

    if(index == TPMSRelearnModeOff) {
        app->relearn = TPMSRelearnOff;
    } else {
        app->relearn = TPMSRelearnOn;
        if(index == TPMSRelearnModeEL50448) {
            app->relearn_type = TPMSRelearnTypeEL50448;
        } else if(index == TPMSRelearnModeFordEL50449) {
            app->relearn_type = TPMSRelearnTypeFordEL50449;
        } else {
            app->relearn_type = TPMSRelearnTypeCommon;
        }
    }

    variable_item_set_current_value_text(item, tpms_relearn_mode_text[index]);
}

static void tpms_relearn_cw_frequency_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSCWFrequencyCount) index = TPMSCWFrequency125;
    app->relearn_cw_frequency = (TPMSCWFrequency)index;
    variable_item_set_current_value_text(item, tpms_relearn_cw_frequency_text[index]);
}

static void tpms_relearn_band_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSRelearnRXBandCount) index = TPMSRelearnRXBand433;
    app->relearn_rx_band_index = index;
    variable_item_set_current_value_text(item, tpms_relearn_band_text[index]);
}

static void tpms_relearn_mod_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSRelearnModCount) index = TPMSRelearnModFSK;
    app->relearn_modulation_index = index;
    variable_item_set_current_value_text(item, tpms_relearn_mod_text[index]);
}

static void tpms_relearn_ford_profile_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSFordLFProfileCount) index = TPMSFordLFProfileAuto;
    app->relearn_ford_profile = (TPMSFordLFProfile)index;
    /* Always start AUTO with a full Ford 5A5A cycle, then VDO/FCA. */
    app->relearn_ford_auto_wake_step = 0U;
    app->relearn_ford_auto_pair_uhf_valid = false;
    variable_item_set_current_value_text(item, tpms_relearn_ford_profile_text[index]);
}

static void tpms_relearn_repeat_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSRelearnRepeatCount) index = TPMSRelearnRepeatManual;
    app->relearn_repeat_mode = (TPMSRelearnRepeatMode)index;
    variable_item_set_current_value_text(item, tpms_relearn_repeat_text[index]);
}

static void tpms_relearn_rx_wait_changed(VariableItem* item) {
    TPMSApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= TPMSRelearnRXWaitCount) index = TPMSRelearnRXWait5;
    app->relearn_rx_wait = (TPMSRelearnRXWait)index;
    variable_item_set_current_value_text(item, tpms_relearn_rx_wait_text[index]);
}

static void tpms_relearn_add_fixed(TPMSApp* app, const char* label, const char* value) {
    VariableItem* item = variable_item_list_add(app->variable_item_list, label, 1U, NULL, app);
    variable_item_set_current_value_text(item, value);
}

static uint32_t tpms_relearn_start_item_index = UINT32_MAX;

static void tpms_relearn_enter_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(index == tpms_relearn_start_item_index) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, TPMSCustomEventSceneRelearnStart);
    }
}

void tpms_scene_relearn_config_on_enter(void* context) {
    TPMSApp* app = context;

    uint8_t mode_index = tpms_relearn_mode_index(app);
    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Relearn type",
        TPMSRelearnModeCount,
        tpms_relearn_mode_changed,
        app);
    variable_item_set_current_value_index(item, mode_index);
    variable_item_set_current_value_text(item, tpms_relearn_mode_text[mode_index]);

    if(app->relearn_cw_frequency >= TPMSCWFrequencyCount) {
        app->relearn_cw_frequency = TPMSCWFrequency125;
    }
    item = variable_item_list_add(
        app->variable_item_list,
        "CW frequency",
        TPMSCWFrequencyCount,
        tpms_relearn_cw_frequency_changed,
        app);
    variable_item_set_current_value_index(item, app->relearn_cw_frequency);
    variable_item_set_current_value_text(
        item, tpms_relearn_cw_frequency_text[app->relearn_cw_frequency]);

    if(app->relearn_ford_profile >= TPMSFordLFProfileCount) {
        app->relearn_ford_profile = TPMSFordLFProfileAuto;
    }
    item = variable_item_list_add(
        app->variable_item_list,
        "50449 profile",
        TPMSFordLFProfileCount,
        tpms_relearn_ford_profile_changed,
        app);
    variable_item_set_current_value_index(item, app->relearn_ford_profile);
    variable_item_set_current_value_text(
        item, tpms_relearn_ford_profile_text[app->relearn_ford_profile]);

    if(app->relearn_repeat_mode >= TPMSRelearnRepeatCount) {
        app->relearn_repeat_mode = TPMSRelearnRepeatManual;
    }
    item = variable_item_list_add(
        app->variable_item_list,
        "Repeat mode",
        TPMSRelearnRepeatCount,
        tpms_relearn_repeat_changed,
        app);
    variable_item_set_current_value_index(item, app->relearn_repeat_mode);
    variable_item_set_current_value_text(
        item, tpms_relearn_repeat_text[app->relearn_repeat_mode]);

    if(app->relearn_rx_wait >= TPMSRelearnRXWaitCount) {
        app->relearn_rx_wait = TPMSRelearnRXWait5;
    }
    item = variable_item_list_add(
        app->variable_item_list,
        "RX pause",
        TPMSRelearnRXWaitCount,
        tpms_relearn_rx_wait_changed,
        app);
    variable_item_set_current_value_index(item, app->relearn_rx_wait);
    variable_item_set_current_value_text(
        item, tpms_relearn_rx_wait_text[app->relearn_rx_wait]);

    if(app->relearn_rx_band_index >= TPMSRelearnRXBandCount) {
        app->relearn_rx_band_index = TPMSRelearnRXBand433;
    }
    item = variable_item_list_add(
        app->variable_item_list,
        "RX band",
        TPMSRelearnRXBandCount,
        tpms_relearn_band_changed,
        app);
    variable_item_set_current_value_index(item, app->relearn_rx_band_index);
    variable_item_set_current_value_text(item, tpms_relearn_band_text[app->relearn_rx_band_index]);

    if(app->relearn_modulation_index >= TPMSRelearnModCount) {
        app->relearn_modulation_index = TPMSRelearnModFSK;
    }
    item = variable_item_list_add(
        app->variable_item_list,
        "RX modulation",
        TPMSRelearnModCount,
        tpms_relearn_mod_changed,
        app);
    variable_item_set_current_value_index(item, app->relearn_modulation_index);
    variable_item_set_current_value_text(
        item, tpms_relearn_mod_text[app->relearn_modulation_index]);

    tpms_relearn_add_fixed(app, "Decoders", "AUTO All");
    tpms_relearn_add_fixed(app, "Controls", "> / OK = REPEAT");
    tpms_relearn_add_fixed(app, "AUTO", "LF -> RX -> LF...");
    tpms_relearn_add_fixed(app, "Wake trace", "saved with RX");

    /* TPMS 3.8.5: START remains the last row. Pressing OK here
       opens the normal RX18 scanner immediately. If Relearn is enabled,
       the first configured LF cycle starts automatically after RX is up. */
    tpms_relearn_start_item_index = 11U;
    item = variable_item_list_add(app->variable_item_list, "START", 1U, NULL, app);
    variable_item_set_current_value_text(item, "OK");
    variable_item_list_set_enter_callback(
        app->variable_item_list, tpms_relearn_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_relearn_config_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == TPMSCustomEventSceneRelearnStart) {
        app->editor_profile_loaded = false;
        app->relearn_auto_rx = false;
        /* START means: enter RX18 now and immediately execute the selected
           wake-up once. MANUAL then waits for the user; AUTO schedules the
           following cycles after the configured RX pause. */
        app->relearn_autostart = app->relearn == TPMSRelearnOn;
        scene_manager_next_scene(app->scene_manager, TPMSSceneReceiver);
        return true;
    }
    return false;
}

void tpms_scene_relearn_config_on_exit(void* context) {
    TPMSApp* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0U);
    variable_item_list_reset(app->variable_item_list);
}
