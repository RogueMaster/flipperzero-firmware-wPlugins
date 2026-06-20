#include "file_manager.h"

#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_version.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <socket.h>
#include <wizchip_conf.h>
#include <w5500.h>

#include <string.h>
#include <stdio.h>

#define TAG "FILEMGR"

/* Max single file/folder name */
#define FM_NAME_MAX 64

/* Simple memmem implementation */
static void* filemgr_memmem(const void* haystack, size_t hlen, const void* needle, size_t nlen) {
    if(nlen == 0) return (void*)haystack;
    if(hlen < nlen) return NULL;
    const uint8_t* h = haystack;
    for(size_t i = 0; i <= hlen - nlen; i++) {
        if(memcmp(h + i, needle, nlen) == 0) return (void*)(h + i);
    }
    return NULL;
}

/* ==================== URL decoding ==================== */

static int hex_digit(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char* dst, const char* src, size_t dst_size) {
    size_t di = 0;
    for(size_t i = 0; src[i] && di < dst_size - 1; i++) {
        if(src[i] == '%' && src[i + 1] && src[i + 2]) {
            int h = hex_digit(src[i + 1]);
            int l = hex_digit(src[i + 2]);
            if(h >= 0 && l >= 0) {
                dst[di++] = (char)((h << 4) | l);
                i += 2;
                continue;
            }
        } else if(src[i] == '+') {
            dst[di++] = ' ';
            continue;
        }
        dst[di++] = src[i];
    }
    dst[di] = '\0';
}

/* ==================== Reliable TCP send ==================== */

/*
 * Wait until WIZnet finishes transmitting the previous send.
 * The W5500 send() sets sock_is_sending; the next send() returns
 * SOCK_BUSY until SEND_OK interrupt fires. We poll the interrupt
 * register directly to wait.
 */
static bool http_wait_send_done(uint8_t sn) {
    uint32_t start = furi_get_tick();
    while(furi_get_tick() - start < 3000) {
        uint8_t ir = getSn_IR(sn);
        if(ir & Sn_IR_SENDOK) {
            /* Do NOT clear Sn_IR_SENDOK here! The WIZnet send() function
             * must see this bit itself to clear its internal sock_is_sending
             * flag. If we clear it, send() will never know the previous
             * send completed and will return SOCK_BUSY forever. */
            return true;
        }
        if(ir & Sn_IR_TIMEOUT) {
            return false;
        }
        uint8_t sr = getSn_SR(sn);
        if(sr != SOCK_ESTABLISHED && sr != SOCK_CLOSE_WAIT) {
            return false;
        }
        furi_delay_ms(1);
    }
    return false;
}

/*
 * Send data reliably over TCP. Handles:
 * - Waiting for previous send to complete (SEND_OK)
 * - Splitting data into TX-buffer-sized chunks
 * - Proper error handling
 */
static bool http_send_buf(uint8_t sn, const uint8_t* data, uint16_t len) {
    uint16_t sent_total = 0;
    while(sent_total < len) {
        uint16_t to_send = len - sent_total;
        int32_t ret = send(sn, (uint8_t*)(data + sent_total), to_send);
        if(ret > 0) {
            sent_total += (uint16_t)ret;
            /* Wait for this chunk to be fully transmitted before sending more */
            if(sent_total < len) {
                if(!http_wait_send_done(sn)) return false;
            }
        } else if(ret == SOCK_BUSY) {
            /* Previous send still in flight — wait for it */
            if(!http_wait_send_done(sn)) return false;
        } else {
            /* Hard error */
            FURI_LOG_E(TAG, "send error: %ld", (long)ret);
            return false;
        }
    }
    return true;
}

static bool http_send_str(uint8_t sn, const char* str) {
    uint16_t len = strlen(str);
    if(len == 0) return true;
    return http_send_buf(sn, (const uint8_t*)str, len);
}

/* Send "/web_path/name?token" path fragment (reused for browse/download/delete links) */
static void http_send_path(uint8_t sn, const char* web_path, const char* name, const char* tsuf) {
    if(strcmp(web_path, "/") != 0) http_send_str(sn, web_path);
    http_send_str(sn, "/");
    http_send_str(sn, name);
    http_send_str(sn, tsuf);
}

/* Stream file contents from SD directly into HTTP socket (no heap buffer needed).
 * Uses a small stack buffer for chunked reading. */
