#pragma once

#include "ccid.h"

typedef struct CCID_Message CCID_Message;

typedef enum {
    SEADER_T1_PCB_I_BLOCK_MORE = 0x20,
    SEADER_T1_PCB_SEQUENCE_BIT = 0x40,
    SEADER_T1_PCB_R_BLOCK = 0x80,
    SEADER_T1_PCB_S_BLOCK = 0xC0,
    SEADER_T1_R_BLOCK_SEQUENCE_MASK = 0x10,
    SEADER_T1_S_BLOCK_IFS = 0x01,
} SeaderT1Constant;

void seader_send_t1(SeaderUartBridge* seader_uart, uint8_t* apdu, size_t len);
bool seader_recv_t1(Seader* seader, CCID_Message* message);
void seader_t_1_set_IFSD(Seader* seader);
void seader_t_1_reset(SeaderUartBridge* seader_uart);
