#include "../uk_mbirth_sonicare.h"
#include "gui/canvas.h"
#include "gui/modules/widget.h"
#include "gui/modules/widget_elements/widget_element.h"
#include "gui/scene_manager.h"
#include "gui/view_dispatcher.h"
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include "nfc/protocols/mf_ultralight/mf_ultralight.h"
#include <uk_mbirth_sonicare_icons.h>
#include <dolphin/dolphin.h>

void sonicare_scene_read_complete_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Sonicare* app = context;
    if (type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void sonicare_scene_read_complete_on_enter(void* context) {
    Sonicare* app = context;
    Widget* widget = app->widget;
    
    widget_reset(widget);
    
    const NfcDevice* nfc_device = app->nfc_device;
    FURI_LOG_D("sonicare_scene_read_complete", "Pulling Mifare Ultralight data from NFC device");
    const MfUltralightData* ul_data = app->nfc_data;

    UNUSED(ul_data);

    FuriString* temp_str = furi_string_alloc();
    
    furi_string_cat_printf(temp_str, "\e#%s\n", nfc_device_get_name(nfc_device, NfcDeviceNameTypeFull));
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);

    widget_add_button_element(widget, GuiButtonTypeRight, "Change", sonicare_scene_read_complete_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewWidget);
}

bool sonicare_scene_read_complete_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);

    bool consumed = false;
    
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == GuiButtonTypeRight) {
            // switch to edit screen
            consumed = true;
        }
    } else if (event.type == SceneManagerEventTypeBack) {
        // Back button pressed
        //consumed = true;
    }
    
    return consumed;
}

void sonicare_scene_read_complete_on_exit(void* context) {
    Sonicare* app = context;
    widget_reset(app->widget);
}
