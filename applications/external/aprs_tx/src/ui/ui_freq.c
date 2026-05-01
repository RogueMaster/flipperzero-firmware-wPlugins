#include "ui_i.h"

#include <furi_hal.h>
#include <gui/view.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t freq_min_hz(void);
static uint32_t freq_max_hz(void);
static bool freq_band(uint32_t a, uint32_t* lo, uint32_t* hi);
static uint32_t freq_next_band(uint32_t a);

uint32_t freq_step(uint32_t a, int8_t d) {
    uint32_t b;
    uint32_t c;
    uint32_t e;

    b = a;
    if(!freq_band(b, &c, &e)) {
        c = freq_min_hz();
        e = freq_max_hz();
        if(b < c) b = c;
        if(b > e) b = e;
        if(!freq_band(b, &c, &e)) return b;
    }

    if(d > 0) {
        if(b >= e) return e;
        b += 2500UL;
        if(b > e) b = e;
    } else {
        if(b <= c) return c;
        b -= 2500UL;
        if(b < c) b = c;
    }

    return b;
}

static uint32_t freq_min_hz(void) {
    uint32_t a;

    for(a = 0; a < 1000000000UL; a += 2500UL)
        if(furi_hal_subghz_is_frequency_valid(a)) return a;

    return CARRIER_HZ;
}

static uint32_t freq_max_hz(void) {
    uint32_t a;

    for(a = 1000000000UL - 1; a > 2500UL; a -= 2500UL)
        if(furi_hal_subghz_is_frequency_valid(a)) return a;

    return CARRIER_HZ;
}

static bool freq_band(uint32_t a, uint32_t* lo, uint32_t* hi) {
    uint32_t b;

    if(!furi_hal_subghz_is_frequency_valid(a)) return false;

    b = a;
    while(b >= 2500UL) {
        if(!furi_hal_subghz_is_frequency_valid(b - 2500UL)) break;
        b -= 2500UL;
    }
    *lo = b;

    b = a;
    while(b <= 1000000000UL - 2500UL) {
        if(!furi_hal_subghz_is_frequency_valid(b + 2500UL)) break;
        b += 2500UL;
    }
    *hi = b;

    return true;
}

static uint32_t freq_next_band(uint32_t a) {
    uint32_t c;
    uint32_t e;
    uint32_t b;
    uint32_t message_pick;

    message_pick = freq_max_hz();
    if(!freq_band(a, &c, &e)) return freq_min_hz();

    b = e;
    while(b <= message_pick - 2500UL) {
        b += 2500UL;
        if(furi_hal_subghz_is_frequency_valid(b)) return b;
    }

    return freq_min_hz();
}

void freq_show(char* o, uint16_t n, uint32_t a) {
    if(!o) return;
    if(!n) return;

    snprintf(
        o, n, "%06lu.%1lu", (unsigned long)(a / 1000UL), (unsigned long)((a % 1000UL) / 100UL));
}

void fsh2(char* o, uint16_t n, uint32_t a) {
    if(!o) return;
    if(!n) return;

    snprintf(o, n, "%06lu", (unsigned long)(a / 1000UL));
}

static bool freq_parse(const char* s, uint32_t* out) {
    char* e;
    uint32_t a;
    uint32_t b;

    if(!s) return false;
    if(!out) return false;
    if(!s[0]) return false;

    a = strtoul(s, &e, 10);
    if(e == s) return false;
    if(*e) return false;

    b = a;
    if(furi_hal_subghz_is_frequency_valid(b)) {
        *out = b;
        return true;
    }

    if(a <= 1000000UL) {
        b = a * 1000UL;
        if(furi_hal_subghz_is_frequency_valid(b)) {
            *out = b;
            return true;
        }
    }

    if(a <= 1000UL) {
        b = a * 1000000UL;
        if(furi_hal_subghz_is_frequency_valid(b)) {
            *out = b;
            return true;
        }
    }

    return false;
}

void freq_menu_build(FlipperHamApp* app) {
    uint8_t i;

    submenu_reset(app->freq_menu);

    if(app->freq_n < FREQ_N)
        submenu_add_item_ex(app->freq_menu, "Add new...", FlipperHamFreqIndexAdd, freq_pick, app);

    for(i = 0; i < FREQ_N; i++) {
        if(!app->freq_used[i]) continue;
        if(app->tx_freq_index == i)
            snprintf(
                app->freq_s[i],
                sizeof(app->freq_s[i]),
                "%06lu.%1lu *",
                (unsigned long)(app->freq[i] / 1000UL),
                (unsigned long)((app->freq[i] % 1000UL) / 100UL));
        else
            freq_show(app->freq_s[i], sizeof(app->freq_s[i]), app->freq[i]);
        submenu_add_item_ex(
            app->freq_menu, app->freq_s[i], FlipperHamFreqIndexBase + i, freq_pick, app);
    }

    if(app->freq_n < FREQ_N)
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexAdd);
    else
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + app->tx_freq_index);

    if(app->freq_sel >= FlipperHamFreqIndexBase) {
        i = app->freq_sel - FlipperHamFreqIndexBase;
        if(i < FREQ_N)
            if(app->freq_used[i]) submenu_set_selected_item(app->freq_menu, app->freq_sel);
    }
}

