#include "src/core/cfs_app_i.h"
#include "src/crypto/cfs_crypto.h"

void cfs_scene_debug_selftest_on_enter(void* context) {
    CfsApp* app = context;

    bool crypto_ok = cfs_crypto_self_test();

    // Data encode/decode round trip using the spec's example values.
    CfsTagData in;
    cfs_data_set_defaults(&in);
    in.length_m = 357;
    in.color_r = 0xFF;
    in.color_g = 0x5F;
    in.color_b = 0x0B;
    strncpy(in.serial, "736314", sizeof(in.serial));
    uint8_t plain[CFS_PLAINTEXT_SIZE];
    cfs_data_encode(&in, plain);
    CfsTagData out;
    bool data_ok = cfs_data_decode(plain, &out) && out.length_m == in.length_m &&
                   out.color_r == in.color_r && out.color_b == in.color_b &&
                   strncmp(out.serial, in.serial, CFS_SERIAL_LEN) == 0;

    bool ok = crypto_ok && data_ok;

    popup_reset(app->popup);
    popup_set_header(app->popup, ok ? "Self-Test PASS" : "Self-Test FAIL", 64, 10, AlignCenter, AlignTop);
    snprintf(
        app->text_scratch,
        sizeof(app->text_scratch),
        "AES: %s\nData: %s",
        crypto_ok ? "ok" : "FAIL",
        data_ok ? "ok" : "FAIL");
    popup_set_text(app->popup, app->text_scratch, 64, 36, AlignCenter, AlignCenter);
    notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewPopup);
}

bool cfs_scene_debug_selftest_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfs_scene_debug_selftest_on_exit(void* context) {
    CfsApp* app = context;
    popup_reset(app->popup);
}
