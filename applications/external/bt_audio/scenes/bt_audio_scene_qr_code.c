#include "../bt_audio.h"
#include "../lib/qrcodegen/qrcodegen.h"

#define README_URL "https://github.com/FatherDivine/bt_audio#readme"

// Forward declaration - typedef is in bt_audio.c
typedef struct {
    uint8_t* qrcode;
    uint8_t* buffer;
} QRCodeModel;

void bt_audio_scene_qr_code_on_enter(void* context) {
    BtAudio* app = context;

    // Generate QR code
    QRCodeModel* qr_model = view_get_model(app->qr_view);

    static const size_t buffer_len = qrcodegen_BUFFER_LEN_MAX;

    if(!qr_model->buffer) {
        qr_model->buffer = malloc(buffer_len);
    }

    if(!qr_model->qrcode) {
        qr_model->qrcode = malloc(buffer_len);
    }

    if(qr_model->buffer && qr_model->qrcode) {
        enum qrcodegen_Ecc errCorLvl = qrcodegen_Ecc_LOW;
        qrcodegen_encodeText(
            README_URL,
            qr_model->buffer,
            qr_model->qrcode,
            errCorLvl,
            qrcodegen_VERSION_MIN,
            qrcodegen_VERSION_MAX,
            qrcodegen_Mask_AUTO,
            true);
    }

    view_commit_model(app->qr_view, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, BtAudioViewQRCode);
}

bool bt_audio_scene_qr_code_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bt_audio_scene_qr_code_on_exit(void* context) {
    BtAudio* app = context;

    // Free QR code data
    QRCodeModel* qr_model = view_get_model(app->qr_view);
    if(qr_model->buffer) {
        free(qr_model->buffer);
        qr_model->buffer = NULL;
    }
    if(qr_model->qrcode) {
        free(qr_model->qrcode);
        qr_model->qrcode = NULL;
    }
    view_commit_model(app->qr_view, false);
}
