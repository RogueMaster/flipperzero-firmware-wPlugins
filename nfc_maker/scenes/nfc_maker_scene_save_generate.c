#include "../nfc_maker.h"

static void nfc_maker_scene_save_generate_populate_ndef_buffer(NfcMaker* app) {
    // NDEF Docs: https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/nfc/index.html#nfc-data-exchange-format-ndef
    uint8_t tnf = 0x00;
    const char* type = "";
    uint8_t* payload = NULL;
    uint8_t* payload_it = NULL;
    size_t payload_len = 0;

    size_t data_len = 0;
    switch(scene_manager_get_scene_state(app->scene_manager, NfcMakerSceneStart)) {
    case NfcMakerSceneBluetooth: {
        tnf = 0x02; // Media-type [RFC 2046]
        type = "application/vnd.bluetooth.ep.oob";

        data_len = sizeof(app->mac_buf);
        payload_len = data_len + 2;
        payload = payload_it = malloc(payload_len);

        *payload_it++ = 0x08;
        *payload_it++ = 0x00;
        memcpy(payload_it, app->mac_buf, data_len);
        payload_it += data_len;
        break;
    }
    case NfcMakerSceneContact: {
        tnf = 0x02; // Media-type [RFC 2046]
        type = "text/vcard";

        FuriString* vcard = furi_string_alloc_set("BEGIN:VCARD\r\nVERSION:3.0\r\n");
        furi_string_cat_printf(
            vcard,
            "PRODID:-//"
#ifdef FW_ORIGIN_Momentum
            "MomentumFW"
#else
            "FlipperZero"
#endif
            "//%s//EN\r\n",
            version_get_version(NULL));
        furi_string_cat_printf(vcard, "N:%s;%s;;;\r\n", app->small_buf2, app->small_buf1);
        furi_string_cat_printf(
            vcard,
            "FN:%s%s%s\r\n",
            app->small_buf1,
            app->small_buf2[0] ? " " : "",
            app->small_buf2);
        if(app->mail_buf[0]) {
            furi_string_cat_printf(vcard, "EMAIL:%s\r\n", app->mail_buf);
        }
        if(app->phone_buf[0]) {
            furi_string_cat_printf(vcard, "TEL:%s\r\n", app->phone_buf);
        }
        if(app->big_buf[0]) {
            furi_string_cat_printf(vcard, "URL:%s\r\n", app->big_buf);
        }
        furi_string_cat_printf(vcard, "END:VCARD\r\n");

        payload_len = furi_string_size(vcard);
        payload = payload_it = malloc(payload_len);
        memcpy(payload_it, furi_string_get_cstr(vcard), payload_len);
        payload_it += payload_len;
        furi_string_free(vcard);
        break;
    }
    case NfcMakerSceneSaveGenerate: { // Empty
        tnf = 0x00; // Empty
        type = "";

        payload_len = 0;
        payload = payload_it = NULL;
        break;
    }
    case NfcMakerSceneHttps: {
        tnf = 0x01; // NFC Forum well-known type [NFC RTD]
        type = "U";

        data_len = strlen(app->big_buf);
        payload_len = data_len + 1;
        payload = payload_it = malloc(payload_len);

        *payload_it++ = 0x04; // Prepend "https://"
        memcpy(payload_it, app->big_buf, data_len);
        payload_it += data_len;
        break;
    }
    case NfcMakerSceneMail: {
        tnf = 0x01; // NFC Forum well-known type [NFC RTD]
        type = "U";

        data_len = strlen(app->mail_buf);
        payload_len = data_len + 1;
        payload = payload_it = malloc(payload_len);

        *payload_it++ = 0x06; // Prepend "mailto:"
        memcpy(payload_it, app->mail_buf, data_len);
        payload_it += data_len;
        break;
    }
    case NfcMakerScenePhone: {
        tnf = 0x01; // NFC Forum well-known type [NFC RTD]
        type = "U";

        data_len = strlen(app->phone_buf);
        payload_len = data_len + 1;
        payload = payload_it = malloc(payload_len);

        *payload_it++ = 0x05; // Prepend "tel:"
        memcpy(payload_it, app->phone_buf, data_len);
        payload_it += data_len;
        break;
    }
    case NfcMakerSceneText: {
        tnf = 0x01; // NFC Forum well-known type [NFC RTD]
        type = "T";

        data_len = strlen(app->big_buf);
        payload_len = data_len + 3;
        payload = payload_it = malloc(payload_len);

        *payload_it++ = 0x02;
        *payload_it++ = 'e';
        *payload_it++ = 'n';
        memcpy(payload_it, app->big_buf, data_len);
        payload_it += data_len;
        break;
    }
    case NfcMakerSceneUrl: {
        tnf = 0x01; // NFC Forum well-known type [NFC RTD]
        type = "U";

        data_len = strlen(app->big_buf);
        payload_len = data_len + 1;
        payload = payload_it = malloc(payload_len);

        *payload_it++ = 0x00; // No prepend
        memcpy(payload_it, app->big_buf, data_len);
        payload_it += data_len;
        break;
    }
    case NfcMakerSceneWifi: {
        tnf = 0x02; // Media-type [RFC 2046]
        type = "application/vnd.wfa.wsc";

        // https://android.googlesource.com/platform/packages/apps/Nfc/+/refs/heads/main/src/com/android/nfc/NfcWifiProtectedSetup.java
        // https://github.com/bparmentier/WiFiKeyShare/blob/master/app/src/main/java/be/brunoparmentier/wifikeyshare/utils/NfcUtils.java
        uint8_t ssid_len = strlen(app->small_buf1);
        uint8_t pass_len = strlen(app->small_buf2);
        uint8_t data_len = ssid_len + pass_len;
        payload_len = data_len + 39;
        payload = payload_it = malloc(payload_len);

        // CREDENTIAL_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x0E;
        // CREDENTIAL_FIELD_LEN
        *payload_it++ = 0x00;
        *payload_it++ = data_len + 35;
        // CREDENTIAL_FIELD (contains all subsequent fields)

        // NETWORK_INDEX_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x26;
        // NETWORK_INDEX_FIELD_LEN
        *payload_it++ = 0x00;
        *payload_it++ = 0x01;
        // NETWORK_INDEX_FIELD
        *payload_it++ = 0x01;

        // SSID_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x45;
        // SSID_FIELD_LEN
        *payload_it++ = ssid_len >> 8 & 0xFF;
        *payload_it++ = ssid_len & 0xFF;
        // SSID_FIELD
        memcpy(payload_it, app->small_buf1, ssid_len);
        payload_it += ssid_len;

        // AUTH_TYPE_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x03;
        // AUTH_TYPE_FIELD_LEN
        *payload_it++ = 0x00;
        *payload_it++ = 0x02;
        // AUTH_TYPE_FIELD
        *payload_it++ = 0x00;
        *payload_it++ = scene_manager_get_scene_state(app->scene_manager, NfcMakerSceneWifiAuth);

        // ENC_TYPE_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x0F;
        // ENC_TYPE_FIELD_LEN
        *payload_it++ = 0x00;
        *payload_it++ = 0x02;
        // ENC_TYPE_FIELD
        *payload_it++ = 0x00;
        *payload_it++ = scene_manager_get_scene_state(app->scene_manager, NfcMakerSceneWifiEncr);

        // NETWORK_KEY_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x27;
        // NETWORK_KEY_FIELD_LEN
        *payload_it++ = pass_len >> 8 & 0xFF;
        *payload_it++ = pass_len & 0xFF;
        // NETWORK_KEY_FIELD
        memcpy(payload_it, app->small_buf2, pass_len);
        payload_it += pass_len;

        // MAC_ADDRESS_FIELD_ID
        *payload_it++ = 0x10;
        *payload_it++ = 0x20;
        // MAC_ADDRESS_FIELD_LEN
        *payload_it++ = 0x00;
        *payload_it++ = 0x06;
        // MAC_ADDRESS_FIELD
        *payload_it++ = 0xFF;
        *payload_it++ = 0xFF;
        *payload_it++ = 0xFF;
        *payload_it++ = 0xFF;
        *payload_it++ = 0xFF;
        *payload_it++ = 0xFF;

        break;
    }
    default:
        break;
    }

    // Setup header
    uint8_t flags = 0;
    flags |= 1 << 7; // MB (Message Begin)
    flags |= 1 << 6; // ME (Message End)
    flags |= tnf; // TNF (Type Name Format)
    size_t type_len = strlen(type);

    size_t header_len = 0;
    header_len += 1; // Flags and TNF
    header_len += 1; // Type length
    if(payload_len < 0xFF) {
        flags |= 1 << 4; // SR (Short Record)
        header_len += 1; // Payload length
    } else {
        header_len += 4; // Payload length
    }
    header_len += type_len; // Payload type

    // Start consolidating into NDEF buffer
    size_t record_len = header_len + payload_len;
    app->ndef_size = 1 // TLV type
                     + (record_len < 0xFF ? 1 : 3) // TLV length
                     + record_len // NDEF Record
                     + 1 // Record terminator
        ;
    if(app->ndef_buffer) {
        free(app->ndef_buffer);
    }
    uint8_t* buf = app->ndef_buffer = malloc(app->ndef_size);

    // NDEF TLV block
    *buf++ = 0x03; // TLV type
    if(record_len < 0xFF) {
        *buf++ = record_len; // TLV length
    } else {
        *buf++ = 0xFF; // TLV length
        *buf++ = record_len >> 8; // ...
        *buf++ = record_len & 0xFF; // ...
    }

    // Record header
    *buf++ = flags; // Flags and TNF
    *buf++ = type_len; // Type length
    if(flags & (1 << 4)) { // SR (Short Record)
        *buf++ = payload_len; // Payload length
    } else {
        *buf++ = payload_len >> 24 & 0xFF; // Payload length
        *buf++ = payload_len >> 16 & 0xFF; // ...
        *buf++ = payload_len >> 8 & 0xFF; // ...
        *buf++ = payload_len & 0xFF; // ...
    }
    memcpy(buf, type, type_len); // Payload type
    buf += type_len;

    // Record payload
    memcpy(buf, payload, payload_len);
    buf += payload_len;
    if(payload) {
        free(payload);
    }

    // Record terminator
    *buf++ = 0xFE;

    // Double check size of NDEF data
    furi_check(app->ndef_size == (size_t)(buf - app->ndef_buffer));
}

