#pragma once

#include "ccid.h"
#include "t_1_logic.h"

typedef struct CCID_Message CCID_Message;

void seader_send_t1(SeaderUartBridge* seader_uart, uint8_t* apdu, size_t len);
bool seader_recv_t1(Seader* seader, CCID_Message* message);
void seader_t_1_set_IFSD(Seader* seader);
void seader_t_1_reset(SeaderUartBridge* seader_uart);
