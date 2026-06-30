#include "eink_goodisplay_i.h"

#define TAG "GD_Listener"

typedef NfcCommand (*NfcEinkGoodisplayListenerCommandCallback)(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length);

typedef struct {
    uint8_t cmd;
    NfcEinkGoodisplayListenerCommandCallback callback;
} NfcEinkGoodisplayListenerCommandHandler;

static inline uint8_t nfc_eink_screen_apdu_command_A4(
    NfcEinkScreen* const screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    UNUSED(screen);
    FURI_LOG_D(TAG, "A4");
    bool equal = false;
    if(command->data_length == 0x07) {
        const uint8_t app_select_data[] = {0xd2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
        equal = memcmp(command->data, app_select_data, 0x07) == 0;

        NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;
        ctx->listener_state = EinkGoodisplayListenerStateWaitingForConfig;
        eink_goodisplay_on_target_detected(screen);
    } else if(command->data_length == 0x02 && command->data[0] == 0xE1) {
        equal = command->data[1] == 0x03 || command->data[1] == 0x04;
    }
    uint16_t status = equal ? 0x9000 : 0x6A82;
    resp->status = __builtin_bswap16(status);

    return 3;
}

static inline uint8_t nfc_eink_screen_apdu_command_B0(
    NfcEinkScreen* const screen,
    const APDU_Command* cmd,
    APDU_Response* resp) {
    UNUSED(screen);
    FURI_LOG_D(TAG, "B0");
    const APDU_Command_Read* command = (APDU_Command_Read*)cmd;
    APDU_Response_Read* response = (APDU_Response_Read*)resp;

    uint16_t length = command->expected_length;
    uint16_t* status = NULL;

    if(length == 0x000F) {
        response->data_length = __builtin_bswap16(0x000F);
        CC_File* cc_file = (CC_File*)response->data;
        cc_file->mapping_version = 0x20;
        cc_file->r_apdu_max_size = __builtin_bswap16(0x0100);
        cc_file->c_apdu_max_size = __builtin_bswap16(0x00FA);
        cc_file->ctrl_file.type = 0x04;
        cc_file->ctrl_file.length = 0x06;
        cc_file->ctrl_file.file_id = __builtin_bswap16(0xE104);
        cc_file->ctrl_file.file_size = __builtin_bswap16(0x01F4);
        cc_file->ctrl_file.read_access_flags = 0x00;
        cc_file->ctrl_file.write_access_flags = 0x00;

        status = (uint16_t*)(response->data + length - 2);
    } else if(length == 0x0002) {
        response->data_length = 0x0000;
        status = (uint16_t*)(response->data);
        length = 2;
    }

    *status = __builtin_bswap16(0x9000);

    return length + 1 + 2;
}

static inline uint8_t nfc_eink_screen_apdu_command_DA(
    NfcEinkScreen* const screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    UNUSED(screen);
    FURI_LOG_D(TAG, "DA");

    uint8_t length = 0;

    const APDU_Header* hdr = &command->header;
    if(hdr->P1 == 0x00 && hdr->P2 == 0x00 && command->data_length == 0x03) {
        resp->status = __builtin_bswap16(0x9000);
        length = 2 + 1;
    }
    return length;
}

static inline uint8_t nfc_eink_screen_apdu_command_DB(
    NfcEinkScreen* const screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    FURI_LOG_D(TAG, "DB");
    const APDU_Header* hdr = &command->header;
    uint8_t length = 0;
    if(hdr->P1 == 0x02 && hdr->P2 == 0x00 && command->data_length == 0) {
        resp->status = __builtin_bswap16(0x9000);
        length = 2;
    } else if(hdr->P1 == 0x00 && hdr->P2 == 0x00) {
        ///TODO: Add here some saving and parsing logic for screen config
        eink_goodisplay_parse_config(screen, command->data, command->data_length);
        resp->status = __builtin_bswap16(0x9000);
        length = 2;
    }

    return length + 1;
}

static inline uint8_t nfc_eink_screen_apdu_command_D2(
    NfcEinkScreen* screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    UNUSED(resp);

    FURI_LOG_D(TAG, "F0 D2: %d", command->data_length);
    if(screen->device->block_current < screen->device->block_total) {
        uint8_t* data = screen->data->image_data + screen->device->received_data;
        memcpy(data, command->data, command->data_length - 2);
        screen->device->received_data += command->data_length - 2;
    }
    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;
    ctx->was_update = true;

    return 1;
}

static inline uint8_t nfc_eink_screen_apdu_command_D4(
    NfcEinkScreen* screen,
    const APDU_Command* command,
    APDU_Response* resp) {
    FURI_LOG_D(TAG, "D4");
    UNUSED(command);
    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;

    resp->status = 0x0101;
    ctx->update_cnt = 0;
    return 2;
}

static NfcCommand nfc_eink_goodisplay_command_handler_C2(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    FURI_LOG_D(TAG, "C2");
    UNUSED(screen);
    UNUSED(apdu);
    *response_length = 1;
    response->response_code = 0xC2;
    return NfcCommandSleep;
}

static NfcCommand nfc_eink_goodisplay_command_handler_B3(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    FURI_LOG_D(TAG, "B3");
    UNUSED(screen);
    UNUSED(apdu);
    *response_length = 1;
    response->response_code = 0xA2;
    return NfcCommandContinue;
}

static NfcCommand nfc_eink_goodisplay_command_handler_B2(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    UNUSED(apdu);

    NfcCommand command = NfcCommandContinue;
    *response_length = 1;
    response->response_code = 0xA3;

    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;
    ctx->was_update = false;
    ctx->response_cnt++;

    FURI_LOG_D(TAG, "Done B2: %d", ctx->response_cnt);
    if(ctx->response_cnt >= 20) {
        ctx->listener_state = EinkGoodisplayListenerStateUpdatedSuccefully;
        eink_goodisplay_on_done(screen);
        command = NfcCommandStop;
    }
    return command;
}

static uint8_t nfc_eink_goodisplay_command_handler_process_apdu(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    APDU_Response* resp) {
    const APDU_Header* apdu_header = &apdu->header;
    uint8_t response_length = 0;
    if(apdu_header->CLA_byte == 0xF0 && apdu_header->CMD_code == 0xD2) {
        response_length = nfc_eink_screen_apdu_command_D2(screen, apdu, resp);
    } else if(apdu_header->CLA_byte == 0 && apdu_header->CMD_code == 0xA4) {
        response_length = nfc_eink_screen_apdu_command_A4(screen, apdu, resp);
    } else if(apdu_header->CLA_byte == 0 && apdu_header->CMD_code == 0xB0) {
        response_length = nfc_eink_screen_apdu_command_B0(screen, apdu, resp);
    } else if(apdu_header->CLA_byte == 0xF0 && apdu_header->CMD_code == 0xDB) {
        response_length = nfc_eink_screen_apdu_command_DB(screen, apdu, resp);
    } else if(apdu_header->CLA_byte == 0xF0 && apdu_header->CMD_code == 0xDA) {
        response_length = nfc_eink_screen_apdu_command_DA(screen, apdu, resp);
    } else if(apdu_header->CLA_byte == 0xF0 && apdu_header->CMD_code == 0xD4) {
        response_length = nfc_eink_screen_apdu_command_D4(screen, apdu, resp);
    }

    return response_length;
}

static NfcCommand nfc_eink_goodisplay_command_handler_13(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    FURI_LOG_D(TAG, "13");

    *response_length = nfc_eink_goodisplay_command_handler_process_apdu(
        screen, apdu, &response->apdu_resp.apdu_response);

    response->response_code = 0xA3;
    return NfcCommandContinue;
}

static NfcCommand nfc_eink_goodisplay_command_handler_F2(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    FURI_LOG_D(TAG, "F2");
    UNUSED(apdu);
    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;
    if(ctx->update_cnt < 4) {
        response->response_code = 0xF2;
        response->apdu_resp.apdu_response.status = 0x01;
        *response_length = 2;
        ctx->update_cnt++;
    } else {
        *response_length = 3;
        response->response_code = 0x03;
        response->apdu_resp.apdu_response.status = __builtin_bswap16(0x9000);
    }
    return NfcCommandContinue;
}

static NfcCommand nfc_eink_goodisplay_command_handler_02(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    //FURI_LOG_D(TAG, "02");
    NfcCommand command = NfcCommandContinue;

    response->response_code = 0x02;
    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;

    if(!ctx->was_update) {
        *response_length = nfc_eink_goodisplay_command_handler_process_apdu(
            screen, apdu, &response->apdu_resp.apdu_response);
    } else {
        if(screen->device->block_current < screen->device->block_total) {
            uint8_t* data = screen->data->image_data + screen->device->received_data;
            ctx->listener_state = EinkGoodisplayListenerStateReadingBlocks;
            memcpy(data, (uint8_t*)apdu, 2);
            screen->device->received_data += 2;
            screen->device->block_current++;

            *response_length = 3;
            response->response_code = 0x02;
            response->apdu_resp.apdu_response.status = __builtin_bswap16(0x9000);
        } else {
            nfc_eink_screen_set_error(screen, NfcEinkScreenErrorUnableToWrite);
            command = NfcCommandStop;
        }
    }
    return command;
}

static NfcCommand nfc_eink_goodisplay_command_handler_03(
    NfcEinkScreen* screen,
    const APDU_Command* apdu,
    NfcEinkGoodisplayScreenResponse* response,
    uint8_t* response_length) {
    UNUSED(screen);
    NfcCommand command = NfcCommandContinue;
    response->response_code = 0x03; //apdu->command_code;

    *response_length = nfc_eink_goodisplay_command_handler_process_apdu(
        screen, apdu, &response->apdu_resp.apdu_response);
    if(apdu->header.CLA_byte == 0xF0 && apdu->header.CMD_code == 0xD4) {
        response->response_code = 0xF2;
    }

    return command;
}

static const NfcEinkGoodisplayListenerCommandHandler nfc_eink_goodisplay_commands[] = {
    {
        .cmd = 0xC2,
        .callback = nfc_eink_goodisplay_command_handler_C2,
    },
    {
        .cmd = 0xB3,
        .callback = nfc_eink_goodisplay_command_handler_B3,
    },
    {
        .cmd = 0xB2,
        .callback = nfc_eink_goodisplay_command_handler_B2,
    },
    {
        .cmd = 0x13,
        .callback = nfc_eink_goodisplay_command_handler_13,
    },
    {
        .cmd = 0xF2,
        .callback = nfc_eink_goodisplay_command_handler_F2,
    },
    {
        .cmd = 0x02,
        .callback = nfc_eink_goodisplay_command_handler_02,
    },
    {
        .cmd = 0x03,
        .callback = nfc_eink_goodisplay_command_handler_03,
    },
};

static NfcEinkGoodisplayListenerCommandCallback eink_goodisplay_get_command_handler(uint8_t cmd) {
    NfcEinkGoodisplayListenerCommandCallback callback = NULL;
    for(size_t i = 0; i < COUNT_OF(nfc_eink_goodisplay_commands); i++) {
        if(cmd != nfc_eink_goodisplay_commands[i].cmd) continue;
        callback = nfc_eink_goodisplay_commands[i].callback;
        break;
    }
    return callback;
}

NfcCommand eink_goodisplay_listener_callback(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    NfcEinkScreen* screen = context;
    Iso14443_4aListenerEvent* Iso14443_4a_event = event.event_data;
    NfcEinkScreenSpecificGoodisplayContext* ctx = screen->device->screen_context;

    if(Iso14443_4a_event->type == Iso14443_4aListenerEventTypeFieldOff) {
        if(ctx->listener_state == EinkGoodisplayListenerStateReadingBlocks)
            nfc_eink_screen_set_error(screen, NfcEinkScreenErrorTargetLost);
    } else if(Iso14443_4a_event->type == Iso14443_4aListenerEventTypeReceivedData) {
        BitBuffer* buffer = Iso14443_4a_event->data->buffer;

        const NfcEinkScreenCommand* cmd = (NfcEinkScreenCommand*)bit_buffer_get_data(buffer);

        NfcEinkGoodisplayListenerCommandCallback handler =
            eink_goodisplay_get_command_handler(cmd->command_code);
        if(handler != NULL) {
            NfcEinkGoodisplayScreenResponse* response = malloc(250);
            uint8_t response_length = 0;
            command =
                handler(screen, (APDU_Command*)cmd->command_data, response, &response_length);

            bit_buffer_reset(screen->tx_buf);
            bit_buffer_append_bytes(screen->tx_buf, (uint8_t*)response, response_length);

            iso14443_crc_append(Iso14443CrcTypeA, screen->tx_buf);
            free(response);

            nfc_listener_tx(event.instance, screen->tx_buf);
        }
    }
    return command;
}
