#include "pxe_server.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <socket.h>
#include <wizchip_conf.h>

#include <string.h>
#include <stdio.h>

#define TAG "PXE"

/* Timeout for external DHCP detection (ms) */
#define PXE_DHCP_DETECT_TIMEOUT_MS 5000

/* ==================== Helpers ==================== */

static uint16_t
    pxe_build_tftp_data(uint8_t* buf, uint16_t block_num, const uint8_t* data, uint16_t data_len) {
    buf[0] = 0;
    buf[1] = TFTP_OP_DATA;
    buf[2] = (block_num >> 8) & 0xFF;
    buf[3] = block_num & 0xFF;
    memcpy(buf + 4, data, data_len);
    return 4 + data_len;
}

static uint16_t pxe_build_tftp_error(uint8_t* buf, uint16_t code, const char* msg) {
    buf[0] = 0;
    buf[1] = TFTP_OP_ERROR;
    buf[2] = (code >> 8) & 0xFF;
    buf[3] = code & 0xFF;
    uint16_t len = strlen(msg);
    memcpy(buf + 4, msg, len);
    buf[4 + len] = 0;
    return 5 + len;
}

/* ==================== DHCP server helpers ==================== */

/* Build DHCP reply packet (Offer or ACK).
 * Returns total packet length. */
static uint16_t pxe_build_dhcp_reply(
    uint8_t* buf,
    const PxeConfig* config,
    uint8_t type, /* DHCP_OFFER=2 or DHCP_ACK=5 */
    uint32_t xid,
    const uint8_t client_mac[6],
    const char* boot_file) {
    memset(buf, 0, 576); /* Minimum DHCP packet size */

    /* Fixed header */
    buf[0] = 2; /* op: BOOTREPLY */
    buf[1] = 1; /* htype: Ethernet */
    buf[2] = 6; /* hlen: MAC length */
    buf[3] = 0; /* hops */

    /* xid (offset 4) */
    buf[4] = (xid >> 24) & 0xFF;
    buf[5] = (xid >> 16) & 0xFF;
    buf[6] = (xid >> 8) & 0xFF;
    buf[7] = xid & 0xFF;

    /* secs=0, flags=0x8000 (broadcast) at offset 8 */
    buf[10] = 0x80;
    buf[11] = 0x00;

    /* ciaddr = 0.0.0.0 (offset 12) — already zeroed */

    /* yiaddr = client IP (offset 16) */
    memcpy(buf + 16, config->client_ip, 4);

    /* siaddr = server IP / TFTP server (offset 20) */
    memcpy(buf + 20, config->server_ip, 4);

    /* giaddr = 0.0.0.0 (offset 24) — already zeroed */

    /* chaddr = client MAC (offset 28, 16 bytes total, padded) */
    memcpy(buf + 28, client_mac, 6);

    /* sname = "FlipperPXE" (offset 44, 64 bytes) */
    strncpy((char*)(buf + 44), "FlipperPXE", 64);

    /* file = boot filename (offset 108, 128 bytes) */
    if(boot_file) {
        strncpy((char*)(buf + 108), boot_file, 128);
    }

    /* Magic cookie (offset 236) */
    buf[236] = 0x63;
    buf[237] = 0x82;
    buf[238] = 0x53;
    buf[239] = 0x63;

    /* DHCP Options (starting at offset 240) */
    uint16_t off = 240;

    /* Option 53: DHCP Message Type */
    buf[off++] = 53;
    buf[off++] = 1;
    buf[off++] = type;

    /* Option 1: Subnet Mask */
    buf[off++] = 1;
    buf[off++] = 4;
    memcpy(buf + off, config->subnet, 4);
    off += 4;

    /* Option 3: Router */
    buf[off++] = 3;
    buf[off++] = 4;
    memcpy(buf + off, config->server_ip, 4);
    off += 4;

    /* Option 6: DNS Server */
    buf[off++] = 6;
    buf[off++] = 4;
    memcpy(buf + off, config->server_ip, 4);
    off += 4;

    /* Option 51: Lease Time (3600 seconds) */
    buf[off++] = 51;
    buf[off++] = 4;
    buf[off++] = 0x00;
    buf[off++] = 0x00;
    buf[off++] = 0x0E;
    buf[off++] = 0x10;

    /* Option 54: Server Identifier */
    buf[off++] = 54;
    buf[off++] = 4;
    memcpy(buf + off, config->server_ip, 4);
    off += 4;

    /* Option 66: TFTP Server Name (IP as string) */
    char ip_str[16];
    snprintf(
        ip_str,
        sizeof(ip_str),
        "%d.%d.%d.%d",
        config->server_ip[0],
        config->server_ip[1],
        config->server_ip[2],
        config->server_ip[3]);
    uint8_t ip_str_len = strlen(ip_str);
    buf[off++] = 66;
    buf[off++] = ip_str_len;
    memcpy(buf + off, ip_str, ip_str_len);
    off += ip_str_len;

    /* Option 67: Bootfile Name */
    if(boot_file) {
        uint8_t bf_len = strlen(boot_file);
        buf[off++] = 67;
        buf[off++] = bf_len;
        memcpy(buf + off, boot_file, bf_len);
        off += bf_len;
    }

    /* Option 255: End */
    buf[off++] = 255;

    /* Pad to minimum 300 bytes (BOOTP minimum) */
    if(off < 300) off = 300;

    return off;
}

