/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SIP_HDR_F_SYNC 0x4
#define SIP_IFIDX_MASK 0xf0
#define SIP_IFIDX_S 4
#define SIP_TYPE_MASK 0x0f
#define SIP_TYPE_S 0

#define SIP_PACKET_SIZE 256

typedef enum {
    SIP_PACKET_TYPE_CTRL = 0,
    SIP_PACKET_TYPE_DATA,
} sip_packet_type_t;

typedef enum {
    SIP_CMD_ID_GET_VER = 0,
    SIP_CMD_ID_WRITE_MEMORY = 1,
    SIP_CMD_ID_BOOTUP = 5,
} sip_cmd_id_t;

typedef struct __attribute__((packed))
{
    uint8_t tid;
    uint8_t ac;
    uint8_t p2p: 1,
            enc_flag: 7;
    uint8_t hw_kid;
} sip_tx_data_info_t;

typedef struct __attribute__((packed))
{
    union {
        uint32_t cmdid;
        sip_tx_data_info_t dinfo;
    } u;
} sip_tx_info_t;

typedef struct __attribute__((packed))
{
    uint8_t fc[2];
    uint16_t len; // Length of packet, including the header itself, must align to 4
    union {
        uint32_t recycled_credits;
        sip_tx_info_t tx_info;
    } u;
    uint32_t sequence_num;
} sip_header_t;

typedef struct __attribute__((packed))
{
    uint32_t addr;
    uint32_t len;
} sip_cmd_write_memory;

typedef struct __attribute__((packed))
{
    uint32_t boot_addr;
    uint32_t discard_link;
} sip_cmd_bootup;
