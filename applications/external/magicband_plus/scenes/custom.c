#include "../ble_spam.h"

static void custom_save_cb(void* _ctx) {
    Ctx* ctx = _ctx;
    // byte_store has the 16 bytes entered by user
    ble_spam_set_custom_payload(ctx, ctx->byte_store, 16);
    // Go back to Start; user can then Browse → lands on Custom Hex attack
    scene_manager_previous_scene(ctx->scene_manager);
}

void scene_custom_on_enter(void* _ctx) {
    Ctx* ctx = _ctx;

    byte_input_set_header_text(ctx->byte_input, "MFR data (after 83 01 hdr):");

    // Initialize buffer with current custom payload so user can edit it
    byte_input_set_result_callback(
        ctx->byte_input,
        custom_save_cb,
        NULL, // changed cb not needed
        ctx,
        ctx->byte_store,
        16);

    view_dispatcher_switch_to_view(ctx->view_dispatcher, ViewByteInput);
}

bool scene_custom_on_event(void* _ctx, SceneManagerEvent event) {
    UNUSED(_ctx);
    UNUSED(event);
    return false;
}

void scene_custom_on_exit(void* _ctx) {
    UNUSED(_ctx);
}
