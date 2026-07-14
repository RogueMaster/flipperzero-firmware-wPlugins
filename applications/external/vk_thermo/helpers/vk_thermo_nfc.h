#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/iso15693_3/iso15693_3.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>

#define VK_THERMO_NFC_UID_LEN 8

// NXP Custom Commands
#define NXP_CMD_READ_CONFIG  0xC0
#define NXP_CMD_WRITE_CONFIG 0xC1
#define NXP_CMD_READ_SRAM    0xD2
#define NXP_CMD_READ_I2C     0xD5
#define NXP_CMD_WRITE_I2C    0xD4
#define NXP_MANUF_CODE       0x04

// NXP Config Addresses
#define NXP_CONFIG_ADDR_EH_CONFIG_REG    0xA7
#define NXP_CONFIG_ADDR_I2C_M_STATUS_REG 0xAD

// Energy Harvesting Flags
#define NXP_EH_ENABLE  (1 << 0)
#define NXP_EH_TRIGGER (1 << 3)
#define NXP_EH_LOAD_OK (1 << 7)

// I2C Status Flags
#define NXP_I2C_M_BUSY_MASK            0x01
#define NXP_I2C_M_TRANS_STATUS_MASK    0x06
#define NXP_I2C_M_TRANS_STATUS_SUCCESS (3 << 1)

// Temperature sensor constants (TMP112 and TMP117 share address and temp register)
#define TEMP_SENSOR_I2C_ADDR   0x48
#define TEMP_SENSOR_REG_RESULT 0x00

// TMP117 Device ID register (0x0F) — TMP112 only has registers 0x00-0x03
#define TMP117_REG_DEVICE_ID  0x0F
#define TMP117_DEVICE_ID      0x0117
#define TMP117_DEVICE_ID_MASK 0x0FFF

// Temperature sensor types
typedef enum {
    VkThermoSensorTmp117, // TMP117: raw * 0.0078125
    VkThermoSensorTmp112, // TMP112: (raw >> 4) * 0.0625
    VkThermoSensorUnknown, // Neither formula gave valid temp
} VkThermoSensorType;

typedef struct VkThermoNfc VkThermoNfc;

typedef enum {
    VkThermoNfcEventSuccess,
    VkThermoNfcEventError,
    VkThermoNfcEventTagDetected,
} VkThermoNfcEvent;

typedef struct {
    uint8_t uid[VK_THERMO_NFC_UID_LEN];
    float temperature_celsius;
    VkThermoSensorType sensor_type;
    bool valid;
} VkThermoNfcData;

typedef void (*VkThermoNfcCallback)(VkThermoNfcEvent event, VkThermoNfcData* data, void* context);

/** Allocate NFC handler
 * @return VkThermoNfc instance
 */
VkThermoNfc* vk_thermo_nfc_alloc(void);

/** Free NFC handler
 * @param instance VkThermoNfc instance
 */
void vk_thermo_nfc_free(VkThermoNfc* instance);

/** Set callback for NFC events
 * @param instance VkThermoNfc instance
 * @param callback Callback function
 * @param context Callback context
 */
void vk_thermo_nfc_set_callback(VkThermoNfc* instance, VkThermoNfcCallback callback, void* context);

/** Set EH timeout in seconds
 * @param instance VkThermoNfc instance
 * @param timeout_seconds Timeout in seconds (1, 2, 5, 10, 30)
 */
void vk_thermo_nfc_set_eh_timeout(VkThermoNfc* instance, uint32_t timeout_seconds);

/** Enable/disable debug diagnostics
 * @param instance VkThermoNfc instance
 * @param enabled true to run diagnostic tests on each scan
 */
void vk_thermo_nfc_set_debug(VkThermoNfc* instance, bool enabled);

/** Start NFC scanning
 * @param instance VkThermoNfc instance
 */
void vk_thermo_nfc_start(VkThermoNfc* instance);

/** Stop NFC scanning
 * @param instance VkThermoNfc instance
 */
void vk_thermo_nfc_stop(VkThermoNfc* instance);

/** Check if NFC is scanning
 * @param instance VkThermoNfc instance
 * @return true if scanning
 */
bool vk_thermo_nfc_is_scanning(VkThermoNfc* instance);

/** Process NFC state machine (call from tick handler)
 * @param instance VkThermoNfc instance
 * @return true if a temperature was read successfully
 */
bool vk_thermo_nfc_tick(VkThermoNfc* instance);
