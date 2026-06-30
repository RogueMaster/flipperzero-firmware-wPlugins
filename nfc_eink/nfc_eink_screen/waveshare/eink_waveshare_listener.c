#include "eink_waveshare_i.h"

#define TAG "WSH_Listener"

typedef NfcCommand (
    *EinkWavashareListenerCommandCallback)(NfcEinkScreen* screen, const uint8_t* data);

typedef struct {
    uint8_t cmd;
    EinkWavashareListenerCommandCallback callback;
} NfcEinkWaveshareListenerCommandHandler;

static NfcCommand eink_waveshare_listener_FF_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    if(data[1] == 0xFE) {
        bit_buffer_append_byte(screen->tx_buf, 0xEE);
        bit_buffer_append_byte(screen->tx_buf, 0xFF);
    }
    return NfcCommandContinue;
}

static NfcCommand
    eink_waveshare_listener_read_page_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    uint8_t index = data[1];
    FURI_LOG_D(TAG, "Read page: %d", index);

    NfcEinkWaveshareSpecificContext* ctx = screen->device->screen_context;

    bit_buffer_append_bytes(screen->tx_buf, &ctx->buf[index * 4], 4 * 4);
    return NfcCommandContinue;
}

static NfcCommand
    eink_waveshare_listener_write_page_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    uint8_t index = data[1];

    NfcEinkWaveshareSpecificContext* ctx = screen->device->screen_context;
    uint8_t* blocks = ctx->buf;

    memcpy(&blocks[index * 4], &data[2], 4);
    bit_buffer_append_byte(screen->tx_buf, 0x0A);
    return NfcCommandContinue;
}

static NfcCommand
    eink_waveshare_listener_default_cmd_response(NfcEinkScreen* screen, const uint8_t* data) {
    UNUSED(data);
    bit_buffer_append_byte(screen->tx_buf, 0x00);
    bit_buffer_append_byte(screen->tx_buf, 0x00);
    return NfcCommandContinue;
}

static NfcCommand eink_waveshare_listener_unknown_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    FURI_LOG_D(TAG, "Unknown: %02X %02X", data[0], data[1]);
    return eink_waveshare_listener_default_cmd_response(screen, data);
}

static NfcCommand
    eink_waveshare_listener_select_type_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    eink_waveshare_parse_config(screen, data + 2, 1);
    return eink_waveshare_listener_default_cmd_response(screen, data);
}

static NfcCommand
    eink_waveshare_listener_power_off_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    NfcEinkWaveshareSpecificContext* ctx = screen->device->screen_context;
    ctx->listener_state = NfcEinkWaveshareListenerStateUpdatedSuccefully;
    return eink_waveshare_listener_default_cmd_response(screen, data);
}

static NfcCommand eink_waveshare_listener_init_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    NfcEinkWaveshareSpecificContext* ctx = screen->device->screen_context;
    ctx->listener_state = NfcEinkWaveshareListenerStateWaitingForConfig;
    eink_waveshare_on_target_detected(screen);
    return eink_waveshare_listener_default_cmd_response(screen, data);
}

static NfcCommand
    eink_waveshare_listener_data_write_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    NfcEinkScreenData* const screen_data = screen->data;
    NfcEinkScreenDevice* const screen_device = screen->device;
    NfcEinkWaveshareSpecificContext* ctx = screen->device->screen_context;

    memcpy(screen_data->image_data + screen_device->received_data, &data[3], data[2]);
    screen_device->received_data += data[2];

    ctx->listener_state = NfcEinkWaveshareListenerStateReadingBlocks;
    return eink_waveshare_listener_default_cmd_response(screen, data);
}

static NfcCommand
    eink_waveshare_listener_wait_for_ready_cmd(NfcEinkScreen* screen, const uint8_t* data) {
    UNUSED(data);
    bit_buffer_append_byte(screen->tx_buf, 0xFF);
    bit_buffer_append_byte(screen->tx_buf, 0x00);
    return NfcCommandContinue;
}

