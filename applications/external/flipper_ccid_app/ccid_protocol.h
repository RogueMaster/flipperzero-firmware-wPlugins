/* CCID class definitions, vendored from the Lightweight USB device Stack for
 * STM32 microcontrollers (lib/libusb_stm32/inc/usb_ccid.h). The header is pure
 * constants and packed structs, so the app carries its own copy instead of
 * depending on the stack.
 *
 * Copyright ©2023 Filipe Rodrigues <filipepazrodrigues[at]gmail[dot]com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *   http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _CCID_PROTOCOL_H_
#define _CCID_PROTOCOL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**\addtogroup USB_CCID USB CCID class
 * \brief Integrated Circuit(s) Cards Interface Devices class definitions
 * \details This module based on
 * + [Specification for Integrated Circuit(s) Cards Interface Devices, Revision 1.1](https://www.usb.org/sites/default/files/DWG_Smart-Card_CCID_Rev110.pdf)
 * @{ */

/**\name USB CCID class subclass and protocol definitions
 * @{ */
#define USB_CLASS_CCID      0x0B /**<\brief Smart Card Device class */
#define USB_CCID_SUBCLASS   0x00 /**<\brief Subclass code*/
#define USB_CCID_PROTO_CCID 0x00 /**<\brief CCID protocol*/
/** @} */

/**\name USB CCID descriptor types
 * @{ */
#define USB_DTYPE_CCID_FUNCTIONAL 0x21 /**<\brief Functional descriptor*/
/** @} */

/**\name CCID Functional descriptor parameters
 * @{ */
#define CCID_CURRENT_SPEC_RELEASE_NUMBER 0x0110

#define CCID_VOLTAGESUPPORT_5V  0
#define CCID_VOLTAGESUPPORT_3V  (1 << 0)
#define CCID_VOLTAGESUPPORT_1V8 (1 << 1)

#define CCID_PROTOCOL_T0 0
#define CCID_PROTOCOL_T1 (1 << 0)
/** @} */

/**\name CCID Slot status register codes
 * @{ */
#define CCID_ICCSTATUS_PRESENTANDACTIVE   0
#define CCID_ICCSTATUS_PRESENTANDINACTIVE (1 << 0)
#define CCID_ICCSTATUS_NOICCPRESENT       (1 << 1)

#define CCID_COMMANDSTATUS_PROCESSEDWITHOUTERROR  0
#define CCID_COMMANDSTATUS_FAILED                 (1 << 6)
#define CCID_COMMANDSTATUS_TIMEEXTENSIONREQUESTED (1 << 7)
/** @} */

/**\name CCID Error codes
 * @{ */
#define CCID_ERROR_NOERROR      0
#define CCID_ERROR_SLOTNOTFOUND 5
/** @} */

/**\name USB CCID class-specific requests
 * @{ */
#define CCID_ABORT                 0x1
#define CCID_GET_CLOCK_FREQUENCIES 0x2
#define CCID_GET_DATA_RATES        0x3
/** @} */

/**\name CCID Bulk-OUT Messages
 * @{ */
#define PC_TO_RDR_ICCPOWERON                   0x62
#define PC_TO_RDR_ICCPOWEROFF                  0x63
#define PC_TO_RDR_GETSLOTSTATUS                0x65
#define PC_TO_RDR_XFRBLOCK                     0x6F
#define PC_TO_RDR_GETPARAMETERS                0x6C
#define PC_TO_RDR_RESETPARAMETERS              0x6D
#define PC_TO_RDR_SETPARAMETERS                0x61
#define PC_TO_RDR_ESCAPE                       0x6B
#define PC_TO_RDR_ICCCLOCK                     0x6E
#define PC_TO_RDR_T0APDU                       0x6A
#define PC_TO_RDR_SECURE                       0x69
#define PC_TO_RDR_MECHANICAL                   0x71
#define PC_TO_RDR_ABORT                        0x72
#define PC_TO_RDR_SETDATARATEANDCLOCKFREQUENCY 0x73
/** @} */

/**\name CCID Bulk-IN Messages
 * @{ */
#define RDR_TO_PC_DATABLOCK                 0x80
#define RDR_TO_PC_SLOTSTATUS                0x81
#define RDR_TO_PC_PARAMETERS                0x82
#define RDR_TO_PC_ESCAPE                    0x83
#define RDR_TO_PC_DATARATEANDCLOCKFREQUENCY 0x84
/** @} */

/**\name CCID Interrupt-IN Messages
 * @{ */
#define RDR_TO_PC_NOTIFYSLOTCHANGE 0x50
#define RDR_TO_PC_HARDWAREERROR    0x51
/** @} */

/**\brief USB CCID functional descriptor */
struct usb_ccid_descriptor {
    uint8_t bLength; /**<\brief Size of the descriptor, in bytes.*/
    uint8_t bDescriptorType; /**<\brief IAD descriptor */

    uint16_t bcdCCID;
    uint8_t bMaxSlotIndex;
    uint8_t bVoltageSupport;
    uint32_t dwProtocols;
    uint32_t dwDefaultClock;
    uint32_t dwMaximumClock;
    uint8_t bNumClockSupported;
    uint32_t dwDataRate;
    uint32_t dwMaxDataRate;
    uint8_t bNumDataRatesSupported;
    uint32_t dwMaxIFSD;
    uint32_t dwSynchProtocols;
    uint32_t dwMechanical;
    uint32_t dwFeatures;
    uint32_t dwMaxCCIDMessageLength;
    uint8_t bClassGetResponse;
    uint8_t bClassEnvelope;
    uint16_t wLcdLayout;
    uint8_t bPINSupport;
    uint8_t bMaxCCIDBusySlots;
} __attribute__((packed));

