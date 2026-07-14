#include "pid_library.h"

// PID TYPICAL NAMES

char* pid_codes_name[] = {
    "PIDs supported A",
    "Monitor Status since DTCs cleared",
    "DTC that caused freeze frame to be store",
    "Fuel System Status",
    "Calculated engine load",
    "Engine Coolant temperature",
    "STFT BANK 1",
    "LTFT BANK 1",
    "STFT BANK 2",
    "LFTF BANK 2",
    "Fuel pressure",
    "Intake manifold absolute pressure",
    "Engine speed",
    "Vehicle speed",
    "Timing advance",
    "Intake air temperature",
    "Mass air flow sensor (MAF) air flow rate",
    "Throttle position",
    "Commanded secondary air status",
    "Oxygen sensors present (in 2 banks)",
    "Oxygen Sensor 1",
    "Oxygen Sensor 2",
    "Oxygen Sensor 3",
    "Oxygen Sensor 4",
    "Oxygen Sensor 5",
    "Oxygen Sensor 6",
    "Oxygen Sensor 7",
    "Oxygen Sensor 8",
    "OBD standards this vehicle conforms to",
    "Oxygen sensors present (in 4 banks)",
    "Auxiliary input status",
    "Run time since engine start",
    "PIDs supported B",
};

// Init the obdii
bool pid_init(OBDII* obdii) {
    obdii->CAN = mcp_alloc(MCP_NORMAL, MCP_16MHZ, obdii->bitrate);

    if(mcp2515_init(obdii->CAN) != ERROR_OK) return false;

    obdii->codes = (pid_code*)malloc(200 * (sizeof(pid_code)));

    init_mask(obdii->CAN, 0, 0x7FF);
    init_filter(obdii->CAN, 0, 0x7E8);

    init_mask(obdii->CAN, 1, 0x7FF);
    init_filter(obdii->CAN, 1, 0x7E8);
    init_filter(obdii->CAN, 2, 0x7E8);
    init_filter(obdii->CAN, 3, 0x7E8);
    init_filter(obdii->CAN, 4, 0x7E8);
    init_filter(obdii->CAN, 5, 0x7E8);

    obdii->frame_to_send.canId = ECU_REQUEST_ID;
    obdii->frame_to_send.data_lenght = 8;

    for(uint8_t i = 0; i < 8; i++) {
        obdii->frame_to_send.buffer[i] = 0;
    }

    return true;
}

// It works to get the current data of the requested pid
bool pid_show_data(OBDII* obdii, uint8_t pid, uint8_t* data, uint8_t size) {
    MCP2515* CAN = obdii->CAN;
    CANFRAME frame = obdii->frame_to_send;
    ERROR_CAN ret = ERROR_NOMSG;

    frame.buffer[0] = 2;
    frame.buffer[1] = SHOW_DATA;
    frame.buffer[2] = pid;

    if(size < 8) return false;

    if(send_can_frame(CAN, &frame) != ERROR_OK) return false;

    uint16_t time_delay = 0;

    do {
        read_can_message(CAN, &obdii->frame_to_received);

        if(obdii->frame_to_received.canId == 0x7e8) ret = ERROR_OK;

        furi_delay_us(1);
        time_delay++;

    } while((ret != ERROR_OK) && (time_delay < 1500));

    if(ret != ERROR_OK) return false;

    for(uint8_t i = 0; i < size; i++) {
        data[i] = obdii->frame_to_received.buffer[i];
    }

    return true;
}

// get frames
bool read_frames(MCP2515* CAN, CANFRAME* frame) {
    uint32_t time_delay = 0;

    do {
        if(read_can_message(CAN, frame) == ERROR_OK) {
            if(frame->canId == 0x7e8) return true;
        }
        furi_delay_us(1);
        time_delay++;

    } while((time_delay < 6000));

    return false;
}