void freq_edit_menu_build(FlipperHamApp* app) {
    VariableItem* it;

    variable_item_list_reset(app->freq_edit_menu);

    it = variable_item_list_add(app->freq_edit_menu, "Frequency", 201, freq_change, app);
    variable_item_set_current_value_index(it, 100);
    fsh2(app->f_edit, sizeof(app->f_edit), app->freq_edit_hz);
    variable_item_set_current_value_text(it, app->f_edit);

    it = variable_item_list_add(app->freq_edit_menu, "Enter frequency", 1, NULL, NULL);
    variable_item_set_current_value_index(it, 0);
    variable_item_set_current_value_text(it, app->f_bad ? "bad" : "");

    variable_item_list_add(app->freq_edit_menu, "Save", 1, NULL, NULL);
    variable_item_list_add(app->freq_edit_menu, "Use this for TX", 1, NULL, NULL);

    if(app->freq_n > 1)
        if(app->freq_index < FREQ_N)
            if(app->freq_used[app->freq_index])
                variable_item_list_add(app->freq_edit_menu, "Delete", 1, NULL, NULL);
    variable_item_list_set_selected_item(app->freq_edit_menu, 0);
}

void freq_edit_enter(void* context, uint32_t index) {
    FlipperHamApp* app = context;
    bool a;

    a = false;
    if(app->freq_n > 1)
        if(app->freq_index < FREQ_N)
            if(app->freq_used[app->freq_index]) a = true;

    if(index == 0) {
        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->f_bad = false;
        app->freq_edit_hz = freq_next_band(app->freq_edit_hz);
        freq_edit_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
        return;
    }

    if(index == 1) {
        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->f_edit[0] = 0;
        app->text_mode = 5;
        app->text_view = FlipperHamViewFreqEdit;
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Frequency");
        text_input_set_result_callback(
            app->text_input, freq_save, app, app->f_edit, sizeof(app->f_edit), false);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewTextInput);
        return;
    }

    if(index == 2) {
        if(app->f_bad) return;
        if(app->freq_index >= FREQ_N) return;
        if(!furi_hal_subghz_is_frequency_valid(app->freq_edit_hz)) return;

        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->freq[app->freq_index] = app->freq_edit_hz;
        app->freq_used[app->freq_index] = 1;
        freq_fix(app);
        cfgsave(app);
        freq_menu_build(app);
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + app->freq_index);
        settings_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreq);
        return;
    }

    if(index == 3) {
        if(app->f_bad) return;
        if(app->freq_index >= FREQ_N) return;
        if(!furi_hal_subghz_is_frequency_valid(app->freq_edit_hz)) return;

        app->freq_sel = FlipperHamFreqIndexBase + app->freq_index;
        app->freq[app->freq_index] = app->freq_edit_hz;
        app->freq_used[app->freq_index] = 1;
        app->tx_freq_index = app->freq_index;
        freq_fix(app);
        cfgsave(app);
        freq_menu_build(app);
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + app->freq_index);
        settings_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreq);
        return;
    }

    if(a && index == 4) {
        app->freq_sel = FlipperHamFreqIndexAdd;
        app->freq[app->freq_index] = 0;
        app->freq_used[app->freq_index] = 0;
        freq_fix(app);
        cfgsave(app);
        freq_menu_build(app);
        settings_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreq);
        return;
    }
}

void freq_pick(void* context, InputType input_type, uint32_t index) {
    FlipperHamApp* app = context;
    uint8_t a;

    if(index == FlipperHamFreqIndexAdd) {
        app->freq_sel = FlipperHamFreqIndexAdd;
        app->freq_index = 0xff;
        app->f_bad = false;

        for(a = 0; a < FREQ_N; a++)
            if(!app->freq_used[a]) {
                app->freq_index = a;
                break;
            }

        if(app->freq_index == 0xff) return;

        app->freq_edit_hz = tx_freq_get(app);
        freq_edit_menu_build(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
        return;
    }

    a = index - FlipperHamFreqIndexBase;
    if(a >= FREQ_N) return;
    if(!app->freq_used[a]) return;

    app->freq_sel = index;

    if(input_type == InputTypeLong) {
        app->tx_freq_index = a;
        app->f_bad = false;
        cfgsave(app);
        freq_menu_build(app);
        settings_menu_build(app);
        submenu_set_selected_item(app->freq_menu, FlipperHamFreqIndexBase + a);
        return;
    }

    if(input_type != InputTypeShort) return;

    app->freq_index = a;
    app->freq_edit_hz = app->freq[a];
    app->f_bad = false;
    freq_edit_menu_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
}

void freq_save(void* context) {
    FlipperHamApp* app = context;
    uint32_t a;

    if(freq_parse(app->f_edit, &a)) {
        app->freq_edit_hz = a;
        app->f_bad = false;
    } else
        app->f_bad = true;

    freq_edit_menu_build(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewFreqEdit);
}
