#include "http_download.h"
#include "dns_lookup.h"

#include <furi.h>
#include <storage/storage.h>
#include <socket.h>
#include <wizchip_conf.h>
#include <w5500.h>

#include <string.h>
#include <stdio.h>

#define TAG "HTTP_DL"

#define HTTP_CONNECT_TIMEOUT_MS 10000
#define HTTP_RECV_TIMEOUT_MS    15000
#define HTTP_PROGRESS_INTERVAL  500 /* ms between progress callbacks */

/* Wait for previous send() to complete on W5500 */
static bool http_dl_wait_send(uint8_t sn) {
    uint32_t start = furi_get_tick();
    while(furi_get_tick() - start < 3000) {
        uint8_t ir = getSn_IR(sn);
        if(ir & Sn_IR_SENDOK) return true;
        if(ir & Sn_IR_TIMEOUT) return false;
        uint8_t sr = getSn_SR(sn);
        if(sr != SOCK_ESTABLISHED && sr != SOCK_CLOSE_WAIT) return false;
        furi_delay_ms(1);
    }
    return false;
}

/* Reliable send over TCP */
static bool http_dl_send(uint8_t sn, const uint8_t* data, uint16_t len) {
    uint16_t sent = 0;
    while(sent < len) {
        int32_t ret = send(sn, (uint8_t*)(data + sent), len - sent);
        if(ret > 0) {
            sent += (uint16_t)ret;
            if(sent < len) {
                if(!http_dl_wait_send(sn)) return false;
            }
        } else if(ret == SOCK_BUSY) {
            if(!http_dl_wait_send(sn)) return false;
        } else {
            return false;
        }
    }
    return true;
}