static void nfc_maker_scene_save_generate_populate_device_mful(NfcMaker* app, Card card_type) {
    const CardDef* card = &cards[card_type];

    nfc_data_generator_fill_data(card->generator, app->nfc_device);
    MfUltralightData* data = mf_ultralight_alloc();
    nfc_device_copy_data(app->nfc_device, NfcProtocolMfUltralight, data);

    size_t size =
        MIN(card->size, // Known size
            data->page[3].data[2] * NTAG_DATA_AREA_UNIT_SIZE // Capability Container
        );
    furi_check(app->ndef_size <= size);
    memcpy(&data->page[4].data[0], app->ndef_buffer, app->ndef_size);
    free(app->ndef_buffer);
    app->ndef_buffer = NULL;

    nfc_device_set_data(app->nfc_device, NfcProtocolMfUltralight, data);
    mf_ultralight_free(data);
}

static void nfc_maker_scene_save_generate_populate_device_t4t(NfcMaker* app, Card card_type) {
    Type4TagData* data = type_4_tag_alloc();

    uint8_t uid[7];
    data->iso14443_4a_data->iso14443_3a_data->uid_len = sizeof(uid);
    furi_hal_random_fill_buf(uid, sizeof(uid));
    if(card_type != CardType4Generic) {
        uid[0] = 0x04; // NXP manufacturer code
    }
    type_4_tag_set_uid(data, uid, sizeof(uid));

    data->iso14443_4a_data->iso14443_3a_data->atqa[0] = 0x44;
    data->iso14443_4a_data->iso14443_3a_data->atqa[1] = 0x03;
    data->iso14443_4a_data->iso14443_3a_data->sak = 0x20;

    data->iso14443_4a_data->ats_data.tl = 6;
    data->iso14443_4a_data->ats_data.t0 = 0x77;
    data->iso14443_4a_data->ats_data.ta_1 = 0x77;
    data->iso14443_4a_data->ats_data.tb_1 = 0x71;
    data->iso14443_4a_data->ats_data.tc_1 = 0x02;
    simple_array_init(data->iso14443_4a_data->ats_data.t1_tk, 1);
    uint8_t* historical_bytes = simple_array_get_data(data->iso14443_4a_data->ats_data.t1_tk);
    historical_bytes[0] = 0x80;

    const bool is_short_record = app->ndef_buffer[1] < 0xFF;
    const size_t ndef_message_size = app->ndef_size - (is_short_record ? 3 : 5);
    const uint8_t* ndef_message = &app->ndef_buffer[is_short_record ? 2 : 4];
    simple_array_init(data->ndef_data, ndef_message_size);
    memcpy(simple_array_get_data(data->ndef_data), ndef_message, ndef_message_size);
    free(app->ndef_buffer);
    app->ndef_buffer = NULL;

    nfc_device_set_data(app->nfc_device, NfcProtocolType4Tag, data);
    type_4_tag_free(data);
}

