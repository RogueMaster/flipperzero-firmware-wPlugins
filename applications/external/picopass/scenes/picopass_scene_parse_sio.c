#include "../picopass_i.h"
#include <dolphin/dolphin.h>

#define ASN_EMIT_DEBUG 0
#include <SIO.h>

#define TAG "PicopassSceneParseSIO"

static char asn1_log[128] = {0};
static int picopass_scene_parse_sio_callback(const void* buffer, size_t size, void* app_key) {
    if(app_key) {
        char* str = (char*)app_key;
        size_t next = strlen(str);
        strncpy(str + next, buffer, size);
    } else {
        uint8_t next = strlen(asn1_log);
        strncpy(asn1_log + next, buffer, size);
    }
    return 0;
}

void picopass_scene_parse_sio_widget_callback(GuiButtonType result, InputType type, void* context) {
    Picopass* picopass = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(picopass->view_dispatcher, result);
    }
}

void picopass_scene_parse_sio_on_enter(void* context) {
    Picopass* picopass = context;
    PicopassDevice* dev = picopass->dev;
    PicopassPacs* pacs = &dev->dev_data.pacs;
    PicopassBlock* card_data = dev->dev_data.card_data;
    furi_string_reset(picopass->text_box_store);
    FuriString* str = picopass->text_box_store;
    char sioDebug[384] = {0};

    uint8_t start_block = 0;

    // TODO: save SR vs SE more properly
    if(pacs->sio) { // SR
        start_block = 10;
    } else if(pacs->se_enabled) { //SE
        start_block = 6;
    }

    uint8_t sio_buffer[128];
    size_t sio_length = card_data[start_block].data[1];
    size_t block_count = (sio_length + PICOPASS_BLOCK_LEN - 1) / PICOPASS_BLOCK_LEN;
    for(uint8_t i = 0; i < block_count; i++) {
        memcpy(
            sio_buffer + (i * PICOPASS_BLOCK_LEN),
            card_data[start_block + i].data,
            PICOPASS_BLOCK_LEN);
    }

    SIO_t* sio = 0;
    sio = calloc(1, sizeof *sio);
    assert(sio);
    asn_dec_rval_t rval = asn_decode(
        0, ATS_DER, &asn_DEF_SIO, (void**)&sio, sio_buffer, block_count * PICOPASS_BLOCK_LEN);

    if(rval.code == RC_OK) {
        (&asn_DEF_SIO)
            ->op->print_struct(&asn_DEF_SIO, sio, 1, picopass_scene_parse_sio_callback, sioDebug);
        if(strlen(sioDebug) > 0) {
            furi_string_cat_printf(str, sioDebug);
            FURI_LOG_D(TAG, "SIO: %s", sioDebug);
        }
    } else {
        FURI_LOG_W(TAG, "Failed to decode SIO: %d.  %d bytes consumed", rval.code, rval.consumed);
        furi_string_cat_printf(str, "Failed to decode SIO");
    }

    ASN_STRUCT_FREE(asn_DEF_SIO, sio);

    text_box_set_font(picopass->text_box, TextBoxFontText);
    text_box_set_text(picopass->text_box, furi_string_get_cstr(picopass->text_box_store));
    view_dispatcher_switch_to_view(picopass->view_dispatcher, PicopassViewTextBox);
}

bool picopass_scene_parse_sio_on_event(void* context, SceneManagerEvent event) {
    Picopass* picopass = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(picopass->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(picopass->scene_manager);
    }
    return consumed;
}

void picopass_scene_parse_sio_on_exit(void* context) {
    Picopass* picopass = context;

    // Clear views
    text_box_reset(picopass->text_box);
}