/* ==================== DHCP server handler ==================== */

static void pxe_dhcp_handle(PxeServerState* state, uint8_t* buf, uint16_t buf_size) {
    int16_t len = getSn_RX_RSR(PXE_DHCP_SOCKET);
    if(len <= 0) return;

    uint8_t client_ip[4];
    uint16_t client_port;
    len = recvfrom(PXE_DHCP_SOCKET, buf, buf_size, client_ip, &client_port);
    if(len < 240) return; /* Too short for DHCP */

    /* Verify BOOTP request (op=1) */
    if(buf[0] != 1) return;

    /* Verify magic cookie at offset 236 */
    if(buf[236] != 0x63 || buf[237] != 0x82 || buf[238] != 0x53 || buf[239] != 0x63) return;

    /* Extract xid (offset 4, big endian) */
    uint32_t xid = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) | ((uint32_t)buf[6] << 8) |
                   buf[7];

    /* Extract client MAC (offset 28, 6 bytes) */
    uint8_t cmac[6];
    memcpy(cmac, buf + 28, 6);

    /* Find Option 53 (DHCP Message Type) in options starting at offset 240 */
    uint8_t msg_type = 0;
    uint16_t opt_off = 240;
    while(opt_off < (uint16_t)len && buf[opt_off] != 255) {
        uint8_t opt = buf[opt_off++];
        if(opt == 0) continue; /* Padding */
        if(opt_off >= (uint16_t)len) break;
        uint8_t opt_len = buf[opt_off++];
        if(opt == 53 && opt_len >= 1) {
            msg_type = buf[opt_off];
        }
        opt_off += opt_len;
    }

    if(msg_type == 0) return;

    FURI_LOG_I(
        TAG,
        "DHCP msg type=%d from %02X:%02X:%02X:%02X:%02X:%02X",
        msg_type,
        cmac[0],
        cmac[1],
        cmac[2],
        cmac[3],
        cmac[4],
        cmac[5]);

    /* Save client MAC for display */
    memcpy(state->client_mac, cmac, 6);
    state->client_seen = true;

    uint8_t bcast[4] = {255, 255, 255, 255};

    if(msg_type == DHCP_DISCOVER) {
        state->dhcp_discovers++;
        uint16_t pkt_len =
            pxe_build_dhcp_reply(buf, &state->config, DHCP_OFFER, xid, cmac, state->boot_filename);
        sendto(PXE_DHCP_SOCKET, buf, pkt_len, bcast, DHCP_CLIENT_PORT);
        state->state = PxeStateDhcpOfferSent;
        FURI_LOG_I(
            TAG,
            "Sent DHCP Offer to %02X:%02X:%02X:%02X:%02X:%02X",
            cmac[0],
            cmac[1],
            cmac[2],
            cmac[3],
            cmac[4],
            cmac[5]);

    } else if(msg_type == DHCP_REQUEST) {
        state->dhcp_requests++;

        /* If TFTP transfer is active from a previous client, reset it */
        if(state->tftp.active) {
            FURI_LOG_I(TAG, "New DHCP Request during TFTP — resetting transfer");
            if(state->tftp.file) {
                storage_file_close(state->tftp.file);
                storage_file_free(state->tftp.file);
                state->tftp.file = NULL;
            }
            if(state->tftp.storage) {
                furi_record_close(RECORD_STORAGE);
                state->tftp.storage = NULL;
            }
            close(PXE_TFTP_DATA_SOCKET);
            memset(&state->tftp, 0, sizeof(state->tftp));
        }

        uint16_t pkt_len =
            pxe_build_dhcp_reply(buf, &state->config, DHCP_ACK, xid, cmac, state->boot_filename);
        sendto(PXE_DHCP_SOCKET, buf, pkt_len, bcast, DHCP_CLIENT_PORT);
        state->state = PxeStateDhcpAckSent;
        FURI_LOG_I(TAG, "Sent DHCP ACK");
    }
    /* Ignore all other message types (Release, Inform, etc.) */
}

