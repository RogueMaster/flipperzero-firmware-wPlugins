#include "uds_library.h"

// Function to malloc the instance
UDS_SERVICE* uds_service_alloc(
    uint32_t id_to_send,
    uint32_t id_to_received,
    MCP_MODE mode,
    MCP_CLOCK clk,
    MCP_BITRATE bitrate) {
    UDS_SERVICE* instance = malloc(sizeof(UDS_SERVICE));
    instance->CAN = mcp_alloc(mode, clk, bitrate);
    instance->id_to_send = id_to_send;
    instance->id_to_received = id_to_received;

    return instance;
}

// Init the mcp2515 with it respeclty mask and filters
bool uds_init(UDS_SERVICE* uds_instance) {
    MCP2515* CAN = uds_instance->CAN;
    CAN->mode = MCP_NORMAL;
    if(mcp2515_init(CAN) != ERROR_OK) return false;

    uint32_t mask = 0x7ff;

    if(uds_instance->id_to_received > 0x7ff) {
        mask = 0x1FFFFFFF;
    }

    init_mask(CAN, 0, mask);
    init_filter(CAN, 0, uds_instance->id_to_received);
    init_filter(CAN, 1, uds_instance->id_to_received);

    init_mask(CAN, 1, mask);
    init_filter(CAN, 2, uds_instance->id_to_received);
    init_filter(CAN, 3, uds_instance->id_to_received);
    init_filter(CAN, 4, uds_instance->id_to_received);
    init_filter(CAN, 5, uds_instance->id_to_received);

    return true;
}

// Free instance
void free_uds(UDS_SERVICE* uds_instance) {
    deinit_mcp2515(uds_instance->CAN);
    free(uds_instance->CAN);
    free(uds_instance);
}

// Get Frames
bool read_frames_uds(MCP2515* CAN, uint32_t id, CANFRAME* frame) {
    uint32_t time_delay = 0;

    do {
        if(read_can_message(CAN, frame) == ERROR_OK) {
            if(frame->canId == id) {
                return true;
            }
        }
        furi_delay_us(1);
        time_delay++;

    } while((time_delay < 6000));

    // log_exception("Error");

    return false;
}

// Function to send a service
bool uds_single_frame_request(
    UDS_SERVICE* uds_instance,
    uint8_t* data_to_send,
    uint8_t count_of_bytes,
    CANFRAME* frames_to_received,
    uint8_t count_of_frames) {
    MCP2515* CAN = uds_instance->CAN;
    CANFRAME frame_to_send = {0};
    frame_to_send.canId = uds_instance->id_to_send;
    frame_to_send.data_lenght = count_of_bytes + 1;
    uint32_t id_to_received = uds_instance->id_to_received;
    ERROR_CAN ret = ERROR_OK;

    for(uint8_t i = 0; i < frame_to_send.data_lenght; i++)
        frame_to_send.buffer[i] = data_to_send[i];

    ret = send_can_frame(CAN, &frame_to_send);

    if(ret != ERROR_OK) return false;

    memset(frame_to_send.buffer, 0, sizeof(frame_to_send.buffer));
    frame_to_send.buffer[0] = 0x30;
    frame_to_send.data_lenght = 3;

    for(uint8_t i = 0; i < count_of_frames; i++) {
        if(i == 1) {
            ret = send_can_frame(CAN, &frame_to_send);
            if(ret != ERROR_OK) return false;
        }

        if(!read_frames_uds(CAN, id_to_received, &(frames_to_received[i]))) {
            if(i == 0)
                return false;
            else
                break;
        }
    }

    return true;
}

// Function to get VIN
bool uds_get_vin(UDS_SERVICE* uds_instance, FuriString* text) {
    CANFRAME canframes[3];

    memset(canframes, 0, sizeof(canframes));

    uint8_t request[4] = {0x3, 0x22, 0xf1, 0x90};

    if(!uds_single_frame_request(uds_instance, request, 3, canframes, 3)) return false;

    if(canframes[0].buffer[2] != 0x62) return false;

    char vin_name[17] = {'\0'};

    uint8_t pos = 0;

    for(uint8_t i = 0; i < 3; i++) {
        uint8_t start_num = (i == 0) ? 5 : 1;

        // Save bytes in the char array
        for(uint8_t j = start_num; j < 8; j++) {
            vin_name[pos] = (char)canframes[i].buffer[j];
            if(pos >= sizeof(vin_name) - 1) break;
            pos++;
        }
    }

    furi_string_reset(text);

    furi_string_cat_printf(text, "%.17s", vin_name);

    return true;
}

