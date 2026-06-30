#include "../ibutton_converter_i.h"

void ibutton_converter_scene_info_on_enter(void* context) {
    iButtonConverter* ibutton_converter = context;
    iButtonKey* key = ibutton_converter->converted_key;
    Widget* widget = ibutton_converter->widget;

    const iButtonProtocolId protocol_id = ibutton_key_get_protocol_id(key);

    FuriString* tmp = furi_string_alloc();
    FuriString* brief_data = furi_string_alloc();

    furi_string_printf(
        tmp,
        "Source key: %s\n\e#%s %s\e#\n",
        ibutton_converter->key_name,
        ibutton_protocols_get_manufacturer(ibutton_converter->protocols, protocol_id),
        ibutton_protocols_get_name(ibutton_converter->protocols, protocol_id));

    ibutton_protocols_render_brief_data(ibutton_converter->protocols, key, brief_data);

    furi_string_cat(tmp, brief_data);

    widget_add_text_box_element(
        widget, 0, 0, 128, 64, AlignLeft, AlignTop, furi_string_get_cstr(tmp), false);

    furi_string_reset(tmp);
    furi_string_reset(brief_data);

    view_dispatcher_switch_to_view(ibutton_converter->view_dispatcher, iButtonConverterViewWidget);
    furi_string_free(tmp);
    furi_string_free(brief_data);
}

bool ibutton_converter_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void ibutton_converter_scene_info_on_exit(void* context) {
    iButtonConverter* ibutton_converter = context;
    widget_reset(ibutton_converter->widget);
}
