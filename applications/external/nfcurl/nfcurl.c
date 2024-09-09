#include "gui/view_dispatcher.h"
#include "nfcurl_i.h"
#include "scenes/nfcurl_scene.h"
#include <gui/view.h> // Remove after debugging view

#include <furi.h>

#include <nfc/helpers/nfc_data_generator.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

static bool nfcurl_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcUrlApp* nfcurl = context;
    return scene_manager_handle_custom_event(nfcurl->scene_manager, event);
}

static bool nfcurl_back_event_callback(void* context) {
    furi_assert(context);
    NfcUrlApp* nfcurl = context;
    return scene_manager_handle_back_event(nfcurl->scene_manager);
}

static NfcUrlApp* nfcurl_alloc() {
    NfcUrlApp* nfcurl = malloc(sizeof(NfcUrlApp));

    nfcurl->scene_manager = scene_manager_alloc(&nfcurl_scene_handlers, nfcurl);

    nfcurl->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(nfcurl->view_dispatcher, nfcurl);
    view_dispatcher_set_custom_event_callback(
        nfcurl->view_dispatcher, nfcurl_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        nfcurl->view_dispatcher, nfcurl_back_event_callback);

    nfcurl->widget = widget_alloc();
    view_dispatcher_add_view(
        nfcurl->view_dispatcher, NfcUrlViewWidget, widget_get_view(nfcurl->widget));

    nfcurl->text_input = text_input_alloc();
    view_dispatcher_add_view(
        nfcurl->view_dispatcher, NfcUrlViewTextInput, text_input_get_view(nfcurl->text_input));

    nfcurl->popup = popup_alloc();
    view_dispatcher_add_view(
        nfcurl->view_dispatcher, NfcUrlViewPopup, popup_get_view(nfcurl->popup));

    nfcurl->submenu = submenu_alloc();
    view_dispatcher_add_view(
        nfcurl->view_dispatcher, NfcUrlViewSubmenu, submenu_get_view(nfcurl->submenu));

    nfcurl->button_menu = button_menu_alloc();
    view_dispatcher_add_view(
        nfcurl->view_dispatcher, NfcUrlViewButtonMenu, button_menu_get_view(nfcurl->button_menu));

    nfcurl->url = furi_string_alloc();
    nfcurl->name = furi_string_alloc();
    nfcurl->path = furi_string_alloc();
    nfcurl->text_buffer = malloc(200);
    nfcurl->urlpairs = malloc(sizeof(UrlPair) * 100); // TODO: support more/dynamic size

    nfcurl->nfc = nfc_alloc();
    nfcurl->notifications = furi_record_open(RECORD_NOTIFICATION);

    return nfcurl;
}

static void nfcurl_free(NfcUrlApp* nfcurl) {
    view_dispatcher_remove_view(nfcurl->view_dispatcher, NfcUrlViewWidget);
    widget_free(nfcurl->widget);
    view_dispatcher_remove_view(nfcurl->view_dispatcher, NfcUrlViewTextInput);
    text_input_free(nfcurl->text_input);
    view_dispatcher_remove_view(nfcurl->view_dispatcher, NfcUrlViewPopup);
    popup_free(nfcurl->popup);
    view_dispatcher_remove_view(nfcurl->view_dispatcher, NfcUrlViewButtonMenu);
    button_menu_free(nfcurl->button_menu);
    view_dispatcher_remove_view(nfcurl->view_dispatcher, NfcUrlViewSubmenu);
    submenu_free(nfcurl->submenu);

    scene_manager_free(nfcurl->scene_manager);
    view_dispatcher_free(nfcurl->view_dispatcher);

    furi_string_free(nfcurl->url);
    furi_string_free(nfcurl->name);
    furi_string_free(nfcurl->path);
    free(nfcurl->text_buffer);
    free(nfcurl->urlpairs);

    nfc_free(nfcurl->nfc);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(nfcurl);
}

void nfcurl_create_tag(NfcUrlApp* app) {
    app->device = nfc_device_alloc();

    nfc_data_generator_fill_data(NfcDataGeneratorTypeNTAG215, app->device);
    const MfUltralightData* data_original =
        nfc_device_get_data(app->device, NfcProtocolMfUltralight);
    MfUltralightData* data = malloc(sizeof(MfUltralightData));
    Iso14443_3aData* isodata = malloc(sizeof(Iso14443_3aData));
    memcpy(isodata, data_original->iso14443_3a_data, sizeof(Iso14443_3aData));
    memcpy(data, data_original, sizeof(MfUltralightData));
    data->iso14443_3a_data = isodata;

    // User data
    data->page[4].data[0] = 0x03; //TLV open
    data->page[4].data[2] = 0xc1; // NDEF header
    data->page[4].data[3] = 0x01; // Size of type
    size_t url_size = furi_string_size(app->url) + 1; //url + prefix size
    unsigned page_index, data_index;
    if(url_size < 254) {
        data->page[4].data[2] |= 0x10;
        data->page[5].data[0] = url_size;
        data->page[5].data[1] = 'U'; // RTD well-known type
        data->page[5].data[2] = app->prefix; // URL prefix
        page_index = 5;
        data_index = 3;
    } else {
        data->page[5].data[0] = url_size;
        data->page[5].data[1] = url_size >> 8;
        data->page[5].data[2] = url_size >> 16;
        data->page[5].data[3] = 'U';
        data->page[6].data[0] = 0x04;
        page_index = 6;
        data_index = 1;
    }

    const char* urlstr = furi_string_get_cstr(app->url);
    for(size_t i = 0; i < url_size - 1; ++i) {
        data->page[page_index].data[data_index++] = *urlstr++;
        if(data_index > 3) {
            data_index = 0;
            page_index++;
        }
    }
    data->page[page_index].data[data_index++] = 0xfe;
    data->page[4].data[1] = page_index * 4 + data_index - (4 * 4 + 1) - 2; //TODO check math
    nfc_device_set_data(app->device, NfcProtocolMfUltralight, data);
    free(data);
    free(isodata);

    //furi_string_set_str(app->path, NFC_APP_FOLDER);
    //path_append(app->path, furi_string_get_cstr(app->name));
    //furi_string_cat_str(app->path, NFC_APP_EXTENSION);
    //bool success = nfc_device_save(app->device, furi_string_get_cstr(app->path));
    //UNUSED(success);

    //furi_string_replace_at(app->path, 1, 3, "ext");
}

int32_t nfcurl_app(void* p) {
    UNUSED(p);
    NfcUrlApp* app = nfcurl_alloc();

    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, NfcUrlSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    nfcurl_free(app);
    FURI_LOG_D("NfcUrl", "free returned");
    return 0;
}