/**\brief PC_to_RDR_IccPowerOn Bulk-Out Message */
struct pc_to_rdr_icc_power_on {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bPowerSelect;
    uint8_t abRFU[2];
} __attribute__((packed));

/**\brief PC_to_RDR_IccPowerOff Bulk-Out Message */
struct pc_to_rdr_icc_power_off {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
} __attribute__((packed));

/**\brief PC_to_RDR_GetSlotStatus Bulk-Out Message */
struct pc_to_rdr_get_slot_status {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
} __attribute__((packed));

/**\brief PC_to_RDR_XfrBlock Bulk-Out Message */
struct pc_to_rdr_xfr_block {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bBWI;
    uint16_t wLevelParameter;
    uint8_t abData[0];
} __attribute__((packed));

/**\brief PC_to_RDR_GetParameters Bulk-Out Message */
struct pc_to_rdr_get_parameters {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
} __attribute__((packed));

/**\brief PC_to_RDR_ResetParameters Bulk-Out Message */
struct pc_to_rdr_reset_parameters {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
} __attribute__((packed));

/**\brief PC_to_RDR_SetParameters (T0 protocol) Bulk-Out Message */
struct pc_to_rdr_set_parameters_t0 {
    //common
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bProtocolNum;
    uint8_t abRFU[2];

    //specific for T=0
    uint8_t bmFindexDindex;
    uint8_t bmTCCKST0;
    uint8_t bGuardTimeT0;
    uint8_t bWaitingIntegerT0;
    uint8_t bClockStop;
} __attribute__((packed));

/**\brief PC_to_RDR_SetParameters (T1 protocol) Bulk-Out Message */
struct pc_to_rdr_set_parameters_t1 {
    //common
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bProtocolNum;
    uint8_t abRFU[2];

    //specific for T=1
    uint8_t bmFindexDindex;
    uint8_t bmTCCKST1;
    uint8_t bGuardTimeT1;
    uint8_t bmWaitingIntegersT1;
    uint8_t bClockStop;
    uint8_t bIFSC;
    uint8_t bNadValue;
} __attribute__((packed));

/**\brief PC_to_RDR_Escape Bulk-Out Message */
struct pc_to_rdr_escape {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
    uint8_t abData[0];
} __attribute__((packed));

/**\brief PC_to_RDR_IccClock Bulk-Out Message */
struct pc_to_rdr_icc_clock {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bClockCommand;
    uint8_t abRFU[2];
} __attribute__((packed));

/**\brief PC_to_RDR_T0APDU Bulk-Out Message */
struct pc_to_rdr_t0_apdu {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bmChanges;
    uint8_t bClassGetResponse;
    uint8_t bClassEnvelope;
} __attribute__((packed));

/**\brief PC_to_RDR_Secure Bulk-Out Message */
struct pc_to_rdr_secure {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bBWI;
    uint16_t wLevelParameter;
    uint8_t abData[0];
} __attribute__((packed));

/**\brief PC_to_RDR_Mechanical Bulk-Out Message */
struct pc_to_rdr_mechanical {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bFunction;
    uint8_t abRFU[2];
} __attribute__((packed));

/**\brief PC_to_RDR_Abort Bulk-Out Message */
struct pc_to_rdr_abort {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
} __attribute__((packed));

/**\brief PC_to_RDR_SetDataRateAndClockFrequency Bulk-Out Message */
struct pc_to_rdr_set_data_rate_and_clock_frequency {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t abRFU[3];
    uint32_t dwClockFrequency;
    uint32_t dwDataRate;
} __attribute__((packed));

/**\brief RDR_to_PC_DataBlock Bulk-In Message */
struct rdr_to_pc_data_block {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t bChainParameter;
    uint8_t abData[0];
} __attribute__((packed));

/**\brief RDR_to_PC_SlotStatus Bulk-In Message */
struct rdr_to_pc_slot_status {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t bClockStatus;
} __attribute__((packed));

/**\brief RDR_to_PC_Parameters (T0 protocol) Bulk-In Message */
struct rdr_to_pc_parameters_t0 {
    //common
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t bProtocolNum;

    //specific for T=0
    uint8_t bmFindexDindex;
    uint8_t bmTCCKST0;
    uint8_t bGuardTimeT0;
    uint8_t bWaitingIntegerT0;
    uint8_t bClockStop;
} __attribute__((packed));

/**\brief RDR_to_PC_Parameters (T1 protocol) Bulk-In Message */
struct rdr_to_pc_parameters_t1 {
    //common
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t bProtocolNum;

    //specific for T=1
    uint8_t bmFindexDindex;
    uint8_t bmTCCKST1;
    uint8_t bGuardTimeT1;
    uint8_t bmWaitingIntegersT1;
    uint8_t bClockStop;
    uint8_t bIFSC;
    uint8_t bNadValue;
} __attribute__((packed));

/**\brief RDR_to_PC_Escape Bulk-In Message */
struct rdr_to_pc_escape {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t bRFU;
    uint8_t abData[0];
} __attribute__((packed));

/**\brief RDR_to_PC_DataRateAndClockFrequency Bulk-In Message */
struct rdr_to_pc_data_rate_and_clock_frequency {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t bRFU;
    uint32_t dwClockFrequency;
    uint32_t dwDataRate;
} __attribute__((packed));

/**\brief RDR_to_PC_NotifySlotChange Interrupt-In Message */
struct rdr_to_pc_notify_slot_change {
    uint8_t bMessageType;
    uint8_t bmSlotICCState[0];
} __attribute__((packed));

/**\brief RDR_to_PC_HardwareError Interrupt-In Message */
struct rdr_to_pc_hardware_error {
    uint8_t bMessageType;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bHardwareErrorCode;
} __attribute__((packed));

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _CCID_PROTOCOL_H_ */
