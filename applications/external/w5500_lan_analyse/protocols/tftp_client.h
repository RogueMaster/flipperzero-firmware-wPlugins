#pragma once

#include <stdint.h>
#include <stdbool.h>

#define TFTP_CLIENT_MAX_FILENAME 64

typedef struct {
    uint32_t bytes_received;
    uint16_t blocks_received;
    uint16_t errors;
    char error_msg[64];
    bool success;
    bool saved_to_sd;
    char save_path[128];
} TftpClientResult;

/**
 * Download a file via TFTP from a server.
 * @param server_ip   TFTP server IP
 * @param filename    Remote filename to download
 * @param save_path   Local SD card path to save (e.g. APP_DATA_PATH("tftp/file"))
 * @param result      Output result
 * @param running     Pointer to volatile bool (set false to cancel)
 * @return true if file downloaded successfully
 */
bool tftp_client_get(
    const uint8_t server_ip[4],
    const char* filename,
    const char* save_path,
    TftpClientResult* result,
    volatile bool* running);