static bool http_dl_send_str(uint8_t sn, const char* str) {
    uint16_t len = strlen(str);
    if(len == 0) return true;
    return http_dl_send(sn, (const uint8_t*)str, len);
}

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
    void* progress_ctx) {
    memset(result, 0, sizeof(HttpDownloadResult));

    /* Step 1: DNS resolve */
    DnsLookupResult dns_result;
    if(!dns_lookup(dns_socket, dns_server, hostname, &dns_result)) {
        strncpy(result->error_msg, "DNS resolve failed", sizeof(result->error_msg));
        return false;
    }

    FURI_LOG_I(
        TAG,
        "Resolved %s -> %d.%d.%d.%d",
        hostname,
        dns_result.resolved_ip[0],
        dns_result.resolved_ip[1],
        dns_result.resolved_ip[2],
        dns_result.resolved_ip[3]);

    /* Step 2: Open TCP socket — use dynamic local port to avoid
     * port reuse issues between sequential downloads */
    close(http_socket);
    static uint16_t local_port = 18080;
    local_port++;
    if(local_port > 18999) local_port = 18080;

    if(socket(http_socket, Sn_MR_TCP, local_port, 0) != http_socket) {
        strncpy(result->error_msg, "Socket open failed", sizeof(result->error_msg));
        return false;
    }

    /* Step 3: TCP connect */
    int8_t cr = connect(http_socket, dns_result.resolved_ip, HTTP_PORT);
    if(cr != SOCK_OK) {
        /* connect() may return immediately on W5500; poll for ESTABLISHED */
        uint32_t start = furi_get_tick();
        bool connected = false;
        while(furi_get_tick() - start < HTTP_CONNECT_TIMEOUT_MS && *running) {
            uint8_t sr = getSn_SR(http_socket);
            if(sr == SOCK_ESTABLISHED) {
                connected = true;
                break;
            }
            if(sr == SOCK_CLOSED) break;
            furi_delay_ms(10);
        }
        if(!connected) {
            strncpy(result->error_msg, "Connect timeout", sizeof(result->error_msg));
            close(http_socket);
            return false;
        }
    }

    /* Step 4: Send HTTP GET request */
    if(!http_dl_send_str(http_socket, "GET ") || !http_dl_send_str(http_socket, path) ||
       !http_dl_send_str(http_socket, " HTTP/1.0\r\nHost: ") ||
       !http_dl_send_str(http_socket, hostname) ||
       !http_dl_send_str(http_socket, "\r\nConnection: close\r\n\r\n")) {
        strncpy(result->error_msg, "Send request failed", sizeof(result->error_msg));
        close(http_socket);
        return false;
    }

    http_dl_wait_send(http_socket);

    /* Step 5: Receive response — find end of headers */
    int32_t total = 0;
    bool headers_done = false;
    uint32_t idle_start = furi_get_tick();

    /* Read until we find \r\n\r\n (end of headers) */
    while(!headers_done && *running) {
        if(furi_get_tick() - idle_start > HTTP_RECV_TIMEOUT_MS) {
            strncpy(result->error_msg, "Header recv timeout", sizeof(result->error_msg));
            close(http_socket);
            return false;
        }

        uint8_t sr = getSn_SR(http_socket);
        if(sr != SOCK_ESTABLISHED && sr != SOCK_CLOSE_WAIT) break;

        int32_t space = (int32_t)buf_size - total - 1;
        if(space <= 0) {
            strncpy(result->error_msg, "Headers too large", sizeof(result->error_msg));
            close(http_socket);
            return false;
        }

        int32_t len = recv(http_socket, buf + total, space);
        if(len > 0) {
            total += len;
            buf[total] = '\0';
            idle_start = furi_get_tick();

            /* Check for end of headers */
            char* hdr_end = strstr((char*)buf, "\r\n\r\n");
            if(hdr_end) {
                /* Check HTTP status line for 200 OK */
                char* status = strstr((char*)buf, " ");
                if(status && strncmp(status + 1, "200", 3) != 0) {
                    /* Extract status code for error message */
                    snprintf(
                        result->error_msg,
                        sizeof(result->error_msg),
                        "HTTP %.3s error",
                        status + 1);
                    close(http_socket);
                    return false;
                }
                headers_done = true;
            }
        } else {
            furi_delay_ms(5);
        }
    }

    if(!headers_done) {
        if(result->error_msg[0] == '\0')
            strncpy(result->error_msg, "No HTTP response", sizeof(result->error_msg));
        close(http_socket);
        return false;
    }

    /* Step 6: Open file for writing */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, save_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        strncpy(result->error_msg, "Cannot create file", sizeof(result->error_msg));
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        close(http_socket);
        return false;
    }

    /* Write any body data already received after headers */
    char* body_start = strstr((char*)buf, "\r\n\r\n") + 4;
    int32_t body_pre = total - (int32_t)(body_start - (char*)buf);
    if(body_pre > 0) {
        storage_file_write(file, body_start, body_pre);
        result->bytes_received += body_pre;
    }

    /* Step 7: Stream remaining body to file */
    idle_start = furi_get_tick();
    uint32_t last_progress = furi_get_tick();
    while(*running) {
        uint8_t sr = getSn_SR(http_socket);

        int32_t len = recv(http_socket, buf, buf_size);
        if(len > 0) {
            storage_file_write(file, buf, len);
            result->bytes_received += len;
            idle_start = furi_get_tick();

            /* Periodic progress callback */
            if(progress_cb && furi_get_tick() - last_progress >= HTTP_PROGRESS_INTERVAL) {
                last_progress = furi_get_tick();
                progress_cb(result->bytes_received, progress_ctx);
            }
        } else {
            /* No data — check if connection closed (HTTP/1.0 signals EOF by close) */
            if(sr == SOCK_CLOSE_WAIT || sr == SOCK_CLOSED) break;
            if(furi_get_tick() - idle_start > HTTP_RECV_TIMEOUT_MS) {
                strncpy(result->error_msg, "Transfer timeout", sizeof(result->error_msg));
                break;
            }
            furi_delay_ms(5);
        }
    }

    /* Final progress callback */
    if(progress_cb) {
        progress_cb(result->bytes_received, progress_ctx);
    }

    /* Cleanup */
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    close(http_socket);

    if(result->bytes_received > 0 && result->error_msg[0] == '\0') {
        result->success = true;
        FURI_LOG_I(
            TAG, "Downloaded %lu bytes -> %s", (unsigned long)result->bytes_received, save_path);
    } else {
        /* Remove empty/partial file on failure */
        if(result->bytes_received == 0) {
            Storage* st = furi_record_open(RECORD_STORAGE);
            storage_simply_remove(st, save_path);
            furi_record_close(RECORD_STORAGE);
        }
        if(result->error_msg[0] == '\0')
            strncpy(result->error_msg, "Download failed", sizeof(result->error_msg));
    }

    return result->success;
}