static const NfcEinkWaveshareListenerCommandHandler nfc_eink_waveshare_commands[] = {
    {
        .cmd = EINK_WAVESHARE_COMMAND_FF_FE,
        .callback = eink_waveshare_listener_FF_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_WRITE_PAGE,
        .callback = eink_waveshare_listener_write_page_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_READ_PAGE,
        .callback = eink_waveshare_listener_read_page_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_SELECT_TYPE,
        .callback = eink_waveshare_listener_select_type_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_POWER_OFF,
        .callback = eink_waveshare_listener_power_off_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_INIT,
        .callback = eink_waveshare_listener_init_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_DATA_WRITE,
        .callback = eink_waveshare_listener_data_write_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_DATA_WRITE_V2,
        .callback = eink_waveshare_listener_data_write_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_WAIT_FOR_READY,
        .callback = eink_waveshare_listener_wait_for_ready_cmd,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_NORMAL_MODE,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_SET_CONFIG_1,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_POWER_ON,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_SET_CONFIG_2,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_LOAD_TO_MAIN,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_PREPARE_DATA,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_REFRESH,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
    {
        .cmd = EINK_WAVESHARE_COMMAND_POWER_ON_V2,
        .callback = eink_waveshare_listener_default_cmd_response,
    },
};

static EinkWavashareListenerCommandCallback
    eink_waveshare_listener_get_command_callback(const uint8_t* command) {
    furi_assert(command);
    uint8_t cmd_code = (command[0] == EINK_WAVESHARE_COMMAND_SPECIFIC) ? command[1] : command[0];

    EinkWavashareListenerCommandCallback callback = eink_waveshare_listener_unknown_cmd;
    for(size_t i = 0; i < COUNT_OF(nfc_eink_waveshare_commands); i++) {
        if(nfc_eink_waveshare_commands[i].cmd != cmd_code) continue;
        callback = nfc_eink_waveshare_commands[i].callback;
        break;
    }
    return callback;
}

NfcCommand eink_waveshare_listener_process(Nfc* nfc, NfcEinkScreen* screen, const uint8_t* data) {
    NfcCommand command = NfcCommandContinue;
    bit_buffer_reset(screen->tx_buf);

    EinkWavashareListenerCommandCallback handler =
        eink_waveshare_listener_get_command_callback(data);
    if(handler != NULL) {
        command = handler(screen, data);
        iso14443_crc_append(Iso14443CrcTypeA, screen->tx_buf);
    }

    if(bit_buffer_get_size_bytes(screen->tx_buf) > 0) {
        NfcError error = nfc_listener_tx(nfc, screen->tx_buf);
        if(error != NfcErrorNone) {
            FURI_LOG_E(TAG, "Tx error");
        }
    }

    return command;
}

NfcCommand eink_waveshare_listener_callback(NfcGenericEvent event, void* context) {
    NfcCommand command = NfcCommandContinue;
    NfcEinkScreen* screen = context;
    Iso14443_3aListenerEvent* Iso14443_3a_event = event.event_data;

    NfcEinkWaveshareSpecificContext* ctx = screen->device->screen_context;

    if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedData) {
        FURI_LOG_D(TAG, "ReceivedData");
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeFieldOff) {
        FURI_LOG_D(TAG, "FieldOff");
        if(ctx->listener_state == NfcEinkWaveshareListenerStateUpdatedSuccefully)
            eink_waveshare_on_done(screen);
        else if(ctx->listener_state != NfcEinkWaveshareListenerStateIdle)
            nfc_eink_screen_set_error(screen, NfcEinkScreenErrorTargetLost);
        command = NfcCommandStop;
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeHalted) {
        FURI_LOG_D(TAG, "Halted");
        if(ctx->listener_state == NfcEinkWaveshareListenerStateUpdatedSuccefully)
            eink_waveshare_on_done(screen);
    } else if(Iso14443_3a_event->type == Iso14443_3aListenerEventTypeReceivedStandardFrame) {
        BitBuffer* buffer = Iso14443_3a_event->data->buffer;

        const uint8_t* data = bit_buffer_get_data(buffer);
        command = eink_waveshare_listener_process(event.instance, screen, data);
    }

    return command;
}
