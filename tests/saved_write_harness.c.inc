/* Host scaffolding; the production functions are inserted below unchanged. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uhf_saved_tag.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define UHF_SINGLE_TAG_STABLE_MS 800U
#define UHF_STOP_SETTLE_MS 50U
#define UHF_TAG_DATA_HEX_PER_LINE 12U
#define UHF_CMD_WRITE_TAG 0x82U

typedef enum {
    UhfPageOther, UhfPageSavedList, UhfPageSavedMenu, UhfPageSavedView,
    UhfPageSavedDelete, UhfPageSavedResult,
} UhfPage;
typedef enum {InputTypeShort, InputTypeLong} InputType;
typedef enum {InputKeyBack, InputKeyOk, InputKeyLeft, InputKeyRight, InputKeyUp, InputKeyDown} InputKey;
typedef struct {InputType type; InputKey key;} InputEvent;
typedef struct {bool unused;} UhfSavedSession;
typedef struct {bool write_visible;} Canvas;
enum {FontPrimary, FontSecondary, FontKeyboard, ColorWhite, ColorBlack};
typedef struct {
    UhfSavedTag saved_tag, saved_live_context;
    UhfTagEntry saved_write_targets[4];
    char access_epc[UHF_EPC_HEX_MAX + 1U], selected_epc[UHF_EPC_HEX_MAX + 1U];
    char selected_tid[UHF_TID_HEX_MAX + 1U], selected_user[UHF_USER_HEX_MAX + 1U];
    char status[128], write_popup_text[128], saved_result[128];
    bool selected_tid_valid, selected_user_valid, saved_live_tid_valid, saved_live_user_valid;
    bool saved_live_unfiltered, tag_access_unfiltered, epc_filter_active;
    bool saved_write_active, saved_write_capturing, saved_write_overflow, inventory_running;
    bool tag_write_led_active, tag_reply_ready, memory_cache_valid[4];
    bool saved_list_ok, action_confirm, write_popup_active;
    uint32_t saved_write_started, saved_write_stable_since, saved_write_overflow_tick;
    uint32_t selected_rssi;
    uint16_t selected_pc, saved_id, saved_ids[UHF_SAVED_TAG_LIMIT];
    uint8_t saved_bank, saved_action, access_keys[UHF_KEY_SLOT_COUNT][4], active_key_slot;
    uint8_t tag_reply[16];
    size_t tag_reply_len, saved_write_count, saved_count, saved_selected, saved_visible_top, saved_scroll;
    char saved_visible_labels[4][32];
    UhfTagBank tag_bank, saved_live_bank;
    UhfPage page, saved_list_return, saved_result_return;
    void* view_port;
} UhfApp;

static uint32_t tick;
static bool clear_fail, select_fail, late_report, reader_unavailable, write_fail, verify_fail;
static unsigned writes, selections, clears, readbacks, inventory_starts, failures;
static unsigned fail_clear_number;
static uint8_t written[11U + UHF_USER_HEX_MAX / 2U];
static size_t written_len;
static char selected_filter[UHF_EPC_HEX_MAX + 1U];
static int uhf_led_write_start;
static bool uhf_capture_saved_write_report(UhfApp*, const char*, uint16_t, uint32_t);
static const char* uhf_saved_write_value(const UhfApp*);

static uint32_t furi_get_tick(void) {return tick;}
static void furi_delay_ms(uint32_t delay) {tick += delay;}
static void uhf_set_status(UhfApp* app, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->status, sizeof(app->status), fmt, args);
    va_end(args);
}
static void uhf_notify_write_result(UhfApp* app, bool ok) {if(!ok) failures++;}
static void uhf_stop_inventory(UhfApp* app) {app->inventory_running = false;}
static void uhf_start_inventory(UhfApp* app) {
    inventory_starts++;
    app->inventory_running = !reader_unavailable;
}
static void uhf_clear_selected_epc_filter(UhfApp* app) {
    clears++;
    if(!clear_fail && clears != fail_clear_number) app->epc_filter_active = false;
}
static bool uhf_set_selected_epc_filter(UhfApp* app) {
    selections++;
    assert(app->access_epc[0]);
    assert(!app->tag_access_unfiltered);
    snprintf(selected_filter, sizeof(selected_filter), "%s", app->access_epc);
    if(select_fail) return false;
    app->epc_filter_active = true;
    return true;
}
static void uhf_process_rx(UhfApp* app) {
    if(late_report) {
        late_report = false;
        assert(uhf_capture_saved_write_report(app, "BBBB", 0x0800U, 50U));
    }
}
static const char* uhf_current_tag_bank_name(UhfTagBank bank) {
    return bank == UhfTagBankEpc ? "EPC" : bank == UhfTagBankTid ? "TID" :
           bank == UhfTagBankUser ? "User" : "Reserved";
}
static bool uhf_hex_to_bytes(const char* hex, uint8_t* out, size_t cap, size_t* length) {
    size_t len = strlen(hex);
    if((len & 1U) || len / 2U > cap) return false;
    for(size_t i = 0; i < len / 2U; i++) {
        unsigned value;
        if(sscanf(hex + 2U * i, "%2x", &value) != 1) return false;
        out[i] = (uint8_t)value;
    }
    *length = len / 2U;
    return true;
}
static bool uhf_wait_tag_reply(UhfApp* app, uint8_t cmd, const uint8_t* payload, size_t len, uint32_t timeout) {
    assert(cmd == UHF_CMD_WRITE_TAG);
    assert(app->epc_filter_active && !app->tag_access_unfiltered);
    assert(len <= sizeof(written));
    writes++;
    memcpy(written, payload, len);
    written_len = len;
    app->tag_reply_len = 1U;
    app->tag_reply[0] = write_fail ? 0x16U : 0x10U;
    return true;
}
static bool uhf_read_selected_bank_words(
    UhfApp* app, uint8_t bank, uint8_t address, uint8_t words,
    char* out, size_t cap, const char* name, bool recover) {
    readbacks++;
    assert(bank == written[4] && address == written[8] && words == written[10]);
    assert(cap > (size_t)words * 4U);
    for(size_t i = 0; i < (size_t)words * 2U; i++) snprintf(out + i * 2U, cap - i * 2U, "%02X", written[11U + i]);
    return !verify_fail;
}
static bool uhf_force_clear_epc_filter(UhfApp* app) {uhf_clear_selected_epc_filter(app); return !app->epc_filter_active;}
static void uhf_settle_reader(UhfApp* app) {}
static void uhf_begin_s0_session(UhfApp* app, UhfSavedSession* session) {}
static void uhf_restore_session(UhfApp* app, const UhfSavedSession* session) {}
static const char* uhf_tag_error_text(uint8_t status) {return "mock write error";}
static void uhf_led_send(UhfApp* app, const void* led) {}
static void view_port_update(void* view) {}

/* Drawing stubs retain the actual application visibility condition. */
static void canvas_set_font(Canvas* c, int font) {}
static void canvas_draw_str(Canvas* c, int x, int y, const char* text) {}
static void canvas_draw_rbox(Canvas* c, int x, int y, int w, int h, int r) {}
static void canvas_set_color(Canvas* c, int color) {}
static void canvas_draw_rframe(Canvas* c, int x, int y, int w, int h, int r) {}
static void canvas_draw_box(Canvas* c, int x, int y, int w, int h) {}
static int canvas_string_width(Canvas* c, const char* text) {return (int)strlen(text);}
static void elements_scrollbar_pos(Canvas* c, int x, int y, int h, size_t pos, size_t count) {}
static void elements_button_center(Canvas* c, const char* label) {}
static void elements_button_left(Canvas* c, const char* label) {}
static void elements_button_right(Canvas* c, const char* label) {assert(!strcmp(label, "Write")); c->write_visible = true;}
static void uhf_draw_centered_text(Canvas* c, int y, const char* text) {}
static void uhf_draw_right_fraction(Canvas* c, int y, size_t a, size_t b) {}
static void uhf_draw_action_menu(Canvas* c, const char* title, const char* const* items, size_t count, size_t selected) {}
static void uhf_format_list_epc_compact(Canvas* c, const char* text, int width, char* out, size_t cap) {snprintf(out, cap, "%s", text);}
static void uhf_format_hex_bytes_spaced(const char* text, size_t len, char* out, size_t cap) {out[0] = '\0';}
static void uhf_refresh_saved_visible_labels(UhfApp* app) {}
static void uhf_refresh_saved_tags(UhfApp* app) {}
static void uhf_delete_saved_tag(UhfApp* app) {}
static void uhf_edit_saved_bank(UhfApp* app) {}
bool uhf_saved_tag_load(uint16_t id, UhfSavedTag* tag) {return false;}

