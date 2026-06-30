#include "eink_goodisplay.h"
#include "eink_goodisplay_i.h"
#include "eink_goodisplay_config.h"
#include <simple_array.h>

static NfcDevice* eink_goodisplay_nfc_device_4a_alloc() {
    const uint8_t uid[] = {0xC0, 0xC9, 0x06, 0x7F};
    const uint8_t atqa[] = {0x04, 0x00};
    const uint8_t ats_data[] = {0x90, 0xC0, 0xC9, 0x06, 0x7F, 0x00};

    Iso14443_4aData* iso14443_4a_edit_data = iso14443_4a_alloc();
    iso14443_4a_set_uid(iso14443_4a_edit_data, uid, sizeof(uid));

    Iso14443_4aAtsData* _4aAtsData = &iso14443_4a_edit_data->ats_data;
    _4aAtsData->tl = 0x0B;
    _4aAtsData->t0 = 0x78;
    _4aAtsData->ta_1 = 0x33;
    _4aAtsData->tb_1 = 0xA0;
    _4aAtsData->tc_1 = 0x02;

    _4aAtsData->t1_tk = simple_array_alloc(&simple_array_config_uint8_t);
    simple_array_init(_4aAtsData->t1_tk, sizeof(ats_data));
    SimpleArrayData* ats_array = simple_array_get_data(_4aAtsData->t1_tk);
    memcpy(ats_array, ats_data, sizeof(ats_data));

    Iso14443_3aData* base_data = iso14443_4a_get_base_data(iso14443_4a_edit_data);
    iso14443_3a_set_sak(base_data, 0x28);
    iso14443_3a_set_atqa(base_data, atqa);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_set_data(nfc_device, NfcProtocolIso14443_4a, iso14443_4a_edit_data);

    iso14443_4a_free(iso14443_4a_edit_data);
    return nfc_device;
}

static NfcEinkScreenDevice* eink_goodisplay_device_alloc() {
    NfcEinkScreenDevice* device = malloc(sizeof(NfcEinkScreenDevice));

    device->nfc_device = eink_goodisplay_nfc_device_4a_alloc();

    NfcEinkScreenSpecificGoodisplayContext* context =
        malloc(sizeof(NfcEinkScreenSpecificGoodisplayContext));

    context->poller_state = EinkGoodisplayPollerStateSendC2Cmd;
    context->listener_state = EinkGoodisplayListenerStateIdle;

    device->screen_context = context;
    return device;
}

static void eink_goodisplay_free(NfcEinkScreenDevice* instance) {
    furi_assert(instance);
    nfc_device_free(instance->nfc_device);
    free(instance->screen_context);
}

void eink_goodisplay_parse_config(NfcEinkScreen* screen, const uint8_t* data, uint8_t data_length) {
    const char* name = eink_goodisplay_config_get_screen_name(data, data_length);
    furi_string_set(screen->name, name);
    eink_goodisplay_on_config_received(screen);
}

const NfcEinkScreenHandlers goodisplay_handlers = {
    .alloc = eink_goodisplay_device_alloc,
    .free = eink_goodisplay_free,
    .listener_callback = eink_goodisplay_listener_callback,
    .poller_callback = eink_goodisplay_poller_callback,
};
