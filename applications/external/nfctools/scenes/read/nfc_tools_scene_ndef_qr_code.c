#include "../../include/nfc_tools_i.h"
#include "../../helpers/qrcode/nfc_tools_qrcodegen.h"

// ── Custom view: QR code display ─────────────────────────────────────────────

typedef struct {
    uint8_t qrcode[QRCODEGEN_BUF_MAX];
    bool    valid;
} NfcToolsQrModel;

static void nfc_tools_qr_draw_cb(Canvas* canvas, void* model_ptr) {
    NfcToolsQrModel* m = model_ptr;

    canvas_clear(canvas);

    if(!m->valid) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "QR unavailable");
        return;
    }

    int qr_size = nfc_tools_qr_size(m->qrcode);

    // Quiet zone of 3 modules on each side, then choose the maximum scale
    // that allows the QR + quiet zone to fit within 128×64
    int qz_modules = 3;
    int total_modules = qr_size + 2 * qz_modules;

    int scale = 1;
    if(total_modules * 3 <= 64)       scale = 3;
    else if(total_modules * 2 <= 64)  scale = 2;

    int total_px = total_modules * scale;
    int origin_x = (128 - total_px) / 2;
    int origin_y = (64  - total_px) / 2;
    if(origin_x < 0) origin_x = 0;
    if(origin_y < 0) origin_y = 0;

    // White background (entire screen for clean quiet zone)
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, 128, 64);

    // Modules noirs
    canvas_set_color(canvas, ColorBlack);
    int qz_px = qz_modules * scale;
    for(int y = 0; y < qr_size; y++) {
        for(int x = 0; x < qr_size; x++) {
            if(nfc_tools_qr_module(m->qrcode, x, y)) {
                int px = origin_x + qz_px + x * scale;
                int py = origin_y + qz_px + y * scale;
                canvas_draw_box(canvas, px, py, (size_t)scale, (size_t)scale);
            }
        }
    }
}

static bool nfc_tools_qr_input_cb(InputEvent* event, void* context) {
    NfcToolsApp* app = context;
    if(event->type == InputTypeShort &&
       (event->key == InputKeyBack || event->key == InputKeyOk)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, 0);
        return true;
    }
    return false;
}

// ── Alloc / Free helpers (called from nfc_tools.c) ───────────────────────────

View* nfc_tools_qr_view_alloc(NfcToolsApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(NfcToolsQrModel));
    view_set_draw_callback(view, nfc_tools_qr_draw_cb);
    view_set_input_callback(view, nfc_tools_qr_input_cb);
    view_set_context(view, app);
    return view;
}

void nfc_tools_qr_view_free(View* view) {
    view_free_model(view);
    view_free(view);
}

// ── Scene ─────────────────────────────────────────────────────────────────────

void nfc_tools_scene_ndef_qr_code_on_enter(void* context) {
    NfcToolsApp* app = context;
    const NfcToolsNdefRecord* rec = &app->ndef_records[app->ndef_selected_record];

    // Generate QR (outside the model to avoid allocating on the callback stack)
    static uint8_t s_temp[QRCODEGEN_BUF_MAX];
    static uint8_t s_qrcode[QRCODEGEN_BUF_MAX];
    bool valid = nfc_tools_qr_encode(rec->value, s_temp, s_qrcode);

    with_view_model(app->qr_view, NfcToolsQrModel * m, {
        m->valid = valid;
        if(valid) memcpy(m->qrcode, s_qrcode, QRCODEGEN_BUF_MAX);
    }, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewQrCode);
}

bool nfc_tools_scene_ndef_qr_code_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    UNUSED(app);
    return false;
}

void nfc_tools_scene_ndef_qr_code_on_exit(void* context) {
    UNUSED(context);
}
