#include <furi.h>
#include <furi/core/timer.h>
#include <furi/core/log.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/number_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <lib/nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_listener.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_listener.h>
#include <nfc/helpers/nfc_data_generator.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flip_crypt_icons.h"
#include "qrcode/qrcodegen.h"
#include "storage.h"
#include "cipher_registry.h"

#define FLIP_CRYPT_SCENES(X) \
    X(MainMenu, flip_crypt_main_menu_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_main_menu_scene_on_exit) \
    X(CipherCategoryMenu, category_menu_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(About, flip_crypt_about_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(CipherOptions, cipher_options_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(KeyAInput, key_a_input_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(KeyBInput, key_b_input_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(TextKeyInput, text_key_input_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(TextInput, text_input_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(Output, output_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(Learn, learn_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(NFC, flip_crypt_nfc_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_nfc_scene_on_exit) \
    X(Save, flip_crypt_save_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(SaveTextInput, save_text_input_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_generic_on_exit) \
    X(QR, flip_crypt_qr_scene_on_enter, flip_crypt_generic_event_handler, flip_crypt_qr_scene_on_exit)


#define X(name, on_enter, on_event, on_exit) FlipCrypt##name##Scene,
typedef enum { FLIP_CRYPT_SCENES(X) FlipCryptSceneCount } FlipCryptScene;
#undef X

typedef enum {
    FlipCryptSubmenuView,
    FlipCryptWidgetView,
    FlipCryptTextInputView,
    FlipCryptNumberInputView,
    FlipCryptDialogExView,
} FlipCryptView;

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    NumberInput* number_input;
    DialogEx* dialog_ex;
    Nfc* nfc;
    NfcListener* listener;
    NfcDevice* nfc_device;
    NotificationApp* notifications;

    char* universal_input;
    uint8_t universal_input_size;
    char* save_name_input;
    uint8_t save_name_input_size;
    char* text_key_input;
    uint8_t text_key_input_size;

    uint8_t current_cipher_index; // index into kCiphers[]
    bool current_is_decrypt;
    int32_t key_a;
    int32_t key_b;

    uint8_t* qr_buffer;
    uint8_t* qrcode;
} App;

static void current_output_path(App* app, char* buf, size_t buf_size);
static void push_first_input_scene(App* app);

static void main_menu_callback(void* context, uint32_t index) {
    App* app = context;
    if(index <= CipherCategoryEncoder) {
        // index lines with CipherCategory (Ciphers 0, Hashes 1, Other 2)
        scene_manager_set_scene_state(app->scene_manager, FlipCryptCipherCategoryMenuScene, index);
        scene_manager_next_scene(app->scene_manager, FlipCryptCipherCategoryMenuScene);
    } else {
        scene_manager_next_scene(app->scene_manager, FlipCryptAboutScene);
    }
}

void flip_crypt_main_menu_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "FlipCrypt");
    submenu_add_item(app->submenu, "Ciphers", CipherCategoryCipher, main_menu_callback, app);
    submenu_add_item(app->submenu, "Hashes", CipherCategoryHash, main_menu_callback, app);
    submenu_add_item(app->submenu, "Other", CipherCategoryEncoder, main_menu_callback, app);
    submenu_add_item(app->submenu, "About", CipherCategoryEncoder + 1, main_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptSubmenuView);
}

void flip_crypt_main_menu_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

static void category_menu_item_callback(void* context, uint32_t index) {
    App* app = context;
    app->current_cipher_index = (uint8_t)index;
    scene_manager_next_scene(app->scene_manager, FlipCryptCipherOptionsScene);
}

void category_menu_scene_on_enter(void* context) {
    App* app = context;
    CipherCategory category = (CipherCategory)scene_manager_get_scene_state(app->scene_manager, FlipCryptCipherCategoryMenuScene);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, category == CipherCategoryCipher ? "Ciphers" : category == CipherCategoryHash   ? "Hashes" : "Other");

    for(size_t i = 0; i < kCipherCount; i++) {
        if(kCiphers[i].category == category) {
            submenu_add_item(app->submenu, kCiphers[i].name, i, category_menu_item_callback, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptSubmenuView);
}

static void cipher_options_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
        case 0: // Encode / Hash
            app->current_is_decrypt = false;
            push_first_input_scene(app);
            break;
        case 1: // Decode
            app->current_is_decrypt = true;
            push_first_input_scene(app);
            break;
        case 2: // Learn
            scene_manager_next_scene(app->scene_manager, FlipCryptLearnScene);
            break;
        default:
            break;
    }
}

void cipher_options_scene_on_enter(void* context) {
    App* app = context;
    const CipherDef* def = &kCiphers[app->current_cipher_index];

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, def->name);
    submenu_add_item(app->submenu, def->category == CipherCategoryHash ? "Hash Text" : "Encode Text", 0, cipher_options_callback, app);
    if(def->decode) {
        submenu_add_item(app->submenu, "Decode Text", 1, cipher_options_callback, app);
    }
    submenu_add_item(app->submenu, "Learn", 2, cipher_options_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptSubmenuView);
}

static void push_first_input_scene(App* app) {
    const CipherDef* def = &kCiphers[app->current_cipher_index];
    switch(def->key_kind) {
        case CipherKeyNumberSingle:
        case CipherKeyNumberDouble:
            scene_manager_next_scene(app->scene_manager, FlipCryptKeyAInputScene);
            break;
        case CipherKeyText:
            scene_manager_next_scene(app->scene_manager, FlipCryptTextKeyInputScene);
            break;
        case CipherKeyNone:
        default:
            scene_manager_next_scene(app->scene_manager, FlipCryptTextInputScene);
            break;
    }
}

static void key_a_input_callback(void* context, int32_t number) {
    App* app = context;
    app->key_a = number;
    const CipherDef* def = &kCiphers[app->current_cipher_index];
    if(def->key_kind == CipherKeyNumberDouble) {
        scene_manager_next_scene(app->scene_manager, FlipCryptKeyBInputScene);
    } else {
        scene_manager_next_scene(app->scene_manager, FlipCryptTextInputScene);
    }
}

void key_a_input_scene_on_enter(void* context) {
    App* app = context;
    const CipherDef* def = &kCiphers[app->current_cipher_index];
    number_input_set_header_text(app->number_input, def->key_a_prompt);
    number_input_set_result_callback(app->number_input, key_a_input_callback, app, app->key_a, def->key_a_min, def->key_a_max);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptNumberInputView);
}

static void key_b_input_callback(void* context, int32_t number) {
    App* app = context;
    app->key_b = number;
    scene_manager_next_scene(app->scene_manager, FlipCryptTextInputScene);
}

void key_b_input_scene_on_enter(void* context) {
    App* app = context;
    const CipherDef* def = &kCiphers[app->current_cipher_index];
    number_input_set_header_text(app->number_input, def->key_b_prompt);
    number_input_set_result_callback(app->number_input, key_b_input_callback, app, app->key_b, def->key_b_min, def->key_b_max);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptNumberInputView);
}

static void text_key_input_callback(void* context) {
    App* app = context;
    scene_manager_next_scene(app->scene_manager, FlipCryptTextInputScene);
}

void text_key_input_scene_on_enter(void* context) {
    App* app = context;
    const CipherDef* def = &kCiphers[app->current_cipher_index];
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, def->key_a_prompt);
    text_input_set_result_callback(app->text_input, text_key_input_callback, app, app->text_key_input, app->text_key_input_size, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptTextInputView);
}

static void text_input_callback(void* context) {
    App* app = context;
    scene_manager_next_scene(app->scene_manager, FlipCryptOutputScene);
}

void text_input_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter text");
    text_input_set_result_callback(app->text_input, text_input_callback, app, app->universal_input, app->universal_input_size, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptTextInputView);
}

void output_scene_on_enter(void* context) {
    App* app = context;
    const CipherDef* def = &kCiphers[app->current_cipher_index];

    CipherResult result = cipher_registry_run(def, app->current_is_decrypt, app->universal_input, app->key_a, app->key_b, app->text_key_input);

    dialog_ex_set_text(app->dialog_ex, result.text, 64, 18, AlignCenter, AlignCenter);

    if(result.ok) {
        char filename[40];
        cipher_registry_build_filename(def, app->current_is_decrypt, filename, sizeof(filename));
        char full_path[96];
        snprintf(full_path, sizeof(full_path), "%s/%s", FLIP_CRYPT_DATA_DIR, filename);
        save_result_generic(full_path, result.text);

        dialog_ex_set_left_button_text(app->dialog_ex, "NFC");
        dialog_ex_set_center_button_text(app->dialog_ex, "Save");
        dialog_ex_set_right_button_text(app->dialog_ex, "QR");
    } else {
        dialog_ex_set_left_button_text(app->dialog_ex, NULL);
        dialog_ex_set_center_button_text(app->dialog_ex, NULL);
        dialog_ex_set_right_button_text(app->dialog_ex, NULL);
    }

    cipher_result_free(&result);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptDialogExView);
}