// Function to send multiframes
// This will be on development
bool uds_multi_frame_request(
    UDS_SERVICE* uds_instance,
    uint8_t* data,
    uint8_t length,
    CANFRAME* canframes_to_send,
    uint8_t count_of_frames_to_received,
    CANFRAME* canframes_to_received) {
    uint8_t size_frames_to_send = 1;

    // Condition to know the count of frames to send if the data need more frames
    if(length > 7) {
        if(((length - 6) % 7) != 0) {
            size_frames_to_send = ((length - 6) / 7) + 2;
        } else {
            size_frames_to_send = ((length - 6) / 7) + 1;
        }
    }

    // Set the frames if the data need more than one can frame
    if(size_frames_to_send > 1) {
        canframes_to_send[0].buffer[0] = 0x10; // Set the byte to indicate the first frame
        canframes_to_send[0].buffer[1] = length; // The length of the data

        // This counter works as a pivot to save the data in it respective byte of any frame
        uint8_t counter = 0;

        for(uint8_t i = 0; i < size_frames_to_send; i++) {
            canframes_to_send[i].canId = uds_instance->id_to_send; // Set the id to be sent
            canframes_to_send[i].data_lenght = 8; // Set the lenght

            if(i >= 1) canframes_to_send[i].buffer[0] = (0x20) + (i & 0xf); // every

            uint8_t start_num = (i == 0) ? 2 : 1;

            for(uint8_t j = start_num; j < 8; j++) {
                canframes_to_send[i].buffer[j] = data[counter++];

                if(counter == length) {
                    canframes_to_send[i].data_lenght = j + 1;
                }
            }
        }
    }

    // If the data only needs one frame
    else {
        canframes_to_send[0].canId = uds_instance->id_to_send;
        canframes_to_send[0].data_lenght = length + 1;
        canframes_to_send[0].buffer[0] = length;
        for(uint8_t i = 1; i < (length + 1); i++) {
            canframes_to_send[0].buffer[i] = data[i - 1];
        }
    }

    canframes_to_send[size_frames_to_send].canId = uds_instance->id_to_send;
    canframes_to_send[size_frames_to_send].data_lenght = 3;
    canframes_to_send[size_frames_to_send].buffer[0] = 0x30;

    //  Just for debbug
    FuriString* text = furi_string_alloc();

    for(uint8_t j = 0; j < size_frames_to_send; j++) {
        furi_string_reset(text);
        furi_string_cat_printf(text, "%lx\t", canframes_to_send[j].canId);
        for(uint8_t i = 0; i < canframes_to_send[j].data_lenght; i++) {
            furi_string_cat_printf(text, "%x ", canframes_to_send[j].buffer[i]);
        }
    }

    furi_string_free(text);

    // From here is the work to send de canbus data

    // Send the first frame
    if(send_can_frame(uds_instance->CAN, &canframes_to_send[0]) != ERROR_OK) {
        return false;
    }

    // Wait message of the response
    if(!read_frames_uds(
           uds_instance->CAN, uds_instance->id_to_received, &canframes_to_received[0])) {
        return false;
    }

    // To received multiple frames
    if(canframes_to_received[0].buffer[0] == 0x10 && count_of_frames_to_received > 1) {
        // If there no more data will pass this code
        send_can_frame(uds_instance->CAN, &canframes_to_send[size_frames_to_send]);

        for(uint8_t i = 1; i < count_of_frames_to_received; i++) {
            if(!read_frames_uds(
                   uds_instance->CAN, uds_instance->id_to_received, &canframes_to_received[i]))
                break;
        }
    }

    // To know if it is only one frame to send
    if(size_frames_to_send == 1) {
        canframes_to_send[size_frames_to_send].canId = 0;
        return true;
    }

    /*
        Here is the end for only one frame received.
        The next code describe how to send multple frames
    */

    // If the flow control is not ok
    if(canframes_to_received[0].buffer[0] != 0x30) {
        return false;
    }

    // Send the rest of the data
    for(uint8_t i = 1; i < size_frames_to_send; i++) {
        send_can_frame(uds_instance->CAN, &canframes_to_send[i]);
    }

    // Read the first ECU's response
    if(!read_frames_uds(
           uds_instance->CAN, uds_instance->id_to_received, &canframes_to_received[0])) {
        return false;
    }

    // To received multiple frames
    if(canframes_to_received[0].buffer[0] == 0x10 && count_of_frames_to_received > 1) {
        // If there no more data will pass this code
        send_can_frame(uds_instance->CAN, &canframes_to_send[size_frames_to_send]);

        for(uint8_t i = 1; i < count_of_frames_to_received; i++) {
            if(!read_frames_uds(
                   uds_instance->CAN, uds_instance->id_to_received, &canframes_to_received[i]))
                break;
        }
    }

    canframes_to_send[size_frames_to_send].canId = 0;

    return true;
}

// Set diagnostic session
bool uds_set_diagnostic_session(UDS_SERVICE* uds_instance, diagnostic_session session) {
    uint8_t data[2] = {0x10, (uint8_t)session};

    if(session == 0) return false;

    CANFRAME frame_to_send = {0};
    CANFRAME frame_to_received = {0};

    if(!uds_multi_frame_request(
           uds_instance, data, COUNT_OF(data), &frame_to_send, 1, &frame_to_received))
        return false;

    if(frame_to_received.buffer[1] != 0x50) return false;

    return true;
}

