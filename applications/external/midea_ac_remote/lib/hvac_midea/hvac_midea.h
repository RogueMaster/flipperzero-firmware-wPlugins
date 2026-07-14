#pragma once

#include <infrared_transmit.h>
#include <infrared_worker.h>

#include "furi_hal.h"

#define HVAC_MIDEA_PACKET_START_MAGIC (uint8_t)0xD
#define HVAC_MIDEA_PACKET_SIZE        6
typedef uint8_t* HvacMideaPacket;

#define HVAC_MIDEA_PACKET_SETTINGS (uint8_t)0x4
#define HVAC_MIDEA_PACKET_COMMAND  (uint8_t)0xA
#define HVAC_MIDEA_COMMAND_MAGIC_1 (uint8_t)0xAF
#define HVAC_MIDEA_COMMAND_MAGIC_2 (uint8_t)0x5
typedef enum {
    HvacMideaPacketSettings,
    HvacMideaPacketCommand,
} HvacMideaPacketType;
HvacMideaPacket hvac_midea_create_packet(HvacMideaPacketType packet_type);
void hvac_midea_free_packet(HvacMideaPacket packet);

#define HVAC_MIDEA_FAN_POWER_LOCKED 0x0 // necessary to differentiate "dry" and "fan" modes
#define HVAC_MIDEA_FAN_POWER_1      0x1
#define HVAC_MIDEA_FAN_POWER_2      0x2
#define HVAC_MIDEA_FAN_POWER_3      0x4
#define HVAC_MIDEA_FAN_POWER_AUTO   0x5
#define HVAC_MIDEA_FAN_POWER_IGNORE 0x6 // necessary to power off, toggle swing
void hvac_midea_set_fan_power_raw(HvacMideaPacket packet, uint8_t fan_power_raw);
typedef enum {
    HvacMideaFanPower1,
    HvacMideaFanPower2,
    HvacMideaFanPower3,
    HvacMideaFanPowerAuto,
} HvacMideaFanPower;
void hvac_midea_set_fan_power(HvacMideaPacket packet, HvacMideaFanPower fan_power);

#define HVAC_MIDEA_MODE_COLD    0x0
#define HVAC_MIDEA_MODE_DRY_FAN 0x2 // differentiated by fan power
#define HVAC_MIDEA_MODE_HEAT    0x3
#define HVAC_MIDEA_MODE_AUTO    0x1
void hvac_midea_set_mode_raw(HvacMideaPacket packet, uint8_t mode_raw);
typedef enum {
    HvacMideaModeCold,
    HvacMideaModeDry,
    HvacMideaModeFan,
    HvacMideaModeHeat,
    HvacMideaModeAuto,
} HvacMideaMode;
void hvac_midea_set_mode(HvacMideaPacket packet, HvacMideaMode mode);

#define HVAC_MIDEA_TEMPERATURE_IGNORE 0x7 // necessary to power off, toggle swing
void hvac_midea_set_temperature_raw(HvacMideaPacket packet, uint8_t temperature_raw);
typedef uint8_t HvacMideaTemperature;
#define HVAC_MIDEA_TEMPERATURE_MIN (HvacMideaTemperature)17
#define HVAC_MIDEA_TEMPERATURE_DEFAULT \
    (HvacMideaTemperature)22 // just reference value used in packet constructor
static const uint8_t hvac_midea_temperature_table[] = {
    0x0,
    0x8,
    0xC,
    0x4,
    0x6,
    0xE,
    0xA,
    0x2,
    0x3,
    0xB,
    0x9,
    0x1,
    0x5,
    0xD,
}; // index - addition to HVAC_MIDEA_TEMPERATURE_MIN
#define HVAC_MIDEA_TEMPERATURE_MAX \
    (HVAC_MIDEA_TEMPERATURE_MIN +  \
     (HvacMideaTemperature)(sizeof(hvac_midea_temperature_table) / sizeof(uint8_t)) - 1)
void hvac_midea_set_temperature(HvacMideaPacket packet, HvacMideaTemperature temperature);

#define HVAC_MIDEA_COMMAND_SILENT         0xD // puts into silent mode
#define HVAC_MIDEA_COMMAND_VERTICAL_SWING 0x6 // toggles vertical swing
#define HVAC_MIDEA_COMMAND_TURBO          0x8 // activates turbo mode
#define HVAC_MIDEA_COMMAND_LED            0x14 // toggles led temperature indicator
#define HVAC_MIDEA_COMMAND_LED_CHANGE_INFO \
    0x4 // toggle between ambient and desired temperature displaying
#define HVAC_MIDEA_COMMAND_CLEAN 0xA // toggle cleaning mode
void hvac_midea_set_command_raw(HvacMideaPacket packet, uint8_t command_raw);
typedef enum {
    HvacMideaCommandSilent,
    HvacMideaCommandVerticalSwing,
    HvacMideaCommandTurbo,
    HvacMideaCommandLED,
    HvacMideaCommandLEDChangeInfo,
    HvacMideaCommandClean,
} HvacMideaCommand;
void hvac_midea_set_command(HvacMideaPacket packet, HvacMideaCommand command);

void hvac_midea_set_toggle_swing(HvacMideaPacket packet);

void hvac_midea_set_power_off(HvacMideaPacket packet);

#define HVAC_MIDEA_TRANSMIT_FREQUENCY  38000
#define HVAC_MIDEA_TRANSMIT_DUTY_CYCLE 0.33
#define HVAC_MIDEA_TRANSMIT_TIMINGS_PER_PACKET \
    (2 + 2 * HVAC_MIDEA_PACKET_SIZE * 8 +      \
     2) // start mark-space, packet mark-space per bit, end mark-space
#define HVAC_MIDEA_HDR_MARK   4400
#define HVAC_MIDEA_HDR_SPACE  4400
#define HVAC_MIDEA_BIT_MARK   560
#define HVAC_MIDEA_ONE_SPACE  1600
#define HVAC_MIDEA_ZERO_SPACE 560
#define HVAC_MIDEA_END_MARK   560
#define HVAC_MIDEA_END_SPACE  5000
typedef struct {
    HvacMideaPacket packet;
    uint8_t repeats;
} HvacMideaSendVec; // in some cases we should send multiple packets simultaneously, i.e. while turning night mode on
void hvac_midea_send_ext(const HvacMideaSendVec* sendvec, size_t sendvec_len);
#define HVAC_MIDEA_TRANSMIT_REPEATS_DEFAULT 2
void hvac_midea_send(const HvacMideaPacket packet);