// It works to send a mode in the pid
bool pid_manual_request(
    OBDII* obdii,
    uint32_t id,
    pid_services mode,
    uint8_t pid,
    CANFRAME* frames_to_read,
    uint8_t lenght,
    uint8_t count_of_bytes) {
    MCP2515* CAN = obdii->CAN;
    CANFRAME frame = obdii->frame_to_send;
    ERROR_CAN ret = ERROR_OK;

    frame.canId = id;

    frame.buffer[0] = count_of_bytes;
    frame.buffer[1] = mode;
    frame.buffer[2] = pid;

    // uint32_t time_delay = 0;

    ret = send_can_frame(CAN, &frame);

    if(ret != ERROR_OK) return false;

    frame.buffer[0] = 0x30;
    frame.buffer[1] = 0;
    frame.buffer[2] = 0;

    for(uint8_t i = 0; i < lenght; i++) {
        if(i == 1) {
            ret = send_can_frame(CAN, &frame);
            if(ret != ERROR_OK) return false;
        }

        ret = ERROR_FAIL;

        if(!read_frames(CAN, &(frames_to_read[i]))) {
            if(i == 0)
                return false;
            else
                break;
        }
    }

    return true;
}

// It works to get the supported datas
bool pid_get_supported_pid(OBDII* obdii, uint8_t block) {
    uint8_t data[8] = {0, 0, 0, 0, 0, 0};

    if(!pid_manual_request(obdii, 0x7df, SHOW_DATA, block, &obdii->frame_to_received, 1, 2))
        return false;

    for(uint8_t i = 0; i < 8; i++) {
        data[i] = obdii->frame_to_received.buffer[i];
    }

    obdii->codes[block].pid_num = block;
    obdii->codes[block].is_supported = true;
    obdii->codes[block].name = "Supported";

    uint32_t codes_available = (data[3] << 24) + (data[4] << 16) + (data[5] << 8) + data[6];

    for(uint8_t i = 1; i <= 32; i++) {
        obdii->codes[block + i].pid_num = block + i;
        obdii->codes[block + i].is_supported = codes_available & (1 << (32 - i));
        if(block == BLOCK_A)
            obdii->codes[i].name = pid_codes_name[i];
        else
            obdii->codes[block + i].name = "UNKNOWN";
    }

    return true;
}

bool clear_dtc(OBDII* obdii) {
    MCP2515* CAN = obdii->CAN;
    CANFRAME frame = obdii->frame_to_send;
    CANFRAME frame_to_received = obdii->frame_to_received;

    ERROR_CAN ret = ERROR_OK;

    frame.buffer[0] = 1;
    frame.buffer[1] = CLEAR_STORAGE_DTC;
    frame.buffer[2] = 0;

    ret = send_can_frame(CAN, &frame);

    if(ret != ERROR_OK) return false;

    if(!read_frames(CAN, &frame_to_received)) return false;

    if(frame_to_received.buffer[1] != 0x44) return false;

    return true;
}

/*
    This part works to get the dtc
*/

// This separate codes by one frame
void separate_code_by_frame(uint16_t* save_codes, CANFRAME frame, uint8_t frame_position) {
    uint16_t position = frame_position * 4;

    for(uint8_t i = 1; i < 8; i = i + 2) {
        save_codes[position] = (frame.buffer[i - 1] << 8) + frame.buffer[i];
        position++;
    }
}

// This separate codes by multiple can frames
void separate_codes(CANFRAME* frames, uint16_t* save_codes, uint8_t length) {
    for(uint8_t i = 0; i < length; i++) {
        separate_code_by_frame(save_codes, frames[i], i);
    }
}

// This is for translate codes
void get_dtc(uint16_t numerical_code, char* dtc_code) {
    char* first;

    FuriString* text = furi_string_alloc();

    uint8_t identifier = numerical_code >> 12; // For the second
    uint8_t firstNumber = numerical_code >> 8;
    uint8_t secondNumber = numerical_code >> 4 & 0xf;
    uint8_t thirdNumber = numerical_code & 0xf;

    switch(identifier) // Set the correct type prefix for the code
    {
    case 0:
        first = "P0";
        break;

    case 1:
        first = "P1";
        break;

    case 2:
        first = "P2";
        break;

    case 3:
        first = "P3";
        break;

    case 4:
        first = "C0";
        break;

    case 5:
        first = "C1";
        break;

    case 6:
        first = "C2";
        break;

    case 7:
        first = "C3";
        break;

    case 8:
        first = "B0";
        break;

    case 9:
        first = "B1";
        break;

    case 10:
        first = "B2";
        break;

    case 11:
        first = "B3";
        break;

    case 12:
        first = "U0";
        break;

    case 13:
        first = "U1";
        break;

    case 14:
        first = "U2";
        break;

    case 15:
        first = "U3";
        break;

    default:
        break;
    }

    furi_string_printf(text, "%s%u%u%u", first, firstNumber, secondNumber, thirdNumber);

    for(uint8_t i = 0; i < 5; i++) {
        dtc_code[i] = furi_string_get_char(text, i);
    }

    furi_string_free(text);
}

