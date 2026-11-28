#include "../tpms_app_i.h"
#include "../tpms_editor_tx.h"
#include "../tpms_encoder.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    TPMSTestItemProtocol = 0,
    TPMSTestItemRepeat,
    TPMSTestItemSendOne,
    TPMSTestItemSendAll,
} TPMSTestItem;

typedef struct {
    size_t selected_index;
    size_t auto_index;
    uint8_t repeat_count;
    uint8_t failed;
    uint32_t next_tx_tick;
    bool running_all;
    VariableItem* send_one_item;
    VariableItem* send_all_item;
} TPMSTestState;

static TPMSTestState test_state;

static void tpms_test_update_protocol_text(VariableItem* item) {
    const TpmsEncoderKind kind = tpms_encoder_test_kind(test_state.selected_index);
    variable_item_set_current_value_text(item, tpms_encoder_display_name(kind));
}

static void tpms_test_protocol_changed(VariableItem* item) {
    test_state.selected_index = variable_item_get_current_value_index(item);
    tpms_test_update_protocol_text(item);
}

static void tpms_test_repeat_changed(VariableItem* item) {
    const uint8_t index = variable_item_get_current_value_index(item);
    test_state.repeat_count = index ? 10U : 3U;
    variable_item_set_current_value_text(item, index ? "10x" : "3x");
}

static bool tpms_test_send_kind(TPMSApp* app, TpmsEncoderKind kind) {
    TpmsEditValues values;
    tpms_encoder_default_values(kind, &values);
    values.frequency_hz = app->txrx->preset->frequency;
    values.repeat_count = test_state.repeat_count;
    return tpms_editor_send(app, &values);
}

static void tpms_test_enter_callback(void* context, uint32_t index) {
    TPMSApp* app = context;
    if(test_state.running_all) return;

    if(index == TPMSTestItemSendOne) {
        const TpmsEncoderKind kind = tpms_encoder_test_kind(test_state.selected_index);
        variable_item_set_current_value_text(test_state.send_one_item, "SENDING");
        const bool ok = tpms_test_send_kind(app, kind);
        if(ok) {
            variable_item_set_current_value_text(
                test_state.send_one_item,
                tpms_radio_is_external(app) ? "TX EXT OK" : "TX INT OK");
        } else {
            variable_item_set_current_value_text(test_state.send_one_item, "TX ERROR");
        }
        notification_message(
            app->notifications, ok ? &sequence_blink_green_10 : &sequence_blink_red_10);
    } else if(index == TPMSTestItemSendAll) {
        test_state.auto_index = 0U;
        test_state.failed = 0U;
        test_state.next_tx_tick = furi_get_tick();
        test_state.running_all = true;
        char text[24];
        snprintf(text, sizeof(text), "START 0/%u", (unsigned)tpms_encoder_test_count());
        variable_item_set_current_value_text(test_state.send_all_item, text);
    }
}

void tpms_scene_protocol_test_on_enter(void* context) {
    TPMSApp* app = context;
    memset(&test_state, 0, sizeof(test_state));
    test_state.repeat_count = 3U;

    VariableItem* item = variable_item_list_add(
        app->variable_item_list,
        "Protocol",
        (uint8_t)tpms_encoder_test_count(),
        tpms_test_protocol_changed,
        app);
    variable_item_set_current_value_index(item, 0U);
    tpms_test_update_protocol_text(item);

    item = variable_item_list_add(
        app->variable_item_list, "Repeats", 2U, tpms_test_repeat_changed, app);
    variable_item_set_current_value_index(item, 0U);
    variable_item_set_current_value_text(item, "3x");

    test_state.send_one_item =
        variable_item_list_add(app->variable_item_list, "Send selected", 1U, NULL, app);
    variable_item_set_current_value_text(test_state.send_one_item, "OK");

    test_state.send_all_item =
        variable_item_list_add(app->variable_item_list, "Test 18 protocols", 1U, NULL, app);
    variable_item_set_current_value_text(test_state.send_all_item, "OK");

    variable_item_list_set_enter_callback(
        app->variable_item_list, tpms_test_enter_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewVariableItemList);
}

bool tpms_scene_protocol_test_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(event.type != SceneManagerEventTypeTick || !test_state.running_all) return false;

    const size_t count = tpms_encoder_test_count();
    const uint32_t now = furi_get_tick();
    if((int32_t)(now - test_state.next_tx_tick) < 0) return true;

    if(test_state.auto_index >= count) {
        char text[20];
        if(test_state.failed) {
            snprintf(text, sizeof(text), "DONE ERR:%u", test_state.failed);
        } else {
            snprintf(text, sizeof(text), "DONE %u/%u", (unsigned)count, (unsigned)count);
        }
        variable_item_set_current_value_text(test_state.send_all_item, text);
        notification_message(
            app->notifications,
            test_state.failed ? &sequence_blink_red_10 : &sequence_blink_green_10);
        test_state.running_all = false;
        return true;
    }

    const TpmsEncoderKind kind = tpms_encoder_test_kind(test_state.auto_index);
    const bool ok = tpms_test_send_kind(app, kind);
    if(!ok) test_state.failed++;
    test_state.auto_index++;
    /* Keep protocols clearly separated in HackRF recordings. */
    test_state.next_tx_tick = furi_get_tick() + furi_ms_to_ticks(1200U);

    char text[20];
    snprintf(
        text,
        sizeof(text),
        "%u/%u %s",
        (unsigned)test_state.auto_index,
        (unsigned)count,
        ok ? "OK" : "ERROR");
    variable_item_set_current_value_text(test_state.send_all_item, text);
    return true;
}

void tpms_scene_protocol_test_on_exit(void* context) {
    TPMSApp* app = context;
    test_state.running_all = false;
    test_state.send_one_item = NULL;
    test_state.send_all_item = NULL;
    variable_item_list_set_selected_item(app->variable_item_list, 0U);
    variable_item_list_reset(app->variable_item_list);
}
