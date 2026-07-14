
#include "lora_receiver_i.h"
#include "lora_app.h"

#include <gui/elements.h>

/**
 * @brief Convert a hex string to a string.
 * @param hex_str Pointer to the hex string.
 * @param output_str Pointer to the output string.
 * @return void
 */
static void hex_to_string(char* hex_str, char* output_str) {
    size_t len = strlen(hex_str);

    if(len % 2 != 0) {
        printf("Error: Hex string length must be even.\n");
        return;
    }

    for(size_t i = 0; i < len; i += 2) {
        char hex_pair[3] = {hex_str[i], hex_str[i + 1], '\0'};
        output_str[i / 2] = (char)strtol(hex_pair, NULL, 16);
    }

    output_str[len / 2] = '\0'; // Null-terminate the string
}

/**
 * Trim everything before and including the given delimiter.
 * Returns -1 if delimiter not found.
 */
static int trim_until_delimiter(FuriString* line, char delimiter) {
    size_t index = furi_string_search_char(line, delimiter, 0);
    if(index == FURI_STRING_FAILURE) {
        return -1;
    }
    furi_string_right(line, index + 1);
    return 0;
}

// -- PARSING METHODS -----------------------------------------------

static int parse_pending(FuriString* line, LoraMsgResponseModel* msg_response) {
    if(furi_string_search_str(line, "FPENDING", 0) != FURI_STRING_FAILURE) {
        msg_response->is_pending = true;
        return 0;
    }
    return 1;
}

static int parse_link_info(FuriString* line, LoraMsgResponseModel* msg_response) {
    size_t index = furi_string_search_str(line, "Link ", 0);
    if(index == FURI_STRING_FAILURE) return 1;

    const char* str_start = furi_string_get_cstr(line) + index + 5;
    char* endptr = NULL;
    msg_response->margin = (uint8_t)strtol(str_start, &endptr, 10);

    if(*endptr == ',') {
        msg_response->gateway_count = (uint8_t)strtol(endptr + 1, NULL, 10);
    }
    return 0;
}

static int parse_rxwin_info(FuriString* line, LoraMsgResponseModel* msg_response) {
    size_t index = furi_string_search_str(line, "RXWIN", 0);
    if(index == FURI_STRING_FAILURE) return 1;

    msg_response->rx_window = (uint8_t)(furi_string_get_char(line, index + 5) - '0');

    const char* str_start = furi_string_get_cstr(line) + index + 14;
    char* endptr = NULL;

    msg_response->rssi = -(int8_t)strtol(str_start, &endptr, 10);
    msg_response->snr = (int8_t)strtol(endptr + 6, NULL, 10);
    return 0;
}

static int parse_ack(FuriString* line, LoraMsgResponseModel* msg_response) {
    if(furi_string_search_str(line, "ACK Received", 0) != FURI_STRING_FAILURE) {
        msg_response->is_ack = true;
        return 0;
    }
    return 1;
}

static int parse_multicast(FuriString* line, LoraMsgResponseModel* msg_response) {
    if(furi_string_search_str(line, "MULTICAST", 0) != FURI_STRING_FAILURE) {
        msg_response->is_multicast = true;
        return 0;
    }
    return 1;
}

static int parse_port(FuriString* line, LoraMsgResponseModel* msg_response) {
    size_t index = furi_string_search_str(line, "PORT: ", 0);
    if(index != FURI_STRING_FAILURE) {
        const char* str_start = furi_string_get_cstr(line) + index + 6;
        msg_response->port = (uint8_t)strtol(str_start, NULL, 10);
    }
    return 0;
}

// Function to parse the RX packet without trimming the input string
static int parse_rx_packet(FuriString* line, LoraMsgResponseModel* rx_response) {
    if(!line || !rx_response) return -1;

    // Check if the line contains "RX"
    if(furi_string_search_str(line, "RX", 0) == FURI_STRING_FAILURE) {
        return 1;
    }

    size_t index = furi_string_search_char(line, '"', 0) + 1; // Skip the first quote

    if(index != FURI_STRING_FAILURE) {
        const char* data_start = furi_string_get_cstr(line) + index;
        strcpy(rx_response->data, data_start);

        size_t length = strlen(rx_response->data);
        if(length > 0 && rx_response->data[length - 2] == '"') {
            rx_response->data[length - 2] = '\0'; // Remove the trailing quote
        }
        return 0;
    }

    return 0;
}