/* ==================== TFTP server handler ==================== */

/* Send current TFTP block from file */
static bool pxe_tftp_send_block(PxeServerState* state, uint8_t* buf) {
    /* Seek to correct position */
    uint32_t offset = (uint32_t)(state->tftp.block_num - 1) * TFTP_BLOCK_SIZE;
    if(!storage_file_seek(state->tftp.file, offset, true)) {
        FURI_LOG_E(TAG, "TFTP seek failed to offset %lu", offset);
        return false;
    }

    /* Read data */
    uint8_t data[TFTP_BLOCK_SIZE];
    uint16_t bytes_read = storage_file_read(state->tftp.file, data, TFTP_BLOCK_SIZE);

    /* Build and send DATA packet */
    uint16_t pkt_len = pxe_build_tftp_data(buf, state->tftp.block_num, data, bytes_read);
    sendto(PXE_TFTP_DATA_SOCKET, buf, pkt_len, state->tftp.client_ip, state->tftp.client_port);

    state->tftp.last_block_size = bytes_read;
    state->tftp.last_send_tick = furi_get_tick();

    FURI_LOG_D(TAG, "TFTP DATA blk=%d len=%d", state->tftp.block_num, bytes_read);
    return true;
}

static void pxe_tftp_handle(PxeServerState* state, uint8_t* buf, uint16_t buf_size) {
    /* 1. Check listen socket (port 69) for new RRQ/WRQ */
    int16_t len = getSn_RX_RSR(PXE_TFTP_SOCKET);
    if(len > 0) {
        uint8_t req_ip[4];
        uint16_t req_port;
        len = recvfrom(PXE_TFTP_SOCKET, buf, buf_size, req_ip, &req_port);
        if(len >= 4) {
            uint16_t opcode = ((uint16_t)buf[0] << 8) | buf[1];

            if(opcode == TFTP_OP_WRQ) {
                /* Write not supported */
                uint16_t err_len = pxe_build_tftp_error(buf, TFTP_ERR_ACCESS, "Access violation");
                sendto(PXE_TFTP_SOCKET, buf, err_len, req_ip, req_port);
                state->tftp_errors++;
                FURI_LOG_I(TAG, "TFTP WRQ rejected");

            } else if(opcode == TFTP_OP_RRQ && state->tftp.active) {
                /* Already busy */
                uint16_t err_len = pxe_build_tftp_error(buf, TFTP_ERR_UNDEFINED, "Server busy");
                sendto(PXE_TFTP_SOCKET, buf, err_len, req_ip, req_port);
                state->tftp_errors++;
                FURI_LOG_I(TAG, "TFTP RRQ rejected (busy)");

            } else if(opcode == TFTP_OP_RRQ) {
                /* New read request */
                state->tftp_requests++;
                char* filename = (char*)(buf + 2);
                /* Reject path traversal attempts */
                bool safe = true;
                for(const char* p = filename; *p; p++) {
                    if(p[0] == '.' && p[1] == '.') {
                        safe = false;
                        break;
                    }
                    if(p[0] == '/') {
                        safe = false;
                        break;
                    }
                }
                if(!safe) {
                    uint16_t err_len = pxe_build_tftp_error(buf, TFTP_ERR_ACCESS, "Access denied");
                    sendto(PXE_TFTP_SOCKET, buf, err_len, req_ip, req_port);
                    state->tftp_errors++;
                    FURI_LOG_W(TAG, "TFTP path traversal rejected: %s", filename);
                } else {
                    FURI_LOG_I(
                        TAG,
                        "TFTP RRQ: %s from %d.%d.%d.%d:%d",
                        filename,
                        req_ip[0],
                        req_ip[1],
                        req_ip[2],
                        req_ip[3],
                        req_port);

                    /* Build file path */
                    char filepath[128];
                    snprintf(filepath, sizeof(filepath), "%s/%s", PXE_BOOT_DIR, filename);

                    /* Open file from SD */
                    Storage* storage = furi_record_open(RECORD_STORAGE);
                    File* file = storage_file_alloc(storage);
                    if(!storage_file_open(file, filepath, FSAM_READ, FSOM_OPEN_EXISTING)) {
                        FURI_LOG_E(TAG, "TFTP file not found: %s", filepath);
                        storage_file_free(file);
                        furi_record_close(RECORD_STORAGE);

                        uint16_t err_len =
                            pxe_build_tftp_error(buf, TFTP_ERR_NOT_FOUND, "File not found");
                        sendto(PXE_TFTP_SOCKET, buf, err_len, req_ip, req_port);
                        state->tftp_errors++;
                    } else {
                        /* Set up transfer session */
                        memcpy(state->tftp.client_ip, req_ip, 4);
                        state->tftp.client_port = req_port;
                        state->tftp.block_num = 1;
                        state->tftp.file_size = (uint32_t)storage_file_size(file);
                        state->tftp.bytes_sent = 0;
                        state->tftp.last_block_size = 0;
                        state->tftp.retries = 0;
                        state->tftp.active = true;
                        state->tftp.file = file;
                        state->tftp.storage = storage;

                        /* Save client info for display (if not already from DHCP) */
                        if(!state->client_seen) {
                            memcpy(
                                state->client_mac,
                                req_ip,
                                4); /* store IP in mac for non-DHCP display */
                            state->client_seen = true;
                        }

                        /* Open data socket on dynamic port */
                        close(PXE_TFTP_DATA_SOCKET);
                        socket(PXE_TFTP_DATA_SOCKET, Sn_MR_UDP, TFTP_DATA_PORT_BASE, 0);

                        /* Send first block */
                        state->state = PxeStateTftpTransfer;
                        pxe_tftp_send_block(state, buf);

                        FURI_LOG_I(
                            TAG,
                            "TFTP transfer started: %s (%lu bytes)",
                            filename,
                            state->tftp.file_size);
                    }
                } /* end else (safe path) */
            }
        }
    }

    /* 2. If transfer active — check data socket for ACKs */
    if(state->tftp.active) {
        len = getSn_RX_RSR(PXE_TFTP_DATA_SOCKET);
        if(len > 0) {
            uint8_t ack_ip[4];
            uint16_t ack_port;
            len = recvfrom(PXE_TFTP_DATA_SOCKET, buf, buf_size, ack_ip, &ack_port);
            if(len >= 4) {
                uint16_t opcode = ((uint16_t)buf[0] << 8) | buf[1];
                uint16_t ack_block = ((uint16_t)buf[2] << 8) | buf[3];

                if(opcode == TFTP_OP_ACK && ack_block == state->tftp.block_num) {
                    /* Correct ACK received */
                    state->tftp.bytes_sent += state->tftp.last_block_size;
                    state->tftp_blocks_sent++;
                    state->tftp.retries = 0;

                    if(state->tftp.last_block_size < TFTP_BLOCK_SIZE) {
                        /* Transfer complete (last block was < 512 bytes) */
                        FURI_LOG_I(
                            TAG,
                            "TFTP transfer complete: %lu bytes in %lu blocks",
                            state->tftp.bytes_sent,
                            state->tftp_blocks_sent);

                        state->state = PxeStateDone;
                        state->tftp.active = false;

                        /* Close file and storage */
                        storage_file_close(state->tftp.file);
                        storage_file_free(state->tftp.file);
                        state->tftp.file = NULL;
                        furi_record_close(RECORD_STORAGE);
                        state->tftp.storage = NULL;

                        close(PXE_TFTP_DATA_SOCKET);
                    } else {
                        /* Send next block */
                        state->tftp.block_num++;
                        pxe_tftp_send_block(state, buf);
                    }
                }
                /* Ignore duplicate or unexpected ACKs */
            }
        }

        /* 3. Timeout handling — retransmit if no ACK within 3 seconds */
        uint32_t now = furi_get_tick();
        if(state->tftp.active && (now - state->tftp.last_send_tick) > TFTP_TIMEOUT_MS) {
            if(state->tftp.retries < TFTP_MAX_RETRIES) {
                state->tftp.retries++;
                FURI_LOG_I(
                    TAG,
                    "TFTP retransmit blk=%d retry=%d",
                    state->tftp.block_num,
                    state->tftp.retries);
                pxe_tftp_send_block(state, buf);
            } else {
                /* Max retries exceeded — abort */
                FURI_LOG_E(TAG, "TFTP transfer aborted after %d retries", TFTP_MAX_RETRIES);
                state->state = PxeStateError;
                state->tftp.active = false;
                state->tftp_errors++;

                if(state->tftp.file) {
                    storage_file_close(state->tftp.file);
                    storage_file_free(state->tftp.file);
                    state->tftp.file = NULL;
                }
                if(state->tftp.storage) {
                    furi_record_close(RECORD_STORAGE);
                    state->tftp.storage = NULL;
                }
                close(PXE_TFTP_DATA_SOCKET);
            }
        }
    }
}