void learn_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, kCiphers[app->current_cipher_index].learn_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

static void dialog_ex_callback(DialogExResult result, void* context) {
    App* app = context;
    switch(result) {
        case DialogExResultLeft:
        case DialogExPressLeft:
            scene_manager_next_scene(app->scene_manager, FlipCryptNFCScene);
            break;
        case DialogExResultRight:
        case DialogExPressRight:
            scene_manager_next_scene(app->scene_manager, FlipCryptQRScene);
            break;
        case DialogExResultCenter:
        case DialogExPressCenter:
            scene_manager_next_scene(app->scene_manager, FlipCryptSaveTextInputScene);
            break;
        default:
            break;
    }
}

static void current_output_path(App* app, char* buf, size_t buf_size) {
    const CipherDef* def = &kCiphers[app->current_cipher_index];
    char filename[40];
    cipher_registry_build_filename(def, app->current_is_decrypt, filename, sizeof(filename));
    snprintf(buf, buf_size, "%s/%s", FLIP_CRYPT_DATA_DIR, filename);
}

void create_nfc_tag(App* app, const char* message) {
    app->nfc_device = nfc_device_alloc();
    nfc_data_generator_fill_data(NfcDataGeneratorTypeNTAG215, app->nfc_device);

    const MfUltralightData* data_original = nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
    MfUltralightData* data = malloc(sizeof(MfUltralightData));
    furi_assert(data);
    memcpy(data, data_original, sizeof(MfUltralightData));

    Iso14443_3aData* isodata = malloc(sizeof(Iso14443_3aData));
    furi_assert(isodata);
    memcpy(isodata, data_original->iso14443_3a_data, sizeof(Iso14443_3aData));
    data->iso14443_3a_data = isodata;

    const char* lang = "en";
    uint8_t lang_len = strlen(lang);
    size_t msg_len = strlen(message);

    size_t text_payload_len = 1 + lang_len + msg_len;
    size_t ndef_record_header_len = 4;
    size_t ndef_message_total_len = ndef_record_header_len + text_payload_len;

    data->page[4].data[0] = 0x03;
    data->page[4].data[1] = ndef_message_total_len;
    data->page[4].data[2] = 0xD1;
    data->page[4].data[3] = 0x01;

    data->page[5].data[0] = text_payload_len;
    data->page[5].data[1] = 'T';
    uint8_t status_byte = (0 << 7) | (lang_len & 0x3F);
    data->page[5].data[2] = status_byte;

    size_t current_byte_idx = 3;
    size_t current_page_idx = 5;

    for(size_t i = 0; i < lang_len; ++i) {
        data->page[current_page_idx].data[current_byte_idx++] = lang[i];
        if(current_byte_idx > 3) {
            current_byte_idx = 0;
            current_page_idx++;
        }
    }
    for(size_t i = 0; i < msg_len; ++i) {
        data->page[current_page_idx].data[current_byte_idx++] = message[i];
        if(current_byte_idx > 3) {
            current_byte_idx = 0;
            current_page_idx++;
        }
    }
    data->page[current_page_idx].data[current_byte_idx++] = 0xFE;

    nfc_device_set_data(app->nfc_device, NfcProtocolMfUltralight, data);
    free(data);
    free(isodata);
}

