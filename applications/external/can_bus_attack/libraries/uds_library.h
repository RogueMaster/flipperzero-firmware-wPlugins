#ifndef UDS_LIBRARY
#define UDS_LIBRARY

#include <furi_hal.h>
#include "mcp_can_2515.h"

#define DEFAULT_ECU_REQUEST  0x7e0
#define DEFAULT_ECU_RESPONSE 0x7e8

// For the type of session
typedef enum {
    DEFAULT_UDS_SESSION = 1,
    PROGRAMMING_UDS_SESSION = 2,
    EXTENDED_UDS_SESSION = 3,
    SAFETY_UDS_SESSION = 4,
} diagnostic_session;

// For the type of RESET
typedef enum {
    HARD_RESET = 1,
    KEY_OFF_ON_RESET = 2,
    SOFT_RESET = 3,
} type_ecu_reset;

typedef struct {
    MCP2515* CAN;
    uint32_t id_to_send;
    uint32_t id_to_received;
} UDS_SERVICE;

UDS_SERVICE* uds_service_alloc(
    uint32_t id_to_send,
    uint32_t id_to_received,
    MCP_MODE mode,
    MCP_CLOCK clk,
    MCP_BITRATE bitrate);

// Init Uds
bool uds_init(UDS_SERVICE* uds_instance);

// send a manual request
bool uds_single_frame_request(
    UDS_SERVICE* uds_instance,
    uint8_t* data_to_send,
    uint8_t count_of_bytes,
    CANFRAME* frames_to_received,
    uint8_t count_of_frames);

// Send a multi frame request
bool uds_multi_frame_request(
    UDS_SERVICE* uds,
    uint8_t* data,
    uint8_t length,
    CANFRAME* canframes_to_send,
    uint8_t count_of_frames_to_received,
    CANFRAME* canframes_to_received);

// Get VIN Number
bool uds_get_vin(UDS_SERVICE* uds_instance, FuriString* text);

// Set the diagnostic session
bool uds_set_diagnostic_session(UDS_SERVICE* uds_instance, diagnostic_session session);

// Reset the ECU
bool uds_reset_ecu(UDS_SERVICE* uds_instance, type_ecu_reset type);

// Get the count of ECU stored Data Troubles Codes
bool uds_get_count_stored_dtc(UDS_SERVICE* uds_instance, uint16_t* count_of_dtc);

// Get the Data Troubles Codes
bool uds_get_stored_dtc(UDS_SERVICE* uds_instance, char* codes[], uint16_t* count_of_dtc);

// Delete DTC
bool uds_delete_dtc(UDS_SERVICE* uds_instance);

// Free uds
void free_uds(UDS_SERVICE* uds_instance);

#endif
