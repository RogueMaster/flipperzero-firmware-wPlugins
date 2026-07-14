// GLobally-relevant CAN transceiver configuration.

// ==== INCLUDES ====

#include "can_transceiver.h"
#include "can_transceiver_globalconf.h"

// ==== CONSTANTS ====

// Tables to configure the CAN baud rate via the CNF1, CNF2, and CNF3 registers on the MCP2515.
// You can thank Adafruit for these, it was taken from their CircuitPython MCP2515 library.
// Source: https://github.com/adafruit/Adafruit_CircuitPython_MCP2515
const uint8_t u8ppCANBaudRates_16MHz[][3] = {
    // CNF1, CNF2, CNF3
    {0x03, 0xF0, 0x86}, // 125 kbps
    {0x41, 0xF1, 0x85}, // 250 kbps
    {0x00, 0xF0, 0x86}, // 500 kbps
    {0x00, 0xD0, 0x82}, // 1 Mbps
};
// TODO: Register values when using other clocks, whenever they get supported.

// ==== PUBLIC FUNCTIONS ====

CANTransceiverConfig* CANTransceiver_GlobalConf_Create() {
    CANTransceiverConfig* pConfig;

    pConfig = malloc(sizeof(CANTransceiverConfig));
    pConfig->u8BaudRate = u8BaudRate_DEFAULT;
    pConfig->u8XtalFreq = u8XtalFreq_DEFAULT;
    pConfig->u8SleepTimeout = u8SleepTimeout_DEFAULT;
    pConfig->bListenOnly = bListenOnly_DEFAULT;
    pConfig->u8FrameIDLen = u8FrameIDLen_DEFAULT;
    pConfig->u8NumDataBytes = u8NumDataBytes_DEFAULT;
    pConfig->bTxOneShot = bTxOneShot_DEFAULT;

    return pConfig;
}

void CANTransceiver_GlobalConf_Free(CANTransceiverConfig* pConfig) {
    free(pConfig);
}
