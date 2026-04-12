#pragma once

#include <stdint.h>
#include <stdbool.h>

/* W5500 socket for HTTP client.
 * Reuses PXE_TFTP_DATA_SOCKET (5) — PXE server and HTTP download
 * never run simultaneously (both are worker-thread operations). */
#define HTTP_CLIENT_SOCKET 5
#define HTTP_PORT          80

typedef struct {
    bool success;
    uint32_t bytes_received;
    char error_msg[48];
} HttpDownloadResult;

/* Progress callback: called periodically during download.
 * bytes_received: total bytes downloaded so far.
 * ctx: opaque user context. */
typedef void (*HttpProgressCb)(uint32_t bytes_received, void* ctx);

/**
 * Download a file via HTTP GET and save to SD card.
 *
 * @param dns_socket   W5500 socket for DNS resolve (e.g. W5500_DNS_SOCKET)
 * @param http_socket  W5500 socket for TCP transfer (e.g. HTTP_CLIENT_SOCKET)
 * @param dns_server   DNS server IP (from DHCP)
 * @param hostname     HTTP server hostname (e.g. "boot.ipxe.org")
 * @param path         URL path (e.g. "/undionly.kpxe")
 * @param save_path    Full SD card path to save file
 * @param buf          Caller-provided recv buffer (use app->frame_buf)
 * @param buf_size     Size of buf (e.g. 1024)
 * @param result       Output result
 * @param running      Pointer to volatile bool (set false to cancel)
 * @param progress_cb  Optional progress callback (NULL to skip)
 * @param progress_ctx Context passed to progress_cb
 * @return true if file downloaded and saved successfully
 */
bool http_download_file(
    uint8_t dns_socket,
    uint8_t http_socket,
    const uint8_t dns_server[4],
    const char* hostname,
    const char* path,
    const char* save_path,
    uint8_t* buf,
    uint16_t buf_size,
    HttpDownloadResult* result,
    volatile bool* running,
    HttpProgressCb progress_cb,
    void* progress_ctx);
