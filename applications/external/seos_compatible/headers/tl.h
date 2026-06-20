/**
 ******************************************************************************
 * @file    tl.h
 * @author  MCD Application Team
 * @brief   Header for tl module
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2018-2021 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TL_H
#define __TL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32_wpan_common.h"

/* Exported defines -----------------------------------------------------------*/
#define TL_BLECMD_PKT_TYPE     (0x01)
#define TL_ACL_DATA_PKT_TYPE   (0x02)
#define TL_BLEEVT_PKT_TYPE     (0x04)
#define TL_OTCMD_PKT_TYPE      (0x08)
#define TL_OTRSP_PKT_TYPE      (0x09)
#define TL_CLICMD_PKT_TYPE     (0x0A)
#define TL_OTNOT_PKT_TYPE      (0x0C)
#define TL_OTACK_PKT_TYPE      (0x0D)
#define TL_CLINOT_PKT_TYPE     (0x0E)
#define TL_CLIACK_PKT_TYPE     (0x0F)
#define TL_SYSCMD_PKT_TYPE     (0x10)
#define TL_SYSRSP_PKT_TYPE     (0x11)
#define TL_SYSEVT_PKT_TYPE     (0x12)
#define TL_CLIRESP_PKT_TYPE    (0x15)
#define TL_M0CMD_PKT_TYPE      (0x16)
#define TL_LOCCMD_PKT_TYPE     (0x20)
#define TL_LOCRSP_PKT_TYPE     (0x21)
#define TL_TRACES_APP_PKT_TYPE (0x40)
#define TL_TRACES_WL_PKT_TYPE  (0x41)

#define TL_CMD_HDR_SIZE        (4)
#define TL_EVT_HDR_SIZE        (3)
#define TL_EVT_CS_PAYLOAD_SIZE (4)

#define TL_BLEEVT_CC_OPCODE (0x0E)
#define TL_BLEEVT_CS_OPCODE (0x0F)
#define TL_BLEEVT_VS_OPCODE (0xFF)

#define TL_BLEEVT_CC_PACKET_SIZE (TL_EVT_HDR_SIZE + sizeof(TL_CcEvt_t))
#define TL_BLEEVT_CC_BUFFER_SIZE (sizeof(TL_PacketHeader_t) + TL_BLEEVT_CC_PACKET_SIZE)
/* Exported types ------------------------------------------------------------*/
/**< Packet header */
typedef PACKED_STRUCT {
    uint32_t* next;
    uint32_t* prev;
}
TL_PacketHeader_t;

/*******************************************************************************
 * Event type
 */

/**
 * This the payload of TL_Evt_t for a command status event
 */
typedef PACKED_STRUCT {
    uint8_t status;
    uint8_t numcmd;
    uint16_t cmdcode;
}
TL_CsEvt_t;

/**
 * This the payload of TL_Evt_t for a command complete event, only used a pointer
 */
typedef PACKED_STRUCT {
    uint8_t numcmd;
    uint16_t cmdcode;
    uint8_t payload[2];
}
TL_CcEvt_t;

/**
 * This the payload of TL_Evt_t for an asynchronous event, only used a pointer
 */
typedef PACKED_STRUCT {
    uint16_t subevtcode;
    uint8_t payload[2];
}
TL_AsynchEvt_t;

/**
 * This the payload of TL_Evt_t, only used a pointer
 */
typedef PACKED_STRUCT {
    uint8_t evtcode;
    uint8_t plen;
    uint8_t payload[2];
}
TL_Evt_t;

typedef PACKED_STRUCT {
    uint8_t type;
    TL_Evt_t evt;
}
TL_EvtSerial_t;

/**
 * This format shall be used for all events (asynchronous and command response) reported
 * by the CPU2 except for the command response of a system command where the header is not there
 * and the format to be used shall be TL_EvtSerial_t.
 * Note: Be careful that the asynchronous events reported by the CPU2 on the system channel do
 * include the header and shall use TL_EvtPacket_t format. Only the command response format on the
 * system channel is different.
 */
typedef PACKED_STRUCT {
    TL_PacketHeader_t header;
    TL_EvtSerial_t evtserial;
}
TL_EvtPacket_t;

