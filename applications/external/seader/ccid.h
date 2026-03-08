#pragma once

#include <stdlib.h> // malloc
#include <stdint.h> // uint32_t
#include <stdarg.h> // __VA_ARGS__
#include <string.h>
#include <stdio.h>

#include "seader_bridge.h"
#include "seader_worker_i.h"

#define SYNC (0x03)
#define CTRL (0x06)
#define NAK  (0x15)

#define BMICCSTATUS_MASK 0x03
/*
 * Bit 0 = Slot 0 current state
 * Bit 1 = Slot 0 changed status
 * Bit 2 = Slot 1 current state
 * Bit 3 = Slot 1 changed status
 */

// TODO: rename/renumber
#define CCID_SLOT_0_MASK     0x03
#define CCID_SLOT_0_CARD_OUT 0x02
#define CCID_SLOT_0_CARD_IN  0x03
#define CCID_SLOT_1_MASK     0x0C
#define CCID_SLOT_1_CARD_IN  0x04
#define CCID_SLOT_1_CARD_OUT 0x0C

/*
 *  * BULK_OUT messages from PC to Reader
 *   * Defined in CCID Rev 1.1 6.1 (page 26)
 *    */
typedef enum {
    CCID_MESSAGE_TYPE_PC_TO_RDR_ICC_POWER_ON = 0x62,
    CCID_MESSAGE_TYPE_PC_TO_RDR_ICC_POWER_OFF = 0x63,
    CCID_MESSAGE_TYPE_PC_TO_RDR_GET_SLOT_STATUS = 0x65,
    CCID_MESSAGE_TYPE_PC_TO_RDR_XFR_BLOCK = 0x6f,
    CCID_MESSAGE_TYPE_PC_TO_RDR_GET_PARAMETERS = 0x6c,
    CCID_MESSAGE_TYPE_PC_TO_RDR_RESET_PARAMETERS = 0x6d,
    CCID_MESSAGE_TYPE_PC_TO_RDR_SET_PARAMETERS = 0x61,
    CCID_MESSAGE_TYPE_PC_TO_RDR_ESCAPE = 0x6b,
    CCID_MESSAGE_TYPE_PC_TO_RDR_ICC_CLOCK = 0x6e,
    CCID_MESSAGE_TYPE_PC_TO_RDR_T0_APDU = 0x6a,
    CCID_MESSAGE_TYPE_PC_TO_RDR_SECURE = 0x69,
    CCID_MESSAGE_TYPE_PC_TO_RDR_MECHANICAL = 0x71,
    CCID_MESSAGE_TYPE_PC_TO_RDR_ABORT = 0x72,
    CCID_MESSAGE_TYPE_PC_TO_RDR_SET_DATA_RATE_AND_CLOCK_FREQUENCY = 0x73,
} SeaderCcidPcToRdrMessageType;
/*
 *  * BULK_IN messages from Reader to PC
 *   * Defined in CCID Rev 1.1 6.2 (page 48)
 *    */
typedef enum {
    CCID_MESSAGE_TYPE_RDR_TO_PC_DATA_BLOCK = 0x80,
    CCID_MESSAGE_TYPE_RDR_TO_PC_SLOT_STATUS = 0x81,
    CCID_MESSAGE_TYPE_RDR_TO_PC_PARAMETERS = 0x82,
    CCID_MESSAGE_TYPE_RDR_TO_PC_ESCAPE = 0x83,
    CCID_MESSAGE_TYPE_RDR_TO_PC_DATA_RATE_AND_CLOCK_FREQUENCY = 0x84,
} SeaderCcidRdrToPcMessageType;
/*
 *  * INTERRUPT_IN messages from Reader to PC
 *   * Defined in CCID Rev 1.1 6.3 (page 56)
 *    */
typedef enum {
    CCID_MESSAGE_TYPE_RDR_TO_PC_NOTIFY_SLOT_CHANGE = 0x50,
    CCID_MESSAGE_TYPE_RDR_TO_PC_HARDWARE_ERROR = 0x51,
} SeaderCcidInterruptMessageType;

/* Status codes that go in bStatus (see 6.2.6) */
typedef enum {
    CCID_ICC_STATUS_PRESENT_ACTIVE = 0,
    CCID_ICC_STATUS_PRESENT_INACTIVE = 1,
    CCID_ICC_STATUS_NOT_PRESENT = 2,
} SeaderCcidIccStatus;

typedef enum {
    CCID_COMMAND_STATUS_NO_ERROR = 0,
    CCID_COMMAND_STATUS_FAILED = 1,
    CCID_COMMAND_STATUS_TIME_EXTENSION_REQUIRED = 2,
} SeaderCcidCommandStatus;
/* Error codes that go in bError (see 6.2.6) */
typedef enum {
    CCID_ERROR_CMD_NOT_SUPPORTED = 0x00,
    CCID_ERROR_CMD_ABORTED = 0xff,
    CCID_ERROR_ICC_MUTE = 0xfe,
    CCID_ERROR_XFR_PARITY_ERROR = 0xfd,
    CCID_ERROR_XFR_OVERRUN = 0xfc,
    CCID_ERROR_HW_ERROR = 0xfb,
} SeaderCcidError;

struct CCID_Message {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;

    uint8_t* payload;
    size_t consumed;
};

void seader_ccid_check_for_sam(SeaderUartBridge* seader_uart);
void seader_ccid_IccPowerOn(SeaderUartBridge* seader_uart, uint8_t slot);
void seader_ccid_GetSlotStatus(SeaderUartBridge* seader_uart, uint8_t slot);
void seader_ccid_GetParameters(SeaderUartBridge* seader_uart);
void seader_ccid_XfrBlock(SeaderUartBridge* seader_uart, uint8_t* data, size_t len);
void seader_ccid_XfrBlockToSlot(
    SeaderUartBridge* seader_uart,
    uint8_t slot,
    uint8_t* data,
    size_t len);
size_t seader_ccid_process(Seader* seader, uint8_t* cmd, size_t cmd_len);