static void http_send_file_inline(uint8_t sn, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char chunk[128];
        uint16_t rd;
        while((rd = storage_file_read(f, chunk, sizeof(chunk))) > 0) {
            http_send_buf(sn, (const uint8_t*)chunk, rd);
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

/*
 * Wait for ALL pending data to be transmitted before closing connection.
 * 1. Wait for SEND_OK (last send command completed)
 * 2. Wait for TX buffer to drain (all data ACK'd by remote)
 * Without this, disconnect() sends FIN while data is still in flight.
 */
static void http_flush(uint8_t sn) {
    /* Wait for last send command to complete */
    http_wait_send_done(sn);

    /* Wait for TX buffer to fully drain (remote ACK'd all data) */
    uint32_t start = furi_get_tick();
    while(furi_get_tick() - start < 5000) {
        uint8_t sr = getSn_SR(sn);
        if(sr != SOCK_ESTABLISHED && sr != SOCK_CLOSE_WAIT) break;
        uint16_t txfree = getSn_TX_FSR(sn);
        uint16_t txmax = getSn_TxMAX(sn);
        if(txfree >= txmax) break; /* All data sent and ACK'd */
        furi_delay_ms(5);
    }
}

/* ==================== HTML generation ==================== */

static const char css[] =
    "<style>"
    "body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;margin:0;padding:16px}"
    "h1{color:#ff8c00;margin:0 0 4px}"
    ".p{color:#888;margin-bottom:12px;word-break:break-all}"
    "table{width:100%;border-collapse:collapse}"
    "th{text-align:left;padding:6px 8px;border-bottom:2px solid #ff8c00;color:#ff8c00}"
    "td{padding:5px 8px;border-bottom:1px solid #333}"
    "a{color:#5dade2;text-decoration:none}"
    ".d{color:#ff8c00;font-weight:bold}"
    ".s{color:#888;text-align:right}"
    ".a{white-space:nowrap}"
    ".a a{margin-left:8px;color:#e74c3c}"
    ".b{display:inline-block;padding:6px 14px;background:#ff8c00;color:#1a1a2e;"
    "border:none;font-weight:bold;text-decoration:none;margin:2px}"
    ".bs{padding:3px 8px}"
    ".uf{margin:12px 0;padding:10px;background:#2a2a4a;border:1px solid #444}"
    ".mf{margin:8px 0}"
    ".ft{margin-top:16px;color:#888}"
    "</style>";

static const char default_js[] = "<script>"
                                 "onload=()=>{let t=document.querySelector('table');"
                                 "if(!t)return;let r=[...t.rows].slice(1),"
                                 "d=r.filter(e=>e.querySelector('.d')),"
                                 "f=r.filter(e=>!e.querySelector('.d'));"
                                 "let s=e=>e.cells[0].textContent.toLowerCase();"
                                 "d.sort((a,b)=>s(a).localeCompare(s(b)));"
                                 "f.sort((a,b)=>s(a).localeCompare(s(b)));"
                                 "[...d,...f].forEach(e=>t.tBodies[0].appendChild(e))}"
                                 "</script>";

/* Format file size human-readable */
static void format_size(uint64_t size, char* buf, size_t buf_size) {
    if(size < 1024) {
        snprintf(buf, buf_size, "%lu B", (unsigned long)size);
    } else if(size < 1024 * 1024) {
        snprintf(
            buf,
            buf_size,
            "%lu.%lu KB",
            (unsigned long)(size / 1024),
            (unsigned long)((size % 1024) * 10 / 1024));
    } else {
        snprintf(
            buf,
            buf_size,
            "%lu.%lu MB",
            (unsigned long)(size / (1024 * 1024)),
            (unsigned long)((size % (1024 * 1024)) * 10 / (1024 * 1024)));
    }
}

/* Escape HTML special chars into a separate buffer.
 * Only escapes <, >, &, " which are dangerous in HTML context.
 * Returns pointer to buf (NOT reentrant, but we are single-threaded). */
static const char* html_escape(const char* src, char* buf, size_t buf_size) {
    size_t di = 0;
    for(size_t i = 0; src[i] && di < buf_size - 6; i++) {
        switch(src[i]) {
        case '<':
            memcpy(buf + di, "&lt;", 4);
            di += 4;
            break;
        case '>':
            memcpy(buf + di, "&gt;", 4);
            di += 4;
            break;
        case '&':
            memcpy(buf + di, "&amp;", 5);
            di += 5;
            break;
        case '"':
            memcpy(buf + di, "&#34;", 5);
            di += 5;
            break;
        default:
            buf[di++] = src[i];
            break;
        }
    }
    buf[di] = '\0';
    return buf;
}

/* Build parent path from current path */
static void get_parent_path(const char* path, char* parent, size_t parent_size) {
    strncpy(parent, path, parent_size);
    parent[parent_size - 1] = '\0';
    size_t len = strlen(parent);
    if(len > 1 && parent[len - 1] == '/') {
        parent[len - 1] = '\0';
        len--;
    }
    char* last_slash = strrchr(parent, '/');
    if(last_slash && last_slash != parent) {
        *last_slash = '\0';
    } else {
        strncpy(parent, "/", parent_size);
    }
}

/* ==================== HTTP request handling ==================== */

/*
 * Stream directory listing page directly to the socket.
 * No FuriString accumulation — constant memory regardless of file count.
 * Only the sorted entry array lives on the heap (~10KB for 128 entries).
 * Uses Connection: close without Content-Length so the browser reads
 * until the server closes the connection.
 */
static void handle_list_dir(
    uint8_t sn,
    const char* sd_path,
    const char* web_path,
    const char* token,
    const FileManagerState* fms) {
    /* Token suffix for all URLs */
    char tsuf[16];
    snprintf(tsuf, sizeof(tsuf), "?t=%s", token);
    /* HTTP headers (no Content-Length — we stream until close) */
    http_send_str(
        sn,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html;charset=utf-8\r\n"
        "Connection: close\r\n\r\n");

    /* HTML head + CSS */
    http_send_str(
        sn,
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Flipper File Manager</title>");
    if(fms->has_custom_css) {
        http_send_str(sn, "<style>");
        http_send_file_inline(sn, APP_DATA_PATH("web/custom.css"));
        http_send_str(sn, "</style>");
    } else {
        http_send_str(sn, css);
    }
    http_send_str(sn, "</head><body>");

    /* Title */
    http_send_str(sn, "<h1>Flipper File Manager</h1>");

    /* Current path */
    http_send_str(sn, "<div class='p'>");
    http_send_str(sn, web_path);
    http_send_str(sn, "</div>");

    /* Parent link */
    if(strcmp(web_path, "/") != 0) {
        char parent[FILEMGR_PATH_MAX];
        get_parent_path(web_path, parent, sizeof(parent));
        http_send_str(sn, "<a href='/browse");
        http_send_str(sn, parent);
        http_send_str(sn, tsuf);
        http_send_str(sn, "' class='b bs'>Up</a> ");
    }

    /* Upload form */
    http_send_str(
        sn,
        "<div class='uf'>"
        "<form method='POST' action='/upload");
    http_send_str(sn, web_path);
    http_send_str(sn, tsuf);
    http_send_str(
        sn,
        "' enctype='multipart/form-data'>"
        "<input type='file' name='file'> "
        "<button type='submit' class='b bs'>Upload</button>"
        "</form></div>");

    /* Mkdir form */
    http_send_str(
        sn,
        "<div class='mf'>"
        "<form method='POST' action='/mkdir");
    http_send_str(sn, web_path);
    http_send_str(sn, tsuf);
    http_send_str(
        sn,
        "'>"
        "<input type='text' name='name' placeholder='New folder' size='16'> "
        "<button type='submit' class='b bs'>Create</button>"
        "</form></div>");

    /* Table header */
    http_send_str(sn, "<table><tr><th>Name</th><th>Size</th><th></th></tr>");

    /* Single-pass rendering — JS on client sorts dirs first, then alphabetically */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    /* Static to avoid 320B stack usage; worker is single-threaded */
    static char esc_name[FM_NAME_MAX * 5];
    bool dir_ok = storage_dir_open(dir, sd_path);
    if(!dir_ok) {
        http_send_str(sn, "<tr><td colspan='3'>Failed to open directory</td></tr>");
    } else {
        FileInfo info;
        char name[FM_NAME_MAX];

        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            html_escape(name, esc_name, sizeof(esc_name));
            bool is_dir = (info.flags & FSF_DIRECTORY);

            http_send_str(sn, "<tr><td>");
            if(is_dir) {
                http_send_str(sn, "<a href='/browse");
                http_send_path(sn, web_path, esc_name, tsuf);
                http_send_str(sn, "' class='d'>");
                http_send_str(sn, esc_name);
                http_send_str(sn, "/</a>");
            } else {
                http_send_str(sn, esc_name);
            }
            http_send_str(sn, "</td><td class='s'>");
            if(is_dir) {
                http_send_str(sn, "-");
            } else {
                char size_str[32];
                format_size(info.size, size_str, sizeof(size_str));
                http_send_str(sn, size_str);
            }
            http_send_str(sn, "</td><td class='a'>");
            if(!is_dir) {
                http_send_str(sn, "<a href='/download");
                http_send_path(sn, web_path, esc_name, tsuf);
                http_send_str(sn, "' class='b bs'>DL</a>");
            }
            http_send_str(sn, "<a href='/delete");
            http_send_path(sn, web_path, esc_name, tsuf);
            http_send_str(
                sn,
                "' onclick=\"return confirm('Delete?')\">"
                "Del</a></td></tr>");
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    /* Footer with device info */
    http_send_str(sn, "</table><div class='ft'>");
    {
        const char* dev_name = furi_hal_version_get_name_ptr();
        uint8_t batt = furi_hal_power_get_pct();
        Storage* fst = furi_record_open(RECORD_STORAGE);
        uint64_t st_total = 0, st_free = 0;
        storage_common_fs_info(fst, "/ext", &st_total, &st_free);
        furi_record_close(RECORD_STORAGE);
        /* Static to avoid 128B stack usage; worker is single-threaded */
        static char ft[128];
        snprintf(
            ft,
            sizeof(ft),
            "%s | Bat: %u%% | SD: %lu/%lu MB free",
            dev_name ? dev_name : "Flipper",
            batt,
            (unsigned long)(st_free / (1024 * 1024)),
            (unsigned long)(st_total / (1024 * 1024)));
        http_send_str(sn, ft);
    }
    http_send_str(sn, "</div>");
    if(fms->has_custom_js) {
        http_send_str(sn, "<script>");
        http_send_file_inline(sn, APP_DATA_PATH("web/custom.js"));
        http_send_str(sn, "</script>");
    } else {
        http_send_str(sn, default_js);
    }
    http_send_str(sn, "</body></html>");
    http_flush(sn);
}

/* Send file download */
static void handle_download(
    uint8_t sn,
    const char* sd_path,
    const char* filename,
    uint8_t* buf,
    uint16_t buf_size,
    FileManagerState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, sd_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        http_send_str(
            sn,
            "HTTP/1.1 404 Not Found\r\nConnection: close\r\n"
            "Content-Length: 14\r\n\r\n404 Not Found\n");
        http_flush(sn);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    uint64_t file_size = storage_file_size(file);

    /* Sanitize filename for Content-Disposition header */
    char safe_name[49];
    size_t si = 0;
    for(size_t i = 0; filename[i] && si < sizeof(safe_name) - 1; i++) {
        char c = filename[i];
        if(c != '"' && c != '\\' && c != '\r' && c != '\n') {
            safe_name[si++] = c;
        }
    }
    safe_name[si] = '\0';

    /* Detect MIME type for CSS/JS (inline), everything else as download */
    const char* content_type = "application/octet-stream";
    bool inline_file = false;
    size_t name_len = strlen(safe_name);
    if(name_len > 4 && strcmp(safe_name + name_len - 4, ".css") == 0) {
        content_type = "text/css";
        inline_file = true;
    } else if(name_len > 3 && strcmp(safe_name + name_len - 3, ".js") == 0) {
        content_type = "text/javascript";
        inline_file = true;
    }

    /* Send response headers
     * Static to avoid 192B stack usage; worker is single-threaded */
    static char hdr[192];
    if(inline_file) {
        snprintf(
            hdr,
            sizeof(hdr),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n\r\n",
            content_type,
            (unsigned long)file_size);
    } else {
        snprintf(
            hdr,
            sizeof(hdr),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Disposition: attachment; filename=\"%.48s\"\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n\r\n",
            content_type,
            safe_name,
            (unsigned long)file_size);
    }
    http_send_str(sn, hdr);

    /* Stream file in chunks */
    while(!storage_file_eof(file) && state->running) {
        uint16_t chunk = buf_size;
        if(chunk > FILEMGR_CHUNK_SIZE) chunk = FILEMGR_CHUNK_SIZE;
        uint16_t read = storage_file_read(file, buf, chunk);
        if(read == 0) break;
        if(!http_send_buf(sn, buf, read)) {
            state->errors++;
            break;
        }
        state->bytes_sent += read;
    }

    http_flush(sn);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/*
 * Streaming file upload handler.
 *
 * Multipart/form-data structure:
 *   --boundary\r\n
 *   Content-Disposition: form-data; name="file"; filename="name.ext"\r\n
 *   Content-Type: ...\r\n
 *   \r\n
 *   <file data...>
 *   \r\n--boundary--\r\n
 *
 * Strategy:
 * 1. Read the first chunk to parse boundary + headers + filename
 * 2. Open file on SD card
 * 3. Write any file data already in the first chunk
 * 4. Stream remaining data: recv() -> write to SD, chunk by chunk
 * 5. Stop when connection closes or boundary is detected
 */
static void handle_upload(
    uint8_t sn,
    const char* sd_dir_path,
    const char* web_dir_path,
    uint8_t* buf,
    uint16_t buf_size,
    int32_t body_pre_read,
    FileManagerState* state) {
    /* Step 1: We already have body_pre_read bytes in buf (from handle_connection).
     * Continue reading until we have multipart headers (boundary + filename). */
    int32_t total_read = body_pre_read;
    uint16_t max_hdr = buf_size;
    if(max_hdr > 512) max_hdr = 512;

    /* Check if we already have enough in pre-read data */
    buf[total_read] = '\0';
    bool have_headers = (strstr((char*)buf, "\r\n\r\n") && total_read > 60);

    if(!have_headers) {
        uint32_t start = furi_get_tick();
        while(furi_get_tick() - start < 5000 && state->running) {
            int32_t space = (int32_t)max_hdr - total_read;
            if(space <= 0) break;
            int32_t len = recv(sn, buf + total_read, space);
            if(len > 0) {
                total_read += len;
                buf[total_read] = '\0';
                if(strstr((char*)buf, "\r\n\r\n") && total_read > 60) break;
                start = furi_get_tick();
            } else {
                furi_delay_ms(5);
            }
        }
    }

    if(total_read <= 0) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 8\r\n\r\nNo data\n");
        http_flush(sn);
        return;
    }
    buf[total_read] = '\0';

    /* Parse boundary (first line up to \r\n) */
    char* boundary_end = strstr((char*)buf, "\r\n");
    if(!boundary_end) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 12\r\n\r\nBad request\n");
        http_flush(sn);
        return;
    }
    char boundary[80];
    size_t boundary_len = boundary_end - (char*)buf;
    if(boundary_len >= sizeof(boundary)) boundary_len = sizeof(boundary) - 1;
    memcpy(boundary, buf, boundary_len);
    boundary[boundary_len] = '\0';

    /* Parse filename */
    char* fn_ptr = strstr((char*)buf, "filename=\"");
    if(!fn_ptr) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 12\r\n\r\nNo filename\n");
        http_flush(sn);
        return;
    }
    fn_ptr += 10;
    char* fn_end = strchr(fn_ptr, '"');
    if(!fn_end || fn_end == fn_ptr) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 12\r\n\r\nNo filename\n");
        http_flush(sn);
        return;
    }
    char filename[FM_NAME_MAX];
    size_t fn_len = fn_end - fn_ptr;
    if(fn_len >= sizeof(filename)) fn_len = sizeof(filename) - 1;
    memcpy(filename, fn_ptr, fn_len);
    filename[fn_len] = '\0';

    /* Sanitize filename: strip path separators to prevent traversal */
    char* slash;
    while((slash = strrchr(filename, '/')) != NULL) {
        memmove(filename, slash + 1, strlen(slash + 1) + 1);
    }
    while((slash = strrchr(filename, '\\')) != NULL) {
        memmove(filename, slash + 1, strlen(slash + 1) + 1);
    }
    fn_len = strlen(filename);
    if(fn_len == 0) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 12\r\n\r\nNo filename\n");
        http_flush(sn);
        return;
    }

    /* Find start of file data (after \r\n\r\n in part headers) */
    char* data_start = strstr(fn_end, "\r\n\r\n");
    if(!data_start) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 12\r\n\r\nBad request\n");
        http_flush(sn);
        return;
    }
    data_start += 4;
    int32_t hdr_consumed = data_start - (char*)buf;
    int32_t leftover = total_read - hdr_consumed;

    /* Build file path */
    char filepath[FILEMGR_PATH_MAX];
    size_t dir_len = strlen(sd_dir_path);
    size_t name_len = strlen(filename);
    if(dir_len + 1 + name_len >= sizeof(filepath)) {
        http_send_str(
            sn,
            "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n"
            "Content-Length: 14\r\n\r\nPath too long\n");
        http_flush(sn);
        return;
    }
    memcpy(filepath, sd_dir_path, dir_len);
    filepath[dir_len] = '/';
    memcpy(filepath + dir_len + 1, filename, name_len + 1);

    /* Step 2: Open file for writing */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, filepath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        http_send_str(
            sn,
            "HTTP/1.1 500 Error\r\nConnection: close\r\n"
            "Content-Length: 12\r\n\r\nWrite error\n");
        http_flush(sn);
        state->errors++;
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    /* Step 3: Write any file data already read in the header chunk.
     * We need to check if the boundary is already in this data
     * (happens for very small files). */
    size_t total_written = 0;
    bool found_boundary = false;

    if(leftover > 0) {
        /* Check for closing boundary in leftover data */
        char* bnd = filemgr_memmem(data_start, leftover, boundary, boundary_len);
        if(bnd) {
            /* Boundary found — file data ends 2 bytes before (strip \r\n) */
            int32_t file_data_len = bnd - data_start;
            if(file_data_len >= 2) file_data_len -= 2; /* strip \r\n before boundary */
            if(file_data_len > 0) {
                total_written += storage_file_write(file, data_start, file_data_len);
            }
            found_boundary = true;
        } else {
            /* No boundary yet — write all leftover data, but keep last
             * boundary_len+4 bytes as overlap (boundary might be split
             * across recv boundaries) */
            int32_t safe = leftover - (int32_t)(boundary_len + 4);
            if(safe > 0) {
                total_written += storage_file_write(file, data_start, safe);
                /* Move the overlap to start of buf */
                int32_t overlap = leftover - safe;
                memmove(buf, data_start + safe, overlap);
                total_read = overlap;
            } else {
                /* Not enough data to safely write — keep it all */
                memmove(buf, data_start, leftover);
                total_read = leftover;
            }
        }
    } else {
        total_read = 0;
    }

    /* Step 4: Stream remaining data from socket to file */
    if(!found_boundary) {
        uint32_t idle_start = furi_get_tick();
        while(state->running) {
            uint8_t sr = getSn_SR(sn);
            if(sr != SOCK_ESTABLISHED && sr != SOCK_CLOSE_WAIT) break;

            int32_t space = (int32_t)buf_size - total_read - 1;
            if(space <= 0) space = 0;

            int32_t len = (space > 0) ? recv(sn, buf + total_read, space) : 0;
            if(len > 0) {
                total_read += len;
                idle_start = furi_get_tick();

                /* Scan for boundary in the buffer */
                char* bnd = filemgr_memmem(buf, total_read, boundary, boundary_len);
                if(bnd) {
                    int32_t file_data_len = bnd - (char*)buf;
                    if(file_data_len >= 2) file_data_len -= 2; /* strip \r\n */
                    if(file_data_len > 0) {
                        total_written += storage_file_write(file, buf, file_data_len);
                    }
                    found_boundary = true;
                    break;
                }

                /* Write safe portion (keep overlap for boundary detection) */
                int32_t safe = total_read - (int32_t)(boundary_len + 4);
                if(safe > 0) {
                    total_written += storage_file_write(file, buf, safe);
                    state->bytes_received = total_written;
                    memmove(buf, buf + safe, total_read - safe);
                    total_read -= safe;
                }
            } else {
                /* No data — check for connection close (end of upload) */
                if(sr == SOCK_CLOSE_WAIT) {
                    /* Client closed connection — write remaining data */
                    if(total_read > 0) {
                        /* Try to strip trailing boundary */
                        char* bnd2 = filemgr_memmem(buf, total_read, boundary, boundary_len);
                        int32_t wlen = bnd2 ? (int32_t)(bnd2 - (char*)buf - 2) : total_read;
                        if(wlen > 0) {
                            total_written += storage_file_write(file, buf, wlen);
                        }
                    }
                    found_boundary = true;
                    break;
                }
                if(furi_get_tick() - idle_start > 10000) break; /* 10s idle timeout */
                furi_delay_ms(5);
            }
        }

        /* If we exited without finding boundary, write whatever is left */
        if(!found_boundary && total_read > 0) {
            total_written += storage_file_write(file, buf, total_read);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    state->bytes_received = total_written;
    FURI_LOG_I(TAG, "Uploaded: %s (%u bytes)", filename, (unsigned)total_written);

    /* Redirect back to directory (with auth token) */
    FuriString* loc = furi_string_alloc_printf("/browse%s?t=%s", web_dir_path, state->auth_token);
    http_send_str(
        sn,
        "HTTP/1.1 303 See Other\r\nConnection: close\r\n"
        "Content-Length: 0\r\nLocation: ");
    http_send_str(sn, furi_string_get_cstr(loc));
    http_send_str(sn, "\r\n\r\n");
    http_flush(sn);
    furi_string_free(loc);
}

/* Handle mkdir */
static void handle_mkdir(
    uint8_t sn,
    const char* sd_dir_path,
    const char* web_dir_path,
    uint8_t* buf,
    uint16_t buf_size,
    int32_t body_pre_read,
    const char* token) {
    /* Body may already be in buf from handle_connection */
    int32_t total_read = body_pre_read;
    uint16_t chunk = buf_size;
    if(chunk > 512) chunk = 512;

    if(total_read <= 0) {
        uint32_t start = furi_get_tick();
        while(furi_get_tick() - start < 3000) {
            int32_t len = recv(sn, buf + total_read, chunk - total_read);
            if(len > 0) {
                total_read += len;
                if(total_read >= (int32_t)chunk) break;
                if(total_read > 5) break;
                start = furi_get_tick();
            } else {
                furi_delay_ms(10);
            }
        }
    }

    char folder_name[FM_NAME_MAX] = {0};

    if(total_read > 0) {
        buf[total_read] = '\0';
        char* name_ptr = strstr((char*)buf, "name=");
        if(name_ptr) {
            name_ptr += 5;
            char* amp = strchr(name_ptr, '&');
            if(amp) *amp = '\0';
            url_decode(folder_name, name_ptr, sizeof(folder_name));
        }
    }

    if(strlen(folder_name) > 0) {
        char dirpath[FILEMGR_PATH_MAX];
        size_t dir_len = strlen(sd_dir_path);
        size_t name_len = strlen(folder_name);
        if(dir_len + 1 + name_len < sizeof(dirpath)) {
            memcpy(dirpath, sd_dir_path, dir_len);
            dirpath[dir_len] = '/';
            memcpy(dirpath + dir_len + 1, folder_name, name_len + 1);

            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_simply_mkdir(storage, dirpath);
            furi_record_close(RECORD_STORAGE);
            FURI_LOG_I(TAG, "Created dir: %s", dirpath);
        }
    }

    /* Redirect */
    FuriString* loc = furi_string_alloc_printf("/browse%s?t=%s", web_dir_path, token);
    http_send_str(
        sn,
        "HTTP/1.1 303 See Other\r\nConnection: close\r\n"
        "Content-Length: 0\r\nLocation: ");
    http_send_str(sn, furi_string_get_cstr(loc));
    http_send_str(sn, "\r\n\r\n");
    http_flush(sn);
    furi_string_free(loc);
}

/* Handle delete */
static void
    handle_delete(uint8_t sn, const char* sd_path, const char* web_parent_path, const char* token) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(storage, sd_path);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Deleted: %s", sd_path);

    FuriString* loc = furi_string_alloc_printf("/browse%s?t=%s", web_parent_path, token);
    http_send_str(
        sn,
        "HTTP/1.1 303 See Other\r\nConnection: close\r\n"
        "Content-Length: 0\r\nLocation: ");
    http_send_str(sn, furi_string_get_cstr(loc));
    http_send_str(sn, "\r\n\r\n");
    http_flush(sn);
    furi_string_free(loc);
}

