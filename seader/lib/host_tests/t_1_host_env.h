#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bit_buffer.h"
#include "lrc.h"
#include "t_1_logic.h"

/* Keep the host harness aligned with the production UART scratchpad size. */
#define SEADER_UART_RX_BUF_SIZE   (272)
#define FURI_LOG_W(tag, fmt, ...) ((void)0)
#define furi_check(expr)                                                                  \
    do {                                                                                  \
        if(!(expr)) {                                                                     \
            fprintf(stderr, "furi_check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            abort();                                                                      \
        }                                                                                 \
    } while(0)

typedef struct BitBuffer BitBuffer;
typedef struct Seader Seader;
typedef struct SeaderWorker SeaderWorker;
typedef struct SeaderUartBridge SeaderUartBridge;

typedef enum { SeaderWorkerEventSamPresent = 53 } SeaderWorkerEvent;

typedef void (*SeaderWorkerCallback)(uint32_t event, void* context);

struct SeaderUartBridge {
    uint8_t tx_buf[SEADER_UART_RX_BUF_SIZE];
    size_t tx_len;
    uint8_t T;
    SeaderT1State t1;
};

struct SeaderWorker {
    SeaderUartBridge* uart;
    SeaderWorkerCallback callback;
    void* context;
};

struct Seader {
    SeaderWorker* worker;
};

typedef struct CCID_Message {
    uint8_t bMessageType;
    uint32_t dwLength;
    uint8_t bSlot;
    uint8_t bSeq;
    uint8_t bStatus;
    uint8_t bError;
    uint8_t* payload;
    size_t consumed;
} CCID_Message;

void seader_ccid_XfrBlock(SeaderUartBridge* seader_uart, uint8_t* data, size_t len);
bool seader_worker_process_sam_message(Seader* seader, uint8_t* apdu, uint32_t len);
void seader_worker_send_version(Seader* seader);

typedef struct {
    /* Captured outbound CCID payload emitted by the T=1 implementation. */
    size_t xfrblock_call_count;
    uint8_t last_frame[SEADER_UART_RX_BUF_SIZE];
    size_t last_frame_len;
    /* Captured inbound APDU delivered upward to the SAM worker boundary. */
    size_t process_call_count;
    uint8_t last_apdu[SEADER_UART_RX_BUF_SIZE];
    size_t last_apdu_len;
    bool process_return_value;
    /* Side effects used by the IFS response path. */
    size_t send_version_call_count;
    size_t callback_call_count;
    uint32_t last_callback_event;
} T1HostTestState;

extern T1HostTestState g_t1_host_test_state;

void t1_host_test_reset(void);

void seader_send_t1(SeaderUartBridge* seader_uart, uint8_t* apdu, size_t len);
bool seader_recv_t1(Seader* seader, CCID_Message* message);
void seader_t_1_set_IFSD(Seader* seader);
void seader_t_1_reset(SeaderUartBridge* seader_uart);
