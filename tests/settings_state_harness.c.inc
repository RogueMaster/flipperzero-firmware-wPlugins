#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "uhf_types.h"
#include "uhf_frequency.h"

#define UHF_CMD_SET_FREQUENCY 0x78U
#define UHF_CMD_GET_FREQUENCY 0x79U
#define UHF_CMD_SET_POWER 0x76U
#define UHF_CMD_SET_BUZZER 0x7AU
#define UHF_CMD_SAVE_PARAMS 0x4AU
#define UHF_CMD_RESET 0x70U

typedef enum {InputKeyUp, InputKeyDown, InputKeyRight, InputKeyLeft, InputKeyOk, InputKeyBack} InputKey;
typedef enum {InputTypePress, InputTypeRelease, InputTypeShort, InputTypeLong, InputTypeRepeat} InputType;
typedef struct {InputKey key; InputType type;} InputEvent;
typedef enum {UhfPageMainMenu, UhfPageSettings, UhfPageHomeSettings} UhfPage;
typedef struct {
    bool sound;
    bool indicator;
    uint8_t power;
    UhfStartupApp startup;
    UhfEpcDisplay display;
    bool confirm;
    UhfFrequency frequency;
} UhfSettingsSnapshot;
typedef enum {
    UhfReaderIndicator, UhfReaderRegion, UhfReaderStart, UhfReaderSpacing,
    UhfReaderCount, UhfReaderPower, UhfReaderReset, UhfReaderRowCount,
} UhfReaderSetting;
typedef struct {
    bool tag_beep_enabled, reader_buzzer_enabled;
    uint8_t reader_power_dbm;
    UhfStartupApp startup_app;
    UhfEpcDisplay epc_display;
    bool action_confirm;
    UhfFrequency reader_frequency, custom_frequency;
    bool frequency_valid, power_valid, reader_frequency_valid;
    bool inventory_running, settings_reader_mode;
    uint8_t settings_selected, settings_region_choice;
    UhfSettingsSnapshot settings_initial, settings_draft;
    bool settings_save_prompt, settings_hardware_changed, settings_number_edit;
    bool settings_feedback, settings_power_valid, settings_message_active;
    uint8_t settings_apply_mask;
    uint32_t settings_number_value;
    uint8_t settings_number_digit;
    char settings_error[32], status[128];
    uint8_t tag_reply[128];
    size_t tag_reply_len;
    UhfPage page, feature_return_page;
} UhfApp;

typedef struct {
    UhfFrequency frequency, flash_frequency;
    uint8_t power, flash_power;
    bool indicator, flash_indicator;
    UhfSettingsSnapshot local;
    unsigned frequency_sets, power_sets, indicator_sets, reader_saves, local_saves, resets;
    bool fail_query, fail_power, fail_indicator, fail_frequency, fail_reader_save, fail_local_save;
    unsigned fail_power_remaining;
    bool fail_frequency_query;
} Mock;
static Mock mock;