/* ==================== Main request router ==================== */

/* Reject paths containing directory traversal */
static bool path_is_safe(const char* path) {
    /* Block ".." anywhere in the path */
    const char* p = path;
    while(*p) {
        if(p[0] == '.' && p[1] == '.') return false;
        p++;
    }
    return true;
}

static void web_to_sd_path(const char* web_path, char* sd_path, size_t sd_size) {
    if(web_path[0] == '\0' || strcmp(web_path, "/") == 0) {
        strncpy(sd_path, "/ext", sd_size);
    } else {
        snprintf(sd_path, sd_size, "/ext%s", web_path);
    }
    sd_path[sd_size - 1] = '\0';
}

static const char* path_filename(const char* path) {
    const char* last = strrchr(path, '/');
    return last ? last + 1 : path;
}

static void handle_request(
    uint8_t sn,
    const char* method,
    const char* raw_uri,
    uint8_t* buf,
    uint16_t buf_size,
    int32_t body_pre_read,
    FileManagerState* state) {
    char uri[FILEMGR_PATH_MAX];
    url_decode(uri, raw_uri, sizeof(uri));

    /* Reject path traversal attempts */
    if(!path_is_safe(uri)) {
        http_send_str(
            sn,
            "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n"
            "Content-Length: 10\r\n\r\nForbidden\n");
        http_flush(sn);
        return;
    }

    char sd_path[FILEMGR_PATH_MAX];
    char web_path[FILEMGR_PATH_MAX];

    state->requests_served++;

    if(strcmp(uri, "/") == 0 || strcmp(uri, "") == 0) {
        char redir[32];
        snprintf(redir, sizeof(redir), "/browse/?t=%s", state->auth_token);
        http_send_str(
            sn,
            "HTTP/1.1 303 See Other\r\nConnection: close\r\n"
            "Content-Length: 0\r\nLocation: ");
        http_send_str(sn, redir);
        http_send_str(sn, "\r\n\r\n");
        http_flush(sn);
        return;
    }

    if(strncmp(uri, "/browse", 7) == 0) {
        const char* path = uri + 7;
        if(path[0] == '\0') path = "/";
        strncpy(web_path, path, sizeof(web_path));
        web_path[sizeof(web_path) - 1] = '\0';
        web_to_sd_path(web_path, sd_path, sizeof(sd_path));

        /* Check if directory can be opened (storage_dir_exists unreliable for /ext) */
        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* dir_check = storage_file_alloc(storage);
        bool is_dir = storage_dir_open(dir_check, sd_path);
        if(is_dir) storage_dir_close(dir_check);
        storage_file_free(dir_check);
        furi_record_close(RECORD_STORAGE);

        if(is_dir) {
            handle_list_dir(sn, sd_path, web_path, state->auth_token, state);
        } else {
            http_send_str(
                sn,
                "HTTP/1.1 404 Not Found\r\nConnection: close\r\n"
                "Content-Length: 10\r\n\r\nNot found\n");
            http_flush(sn);
        }
        return;
    }

    if(strncmp(uri, "/download", 9) == 0) {
        const char* path = uri + 9;
        strncpy(web_path, path, sizeof(web_path));
        web_path[sizeof(web_path) - 1] = '\0';
        web_to_sd_path(web_path, sd_path, sizeof(sd_path));
        handle_download(sn, sd_path, path_filename(web_path), buf, buf_size, state);
        return;
    }

    if(strncmp(uri, "/delete", 7) == 0) {
        const char* path = uri + 7;
        strncpy(web_path, path, sizeof(web_path));
        web_path[sizeof(web_path) - 1] = '\0';
        web_to_sd_path(web_path, sd_path, sizeof(sd_path));

        char parent[FILEMGR_PATH_MAX];
        get_parent_path(web_path, parent, sizeof(parent));
        handle_delete(sn, sd_path, parent, state->auth_token);
        return;
    }

    if(strncmp(uri, "/upload", 7) == 0 && strcmp(method, "POST") == 0) {
        const char* path = uri + 7;
        if(path[0] == '\0') path = "/";
        strncpy(web_path, path, sizeof(web_path));
        web_path[sizeof(web_path) - 1] = '\0';
        web_to_sd_path(web_path, sd_path, sizeof(sd_path));
        handle_upload(sn, sd_path, web_path, buf, buf_size, body_pre_read, state);
        return;
    }

    if(strncmp(uri, "/mkdir", 6) == 0 && strcmp(method, "POST") == 0) {
        const char* path = uri + 6;
        if(path[0] == '\0') path = "/";
        strncpy(web_path, path, sizeof(web_path));
        web_path[sizeof(web_path) - 1] = '\0';
        web_to_sd_path(web_path, sd_path, sizeof(sd_path));
        handle_mkdir(sn, sd_path, web_path, buf, buf_size, body_pre_read, state->auth_token);
        return;
    }

    /* 404 */
    http_send_str(
        sn,
        "HTTP/1.1 404 Not Found\r\nConnection: close\r\n"
        "Content-Length: 10\r\n\r\nNot found\n");
    http_flush(sn);
}

