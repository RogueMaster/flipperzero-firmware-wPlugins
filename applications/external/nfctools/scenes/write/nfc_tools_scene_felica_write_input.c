#include "../../include/nfc_tools_i.h"

// ── Validation + parsing ──────────────────────────────────────────────────────
// Format: 2 decimal digits (block 00-13) + 32 hex chars (16 bytes) = 34 chars
// Ex : "0600112233445566778899AABBCCDDEEFF" → bloc 6

static bool felica_parse_input(NfcToolsApp* app) {
    const char* s   = app->ndef_buf1;
    size_t      len = strlen(s);

    if(len != 34) return false;

    // first 2 decimal chars
    if(s[0] < '0' || s[0] > '9') return false;
    if(s[1] < '0' || s[1] > '9') return false;

    uint8_t block = (uint8_t)((s[0] - '0') * 10 + (s[1] - '0'));
    if(block > 13) return false;

    // 32 hex chars (case-insensitive)
    for(size_t i = 2; i < 34; i++) {
        char c = s[i];
        if(!((c >= '0' && c <= '9') ||
             (c >= 'A' && c <= 'F') ||
             (c >= 'a' && c <= 'f'))) return false;
    }

    app->felica_write_block = block;
    return true;
}

// ── Error popup callback → return to input ────────────────────────────────────

static void nfc_tools_felica_error_popup_cb(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewSpecialInput);
}

// ── Callback SpecialInput ─────────────────────────────────────────────────────

static void nfc_tools_felica_write_input_cb(void* context) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

// ── Scene ─────────────────────────────────────────────────────────────────────

void nfc_tools_scene_felica_write_input_on_enter(void* context) {
    NfcToolsApp* app = context;

    app->ndef_buf1[0] = '\0';

    special_input_set_header_text(app->special_input, NTS_INPUT_FELICA_BLOCK);
    special_input_set_result_callback(
        app->special_input,
        nfc_tools_felica_write_input_cb,
        app,
        app->ndef_buf1,
        sizeof(app->ndef_buf1),
        false);
    special_input_set_minimum_length(app->special_input, 34);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewSpecialInput);
}

bool nfc_tools_scene_felica_write_input_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        if(!felica_parse_input(app)) {
            // Format invalide → popup d'erreur qui revient sur la saisie
            const char* err = (app->ndef_buf1[0] >= '0' && app->ndef_buf1[0] <= '9' &&
                               app->ndef_buf1[1] >= '0' && app->ndef_buf1[1] <= '9' &&
                               (app->ndef_buf1[0] - '0') * 10 + (app->ndef_buf1[1] - '0') > 13)
                                  ? NTS_ERR_FELICA_BLOCK_RANGE
                                  : NTS_ERR_FELICA_FORMAT;
            popup_reset(app->popup);
            popup_set_header(app->popup, err, 64, 22, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, nfc_tools_felica_error_popup_cb);
            popup_set_context(app->popup, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
        } else {
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdFelicaWrite);
        }
        consumed = true;
    }

    return consumed;
}

void nfc_tools_scene_felica_write_input_on_exit(void* context) {
    NfcToolsApp* app = context;
    special_input_reset(app->special_input);
    popup_reset(app->popup);
}
