#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PCSG_DEFAULT_FREQ    439987500U
#define PCSG_DEFAULT_RIC     123456U
#define PCSG_MAX_MSG_LEN     160U
#define PCSG_RIC_MAX         2097151U

typedef enum {
    PocsagBaud512 = 512,
    PocsagBaud1200 = 1200,
    PocsagBaud2400 = 2400,
} PocsagBaudRate;

typedef enum {
    PocsagMsgTypeNumeric = 0, // Function 00: Numeric (BCD)
    PocsagMsgTypeTone = 1, // Function 01: Tone only (Alert 1)
    PocsagMsgTypeAlert2 = 2, // Function 10: Alert 2 (tone or alphanumeric)
    PocsagMsgTypeAlpha = 3, // Function 11: Alphanumeric (7-bit ASCII)
} PocsagMsgType;

typedef struct {
    uint32_t frequency;
    uint32_t ric;
    PocsagBaudRate baud;
    PocsagMsgType msg_type;
    bool inverted;
    char message[PCSG_MAX_MSG_LEN + 1];
} PCSGSettings;

void pcsg_settings_init_defaults(PCSGSettings* settings);
bool pcsg_settings_load(PCSGSettings* settings);
bool pcsg_settings_save(const PCSGSettings* settings);

#ifdef __cplusplus
}
#endif
