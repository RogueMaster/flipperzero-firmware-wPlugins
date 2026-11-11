#include "ir_maker.h"

/* ------------------------------------------------------------------ */
/* Shared helpers                                                      */
/* ------------------------------------------------------------------ */

size_t ir_maker_valid_protocol_count(void) {
    size_t count = 0;
    for(InfraredProtocol p = 0; p < InfraredProtocolMAX; p++) {
        if(infrared_is_protocol_valid(p)) count++;
    }
    return count;
}

InfraredProtocol ir_maker_valid_protocol_at(size_t index) {
    size_t seen = 0;
    for(InfraredProtocol p = 0; p < InfraredProtocolMAX; p++) {
        if(!infrared_is_protocol_valid(p)) continue;
        if(seen == index) return p;
        seen++;
    }
    return InfraredProtocolNEC;
}

size_t ir_maker_valid_protocol_index(InfraredProtocol protocol) {
    size_t seen = 0;
    for(InfraredProtocol p = 0; p < InfraredProtocolMAX; p++) {
        if(!infrared_is_protocol_valid(p)) continue;
        if(p == protocol) return seen;
        seen++;
    }
    return 0;
}

size_t ir_maker_field_bytes(uint8_t bits) {
    size_t n = (bits + 7U) / 8U;
    if(n < 1U) n = 1U;
    if(n > 4U) n = 4U;
    return n;
}

void ir_maker_u32_to_hex_be(uint32_t value, uint8_t* buf, size_t n_bytes) {
    for(size_t i = 0; i < n_bytes; i++) {
        const size_t shift = (n_bytes - 1 - i) * 8U;
        buf[i] = (uint8_t)((value >> shift) & 0xFFU);
    }
}

uint32_t ir_maker_hex_be_to_u32(const uint8_t* buf, size_t n_bytes) {
    uint32_t value = 0;
    for(size_t i = 0; i < n_bytes; i++) {
        value = (value << 8U) | buf[i];
    }
    return value;
}

void ir_maker_clamp_message(InfraredMessage* msg) {
    if(!infrared_is_protocol_valid(msg->protocol)) return;
    const uint8_t addr_bits = infrared_get_protocol_address_length(msg->protocol);
    const uint8_t cmd_bits = infrared_get_protocol_command_length(msg->protocol);
    const uint32_t addr_mask = (addr_bits >= 32U) ? 0xFFFFFFFFUL : ((1UL << addr_bits) - 1UL);
    const uint32_t cmd_mask = (cmd_bits >= 32U) ? 0xFFFFFFFFUL : ((1UL << cmd_bits) - 1UL);
    msg->address &= addr_mask;
    msg->command &= cmd_mask;
}

void ir_maker_send_test(const InfraredSignal* signal) {
    furi_assert(signal);
    if(infrared_signal_is_raw(signal)) {
        const InfraredRawSignal* raw = infrared_signal_get_raw_signal(signal);
        infrared_send_raw_ext(
            raw->timings, raw->timings_size, true, raw->frequency, raw->duty_cycle);
    } else {
        const InfraredMessage* message = infrared_signal_get_message(signal);
        /* One button press: infrared_send already floors this at the protocol's
         * minimum repeat count, so a single call is exactly one shot. */
        int times = (int)infrared_get_protocol_min_repeat_count(message->protocol);
        if(times < 1) times = 1;
        infrared_send(message, times);
    }
}

void ir_maker_find_vacant_path(IrMakerApp* app, const char* suggestion, FuriString* out) {
    furi_string_printf(out, "%s/%s%s", IR_MAKER_FOLDER, suggestion, IR_MAKER_EXT);
    if(storage_common_stat(app->storage, furi_string_get_cstr(out), NULL) != FSE_OK) {
        return; // free
    }
    /* Suggestion taken: try "<suggestion>2", "<suggestion>3", ... */
    for(uint32_t i = 2; i < 1000; i++) {
        furi_string_printf(
            out, "%s/%s%lu%s", IR_MAKER_FOLDER, suggestion, (unsigned long)i, IR_MAKER_EXT);
        if(storage_common_stat(app->storage, furi_string_get_cstr(out), NULL) != FSE_OK) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* App lifecycle                                                       */
/* ------------------------------------------------------------------ */

static bool ir_maker_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    IrMakerApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool ir_maker_back_event_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static IrMakerApp* ir_maker_alloc(void) {
    IrMakerApp* app = malloc(sizeof(IrMakerApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    storage_simply_mkdir(app->storage, IR_MAKER_FOLDER);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&ir_maker_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ir_maker_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ir_maker_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    app->text_input = text_input_alloc();
    app->byte_input = byte_input_alloc();
    app->var_item_list = variable_item_list_alloc();
    app->dialog_ex = dialog_ex_alloc();
    app->popup = popup_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, IrMakerViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, IrMakerViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, IrMakerViewByteInput, byte_input_get_view(app->byte_input));
    view_dispatcher_add_view(
        app->view_dispatcher,
        IrMakerViewVarItemList,
        variable_item_list_get_view(app->var_item_list));
    view_dispatcher_add_view(
        app->view_dispatcher, IrMakerViewDialogEx, dialog_ex_get_view(app->dialog_ex));
    view_dispatcher_add_view(app->view_dispatcher, IrMakerViewPopup, popup_get_view(app->popup));

    app->worker = NULL;
    app->draft = ir_maker_draft_alloc();
    app->pending = infrared_signal_alloc();
    app->file_path = furi_string_alloc();

    app->def_protocol = InfraredProtocolNEC;
    app->def_address = 0;
    memset(&app->edit_msg, 0, sizeof(app->edit_msg));
    app->edit_msg.protocol = InfraredProtocolNEC;
    app->edit_mode = IrMakerEditModeNew;
    app->hex_target = IrMakerHexTargetDefaultAddress;
    memset(app->hex_buf, 0, sizeof(app->hex_buf));
    app->hex_n_bytes = 1;
    app->selected_button = 0;
    app->name_buf[0] = '\0';

    return app;
}

static void ir_maker_free(IrMakerApp* app) {
    furi_assert(app);

    /* The Learn scene owns the worker and frees it on exit. Guard here in case
     * of an abnormal teardown so the IR HAL is always released. */
    if(app->worker) {
        infrared_worker_rx_set_received_signal_callback(app->worker, NULL, NULL);
        infrared_worker_rx_stop(app->worker);
        infrared_worker_free(app->worker);
        app->worker = NULL;
    }

    ir_maker_draft_free(app->draft);
    infrared_signal_free(app->pending);
    furi_string_free(app->file_path);

    view_dispatcher_remove_view(app->view_dispatcher, IrMakerViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, IrMakerViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, IrMakerViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, IrMakerViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, IrMakerViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, IrMakerViewSubmenu);

    popup_free(app->popup);
    dialog_ex_free(app->dialog_ex);
    variable_item_list_free(app->var_item_list);
    byte_input_free(app->byte_input);
    text_input_free(app->text_input);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t ir_maker_app(void* p) {
    UNUSED(p);
    IrMakerApp* app = ir_maker_alloc();

    scene_manager_next_scene(app->scene_manager, IrMakerSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    ir_maker_free(app);
    return 0;
}