/* APPLICATION_SAVED_WRITE_BLOCK */

static UhfApp setup(uint8_t bank) {
    tick = 100U;
    clear_fail = select_fail = late_report = reader_unavailable = write_fail = verify_fail = false;
    writes = selections = clears = readbacks = inventory_starts = failures = fail_clear_number = 0;
    written_len = 0;
    memset(written, 0, sizeof(written));
    selected_filter[0] = '\0';
    UhfApp app = {.page = UhfPageSavedView, .saved_bank = bank, .tag_bank = UhfTagBankReserved,
                  .selected_pc = 0x1234, .selected_rssi = 42U, .tag_access_unfiltered = true,
                  .selected_tid_valid = true, .selected_user_valid = true};
    strcpy(app.access_epc, "CAFE");
    strcpy(app.selected_epc, "CAFE");
    strcpy(app.selected_tid, "1234");
    strcpy(app.selected_user, "5678");
    strcpy(app.saved_tag.epc, "AABB");
    strcpy(app.saved_tag.tid, "E280123456789ABC");
    strcpy(app.saved_tag.user, "010203040506");
    return app;
}
static void assert_restored(const UhfApp* app) {
    assert(!app->saved_write_active && !app->saved_write_capturing && !app->inventory_running);
    assert(!app->epc_filter_active);
    assert(!strcmp(app->access_epc, "CAFE") && !strcmp(app->selected_epc, "CAFE"));
    assert(!strcmp(app->selected_tid, "1234") && !strcmp(app->selected_user, "5678"));
    assert(app->selected_tid_valid && app->selected_user_valid);
    assert(app->selected_pc == 0x1234 && app->selected_rssi == 42U);
    assert(app->tag_bank == UhfTagBankReserved && app->tag_access_unfiltered);
}
static void press_write(UhfApp* app) {
    const InputEvent input = {.type = InputTypeShort, .key = InputKeyRight};
    assert(uhf_handle_saved_tags(app, &input));
}
static void target_ready(UhfApp* app, const char* epc) {
    assert(app->saved_write_active && app->saved_write_capturing);
    assert(uhf_capture_saved_write_report(app, epc, 0x0800U, 60U));
    uhf_service_saved_write_target(app);
    assert(!writes);
    tick += UHF_SINGLE_TAG_STABLE_MS;
    assert(uhf_capture_saved_write_report(app, epc, 0x0800U, 60U));
    uhf_service_saved_write_target(app);
}
static void assert_payload(const char* hex, uint8_t bank, uint8_t address) {
    uint8_t expected[UHF_USER_HEX_MAX / 2U];
    size_t len;
    assert(uhf_hex_to_bytes(hex, expected, sizeof(expected), &len));
    assert(writes == 1U && readbacks == 1U);
    assert(written_len == 11U + len && written[4] == bank && written[8] == address);
    assert(written[10] == len / 2U && !memcmp(written + 11U, expected, len));
}
static void test_selected_banks(void) {
    UhfApp app = setup(0U);
    /* Maximum EPC proves the old 12-byte clone truncation cannot return. */
    for(size_t i = 0; i < UHF_EPC_HEX_MAX; i++) app.saved_tag.epc[i] = "0123456789ABCDEF"[i % 16U];
    app.saved_tag.epc[UHF_EPC_HEX_MAX] = '\0';
    char target[UHF_EPC_HEX_MAX + 1U];
    memset(target, 'F', UHF_EPC_HEX_MAX); target[UHF_EPC_HEX_MAX] = '\0';
    press_write(&app); target_ready(&app, target);
    assert_payload(app.saved_tag.epc, 1U, 2U);
    assert_restored(&app);
    assert(!strstr(app.status, "Clone"));

    app = setup(2U);
    press_write(&app); target_ready(&app, "FFFF");
    assert_payload(app.saved_tag.user, 3U, 0U);
    assert(!strcmp(selected_filter, "FFFF"));
    assert_restored(&app);

    app = setup(1U); app.saved_tag.epc[0] = '\0';
    press_write(&app); target_ready(&app, "FFFF");
    assert_payload(app.saved_tag.tid, 2U, 0U);
    assert(!strcmp(selected_filter, "FFFF"));
    assert_restored(&app);
}
static void test_visibility_and_empty_buttons(void) {
    for(uint8_t bank = 0; bank < 4; bank++) {
        UhfApp app = setup(bank);
        Canvas canvas = {0};
        uhf_draw_saved_tags(&canvas, &app);
        assert(canvas.write_visible == (bank < 3U));
        if(bank < 3U) {
            char* value = bank == 0U ? app.saved_tag.epc : bank == 1U ? app.saved_tag.tid : app.saved_tag.user;
            value[0] = '\0';
        }
        canvas.write_visible = false;
        uhf_draw_saved_tags(&canvas, &app);
        assert(!canvas.write_visible);
        press_write(&app);
        assert(!writes && !inventory_starts && !selections && !clears && !app.saved_write_active);
    }
}
static void test_refusals(void) {
    UhfApp app = setup(0U);
    strcpy(app.saved_tag.epc, "AABBCCDD");
    press_write(&app); target_ready(&app, "FFFF");
    assert(!writes && !selections && !strcmp(app.status, "EPC length differs"));
    assert_restored(&app);

    for(uint8_t bank = 0; bank < 3U; bank++) {
        app = setup(bank);
        press_write(&app); target_ready(&app, "");
        assert(!writes && !selections && !strcmp(app.status, "Blank EPC target refused"));
        assert_restored(&app);
    }
    app = setup(0U); press_write(&app); late_report = true; target_ready(&app, "FFFF");
    assert(!writes && !selections && app.inventory_running && app.saved_write_capturing);
    assert(app.saved_write_stable_since == 0U && inventory_starts == 2U);
    assert(uhf_finish_saved_write(&app)); assert_restored(&app);
}
static void test_errors_and_cleanup(void) {
    UhfApp app = setup(2U); select_fail = true;
    press_write(&app); target_ready(&app, "FFFF");
    assert(!writes && !strcmp(app.status, "Target selection failed")); assert_restored(&app);

    app = setup(0U); app.epc_filter_active = true; clear_fail = true;
    press_write(&app);
    assert(!writes && !inventory_starts && app.saved_write_active);
    assert(!strcmp(app.status, "Clear target filter failed"));
    clear_fail = false;
    const InputEvent back = {.type = InputTypeShort, .key = InputKeyBack};
    assert(uhf_handle_saved_tags(&app, &back)); assert_restored(&app);

    app = setup(2U); write_fail = true;
    press_write(&app); target_ready(&app, "FFFF");
    assert(writes == 1U && !readbacks && failures); assert_restored(&app);

    app = setup(1U); verify_fail = true;
    press_write(&app); target_ready(&app, "FFFF");
    assert(writes == 1U && readbacks == 1U && failures); assert_restored(&app);

    app = setup(2U); fail_clear_number = 2U;
    press_write(&app); target_ready(&app, "FFFF");
    assert(writes == 1U && app.saved_write_active && app.epc_filter_active);
    assert(!strcmp(app.status, "Clear target filter failed"));
    fail_clear_number = 0U;
    assert(uhf_handle_saved_tags(&app, &back)); assert_restored(&app);

    app = setup(2U); reader_unavailable = true; press_write(&app);
    assert(!writes && !strcmp(app.status, "Reader unavailable")); assert_restored(&app);

    app = setup(1U); press_write(&app); tick += 10001U;
    uhf_service_saved_write_target(&app);
    assert(!writes && !strcmp(app.status, "Target capture timeout")); assert_restored(&app);
}
int main(void) {
    test_selected_banks();
    test_visibility_and_empty_buttons();
    test_refusals();
    test_errors_and_cleanup();
    puts("Saved Write extracted-function regressions passed");
    return 0;
}