void flip_crypt_nfc_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 0, 3, &I_NFC_dolphin_emulation_51x64);
    widget_add_string_element(app->widget, 90, 25, AlignCenter, AlignTop, FontPrimary, "Emulating...");

    char path[96];
    current_output_path(app, path, sizeof(path));
    create_nfc_tag(app, load_result_generic(path));

    const MfUltralightData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(app->listener, NULL, NULL);
    notification_message(app->notifications, &sequence_blink_start_magenta);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

void flip_crypt_nfc_scene_on_exit(void* context) {
    App* app = context;
    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
    widget_reset(app->widget);
    notification_message(app->notifications, &sequence_blink_stop);
}

void flip_crypt_qr_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    char path[96];
    current_output_path(app, path, sizeof(path));
    const char* text = load_result_generic(path);

    bool fits = qrcodegen_encodeText(text, app->qr_buffer, app->qrcode, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 5, qrcodegen_Mask_AUTO, true);

    if(fits) {
        int size = qrcodegen_getSize(app->qrcode);
        int offset_x = 64 - size / 2;
        int offset_y = 32 - size / 2;
        for(int y = 0; y < size; y++) {
            for(int x = 0; x < size; x++) {
                if(qrcodegen_getModule(app->qrcode, x, y)) {
                    widget_add_rect_element(app->widget, offset_x + x, offset_y + y, 1, 1, 0, true);
                }
            }
        }
    } else {
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "Output too long");
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

