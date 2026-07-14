// Header for can_transceiver_globalconf.c.

#ifndef GLOBALCONF_H
#define GLOBALCONF_H

// ==== TYPEDEFS ====

typedef struct {
    // General options.
    uint8_t u8BaudRate;
    uint8_t u8XtalFreq;

    // Rx options.
    uint8_t u8SleepTimeout;
    uint8_t bListenOnly;
    // TODO: In Rx mode, support up to 2 filters and up to 2 masks.

    // Tx options.
    uint8_t u8FrameIDLen;
    volatile uint8_t u8NumDataBytes;
    uint8_t bTxOneShot;
} CANTransceiverConfig;

// ==== CONSTANTS ====

// NOTE: These values correspond to indices in the text arrays of can_transceiver_scene_setup.c.
// Don't change them without changing the labels in that file too!

// If any new constants are added here, change the assertions in CANTransceiver_SceneSetup_OnEnter() (can_transceiver_scene_setup.c).
// Otherwise an assertion failure happens wheh the new values are used and the Setup menu is entered.

// Baud rate types.
#define u8BAUD_RATE_125kbps (uint8_t)0x00
#define u8BAUD_RATE_250kbps (uint8_t)0x01
#define u8BAUD_RATE_500kbps (uint8_t)0x02
#define u8BAUD_RATE_1Mbps   (uint8_t)0x03
//#define u8BAUD_RATE_AUTODETECT	(uint8_t)0x04	// TODO: NYI

// Crystal frequencies. Currently only 16 MHz supported.
#define u8XTAL_FREQ_16MHz (uint8_t)0x00

// Sleep timeouts.
#define u8SLEEP_TIMEOUT_NEVER (uint8_t)0x00
#define u8SLEEP_TIMEOUT_5s    (uint8_t)0x01
#define u8SLEEP_TIMEOUT_10s   (uint8_t)0x02
#define u8SLEEP_TIMEOUT_30s   (uint8_t)0x03
#define u8SLEEP_TIMEOUT_60s   (uint8_t)0x04
#define u8SLEEP_TIMEOUT_120s  (uint8_t)0x05

// TODO: Eventually add support for Rx filters and masks.

// Frame ID lengths.
#define u8FRAME_ID_LEN_11 (uint8_t)0x00
#define u8FRAME_ID_LEN_29 (uint8_t)0x01

// Default config values.
#define u8BaudRate_DEFAULT     u8BAUD_RATE_500kbps
#define u8XtalFreq_DEFAULT     u8XTAL_FREQ_16MHz
#define u8SleepTimeout_DEFAULT u8SLEEP_TIMEOUT_NEVER
#define bListenOnly_DEFAULT    0
// TODO: Filter options, when message filtering is implemented. All messages should be received by default.
#define u8FrameIDLen_DEFAULT   u8FRAME_ID_LEN_11
#define u8NumDataBytes_DEFAULT (uint8_t)8
#define bTxOneShot_DEFAULT     1

// ==== EXTERNALS ====

extern const uint8_t u8ppCANBaudRates_16MHz[][3];

// ==== PROTOTYPES ====

CANTransceiverConfig* CANTransceiver_GlobalConf_Create();
void CANTransceiver_GlobalConf_Free(CANTransceiverConfig* pConfig);

#endif // GLOBALCONF_H