// Reset the ECU
bool uds_reset_ecu(UDS_SERVICE* uds_instance, type_ecu_reset type) {
    uint8_t data[2] = {0x11, (uint8_t)type};

    if(type == 0) return false;

    CANFRAME frame_to_send = {0};
    CANFRAME frame_to_received = {0};

    if(!uds_multi_frame_request(
           uds_instance, data, COUNT_OF(data), &frame_to_send, 1, &frame_to_received))
        return false;

    if(frame_to_received.buffer[1] != 0x51) return false;

    return true;
}

// Get count of DTC
bool uds_get_count_stored_dtc(UDS_SERVICE* uds_instance, uint16_t* count_of_dtc) {
    uint8_t data[3] = {0x19, 0x1, 0xff};

    CANFRAME frame_to_send = {0};
    CANFRAME frame_to_received = {0};

    if(!uds_multi_frame_request(
           uds_instance, data, COUNT_OF(data), &frame_to_send, 1, &frame_to_received))
        return false;

    if(frame_to_received.buffer[1] != 0x59) return false;

    *count_of_dtc = (uint16_t)frame_to_received.buffer[5] << 8 | frame_to_received.buffer[6];

    return true;
}

// Show the real DTC
void get_data_trouble_code(char* text, uint8_t* data) {
    FuriString* code = furi_string_alloc();

    uint8_t letter = data[0] >> 6;
    uint8_t first_digit = (data[0] >> 4) & 0b0011;
    uint8_t second_digit = data[0] & 0xf;
    uint8_t third_digit = (data[1]) >> 4;
    uint8_t fourth_digit = data[1] & 0xf;

    switch(letter) {
    case 0:
        text[0] = 'P';
        break;

    case 1:
        text[0] = 'C';
        break;

    case 2:
        text[0] = 'B';
        break;

    case 3:
        text[0] = 'U';
        break;

    default:
        break;
    }

    furi_string_printf(
        code, "%c%u%u%u%u", text[0], first_digit, second_digit, third_digit, fourth_digit);

    for(uint8_t i = 0; i < 5; i++) {
        text[i] = furi_string_get_char(code, i);
    }

    furi_string_free(code);
}

// Get the DTC
bool uds_get_stored_dtc(UDS_SERVICE* uds_instance, char* codes[], uint16_t* count_of_dtc) {
    // To get the count of DTC stored
    if(!uds_get_count_stored_dtc(uds_instance, count_of_dtc)) {
        return false;
    }

    uint8_t data[3] = {0x19, 0x2, 0xff};

    CANFRAME frame_to_send = {0};
    CANFRAME* frame_to_received = calloc(20, sizeof(CANFRAME));

    // Get the canframes with the data
    if(!uds_multi_frame_request(
           uds_instance, data, COUNT_OF(data), &frame_to_send, 20, frame_to_received)) {
        free(frame_to_received);
        return false;
    }

    uint8_t data_count = *count_of_dtc * 4;
    uint8_t data_dtc[*count_of_dtc][data_count];

    // If the message has error
    if(frame_to_received[0].buffer[0] == 0x7F) {
        free(frame_to_received);
        return false;
    }

    // If the data has only 1 DTC code
    if(*count_of_dtc == 1) {
        for(uint8_t i = 4; i < frame_to_received[0].data_lenght; i++) {
            data_dtc[0][i - 4] = frame_to_received[0].buffer[i];
        }
        free(frame_to_received);
        get_data_trouble_code(codes[0], data_dtc[0]);

        return true;
    }

    // If the data has more than only one dtc

    uint8_t data_saver[data_count];

    memset(data_saver, 0, sizeof(data_saver));

    uint8_t counter = 0;

    for(uint8_t i = 0; i < 5; i++) {
        if(frame_to_received[i].canId != uds_instance->id_to_received) break;

        uint32_t start_num = (i == 0) ? 5 : 1;

        for(uint8_t j = start_num; j < frame_to_received[i].data_lenght; j++) {
            data_saver[counter++] = frame_to_received[i].buffer[j];
        }
    }

    counter = 0;

    for(uint8_t i = 0; i < (*count_of_dtc); i++) {
        for(uint8_t j = 0; j < 4; j++) {
            data_dtc[i][j] = data_saver[counter++];
        }
    }

    for(uint8_t i = 0; i < *count_of_dtc; i++) {
        get_data_trouble_code(codes[i], data_dtc[i]);
    }

    free(frame_to_received);
    return true;
}

// Delete DTC Storaged
bool uds_delete_dtc(UDS_SERVICE* uds_instance) {
    uint8_t data[4] = {0x14, 0xff, 0xff, 0xff};

    CANFRAME frame_to_send = {0};
    CANFRAME frame_to_received = {0};

    if(!uds_multi_frame_request(
           uds_instance, data, COUNT_OF(data), &frame_to_send, 1, &frame_to_received)) {
        return false;
    }

    if(frame_to_received.buffer[0] == 0x7e) {
        return false;
    }

    return true;
}