/*****************************************************************************************
 * Command type
 */

typedef PACKED_STRUCT {
    uint16_t cmdcode;
    uint8_t plen;
    uint8_t payload[255];
}
TL_Cmd_t;

typedef PACKED_STRUCT {
    uint8_t type;
    TL_Cmd_t cmd;
}
TL_CmdSerial_t;

typedef PACKED_STRUCT {
    TL_PacketHeader_t header;
    TL_CmdSerial_t cmdserial;
}
TL_CmdPacket_t;

/*****************************************************************************************
 * HCI ACL DATA type
 */
typedef PACKED_STRUCT {
    uint8_t type;
    uint16_t handle;
    uint16_t length;
    uint8_t acl_data[1];
}
TL_AclDataSerial_t;

typedef PACKED_STRUCT {
    TL_PacketHeader_t header;
    TL_AclDataSerial_t AclDataSerial;
}
TL_AclDataPacket_t;

typedef struct {
    uint8_t* p_BleSpareEvtBuffer;
    uint8_t* p_SystemSpareEvtBuffer;
    uint8_t* p_AsynchEvtPool;
    uint32_t AsynchEvtPoolSize;
    uint8_t* p_TracesEvtPool;
    uint32_t TracesEvtPoolSize;
} TL_MM_Config_t;

typedef struct {
    uint8_t* p_ThreadOtCmdRspBuffer;
    uint8_t* p_ThreadCliRspBuffer;
    uint8_t* p_ThreadNotAckBuffer;
    uint8_t* p_ThreadCliNotBuffer;
} TL_TH_Config_t;

typedef struct {
    uint8_t* p_LldTestsCliCmdRspBuffer;
    uint8_t* p_LldTestsM0CmdBuffer;
} TL_LLD_tests_Config_t;

typedef struct {
    uint8_t* p_BleLldCmdRspBuffer;
    uint8_t* p_BleLldM0CmdBuffer;
} TL_BLE_LLD_Config_t;

typedef struct {
    uint8_t* p_Mac_802_15_4_CmdRspBuffer;
    uint8_t* p_Mac_802_15_4_NotAckBuffer;
} TL_MAC_802_15_4_Config_t;

typedef struct {
    uint8_t* p_ZigbeeOtCmdRspBuffer;
    uint8_t* p_ZigbeeNotAckBuffer;
    uint8_t* p_ZigbeeNotifRequestBuffer;
} TL_ZIGBEE_Config_t;

/**
 * @brief Contain the BLE HCI Init Configuration
 * @{
 */
typedef struct {
    void (*IoBusEvtCallBack)(TL_EvtPacket_t* phcievt);
    void (*IoBusAclDataTxAck)(void);
    uint8_t* p_cmdbuffer;
    uint8_t* p_AclDataBuffer;
} TL_BLE_InitConf_t;

/**
 * @brief Contain the SYSTEM HCI Init Configuration
 * @{
 */
typedef struct {
    void (*IoBusCallBackCmdEvt)(TL_EvtPacket_t* phcievt);
    void (*IoBusCallBackUserEvt)(TL_EvtPacket_t* phcievt);
    uint8_t* p_cmdbuffer;
} TL_SYS_InitConf_t;

/*****************************************************************************************
 * Event type copied from ble_legacy.h 
 */

typedef PACKED_STRUCT {
    uint8_t type;
    uint8_t data[1];
}
hci_uart_pckt;

typedef PACKED_STRUCT {
    uint8_t evt;
    uint8_t plen;
    uint8_t data[1];
}
hci_event_pckt;

typedef PACKED_STRUCT {
    uint8_t subevent;
    uint8_t data[1];
}
evt_le_meta_event;

/**
 * Vendor specific event for BLE core.
 */
typedef PACKED_STRUCT {
    uint16_t ecode; /**< One of the BLE core event codes. */
    uint8_t data[1];
}
evt_blecore_aci;

/* Bluetooth 48 bit address (in little-endian order).
 */
typedef uint8_t tBDAddr[6];

/* Exported constants --------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/******************************************************************************
 * GENERAL
 ******************************************************************************/