static void nfc_maker_scene_save_generate_populate_device_mfc(NfcMaker* app, Card card_type) {
    const CardDef* card = &cards[card_type];

    nfc_data_generator_fill_data(card->generator, app->nfc_device);
    MfClassicData* data = mf_classic_alloc();
    nfc_device_copy_data(app->nfc_device, NfcProtocolMfClassic, data);
    const size_t sector_count = mf_classic_get_total_sectors_num(data->type);

    const uint8_t* buf = app->ndef_buffer;
    size_t len = app->ndef_size;
    size_t real_block = 4; // Skip MAD1

    uint8_t* cur = &data->block[real_block].data[0];
    while(len) {
        size_t sector_trailer = mf_classic_get_sector_trailer_num_by_block(real_block);
        const uint8_t* end = &data->block[sector_trailer].data[0];

        const size_t chunk_len = MIN((size_t)(end - cur), len);
        memcpy(cur, buf, chunk_len);
        buf += chunk_len;
        len -= chunk_len;

        if(len) {
            real_block = sector_trailer + 1;
            if(real_block == 64) {
                real_block += 4; // Skip MAD2
            }
            cur = &data->block[real_block].data[0];
        }
    }

    // Format data sector trailers
    MfClassicSectorTrailer data_tr = {
        .key_a = {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}}, // NFC key
        .access_bits = {{0x7F, 0x07, 0x88, 0x40}}, // Default access rights
        .key_b = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}, // Default key
    };
    for(size_t sector = 0; sector < sector_count; sector++) {
        mf_classic_set_sector_trailer_read(
            data, mf_classic_get_sector_trailer_num_by_sector(sector), &data_tr);
    }

    // https://www.nxp.com/docs/en/application-note/AN10787.pdf
    // Format MAD1
    size_t mad_block = 1;
    uint8_t* mad = &data->block[mad_block].data[0];
    mad[1] = 0x01; // Info byte
    mad[2] = 0x03; // NDEF app ID
    mad[3] = 0xE1; // NDEF app ID
    mad[0] = bit_lib_crc8(&mad[1], MF_CLASSIC_BLOCK_SIZE * 2 - 1, 0x1D, 0xC7, false, false, 0x00);
    MfClassicSectorTrailer mad_tr = {
        .key_a = {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}}, // MAD key
        .access_bits = {{0x78, 0x77, 0x88, 0xC1}}, // Read with A/B, write with B
        .key_b = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}, // Default key
    };
    mf_classic_set_sector_trailer_read(
        data, mf_classic_get_sector_trailer_num_by_block(mad_block), &mad_tr);
    // Format MAD2
    if(sector_count > 16) {
        mad_block = 64;
        mad = &data->block[mad_block].data[0];
        mad[1] = 0x01; // Info byte
        mad[0] =
            bit_lib_crc8(&mad[1], MF_CLASSIC_BLOCK_SIZE * 3 - 1, 0x1D, 0xC7, false, false, 0x00);
        mf_classic_set_sector_trailer_read(
            data, mf_classic_get_sector_trailer_num_by_block(mad_block), &mad_tr);
    }

    free(app->ndef_buffer);
    app->ndef_buffer = NULL;

    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, data);
    mf_classic_free(data);
}