/* ==================== TCP connection handling ==================== */

static void
    handle_connection(uint8_t sn, uint8_t* buf, uint16_t buf_size, FileManagerState* state) {
    int32_t total_read = 0;
    uint32_t start = furi_get_tick();
    bool headers_complete = false;

    while(furi_get_tick() - start < 5000 && state->running && !headers_complete) {
        int32_t avail = buf_size - total_read - 1;
        if(avail <= 0) break;

        int32_t len = recv(sn, buf + total_read, avail);
        if(len > 0) {
            total_read += len;
            buf[total_read] = '\0';

            if(strstr((char*)buf, "\r\n\r\n")) {
                headers_complete = true;
            }
            start = furi_get_tick();
        } else {
            furi_delay_ms(5);
        }
    }

    if(total_read <= 0) return;
    buf[total_read] = '\0';

    char method[8] = {0};
    char uri[FILEMGR_PATH_MAX] = {0};

    char* space1 = strchr((char*)buf, ' ');
    if(!space1) return;

    size_t method_len = space1 - (char*)buf;
    if(method_len >= sizeof(method)) method_len = sizeof(method) - 1;
    memcpy(method, buf, method_len);
    method[method_len] = '\0';

    char* uri_start = space1 + 1;
    char* space2 = strchr(uri_start, ' ');
    if(!space2) return;

    size_t uri_len = space2 - uri_start;
    if(uri_len >= sizeof(uri)) uri_len = sizeof(uri) - 1;
    memcpy(uri, uri_start, uri_len);
    uri[uri_len] = '\0';

    /* Extract auth token from query string before stripping it */
    char* query = strchr(uri, '?');
    bool token_ok = false;
    if(query) {
        char* t_param = strstr(query, "t=");
        if(t_param) {
            t_param += 2;
            if(strncmp(t_param, state->auth_token, FILEMGR_TOKEN_LEN) == 0) {
                token_ok = true;
            }
        }
        *query = '\0'; /* Strip query from URI */
    }

    /* Validate auth token (required on all requests) */
    if(!token_ok) {
        http_send_str(
            sn,
            "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n"
            "Content-Type: text/plain\r\nContent-Length: 42\r\n\r\n"
            "Access denied. Use URL with ?t=<token>.\n");
        http_flush(sn);
        return;
    }

    FURI_LOG_I(TAG, "%s %s", method, uri);

    /* For POST: shift body data to start of buf and track how much we have */
    int32_t body_pre_read = 0;
    if(strcmp(method, "POST") == 0) {
        char* body = strstr((char*)buf, "\r\n\r\n");
        if(body) {
            body += 4;
            body_pre_read = total_read - (int32_t)(body - (char*)buf);
            if(body_pre_read > 0) {
                memmove(buf, body, body_pre_read);
            } else {
                body_pre_read = 0;
            }
        }
    }

    handle_request(sn, method, uri, buf, buf_size, body_pre_read, state);
}