/* ==================== Boot file detection ==================== */

/* Priority list for auto-detection */
static const char* pxe_preferred_files[] = {
    "undionly.kpxe",
    "ipxe.efi",
    "snponly.efi",
    NULL,
};

/* Valid boot file extensions */
static bool pxe_is_boot_extension(const char* name) {
    uint16_t len = strlen(name);
    if(len < 2) return false;
    const char* ext = NULL;
    for(int i = len - 1; i >= 0; i--) {
        if(name[i] == '.') {
            ext = &name[i];
            break;
        }
    }
    if(!ext) return false;
    return (
        strcmp(ext, ".kpxe") == 0 || strcmp(ext, ".efi") == 0 || strcmp(ext, ".pxe") == 0 ||
        strcmp(ext, ".0") == 0);
}

static void pxe_add_boot_file(PxeServerState* state, const char* name, uint32_t size) {
    if(state->boot_file_count >= PXE_MAX_BOOT_FILES) return;
    /* Skip duplicates */
    for(uint8_t i = 0; i < state->boot_file_count; i++) {
        if(strcmp(state->boot_files[i].filename, name) == 0) return;
    }
    PxeBootFile* bf = &state->boot_files[state->boot_file_count];
    strncpy(bf->filename, name, sizeof(bf->filename) - 1);
    bf->file_size = size;
    state->boot_file_count++;
}

