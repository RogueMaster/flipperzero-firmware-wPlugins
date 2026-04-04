#pragma once

#include <stdint.h>
#include <stdbool.h>

/* W5500 socket assignments for HTTP file manager.
 * Uses socket 3 (2KB TX/RX buffer) — same as DNS/WOL/discovery,
 * but those tools never run simultaneously with file manager. */
#define FILEMGR_HTTP_SOCKET 3
#define FILEMGR_HTTP_PORT   80

/* Buffer sizes */
#define FILEMGR_PATH_MAX   256
#define FILEMGR_CHUNK_SIZE 512 /* must fit in socket TX buffer (2KB) */

/* Auth token length (4 hex chars = 16 bits) */
#define FILEMGR_TOKEN_LEN 4

/* File manager state */
typedef struct {
    volatile bool running;
    uint32_t requests_served;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t errors;
    char current_path[FILEMGR_PATH_MAX]; /* path being browsed */
    char auth_token[FILEMGR_TOKEN_LEN + 1]; /* random access token */
} FileManagerState;

/**
 * Open HTTP socket and start listening.
 * Returns true on success.
 */
bool file_manager_start(FileManagerState* state);

/**
 * Single poll cycle of the HTTP file manager.
 * Call repeatedly from the worker thread.
 * buf: shared buffer (>= FILEMGR_CHUNK_SIZE bytes)
 * buf_size: buffer size
 */
void file_manager_poll(FileManagerState* state, uint8_t* buf, uint16_t buf_size);

/**
 * Close sockets and clean up.
 */
void file_manager_stop(FileManagerState* state);