/* ==================== Public API ==================== */

bool file_manager_start(FileManagerState* state) {
    memset(state, 0, sizeof(FileManagerState));
    strncpy(state->current_path, "/ext", sizeof(state->current_path));
    state->running = true;

    /* Generate random 4-digit numeric auth token */
    static const char digits[] = "0123456789";
    uint8_t rnd[4];
    furi_hal_random_fill_buf(rnd, 4);
    state->auth_token[0] = digits[rnd[0] % 10];
    state->auth_token[1] = digits[rnd[1] % 10];
    state->auth_token[2] = digits[rnd[2] % 10];
    state->auth_token[3] = digits[rnd[3] % 10];
    state->auth_token[4] = '\0';
    FURI_LOG_I(TAG, "File Manager token: %s", state->auth_token);

    /* Check for custom CSS/JS on SD (once at startup) */
    Storage* st = furi_record_open(RECORD_STORAGE);
    state->has_custom_css =
        (storage_common_stat(st, APP_DATA_PATH("web/custom.css"), NULL) == FSE_OK);
    state->has_custom_js =
        (storage_common_stat(st, APP_DATA_PATH("web/custom.js"), NULL) == FSE_OK);
    furi_record_close(RECORD_STORAGE);

    int8_t ret = socket(FILEMGR_HTTP_SOCKET, Sn_MR_TCP, FILEMGR_HTTP_PORT, 0);
    if(ret != FILEMGR_HTTP_SOCKET) {
        FURI_LOG_E(TAG, "Failed to open socket %d (ret=%d)", FILEMGR_HTTP_SOCKET, ret);
        return false;
    }

    ret = listen(FILEMGR_HTTP_SOCKET);
    if(ret != SOCK_OK) {
        FURI_LOG_E(TAG, "Failed to listen on socket %d", FILEMGR_HTTP_SOCKET);
        close(FILEMGR_HTTP_SOCKET);
        return false;
    }

    FURI_LOG_I(TAG, "HTTP server listening on port %d", FILEMGR_HTTP_PORT);
    return true;
}