bool pxe_detect_boot_file(PxeServerState* state) {
    state->boot_file_count = 0;
    state->boot_file_found = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PXE_BOOT_DIR);

    /* Check preferred files first (these get priority ordering) */
    for(int i = 0; pxe_preferred_files[i] != NULL; i++) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", PXE_BOOT_DIR, pxe_preferred_files[i]);
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            pxe_add_boot_file(state, pxe_preferred_files[i], (uint32_t)storage_file_size(file));
            storage_file_close(file);
        }
        storage_file_free(file);
    }

    /* Scan directory for all valid boot files */
    File* dir = storage_file_alloc(storage);
    if(storage_dir_open(dir, PXE_BOOT_DIR)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(pxe_is_boot_extension(name)) {
                pxe_add_boot_file(state, name, (uint32_t)info.size);
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    if(state->boot_file_count > 0) {
        /* Select first file as default */
        strncpy(
            state->boot_filename, state->boot_files[0].filename, sizeof(state->boot_filename) - 1);
        state->boot_file_size = state->boot_files[0].file_size;
        state->boot_file_found = true;
        FURI_LOG_I(
            TAG,
            "Found %d boot file(s), selected: %s (%lu bytes)",
            state->boot_file_count,
            state->boot_filename,
            state->boot_file_size);
        return true;
    }

    FURI_LOG_I(TAG, "No boot file found in %s", PXE_BOOT_DIR);
    return false;
}

/* ==================== External DHCP detection ==================== */

bool pxe_detect_external_dhcp(uint8_t socket_num, const uint8_t mac[6], PxeExternalDhcp* result) {
    memset(result, 0, sizeof(PxeExternalDhcp));

    /* Build a minimal DHCP Discover packet */
    uint8_t* pkt = malloc(548);
    if(!pkt) return false;
    memset(pkt, 0, 548);

    uint32_t xid;
    furi_hal_random_fill_buf((uint8_t*)&xid, sizeof(xid));

    /* BOOTP header */
    pkt[0] = 1; /* op: BOOTREQUEST */
    pkt[1] = 1; /* htype: Ethernet */
    pkt[2] = 6; /* hlen */
    pkt[4] = (xid >> 24) & 0xFF;
    pkt[5] = (xid >> 16) & 0xFF;
    pkt[6] = (xid >> 8) & 0xFF;
    pkt[7] = xid & 0xFF;
    pkt[10] = 0x80; /* flags: broadcast */
    memcpy(pkt + 28, mac, 6); /* chaddr */

    /* Magic cookie */
    pkt[236] = 0x63;
    pkt[237] = 0x82;
    pkt[238] = 0x53;
    pkt[239] = 0x63;

    /* Option 53: DHCP Discover */
    pkt[240] = 53;
    pkt[241] = 1;
    pkt[242] = 1;
    /* Option 255: End */
    pkt[243] = 255;

    /* Open UDP socket on port 68 */
    close(socket_num);
    int8_t ret = socket(socket_num, Sn_MR_UDP, DHCP_CLIENT_PORT, 0);
    if(ret != socket_num) {
        free(pkt);
        return false;
    }

    /* Send to broadcast */
    uint8_t bcast[4] = {255, 255, 255, 255};
    int32_t sent = sendto(socket_num, pkt, 300, bcast, DHCP_SERVER_PORT);
    if(sent <= 0) {
        close(socket_num);
        free(pkt);
        return false;
    }

    FURI_LOG_I(TAG, "DHCP Discover sent for network detection (xid=0x%08lX)", (unsigned long)xid);

    /* Wait for Offer */
    uint32_t start = furi_get_tick();
    bool found = false;

    while(furi_get_tick() - start < PXE_DHCP_DETECT_TIMEOUT_MS) {
        uint16_t rx_size = getSn_RX_RSR(socket_num);
        if(rx_size > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t received = recvfrom(socket_num, pkt, 548, from_ip, &from_port);
            if(received >= 240) {
                /* Check op=BOOTREPLY, magic cookie, and xid */
                if(pkt[0] == 2 && pkt[236] == 0x63 && pkt[237] == 0x82 && pkt[238] == 0x53 &&
                   pkt[239] == 0x63) {
                    uint32_t recv_xid = ((uint32_t)pkt[4] << 24) | ((uint32_t)pkt[5] << 16) |
                                        ((uint32_t)pkt[6] << 8) | pkt[7];
                    if(recv_xid == xid) {
                        /* Parse offered IP, server IP, subnet, gateway */
                        memcpy(result->offered_ip, pkt + 16, 4); /* yiaddr */
                        memcpy(result->server_ip, from_ip, 4);

                        /* Parse options for subnet and router */
                        uint16_t opt_off = 240;
                        while(opt_off < (uint16_t)received && pkt[opt_off] != 255) {
                            uint8_t opt = pkt[opt_off++];
                            if(opt == 0) continue;
                            if(opt_off >= (uint16_t)received) break;
                            uint8_t opt_len = pkt[opt_off++];
                            if(opt == 1 && opt_len >= 4) { /* Subnet Mask */
                                memcpy(result->subnet, pkt + opt_off, 4);
                            } else if(opt == 3 && opt_len >= 4) { /* Router */
                                memcpy(result->gateway, pkt + opt_off, 4);
                            }
                            opt_off += opt_len;
                        }

                        result->found = true;
                        found = true;
                        FURI_LOG_I(
                            TAG,
                            "External DHCP detected: server %d.%d.%d.%d, offered %d.%d.%d.%d",
                            from_ip[0],
                            from_ip[1],
                            from_ip[2],
                            from_ip[3],
                            result->offered_ip[0],
                            result->offered_ip[1],
                            result->offered_ip[2],
                            result->offered_ip[3]);
                        break;
                    }
                }
            }
        }
        furi_delay_ms(50);
    }

    close(socket_num);
    free(pkt);

    if(!found) {
        FURI_LOG_I(TAG, "No external DHCP detected within %d ms", PXE_DHCP_DETECT_TIMEOUT_MS);
    }
    return found;
}

/* ==================== Public API ==================== */

bool pxe_server_start(PxeServerState* state) {
    /* Open TFTP listen socket on port 69 */
    close(PXE_TFTP_SOCKET);
    if(socket(PXE_TFTP_SOCKET, Sn_MR_UDP, TFTP_SERVER_PORT, 0) != PXE_TFTP_SOCKET) {
        FURI_LOG_E(TAG, "Failed to open TFTP socket");
        return false;
    }
    FURI_LOG_I(TAG, "TFTP socket opened on port %d", TFTP_SERVER_PORT);

    /* Open DHCP server socket if enabled */
    if(state->config.dhcp_enabled) {
        close(PXE_DHCP_SOCKET);
        if(socket(PXE_DHCP_SOCKET, Sn_MR_UDP, DHCP_SERVER_PORT, 0) != PXE_DHCP_SOCKET) {
            FURI_LOG_E(TAG, "Failed to open DHCP socket");
            close(PXE_TFTP_SOCKET);
            return false;
        }
        FURI_LOG_I(TAG, "DHCP socket opened on port %d", DHCP_SERVER_PORT);
    }

    state->running = true;
    return true;
}

void pxe_server_stop(PxeServerState* state) {
    state->running = false;

    /* Close TFTP transfer if active */
    if(state->tftp.active) {
        if(state->tftp.file) {
            storage_file_close(state->tftp.file);
            storage_file_free(state->tftp.file);
            state->tftp.file = NULL;
        }
        if(state->tftp.storage) {
            furi_record_close(RECORD_STORAGE);
            state->tftp.storage = NULL;
        }
        state->tftp.active = false;
    }

    /* Close sockets */
    close(PXE_TFTP_SOCKET);
    close(PXE_TFTP_DATA_SOCKET);
    if(state->config.dhcp_enabled) {
        close(PXE_DHCP_SOCKET);
    }

    FURI_LOG_I(TAG, "PXE server stopped");
}

void pxe_server_poll(PxeServerState* state, uint8_t* buf, uint16_t buf_size) {
    if(state->config.dhcp_enabled) {
        pxe_dhcp_handle(state, buf, buf_size);
    }
    pxe_tftp_handle(state, buf, buf_size);
}
