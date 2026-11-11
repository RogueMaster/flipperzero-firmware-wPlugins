#include "../ir_maker.h"

/* One shared ByteInput scene serves three fields (default address, button
 * address, button command), selected by app->hex_target.
 *
 * ENDIAN NOTE: infrared_signal serialises the address/command uint32 as a raw
 * little-endian byte dump, but ByteInput edits bytes most-significant-first.
 * We therefore seed hex_buf big-endian from the current value and read it back
 * big-endian, never aliasing ByteInput onto &message.address directly. */

static void ir_maker_scene_edit_hex_callback(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void ir_maker_scene_edit_hex_on_enter(void* context) {
    furi_assert(context);
    IrMakerApp* app = context;

    InfraredProtocol protocol;
    uint32_t value;
    uint8_t bits;
    const char* header;

    if(app->hex_target == IrMakerHexTargetDefaultAddress) {
        protocol = app->def_protocol;
        value = app->def_address;
        bits = infrared_get_protocol_address_length(protocol);
        header = "Address (hex)";
    } else if(app->hex_target == IrMakerHexTargetButtonAddress) {
        protocol = app->edit_msg.protocol;
        value = app->edit_msg.address;
        bits = infrared_get_protocol_address_length(protocol);
        header = "Address (hex)";
    } else {
        protocol = app->edit_msg.protocol;
        value = app->edit_msg.command;
        bits = infrared_get_protocol_command_length(protocol);
        header = "Command (hex)";
    }

    app->hex_n_bytes = ir_maker_field_bytes(bits);
    memset(app->hex_buf, 0, sizeof(app->hex_buf));
    ir_maker_u32_to_hex_be(value, app->hex_buf, app->hex_n_bytes);

    byte_input_set_header_text(app->byte_input, header);
    byte_input_set_result_callback(
        app->byte_input,
        ir_maker_scene_edit_hex_callback,
        NULL,
        app,
        app->hex_buf,
        (uint8_t)app->hex_n_bytes);

    view_dispatcher_switch_to_view(app->view_dispatcher, IrMakerViewByteInput);
}

bool ir_maker_scene_edit_hex_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    IrMakerApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        const uint32_t value = ir_maker_hex_be_to_u32(app->hex_buf, app->hex_n_bytes);
        switch(app->hex_target) {
        case IrMakerHexTargetDefaultAddress:
            app->def_address = value;
            break;
        case IrMakerHexTargetButtonAddress:
            app->edit_msg.address = value;
            break;
        case IrMakerHexTargetButtonCommand:
            app->edit_msg.command = value;
            break;
        default:
            break;
        }
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void ir_maker_scene_edit_hex_on_exit(void* context) {
    UNUSED(context);
}