static bool uhf_ensure_hardware(UhfApp* app) {(void)app; return true;}
static void uhf_stop_inventory(UhfApp* app) {app->inventory_running = false;}
static void uhf_set_status(UhfApp* app, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(app->status, sizeof(app->status), format, args);
    va_end(args);
}
static void uhf_notify_result_sound(UhfApp* app, bool success) {(void)app; (void)success;}
static bool uhf_query_reader_power(UhfApp* app, bool wait) {
    (void)wait;
    if(mock.fail_query) return false;
    app->reader_power_dbm = mock.power;
    app->power_valid = true;
    return true;
}
static bool uhf_set_reader_power(UhfApp* app, uint8_t power) {
    mock.power_sets++;
    if(mock.fail_power_remaining) {mock.fail_power_remaining--; return false;}
    if(mock.fail_power) return false;
    assert(power <= 20U);
    mock.power = power;
    app->reader_power_dbm = power;
    app->power_valid = true;
    return true;
}
static bool uhf_set_reader_buzzer(UhfApp* app) {
    mock.indicator_sets++;
    if(mock.fail_indicator) return false;
    mock.indicator = app->reader_buzzer_enabled;
    return true;
}
static bool uhf_save_reader_params(UhfApp* app) {
    (void)app;
    mock.reader_saves++;
    if(mock.fail_reader_save) return false;
    mock.flash_frequency = mock.frequency;
    mock.flash_power = mock.power;
    mock.flash_indicator = mock.indicator;
    return true;
}
static bool uhf_reset_reader_command(UhfApp* app) {
    (void)app;
    mock.resets++;
    mock.frequency = mock.flash_frequency;
    mock.power = mock.flash_power;
    mock.indicator = mock.flash_indicator;
    return true;
}
static bool uhf_wait_tag_reply(UhfApp* app, uint8_t command, const uint8_t* payload,
    size_t length, uint32_t timeout) {
    (void)timeout;
    app->tag_reply_len = 0;
    if(command == UHF_CMD_GET_FREQUENCY) {
        if(mock.fail_query || mock.fail_frequency_query) return false;
        return uhf_frequency_encode(&mock.frequency, app->tag_reply, sizeof(app->tag_reply), &app->tag_reply_len);
    }
    assert(command == UHF_CMD_SET_FREQUENCY);
    mock.frequency_sets++;
    if(mock.fail_frequency) return false;
    assert(uhf_frequency_parse(payload, length, &mock.frequency));
    app->tag_reply[0] = 0x10;
    app->tag_reply_len = 1;
    return true;
}
static bool uhf_save_settings_snapshot(const UhfSettingsSnapshot* snapshot) {
    mock.local_saves++;
    if(mock.fail_local_save) return false;
    mock.local = *snapshot;
    return true;
}

/* APPLICATION_SETTINGS_BLOCK */

static void key(UhfApp* app, InputKey key, InputType type) {
    const InputEvent event = {.key = key, .type = type};
    assert(uhf_handle_settings(app, &event));
}
static void short_key(UhfApp* app, InputKey input) {key(app, input, InputTypeShort);}
static UhfApp fixture(void) {
    memset(&mock, 0, sizeof(mock));
    assert(uhf_frequency_preset(UHF_FREQUENCY_REGION_FCC, &mock.frequency));
    mock.flash_frequency = mock.frequency;
    mock.power = mock.flash_power = 20;
    mock.indicator = mock.flash_indicator = true;
    UhfApp app = {
        .tag_beep_enabled = true, .reader_buzzer_enabled = true,
        .reader_power_dbm = 20, .power_valid = true,
        .feature_return_page = UhfPageHomeSettings,
        .action_confirm = true,
    };
    return app;
}
static void open_reader(UhfApp* app) {
    uhf_settings_begin(app, true);
    assert(app->page == UhfPageSettings);
}
static void edit_power(UhfApp* app, bool increase) {
    app->settings_selected = UhfReaderPower;
    short_key(app, increase ? InputKeyRight : InputKeyLeft);
}
static void exit_prompt(UhfApp* app) {
    short_key(app, InputKeyBack);
    assert(app->page == UhfPageSettings);
    assert(app->settings_save_prompt);
}
static void assert_reader(const UhfFrequency* frequency, uint8_t power, bool indicator) {
    assert(uhf_frequency_equal(&mock.frequency, frequency));
    assert(mock.power == power);
    assert(mock.indicator == indicator);
}

