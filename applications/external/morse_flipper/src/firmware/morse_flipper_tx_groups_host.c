#include "morse_flipper_app_i.h"

#define MORSE_FLIPPER_TX_GROUPS_PLUGIN_PATH APP_ASSETS_PATH("plugins/morse_flipper_tx_groups.fal")

static const MfTxGroupsApi* mf_tx_groups_api;

static void
    mf_tx_groups_draw_prompt(Canvas* canvas, void* context, int32_t cx, int32_t cy, char ch) {
    morse_flipper_draw_straight_prompt(canvas, context, cx, cy, ch);
}

static char mf_tx_groups_answer_preview(void* context) {
    MorseFlipperApp* app = context;
    char preview;

    if(app == NULL || !app->txg_wait_answer) return '\0';
    preview = (char)morse_flipper_upper_char(morse_flipper_cw_decoder_preview(&app->tx_decoder));
    return preview == ' ' || preview == '|' ? '\0' : preview;
}

bool morse_flipper_tx_groups_host_enter(MorseFlipperApp* app) {
    MorseFlipperMappedFalResult initial = {0};
    MfTxGroupsEnterArgs args;
    bool entered;

    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    args.draw_services = (MfTxGroupsDrawServices){
        .context = app,
        .group = &app->tx_group,
        .sum_speed = &app->txg_sum_speed,
        .sum_lgap = &app->txg_sum_lgap,
        .sum_ratio = &app->txg_sum_ratio,
        .sum_accuracy = &app->txg_sum_accuracy,
        .sum_dgap = &app->txg_sum_dgap,
        .sum_variance = &app->txg_sum_variance,
        .session_total = &app->txg_session_total,
        .session_good = &app->txg_session_good,
        .session_sk = &app->txg_session_sk,
        .result_until = &app->txg_result_until,
        .screen = &app->screen,
        .input_source = &app->input_source,
        .started = &app->txg_started,
        .txg_sk = &app->txg_sk,
        .screen_practice = MorseFlipperScreenTxGroups,
        .screen_result = MorseFlipperScreenTxGroupsResult,
        .input_buttons = MorseFlipperInputSourceButtons,
        .prompt_width = MORSE_FLIPPER_TERMINUS24_WIDTH,
        .draw_prompt = mf_tx_groups_draw_prompt,
        .draw_history_divider = morse_flipper_draw_tx_history_divider,
        .draw_left_exit_hint = morse_flipper_draw_left_exit_hint,
        .answer_preview = mf_tx_groups_answer_preview,
    };
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    if(app->plugin_slot.owner == MorseFlipperPluginOwnerTxGroups &&
       app->plugin_slot.error == MorseFlipperPluginErrorNone && app->plugin_slot.api != NULL &&
       app->plugin_slot.state != NULL) {
        entered = true;
    } else {
        entered = morse_flipper_plugin_runtime_open_mapped_locked(
            app,
            MorseFlipperPluginOwnerTxGroups,
            0U,
            MORSE_FLIPPER_TX_GROUPS_PLUGIN_PATH,
            MF_TX_GROUPS_API_VERSION,
            MF_TX_GROUPS_API_MAGIC,
            sizeof(MfTxGroupsApi),
            &args,
            &initial);
    }
    mf_tx_groups_api = entered ? app->plugin_slot.api : NULL;
    furi_mutex_release(app->plugin_slot.mutex);
    return entered;
}

void morse_flipper_tx_groups_host_detach(void) {
    mf_tx_groups_api = NULL;
}

void morse_flipper_draw_tx_groups_screen(Canvas* canvas, MorseFlipperApp* app) {
    bool overlay;
    bool active;

    if(canvas == NULL || app == NULL || app->plugin_slot.mutex == NULL) return;
    overlay = app->screen == MorseFlipperScreenTxGroups && !app->txg_started &&
              (morse_flipper_gpio_probe_notice_active(app) ||
               morse_flipper_gpio_probe_blocks_start(app));
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    active = app->plugin_slot.owner == MorseFlipperPluginOwnerTxGroups &&
             app->plugin_slot.error == MorseFlipperPluginErrorNone &&
             app->plugin_slot.api != NULL && app->plugin_slot.state != NULL;
    if(active && !overlay)
        ((const MorseFlipperMappedFalApi*)app->plugin_slot.api)
            ->draw(app->plugin_slot.state, canvas, furi_get_tick());
    furi_mutex_release(app->plugin_slot.mutex);
    if(!active) {
        morse_flipper_draw_plugin_unavailable(canvas);
        return;
    }
    if(overlay) {
        morse_flipper_draw_gpio_probe_overlay(canvas, app);
        return;
    }
}

void morse_flipper_tx_group_init(MorseFlipperTxGroup* group) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->init(group);
}

void morse_flipper_tx_group_set_seed(MorseFlipperTxGroup* group, uint32_t seed) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->set_seed(group, seed);
}

void morse_flipper_tx_group_start(MorseFlipperTxGroup* group, bool sk) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->start(group, sk);
}

void morse_flipper_tx_group_feed_mark(MorseFlipperTxGroup* group, uint16_t ms) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->feed_mark(group, ms);
}

void morse_flipper_tx_group_feed_space(MorseFlipperTxGroup* group, uint16_t ms) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->feed_space(group, ms);
}

void morse_flipper_tx_group_feed_text(MorseFlipperTxGroup* group, const char* text) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->feed_text(group, text);
}

bool morse_flipper_tx_group_finalize_answer_from_raw(MorseFlipperTxGroup* group, uint16_t dit_ms) {
    return mf_tx_groups_api != NULL && mf_tx_groups_api->finalize_answer_from_raw(group, dit_ms);
}

void morse_flipper_tx_group_set_range(
    MorseFlipperTxGroup* group,
    uint8_t pass_min,
    uint8_t pass_max) {
    if(mf_tx_groups_api != NULL) {
        mf_tx_groups_api->set_range(group, pass_min, pass_max);
    }
}

void morse_flipper_tx_group_score(MorseFlipperTxGroup* group, uint16_t dit_ms, bool timed_out) {
    if(mf_tx_groups_api != NULL) mf_tx_groups_api->score(group, dit_ms, timed_out);
}

bool morse_flipper_tx_group_complete(const MorseFlipperTxGroup* group) {
    return mf_tx_groups_api != NULL && mf_tx_groups_api->complete(group);
}

bool morse_flipper_tx_group_marks_complete(const MorseFlipperTxGroup* group) {
    return mf_tx_groups_api != NULL && mf_tx_groups_api->marks_complete(group);
}
