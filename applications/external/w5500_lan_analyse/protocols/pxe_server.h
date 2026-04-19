#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <storage/storage.h>

/* W5500 socket assignment for PXE */
#define PXE_DHCP_SOCKET      3 /* UDP port 67 */
#define PXE_TFTP_SOCKET      4 /* UDP port 69 (listen) */
#define PXE_TFTP_DATA_SOCKET 5 /* UDP dynamic port (data transfer) */

/* TFTP constants (RFC 1350) */
#define TFTP_BLOCK_SIZE     512
#define TFTP_SERVER_PORT    69
#define TFTP_DATA_PORT_BASE 51000

/* PXE boot file path on SD card */
#define PXE_BOOT_DIR          EXT_PATH("apps_data/lan_tester/pxe")
#define PXE_DEFAULT_BOOT_FILE "undionly.kpxe"

/* TFTP opcodes (RFC 1350 + RFC 2347) */
#define TFTP_OP_RRQ   1
#define TFTP_OP_WRQ   2
#define TFTP_OP_DATA  3
#define TFTP_OP_ACK   4
#define TFTP_OP_ERROR 5
#define TFTP_OP_OACK  6

/* TFTP error codes */
#define TFTP_ERR_UNDEFINED 0
#define TFTP_ERR_NOT_FOUND 1
#define TFTP_ERR_ACCESS    2
#define TFTP_ERR_ILLEGAL   4

/* DHCP constants */
#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68
#define DHCP_MAGIC_COOKIE 0x63825363

/* DHCP message types (Option 53) */
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

/* Timeout / retry */
#define TFTP_TIMEOUT_MS  3000
#define TFTP_MAX_RETRIES 5

/* PXE server states */
typedef enum {
    PxeStateIdle, /* Waiting for client */
    PxeStateDhcpOfferSent, /* Sent DHCP Offer, waiting for Request */
    PxeStateDhcpAckSent, /* Sent DHCP ACK, waiting for TFTP RRQ */
    PxeStateTftpTransfer, /* TFTP file transfer in progress */
    PxeStateDone, /* Transfer complete */
    PxeStateError, /* Error occurred */
} PxeState;

/* TFTP transfer session */
typedef struct {
    uint8_t client_ip[4];
    uint16_t client_port; /* Client's TID (source port) */
    uint16_t block_num; /* Current block being sent */
    uint16_t blksize; /* Negotiated block size (512 default, up to 1468) */
    uint32_t file_size; /* Total file size in bytes */
    uint32_t bytes_sent; /* Bytes successfully ACK'd */
    uint16_t last_block_size; /* Size of last DATA sent (< blksize = EOF) */
    uint8_t retries; /* Retry counter for current block */
    uint32_t last_send_tick; /* Tick when last DATA was sent */
    bool active;
    bool oack_pending; /* Waiting for ACK 0 after OACK */
    File* file; /* Open file handle during transfer */
    Storage* storage; /* Storage record handle */
} TftpSession;

/* PXE server configuration (from user settings) */
typedef struct {
    uint8_t server_ip[4]; /* Flipper's IP */
    uint8_t client_ip[4]; /* IP to offer via DHCP */
    uint8_t subnet[4]; /* Subnet mask */
    bool dhcp_enabled; /* Run built-in DHCP server? */
} PxeConfig;

/* Maximum number of detected boot files */
#define PXE_MAX_BOOT_FILES 8

/* Single boot file entry */
typedef struct {
    char filename[64];
    uint32_t file_size;
} PxeBootFile;

/* PXE server overall state */
typedef struct {
    volatile bool running;
    PxeState state;
    PxeConfig config;
    TftpSession tftp;

    /* Stats for display */
    uint32_t dhcp_discovers;
    uint32_t dhcp_requests;
    uint32_t tftp_requests;
    uint32_t tftp_blocks_sent;
    uint32_t tftp_errors;

    /* Boot file info (selected) */
    char boot_filename[64];
    uint32_t boot_file_size;
    bool boot_file_found;

    /* All detected boot files */
    PxeBootFile boot_files[PXE_MAX_BOOT_FILES];
    uint8_t boot_file_count;

    /* Client info (for display) */
    uint8_t client_mac[6];
    bool client_seen;
} PxeServerState;

/* Result of external DHCP detection */
typedef struct {
    bool found; /* true if external DHCP responded */
    uint8_t offered_ip[4]; /* IP offered to us */
    uint8_t server_ip[4]; /* DHCP server IP */
    uint8_t subnet[4]; /* Subnet from DHCP */
    uint8_t gateway[4]; /* Gateway from DHCP */
} PxeExternalDhcp;

/**
 * Probe the network for an existing DHCP server.
 * Sends a DHCP Discover and waits up to 5 seconds for an Offer.
 * If found, populates result with the external DHCP info.
 * Returns true if an external DHCP server was detected.
 */
bool pxe_detect_external_dhcp(uint8_t socket_num, const uint8_t mac[6], PxeExternalDhcp* result);

/**
 * Detect boot file on SD card.
 * Populates state->boot_filename and state->boot_file_size.
 * Returns true if a valid boot file was found.
 */
bool pxe_detect_boot_file(PxeServerState* state);

/**
 * Single poll cycle of the PXE server.
 * Call repeatedly from the worker thread.
 * buf: shared frame buffer (>= 1024 bytes)
 * buf_size: buffer size
 */
void pxe_server_poll(PxeServerState* state, uint8_t* buf, uint16_t buf_size);

/**
 * Open DHCP + TFTP sockets. Call once before the poll loop.
 * Returns true on success.
 */
bool pxe_server_start(PxeServerState* state);

/**
 * Close sockets and clean up. Call after the poll loop ends.
 */
void pxe_server_stop(PxeServerState* state);