static void nfc_maker_scene_save_generate_populate_device_slix(NfcMaker* app, Card card_type) {
    SlixData* data = slix_alloc();

    size_t block_count = 0;
    data->iso15693_3_data->system_info.flags =
        ISO15693_3_SYSINFO_FLAG_DSFID | ISO15693_3_SYSINFO_FLAG_AFI |
        ISO15693_3_SYSINFO_FLAG_MEMORY | ISO15693_3_SYSINFO_FLAG_IC_REF;
    uint8_t uid[8];
    furi_hal_random_fill_buf(uid, sizeof(uid));
    uid[0] = 0xE0; // All ISO15693-3 cards must have this as first UID byte
    uid[1] = 0x04; // NXP manufacturer code

    switch(card_type) {
    case CardSlix:
        block_count = 28;
        uid[2] = 0x01; // ICODE Type
        uid[3] &= ~(0x03 << 3);
        uid[3] |= 0x02 << 3; // Type Indicator
        break;
    case CardSlixS:
        block_count = 40;
        uid[2] = 0x02; // ICODE Type
        break;
    case CardSlixL:
        block_count = 8;
        uid[2] = 0x03; // ICODE Type
        break;
    case CardSlix2:
        block_count = 80;
        uid[2] = 0x01; // ICODE Type
        uid[3] &= ~(0x03 << 3);
        uid[3] |= 0x01 << 3; // Type Indicator
        break;
    default:
        break;
    }

    slix_set_uid(data, uid, sizeof(uid));
    const size_t block_size = SLIX_BLOCK_SIZE;
    const size_t data_area = block_count * block_size;
    data->iso15693_3_data->system_info.block_size = block_size;
    data->iso15693_3_data->system_info.block_count = block_count;
    simple_array_init(data->iso15693_3_data->block_data, data_area);
    simple_array_init(data->iso15693_3_data->block_security, block_count);

    uint8_t* blocks = simple_array_get_data(data->iso15693_3_data->block_data);
    memcpy(&blocks[1 * block_size], app->ndef_buffer, app->ndef_size);

    // https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/nfc/7583/1/EEOL_2011FEB16_EMS_RFD_AN_01.pdf
    // Format Capability Container
    blocks[0] = 0xE1; // NFC Magic Number
    blocks[1] = 0x40; // 0x4X: Version 1, 0xX0: Full R/W access
    blocks[2] = data_area / 8; // Data Area Size: Total byte size / 8
    blocks[3] = 0x01; // MBREAD: Supports Multiple Block Read command

    free(app->ndef_buffer);
    app->ndef_buffer = NULL;

    nfc_device_set_data(app->nfc_device, NfcProtocolSlix, data);
    slix_free(data);
}

