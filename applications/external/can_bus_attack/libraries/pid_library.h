#ifndef PID_LIBRARY
#define PID_LIBRARY

#include "mcp_can_2515.h"

#define ECU_TYPICAL_ID_STANDAR 0x7E8
#define ECU_STANDAR_ID_2       0x7E9
#define ECU_STANDAR_ID_3       0x7EA

#define ECU_REQUEST_ID 0x7DF

typedef enum {
    SHOW_DATA = 0x01,
    SHOW_FREEZE_DATA = 0x02,
    SHOW_STORAGE_DTC = 0x03,
    CLEAR_STORAGE_DTC = 0x04,
    OXIGEN_SENSOR_MONITORING = 0x05,
    SYSTEM_MONITORING = 0x06,
    SHOW_PENDING_DTC = 0x07,
    REQUEST_VEHICLE_INFORMATION = 0x09,
    PERMANENT_DTC = 0x0A

} pid_services;

typedef enum {
    BLOCK_A = 0x00,
    BLOCK_B = 0x20,
    BLOCK_C = 0x40,
    BLOCK_D = 0x60,
} pid_request_supported_codes;

typedef enum {
    CODE_NUM_00 = 0x00, /*< PIDs supported 0x01-0x20*/
    CODE_NUM_01 = 0x01, /*< Monitor status since DTCs cleared*/
    CODE_NUM_02 = 0x02, /*< DTC that caused freeze frame to be stored. */
    CODE_NUM_03 = 0x03, /*< Fuel system status */
    CODE_NUM_04 = 0x04, /*< Calculated engine load*/
    CODE_NUM_05 = 0x05, /*< Engine coolant temperature*/
    CODE_NUM_06 = 0x06, /*< Short term fuel trim (STFT)—Bank 1*/
    CODE_NUM_07 = 0x07, /*< Long term fuel trim (LTFT)—Bank 1*/
    CODE_NUM_08 = 0x08, /*< Short term fuel trim (STFT)—Bank 2*/
    CODE_NUM_09 = 0x09, /*< Long term fuel trim (LTFT)—Bank 2*/
    CODE_NUM_0A = 0x0A, /*< Fuel pressure (gauge pressure)*/
    CODE_NUM_0B = 0x0B, /*< Intake manifold absolute pressure*/
    CODE_NUM_0C = 0x0C, /*< Engine speed*/
    CODE_NUM_0D = 0x0D, /*< Vehicle speed*/
    CODE_NUM_0E = 0x0E, /*< Timing advance*/
    CODE_NUM_0F = 0x0F, /*< Intake air temperature*/
    CODE_NUM_10 = 0x10, /*< Mass air flow sensor (MAF) air flow rate*/
    CODE_NUM_11 = 0x11, /*< Throttle position*/
    CODE_NUM_12 = 0x12, /*< Commanded secondary air status*/
    CODE_NUM_13 = 0x13, /*< Oxygen sensors present (in 2 banks)*/
    CODE_NUM_14 = 0x14, /*< Oxygen Sensor 1*/
    CODE_NUM_15 = 0x15, /*< Oxygen Sensor 2*/
    CODE_NUM_16 = 0x16, /*< Oxygen Sensor 3*/
    CODE_NUM_17 = 0x17, /*< Oxygen Sensor 4*/
    CODE_NUM_18 = 0x18, /*< Oxygen Sensor 5*/
    CODE_NUM_19 = 0x19, /*< Oxygen Sensor 6*/
    CODE_NUM_1A = 0x1A, /*< Oxygen Sensor 7*/
    CODE_NUM_1B = 0x1B, /*< Oxygen Sensor 8*/
    CODE_NUM_1C = 0x1C, /*< OBD standards this vehicle conforms to*/
    CODE_NUM_1D = 0x1D, /*< Oxygen sensors present (in 4 banks)*/
    CODE_NUM_1E = 0x1E, /*< Auxiliary input status*/
    CODE_NUM_1F = 0x1F, /*< Run time since engine start*/
} pid_all_codes;

typedef enum {
    RUN_TIME_SINCE_ENGINE_START = 0x1F,
    CALCULATED_ENGINE_LOAD = 0x04,
    ENGINE_SPEED = 0x0C,
    VEHICLE_SPEED = 0x0D,
    THROTTLE_POSITION = 0x11,
    FUEL_INPUT_POSITION = 0x2F,
    THROTTLE_RELATIVE_POSITION = 0x45
} pid_typical_codes;

typedef struct {
    uint8_t pid_num;
    char* name;
    char* unit;
    bool is_supported;
} pid_code;

typedef struct {
    MCP2515* CAN;
    CANFRAME frame_to_send;
    CANFRAME frame_to_received;

    MCP_BITRATE bitrate;

    pid_code* codes;
} OBDII;

extern char* pid_codes_name[];

/*
  Convertions
*/

// Function to init the obdii scan
bool pid_init(OBDII* obdii);

// Function to deinit and free obdii
void pid_deinit(OBDII* obdii);

// request data
bool pid_show_data(OBDII* obdii, uint8_t pid, uint8_t* data, uint8_t size);

// request a PID in a manual form
bool pid_manual_request(
    OBDII* obdii,
    uint32_t id,
    pid_services mode,
    uint8_t pid,
    CANFRAME* frames_to_read,
    uint8_t lenght,
    uint8_t count_of_bytes);

// get the pid's supported in a block of pid
bool pid_get_supported_pid(OBDII* obdii, uint8_t block);

// Request DTC
bool request_dtc(OBDII* obdii, uint8_t* count, char* codes[]);

// Clear DTC
bool clear_dtc(OBDII* obdii);

// Get VIN
bool get_VIN(OBDII* obdii, FuriString* String);

// Get ECU Name
bool get_ECU_name(OBDII* obdii, FuriString* ecu_name);

// Function to calculate the engine speed
uint16_t calculate_engine_speed(uint8_t value_a, uint8_t value_b);

// Calculate the porcent of calculated load engine
float calculate_engine_load(uint8_t value);

// Calculate the units that got sum
uint16_t sum_value(uint8_t A, uint8_t B);

#endif