static int parse_msg_response(FuriString* line, LoraMsgResponseModel* msg_response) {
    if(!line || !msg_response) return -1;

    FURI_LOG_D("parse_msg_response", "%s", furi_string_get_cstr(line));

    if(trim_until_delimiter(line, ':') < 0) {
        return -1; // Delimiter not found
    }
    // Try each parser until one succeeds
    if(parse_pending(line, msg_response) == 0) return LoraPacketFpending;
    if(parse_link_info(line, msg_response) == 0) return LoraPacketLinkInfo;
    if(parse_rxwin_info(line, msg_response) == 0) return LoraPacketRxwinInfo;
    if(parse_ack(line, msg_response) == 0) return LoraPacketAck;
    if(parse_multicast(line, msg_response) == 0) return LoraPacketMulticast;
    if(parse_rx_packet(line, msg_response) == 0) {
        if(parse_port(line, msg_response) == 0) { // Port and Data are in the same line in MSG
            return LoraPacketRxPacket;
        }
    }
    // If none matched, fallback to RX packet
    return 1;
}

/**
 * @brief Set the data message response and update the view.
 * @param receiver Pointer to the LoraReceiver object.
 * @param data Pointer to the data to be set.
 */
void lora_receiver_decode_msg_response(void* context, FuriString* line) {
    LoraApp* app = context;
    furi_assert(app);
    furi_assert(line);
    /* *INDENT-OFF* */
    with_view_model(
        app->receiver->view,
        LoraReceiverModel * model,
        {
            LoraPacketType type_packet = parse_msg_response(line, &model->msg_response);

            // Copy the string data to be sent by Bluetooth
            if(type_packet == LoraPacketRxPacket) {
                hex_to_string(model->msg_response.data, model->msg_response.decoded_data);
                prepare_bt_data_str(app->bt_transmitter, model->msg_response.decoded_data);
            }

            // Can call more prepare bluetooth data functions here depending of the type of packet
        },
        true);
    /* *INDENT-ON* */
}

static void lora_receiver_draw_callback(Canvas* canvas, void* _model) {
    LoraReceiverModel* model = _model;
    char temp_str[18];
    canvas_draw_line(canvas, 2, 22, 126, 22);
    canvas_set_font(canvas, FontPrimary);

    snprintf(temp_str, 18, "%ld.%d MHz", model->config.freq, canal_list[model->config.canal_idx]);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, temp_str);

    canvas_set_font(canvas, FontSecondary);
    snprintf(temp_str, 18, "RSSI: %d dBm", model->msg_response.rssi);
    canvas_draw_str(canvas, 2, 20, "RSSI: ");

    snprintf(temp_str, 18, "SNR: %d dB", model->msg_response.snr);
    canvas_draw_str_aligned(canvas, 126, 20, AlignRight, AlignBottom, temp_str);

    elements_text_box(
        canvas, 2, 25, 128, 25, AlignLeft, AlignTop, model->msg_response.decoded_data, true);

    elements_button_up(canvas, "");
    elements_button_down(canvas, "");
    elements_button_right(canvas, "Config");
}

static void lora_receiver_next_canal_callback(LoraReceiver* receiver) {
    furi_assert(receiver);
    /* *INDENT-OFF* */
    with_view_model(
        receiver->view,
        LoraReceiverModel * model,
        { model->config.canal_idx = (model->config.canal_idx + 1) % CANAL_LIST_SIZE; },
        true);
    /* *INDENT-ON* */
}

static void lora_receiver_prev_canal_callback(LoraReceiver* receiver) {
    furi_assert(receiver);
    /* *INDENT-OFF* */
    with_view_model(
        receiver->view,
        LoraReceiverModel * model,
        {
            if(model->config.canal_idx > 0) {
                model->config.canal_idx--;
            } else {
                model->config.canal_idx = CANAL_LIST_SIZE - 1;
            }
        },
        true);
    /* *INDENT-ON* */
}

static bool lora_receiver_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    LoraReceiver* receiver = context;
    bool consumed = false;
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyUp) {
            consumed = true;
            lora_receiver_next_canal_callback(receiver);
            receiver->view_callback(LoraReceiverEventCfgSet, receiver->context);
        } else if(event->key == InputKeyDown) {
            consumed = true;
            lora_receiver_prev_canal_callback(receiver);
            receiver->view_callback(LoraReceiverEventCfgSet, receiver->context);
        } else if(event->key == InputKeyRight) {
            consumed = true;
            receiver->view_callback(LoraReceiverEventConfig, receiver->context);
        }
    }
    return consumed;
}