void flip_crypt_qr_scene_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

static void save_text_input_callback(void* context) {
    App* app = context;
    scene_manager_next_scene(app->scene_manager, FlipCryptSaveScene);
}

void save_text_input_scene_on_enter(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Enter file name");
    text_input_set_result_callback(app->text_input, save_text_input_callback, app, app->save_name_input, app->save_name_input_size, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptTextInputView);
}

void flip_crypt_save_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    char path[96];
    current_output_path(app, path, sizeof(path));
    save_result(load_result_generic(path), app->save_name_input);

    widget_add_icon_element(app->widget, 36, 6, &I_DolphinSaved_92x58);
    widget_add_string_element(app->widget, 25, 15, AlignCenter, AlignCenter, FontPrimary, "Saved!");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

void flip_crypt_about_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64,
        "FlipCrypt\n"
        "v0.7\n"
        "Explore and learn about various cryptographic and text encoding methods.\n\n"
        "Usage:\n"
        "Select the method you want to use for encoding / decoding text and fill in the necessary inputs.\n"
        "On the output screen, there are up to three options for actions you can do with the output - Save, NFC, and QR. The save button saves the output to a text file in the folder located at /ext/flip_crypt_saved/. The NFC button emulates the output using NTAG215. The QR button generates and displays a QR code of your output. Not all three options will be available on every output screen due to memory limitations - for instance the flipper just can't handle the QR code for a SHA-512 output.\n\n"
        "Feel free to leave any issues / PRs on the repo with new feature ideas!\n\n"
        "Author: @Tyl3rA\n"
        "Source Code: https://github.com/Tyl3rA/FlipCrypt\n\n\n"
        "SHA 224-512 LICENSE INFO:"
        "FIPS 180-2 SHA-224/256/384/512 implementation\n"
        "\n"
        "Copyright (C) 2005-2023 Olivier Gay <olivier.gay@a3.epfl.ch>\n"
        "All rights reserved.\n"
        "\n"
        "Redistribution and use in source and binary forms, with or without\n"
        "modification, are permitted provided that the following conditions\n"
        "are met:\n"
        "1. Redistributions of source code must retain the above copyright\n"
        "notice, this list of conditions and the following disclaimer.\n"
        "2. Redistributions in binary form must reproduce the above copyright\n"
        "notice, this list of conditions and the following disclaimer in the\n"
        "documentation and/or other materials provided with the distribution.\n"
        "3. Neither the name of the project nor the names of its contributors\n"
        "may be used to endorse or promote products derived from this software\n"
        "without specific prior written permission.\n"
        "\n"
        "THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND\n"
        "ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
        "IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
        "ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE\n"
        "FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
        "DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n"
        "OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n"
        "HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n"
        "LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n"
        "OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n"
        "SUCH DAMAGE.");
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipCryptWidgetView);
}