static void test_no_change_and_revert(void) {
    UhfApp app = fixture();
    open_reader(&app);
    assert(!uhf_settings_dirty(&app));
    short_key(&app, InputKeyBack);
    assert(app.page == UhfPageHomeSettings);
    assert(mock.reader_saves == 0 && mock.local_saves == 0);
    open_reader(&app);
    edit_power(&app, false);
    assert(uhf_settings_dirty(&app));
    edit_power(&app, true);
    assert(!uhf_settings_dirty(&app));
    short_key(&app, InputKeyBack);
    assert(app.page == UhfPageHomeSettings);
}
static void test_draft_and_discard(void) {
    UhfApp app = fixture();
    open_reader(&app);
    const UhfFrequency original = mock.frequency;
    edit_power(&app, false);
    app.settings_selected = UhfReaderIndicator;
    short_key(&app, InputKeyRight);
    app.settings_selected = UhfReaderRegion;
    short_key(&app, InputKeyRight);
    assert(app.reader_power_dbm == 20);
    assert(app.reader_buzzer_enabled);
    assert(uhf_frequency_equal(&app.reader_frequency, &original));
    assert_reader(&original, 20, true);
    assert(mock.frequency_sets == 0 && mock.power_sets == 0 && mock.indicator_sets == 0);
    exit_prompt(&app);
    short_key(&app, InputKeyBack);
    assert(!app.settings_save_prompt && uhf_settings_dirty(&app));
    exit_prompt(&app);
    short_key(&app, InputKeyLeft);
    assert(app.page == UhfPageHomeSettings);
    assert_reader(&original, 20, true);
    assert(mock.local_saves == 0);
}
static void test_save_and_local_failure(void) {
    UhfApp app = fixture();
    open_reader(&app);
    edit_power(&app, false);
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageHomeSettings);
    assert(mock.power == 19 && mock.flash_power == 19 && mock.local.power == 19);
    assert(app.reader_power_dbm == 19);

    app = fixture();
    open_reader(&app);
    const UhfFrequency original = mock.frequency;
    edit_power(&app, false);
    mock.fail_local_save = true;
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageSettings && app.settings_save_prompt);
    assert_reader(&original, 20, true);
    assert(mock.flash_power == 20);
    assert(app.settings_draft.power == 19);
    assert(app.reader_power_dbm == 20);
    mock.fail_local_save = false;
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageHomeSettings);
    assert(mock.power == 19 && mock.local.power == 19);
}
static void test_reset_preserves_intended_power(void) {
    UhfApp app = fixture();
    mock.flash_power = 10;
    open_reader(&app);
    app.settings_selected = UhfReaderReset;
    key(&app, InputKeyOk, InputTypeLong);
    assert(mock.resets == 1);
    assert(app.settings_message_active);
    short_key(&app, InputKeyOk);
    assert(app.settings_draft.power == 20);
    app.settings_selected = UhfReaderRegion;
    short_key(&app, InputKeyRight);
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageHomeSettings);
    assert(mock.power == 20 && mock.flash_power == 20 && mock.local.power == 20);
}
static void test_query_retry_custom_cache(void) {
    UhfApp app = fixture();
    mock.frequency = (UhfFrequency){.region = UHF_FREQUENCY_REGION_CUSTOM,
        .start_khz = 915000, .spacing_khz = 250, .count = 5};
    const UhfFrequency queried = mock.frequency;
    mock.fail_query = true;
    open_reader(&app);
    assert(!app.frequency_valid);
    mock.fail_query = false;
    app.settings_selected = UhfReaderRegion;
    short_key(&app, InputKeyOk);
    assert(app.frequency_valid);
    assert(app.settings_message_active);
    short_key(&app, InputKeyOk);
    assert(uhf_frequency_equal(&app.settings_draft.frequency, &queried));
    short_key(&app, InputKeyRight);
    short_key(&app, InputKeyLeft);
    assert(uhf_frequency_equal(&app.settings_draft.frequency, &queried));
}
static void test_partial_apply_failure(void) {
    UhfApp app = fixture();
    open_reader(&app);
    const UhfFrequency original = mock.frequency;
    edit_power(&app, false);
    app.settings_selected = UhfReaderRegion;
    short_key(&app, InputKeyRight);
    const UhfFrequency wanted = app.settings_draft.frequency;
    mock.fail_power_remaining = 1;
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageSettings && app.settings_save_prompt);
    assert_reader(&original, 20, true);
    assert(app.settings_draft.power == 19);
    assert(uhf_frequency_equal(&app.settings_draft.frequency, &wanted));
    assert(mock.local_saves == 0);
    short_key(&app, InputKeyLeft);
    assert(app.page == UhfPageHomeSettings);
    assert_reader(&original, 20, true);
}
static void test_pending_rollback_forces_prompt(void) {
    UhfApp app = fixture();
    open_reader(&app);
    edit_power(&app, false);
    app.settings_selected = UhfReaderRegion;
    short_key(&app, InputKeyRight);
    mock.fail_power = true;
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageSettings);
    assert(app.settings_hardware_changed);
    short_key(&app, InputKeyBack);
    app.settings_draft = app.settings_initial;
    assert(uhf_settings_dirty(&app));
    exit_prompt(&app);
    mock.fail_power = false;
    short_key(&app, InputKeyLeft);
    assert(app.page == UhfPageHomeSettings);
    assert(!app.settings_hardware_changed);
    assert(mock.power == 20);
}
static void test_live_telemetry_cannot_replace_draft(void) {
    UhfApp app = fixture();
    open_reader(&app);
    edit_power(&app, false);
    /* Simulate a late 0x77 response updating actual telemetry after editing. */
    app.reader_power_dbm = mock.power = 17;
    assert(app.settings_draft.power == 19);
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageHomeSettings);
    assert(mock.power == 19 && mock.local.power == 19);
}
static void test_number_editor_accept_and_cancel(void) {
    UhfApp app = fixture();
    mock.frequency = (UhfFrequency){.region = UHF_FREQUENCY_REGION_CUSTOM,
        .start_khz = 915000, .spacing_khz = 250, .count = 5};
    open_reader(&app);
    const UhfFrequency before = app.settings_draft.frequency;
    app.settings_selected = UhfReaderStart;
    short_key(&app, InputKeyOk);
    assert(app.settings_number_edit);
    short_key(&app, InputKeyUp);
    assert(app.settings_number_value != before.start_khz);
    assert(uhf_frequency_equal(&app.settings_draft.frequency, &before));
    short_key(&app, InputKeyBack);
    assert(!app.settings_number_edit);
    assert(!uhf_settings_dirty(&app));
    short_key(&app, InputKeyOk);
    short_key(&app, InputKeyUp);
    const uint32_t edited = app.settings_number_value;
    short_key(&app, InputKeyOk);
    assert(!app.settings_number_edit);
    assert(app.settings_draft.frequency.start_khz == edited);
    assert(uhf_frequency_equal(&app.reader_frequency, &before));
    assert(mock.frequency_sets == 0);
    short_key(&app, InputKeyOk);
    app.settings_number_value = 960000;
    short_key(&app, InputKeyOk);
    assert(app.settings_number_edit);
    assert(app.settings_error[0]);
    assert(app.settings_draft.frequency.start_khz == edited);
}
static void test_app_settings_draft_and_save(void) {
    UhfApp app = fixture();
    uhf_settings_begin(&app, false);
    short_key(&app, InputKeyRight);
    assert(app.tag_beep_enabled);
    assert(!app.settings_draft.sound);
    exit_prompt(&app);
    short_key(&app, InputKeyLeft);
    assert(app.page == UhfPageHomeSettings && app.tag_beep_enabled);
    assert(mock.local_saves == 0 && mock.power_sets == 0);
    uhf_settings_begin(&app, false);
    short_key(&app, InputKeyRight);
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageHomeSettings && !app.tag_beep_enabled);
    assert(!mock.local.sound);
    assert(mock.local_saves == 1 && mock.reader_saves == 0);
}
static void test_reset_query_failure_does_not_skip_frequency(void) {
    UhfApp app = fixture();
    assert(uhf_frequency_preset(UHF_FREQUENCY_REGION_ETSI, &mock.flash_frequency));
    const UhfFrequency intended = mock.frequency;
    open_reader(&app);
    mock.fail_frequency_query = true;
    app.settings_selected = UhfReaderReset;
    key(&app, InputKeyOk, InputTypeLong);
    assert(app.settings_message_active);
    short_key(&app, InputKeyOk);
    mock.fail_frequency_query = false;
    edit_power(&app, false);
    exit_prompt(&app);
    short_key(&app, InputKeyRight);
    assert(app.page == UhfPageHomeSettings);
    assert(uhf_frequency_equal(&mock.frequency, &intended));
    assert(uhf_frequency_equal(&mock.flash_frequency, &intended));
}
int main(void) {
    test_no_change_and_revert();
    test_draft_and_discard();
    test_save_and_local_failure();
    test_reset_preserves_intended_power();
    test_query_retry_custom_cache();
    test_partial_apply_failure();
    test_pending_rollback_forces_prompt();
    test_live_telemetry_cannot_replace_draft();
    test_number_editor_accept_and_cancel();
    test_app_settings_draft_and_save();
    test_reset_query_failure_does_not_skip_frequency();
    puts("settings editor: all host regression tests passed");
    return 0;
}