static void lora_receiver_init_cfg_model(void* context) {
    furi_assert(context);
    LoraReceiver* lora_receiver = context;
    /* *INDENT-OFF* */
    with_view_model(
        lora_receiver->view,
        LoraReceiverModel * model,
        { init_lora_config_default(&model->config); },
        true);
    /* *INDENT-ON* */
}

static void lora_receiver_init_msg_model(void* context) {
    furi_assert(context);
    LoraReceiver* lora_receiver = context;
    /* *INDENT-OFF* */
    with_view_model(
        lora_receiver->view,
        LoraReceiverModel * model,
        {
            model->msg_response.margin = 0;
            model->msg_response.gateway_count = 0;
            model->msg_response.rx_window = 0;
            model->msg_response.rssi = 0;
            model->msg_response.snr = 0;
            model->msg_response.port = 0;
            model->msg_response.is_multicast = false;
            model->msg_response.is_pending = false;
            model->msg_response.is_ack = false;
            model->msg_response.data[0] = '\0';
            model->msg_response.decoded_data[0] = '\0';
        },
        true)
    /* *INDENT-ON* */
}

static void lora_receiver_init(void* context) {
    furi_assert(context);
    lora_receiver_init_cfg_model(context);
    lora_receiver_init_msg_model(context);
}

LoraReceiver* lora_receiver_alloc(void) {
    LoraReceiver* receiver = malloc(sizeof(LoraReceiver));
    furi_assert(receiver);

    receiver->process_callback = lora_receiver_default_response_callback;
    receiver->view = view_alloc();
    view_allocate_model(receiver->view, ViewModelTypeLocking, sizeof(LoraReceiverModel));

    lora_receiver_init(receiver);

    view_set_context(receiver->view, receiver);
    view_set_draw_callback(receiver->view, lora_receiver_draw_callback);
    view_set_input_callback(receiver->view, lora_receiver_input_callback);
    return receiver;
}

void lora_receiver_free(LoraReceiver* receiver) {
    furi_assert(receiver);
    view_free_model(receiver->view);
    view_free(receiver->view);
    free(receiver);
}

View* lora_receiver_get_view(LoraReceiver* receiver) {
    furi_assert(receiver);
    return receiver->view;
}

void lora_receiver_rx_response_callback(FuriString* line, void* context) {
    furi_assert(context);
    FURI_LOG_D("lora_receiver_rx_response_callback", "%s", furi_string_get_cstr(line));
    LoraApp* app = context;
    lora_receiver_decode_msg_response(app, line);

    bt_transmitter_send(app->bt_transmitter);
}

void lora_receiver_default_response_callback(FuriString* line, void* context) {
    UNUSED(line);
    UNUSED(context);
    FURI_LOG_D(
        "lora_receiver_default_response_callback", "received: %s", furi_string_get_cstr(line));
}

void lora_receiver_join_response_callback(FuriString* line, void* context) {
    FURI_LOG_D("lora_receiver_join_response", "%s", furi_string_get_cstr(line));
    LoraApp* app = context;
    if(furi_string_start_with(line, "+JOIN_CMD: Network joined")) {
        lora_state_manager_set_state(app->state_manager, JOINED);
        FURI_LOG_D("lora_receiver_join_response", "Network joined");
        return;
    }
}

static LoraReceiverProcessCallback lora_receiver_retrieve_callback(LoraState state) {
    switch(state) {
        {
        case JOINED:
            return lora_receiver_join_response_callback;
        case RX:
            return lora_receiver_rx_response_callback;
        }
    default:
        return lora_receiver_default_response_callback;
    }
}

void lora_receiver_update_process_callback(LoraReceiver* receiver, LoraState state) {
    furi_assert(receiver);
    receiver->process_callback = lora_receiver_retrieve_callback(state);
}

LoraReceiverProcessCallback lora_receiver_get_callback(LoraReceiver* receiver) {
    furi_assert(receiver);
    return receiver->process_callback;
}

void lora_receiver_set_state_manager(LoraReceiver* receiver, LoraStateManager* state_manager) {
    furi_assert(receiver);
    furi_assert(state_manager);
    receiver->state_manager = state_manager;
}

void lora_receiver_set_view_callback(
    LoraReceiver* receiver,
    LoraReceiverViewCallbak callback,
    void* context) {
    furi_assert(receiver);
    furi_assert(callback);

    /* *INDENT-OFF* */
    with_view_model(
        receiver->view,
        LoraReceiverModel * model,
        {
            UNUSED(model);
            receiver->view_callback = callback;
            receiver->context = context;
        },
        false);
    /* *INDENT-ON* */
}
