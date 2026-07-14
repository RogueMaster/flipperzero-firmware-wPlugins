/**
 * @file config.c
 * @brief Variable-item handlers for the Config screen.
 */
#include "src/fred_fm/include/config.h"
#include "src/fred_fm/include/types.h"
#include "src/fred_fm/core/core.h"
#include "src/fred_fm/audio/audio.h"
#include "src/fred_fm/ui/ui.h"

#include "src/drivers/tea5767/TEA5767.h"
#include "src/drivers/pam/PAM8406.h"
#ifdef ENABLE_RDS
#include "src/fred_fm/rds/rds.h"
#endif

/** TEA5767 search-noise cancellation toggle. */
void fred_fm_snc_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));

    uint8_t index = variable_item_get_current_value_index(item);
    tea_snc_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_snc_enabled ? "On" : "Off");

    tea5767_set_snc_enabled(tea_snc_enabled);
    (void)tea5767_set_snc(tea_snc_enabled);
    fred_fm_settings_mark_dirty();
}

/** TEA5767 de-emphasis (50 µs / 75 µs). */
void fred_fm_deemph_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);

    tea_deemph_75us = (index != 0);
    variable_item_set_current_value_text(item, tea_deemph_75us ? "75us" : "50us");

    tea5767_set_deemphasis_75us_enabled(tea_deemph_75us);
    (void)tea5767_set_deemphasis_75us(tea_deemph_75us);
    fred_fm_settings_mark_dirty();
}

/** TEA5767 high-cut filter toggle. */
void fred_fm_highcut_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_highcut_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_highcut_enabled ? "On" : "Off");
    tea5767_set_high_cut_enabled(tea_highcut_enabled);
    (void)tea5767_set_high_cut(tea_highcut_enabled);
    fred_fm_settings_mark_dirty();
}

/** TEA5767 force-mono toggle. */
void fred_fm_mono_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    tea_force_mono_enabled = (index != 0);
    variable_item_set_current_value_text(item, tea_force_mono_enabled ? "On" : "Off");
    tea5767_set_force_mono_enabled(tea_force_mono_enabled);
    (void)tea5767_set_force_mono(tea_force_mono_enabled);
    fred_fm_settings_mark_dirty();
}

/** Keep backlight on while app is open. */
void fred_fm_backlight_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    backlight_keep_on = (index != 0);
    variable_item_set_current_value_text(item, backlight_keep_on ? "On" : "Off");

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    fred_fm_apply_backlight(notifications);
    furi_record_close(RECORD_NOTIFICATION);

    fred_fm_settings_mark_dirty();
}

/** PAM8406 amplifier power on/off. */
void fred_fm_amp_power_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(amp_power_names)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    amp_power_enabled = (index != 0);
    variable_item_set_current_value_text(item, amp_power_names[index]);
    fred_fm_apply_audio_output_state();
    fred_fm_settings_mark_dirty();
}

/** PAM8406 Class-AB vs Class-D mode. */
void fred_fm_amp_mode_change(VariableItem* item) {
    UNUSED(variable_item_get_context(item));
    uint8_t index = variable_item_get_current_value_index(item);
    if(index >= COUNT_OF(amp_mode_names)) {
        index = 0;
        variable_item_set_current_value_index(item, index);
    }

    amp_mode_class_d = (index != 0);
    variable_item_set_current_value_text(item, amp_mode_names[index]);
    fred_fm_apply_audio_output_state();
    fred_fm_settings_mark_dirty();
}