void nfc_maker_scene_save_generate_submenu_callback(void* context, uint32_t index) {
    NfcMaker* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void nfc_maker_scene_save_generate_on_enter(void* context) {
    NfcMaker* app = context;
    Submenu* submenu = app->submenu;
    nfc_maker_scene_save_generate_populate_ndef_buffer(app);

    submenu_set_header(submenu, "Tag Type:");

    for(Card card = 0; card < CardMAX; card++) {
        submenu_add_lockable_item(
            submenu,
            cards[card].name,
            card,
            nfc_maker_scene_save_generate_submenu_callback,
            app,
            app->ndef_size > cards[card].size,
            "Data is\ntoo large!");
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, NfcMakerSceneSaveGenerate));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcMakerViewSubmenu);
}

bool nfc_maker_scene_save_generate_on_event(void* context, SceneManagerEvent event) {
    NfcMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, NfcMakerSceneSaveGenerate, event.event);
        if(event.event >= CardMAX) return consumed;
        consumed = true;

        switch(cards[event.event].protocol) {
        case NfcProtocolMfUltralight:
            nfc_maker_scene_save_generate_populate_device_mful(app, event.event);
            break;
        case NfcProtocolType4Tag:
            nfc_maker_scene_save_generate_populate_device_t4t(app, event.event);
            break;
        case NfcProtocolMfClassic:
            nfc_maker_scene_save_generate_populate_device_mfc(app, event.event);
            break;
        case NfcProtocolSlix:
            nfc_maker_scene_save_generate_populate_device_slix(app, event.event);
            break;
        default:
            break;
        }

        scene_manager_next_scene(app->scene_manager, NfcMakerSceneSaveUid);
    }

    return consumed;
}

void nfc_maker_scene_save_generate_on_exit(void* context) {
    NfcMaker* app = context;
    submenu_reset(app->submenu);
}