void TL_Enable(void);
void TL_Init(void);

/******************************************************************************
 * BLE
 ******************************************************************************/
int32_t TL_BLE_Init(void* pConf);
int32_t TL_BLE_SendCmd(uint8_t* buffer, uint16_t size);
int32_t TL_BLE_SendAclData(uint8_t* buffer, uint16_t size);

/******************************************************************************
 * SYSTEM
 ******************************************************************************/
int32_t TL_SYS_Init(void* pConf);
int32_t TL_SYS_SendCmd(uint8_t* buffer, uint16_t size);

/******************************************************************************
 * THREAD
 ******************************************************************************/
void TL_THREAD_Init(TL_TH_Config_t* p_Config);
void TL_OT_SendCmd(void);
void TL_CLI_SendCmd(void);
void TL_OT_CmdEvtReceived(TL_EvtPacket_t* Otbuffer);
void TL_THREAD_NotReceived(TL_EvtPacket_t* Notbuffer);
void TL_THREAD_SendAck(void);
void TL_THREAD_CliSendAck(void);
void TL_THREAD_CliNotReceived(TL_EvtPacket_t* Notbuffer);

/******************************************************************************
 * LLD TESTS
 ******************************************************************************/
void TL_LLDTESTS_Init(TL_LLD_tests_Config_t* p_Config);
void TL_LLDTESTS_SendCliCmd(void);
void TL_LLDTESTS_ReceiveCliRsp(TL_CmdPacket_t* Notbuffer);
void TL_LLDTESTS_SendCliRspAck(void);
void TL_LLDTESTS_ReceiveM0Cmd(TL_CmdPacket_t* Notbuffer);
void TL_LLDTESTS_SendM0CmdAck(void);

/******************************************************************************
 * BLE LLD
 ******************************************************************************/
void TL_BLE_LLD_Init(TL_BLE_LLD_Config_t* p_Config);
void TL_BLE_LLD_SendCliCmd(void);
void TL_BLE_LLD_ReceiveCliRsp(TL_CmdPacket_t* Notbuffer);
void TL_BLE_LLD_SendCliRspAck(void);
void TL_BLE_LLD_ReceiveM0Cmd(TL_CmdPacket_t* Notbuffer);
void TL_BLE_LLD_SendM0CmdAck(void);
void TL_BLE_LLD_SendCmd(void);
void TL_BLE_LLD_ReceiveRsp(TL_CmdPacket_t* Notbuffer);
void TL_BLE_LLD_SendRspAck(void);
/******************************************************************************
 * MEMORY MANAGER
 ******************************************************************************/
void TL_MM_Init(TL_MM_Config_t* p_Config);
void TL_MM_EvtDone(TL_EvtPacket_t* hcievt);

/******************************************************************************
 * TRACES
 ******************************************************************************/
void TL_TRACES_Init(void);
void TL_TRACES_EvtReceived(TL_EvtPacket_t* hcievt);

/******************************************************************************
 * MAC 802.15.4
 ******************************************************************************/
void TL_MAC_802_15_4_Init(TL_MAC_802_15_4_Config_t* p_Config);
void TL_MAC_802_15_4_SendCmd(void);
void TL_MAC_802_15_4_CmdEvtReceived(TL_EvtPacket_t* Otbuffer);
void TL_MAC_802_15_4_NotReceived(TL_EvtPacket_t* Notbuffer);
void TL_MAC_802_15_4_SendAck(void);

/******************************************************************************
 * ZIGBEE
 ******************************************************************************/
void TL_ZIGBEE_Init(TL_ZIGBEE_Config_t* p_Config);
void TL_ZIGBEE_SendM4RequestToM0(void);
void TL_ZIGBEE_SendM4AckToM0Notify(void);
void TL_ZIGBEE_NotReceived(TL_EvtPacket_t* Notbuffer);
void TL_ZIGBEE_CmdEvtReceived(TL_EvtPacket_t* Otbuffer);
void TL_ZIGBEE_M0RequestReceived(TL_EvtPacket_t* Otbuffer);
void TL_ZIGBEE_SendM4AckToM0Request(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*__TL_H */