void file_manager_poll(FileManagerState* state, uint8_t* buf, uint16_t buf_size) {
    uint8_t status = getSn_SR(FILEMGR_HTTP_SOCKET);

    switch(status) {
    case SOCK_ESTABLISHED:
        handle_connection(FILEMGR_HTTP_SOCKET, buf, buf_size, state);
        /* Graceful close: FIN lets the browser read the full response.
         * RST (close) would discard unread data in the browser's buffer. */
        disconnect(FILEMGR_HTTP_SOCKET);
        /* Brief wait for FIN handshake (typically <5ms on LAN) */
        {
            uint32_t dstart = furi_get_tick();
            while(furi_get_tick() - dstart < 100) {
                uint8_t dsr = getSn_SR(FILEMGR_HTTP_SOCKET);
                if(dsr == SOCK_CLOSED) break;
                furi_delay_ms(1);
            }
        }
        /* Force close if FIN handshake didn't complete in time */
        if(getSn_SR(FILEMGR_HTTP_SOCKET) != SOCK_CLOSED) {
            close(FILEMGR_HTTP_SOCKET);
        }
        socket(FILEMGR_HTTP_SOCKET, Sn_MR_TCP, FILEMGR_HTTP_PORT, 0);
        listen(FILEMGR_HTTP_SOCKET);
        break;

    case SOCK_CLOSE_WAIT:
        disconnect(FILEMGR_HTTP_SOCKET);
        break;

    case SOCK_CLOSED:
        socket(FILEMGR_HTTP_SOCKET, Sn_MR_TCP, FILEMGR_HTTP_PORT, 0);
        listen(FILEMGR_HTTP_SOCKET);
        break;

    case SOCK_LISTEN:
        break;

    default:
        /* Socket stuck in transition (FIN_WAIT, TIME_WAIT, etc.)
         * — force reset and re-listen */
        close(FILEMGR_HTTP_SOCKET);
        socket(FILEMGR_HTTP_SOCKET, Sn_MR_TCP, FILEMGR_HTTP_PORT, 0);
        listen(FILEMGR_HTTP_SOCKET);
        break;
    }

    furi_delay_ms(10);
}

void file_manager_stop(FileManagerState* state) {
    state->running = false;
    close(FILEMGR_HTTP_SOCKET);
    FURI_LOG_I(TAG, "HTTP server stopped");
}