bool flip_crypt_generic_event_handler(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void flip_crypt_generic_on_exit(void* context) {
    UNUSED(context);
}

#define X(name, on_enter, on_event, on_exit) on_enter,
void (*const flip_crypt_scene_on_enter_handlers[])(void*) = {FLIP_CRYPT_SCENES(X)};
#undef X

#define X(name, on_enter, on_event, on_exit) on_event,
bool (*const flip_crypt_scene_on_event_handlers[])(void*, SceneManagerEvent) = {FLIP_CRYPT_SCENES(X)};
#undef X

#define X(name, on_enter, on_event, on_exit) on_exit,
void (*const flip_crypt_scene_on_exit_handlers[])(void*) = {FLIP_CRYPT_SCENES(X)};
#undef X

static const SceneManagerHandlers flip_crypt_scene_manager_handlers = {
    .on_enter_handlers = flip_crypt_scene_on_enter_handlers,
    .on_event_handlers = flip_crypt_scene_on_event_handlers,
    .on_exit_handlers = flip_crypt_scene_on_exit_handlers,
    .scene_num = FlipCryptSceneCount,
};

static bool basic_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

bool basic_scene_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    furi_assert(app);

    app->universal_input_size = 255;
    app->universal_input = malloc(app->universal_input_size);
    app->save_name_input_size = 64;
    app->save_name_input = malloc(app->save_name_input_size);
    app->text_key_input_size = 64;
    app->text_key_input = malloc(app->text_key_input_size);

    app->current_cipher_index = 0;
    app->current_is_decrypt = false;
    app->key_a = 1;
    app->key_b = 1;
    app->listener = NULL;

    app->scene_manager = scene_manager_alloc(&flip_crypt_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, basic_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, basic_scene_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptSubmenuView, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptWidgetView, widget_get_view(app->widget));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptTextInputView, text_input_get_view(app->text_input));
    app->number_input = number_input_alloc();
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptNumberInputView, number_input_get_view(app->number_input));
    app->dialog_ex = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, dialog_ex_callback);
    view_dispatcher_add_view(app->view_dispatcher, FlipCryptDialogExView, dialog_ex_get_view(app->dialog_ex));

    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->qr_buffer = malloc(qrcodegen_BUFFER_LEN_MAX);
    app->qrcode = malloc(qrcodegen_BUFFER_LEN_MAX);

    return app;
}

static void app_free(App* app) {
    furi_assert(app);

    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptTextInputView);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptNumberInputView);
    view_dispatcher_remove_view(app->view_dispatcher, FlipCryptDialogExView);

    dialog_ex_free(app->dialog_ex);
    number_input_free(app->number_input);
    text_input_free(app->text_input);
    widget_free(app->widget);
    submenu_free(app->submenu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    nfc_free(app->nfc);
    nfc_device_free(app->nfc_device);
    furi_record_close(RECORD_NOTIFICATION);

    free(app->qr_buffer);
    free(app->qrcode);
    free(app->universal_input);
    free(app->save_name_input);
    free(app->text_key_input);

    free(app);
}

int32_t flip_crypt_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FlipCryptMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    app_free(app);
    return 0;
}