// Request the codes
bool request_dtc(OBDII* obdii, uint8_t* count, char* codes[]) {
    CANFRAME canframes[5];
    uint16_t save_error_codes[20];

    memset(canframes, 0, sizeof(canframes));
    memset(save_error_codes, 0, sizeof(save_error_codes));

    if(!pid_manual_request(obdii, 0x7df, SHOW_STORAGE_DTC, 0, canframes, 20, 1)) {
        return false;
    }

    if(canframes[0].buffer[0] != 0x03 && canframes[0].buffer[1] != 0x43) {
        return false;
    }

    separate_codes(canframes, save_error_codes, 5);

    for(uint8_t i = 1; i < 20; i++) {
        if((save_error_codes[i] == 0xaa) || (save_error_codes[i] == 0)) {
            *count = i - 1;
            break;
        }
    }

    uint8_t quantity = *count;

    if(quantity == 0) return true;

    for(uint8_t i = 1; i < (quantity + 1); i++) {
        get_dtc(save_error_codes[i], codes[i - 1]);
    }

    return true;
}

// Separate frames in arrays
void separate_VIN_data(CANFRAME frame, uint8_t* vin, uint8_t count) {
    count = count * 7;

    for(uint8_t i = 0; i < 7; i++) {
        vin[i + count] = frame.buffer[i + 1];
    }
}

// Get the CAR VIN
bool get_VIN(OBDII* obdii, FuriString* vin_number) {
    CANFRAME canframes[5];

    memset(canframes, 0, sizeof(canframes));

    if(!pid_manual_request(obdii, 0x7df, REQUEST_VEHICLE_INFORMATION, 0x2, canframes, 5, 2))
        return false;

    if(canframes[0].buffer[2] != 0x49) return false;

    uint32_t extension = canframes[0].buffer[1];

    uint8_t vin[40];

    memset(vin, 0, sizeof(vin));

    for(uint8_t i = 0; i < 5; i++) {
        separate_VIN_data(canframes[i], vin, i);
    }

    char letters_vin[extension];

    for(uint8_t i = 0; i < extension; i++) {
        if(vin[i + 1] < 32) {
            letters_vin[i] = ' ';
        } else {
            letters_vin[i] = vin[i + 1];
        }
    }

    furi_string_reset(vin_number);

    for(uint8_t i = 0; i < extension; i++) {
        furi_string_cat_printf(vin_number, "%c", letters_vin[i]);
    }

    return true;
}

// Get the ECU Name
bool get_ECU_name(OBDII* obdii, FuriString* ecu_name) {
    CANFRAME canframes[5];

    memset(canframes, 0, sizeof(canframes));

    if(!pid_manual_request(obdii, 0x7df, REQUEST_VEHICLE_INFORMATION, 0xA, canframes, 5, 2))
        return false;

    if(canframes[0].buffer[2] != 0x49) return false;

    uint32_t extension = canframes[0].buffer[1];

    uint8_t vin[40];

    memset(vin, 0, sizeof(vin));

    for(uint8_t i = 0; i < 5; i++) {
        separate_VIN_data(canframes[i], vin, i);
    }

    char letters_vin[extension];

    for(uint8_t i = 0; i < extension; i++) {
        if(vin[i + 1] < 32) {
            letters_vin[i] = ' ';
        } else {
            letters_vin[i] = vin[i + 1];
        }
    }

    furi_string_reset(ecu_name);

    for(uint8_t i = 0; i < extension; i++) {
        furi_string_cat_printf(ecu_name, "%c", letters_vin[i]);
    }

    return true;
}

// It works to free
void pid_deinit(OBDII* obdii) {
    deinit_mcp2515(obdii->CAN);
    free(obdii->codes);
}

// Calculate the engine speed
uint16_t calculate_engine_speed(uint8_t value_a, uint8_t value_b) {
    uint16_t operation_a = 0;
    uint16_t operation_b = 0;

    if(value_a != 0) operation_a = value_a * 64;
    if(value_b != 0) operation_b = value_b / 4;

    return operation_a + operation_b;
}

// Calcuate the engine load
float calculate_engine_load(uint8_t value) {
    if(value == 0) return 0;
    return value / 2.55;
}

// To add between the A value and B value
uint16_t sum_value(uint8_t A, uint8_t B) {
    return (A * 256) + B;
}
