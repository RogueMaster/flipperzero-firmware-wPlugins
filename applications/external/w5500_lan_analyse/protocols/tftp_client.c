#include "tftp_client.h"

#include <furi.h>
#include <socket.h>
#include <storage/storage.h>
#include <string.h>

#define TFTP_SOCK        3
#define TFTP_SERVER_PORT 69
#define TFTP_LOCAL_PORT  16900
#define TFTP_TIMEOUT_MS  5000
#define TFTP_BLOCK_SIZE  512

#define TFTP_HELP_PATH APP_DATA_PATH("tftp/help.txt")

/* Write help.txt on first TFTP use. Text lives in flash (static const),
 * file existence check is a single syscall — near-zero cost. */
static void tftp_ensure_help_file(Storage* storage) {
    if(storage_common_stat(storage, TFTP_HELP_PATH, NULL) == FSE_OK) return;

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, TFTP_HELP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        static const char help[] =
            "PXE Boot & TFTP — LAN Tester for Flipper Zero\n"
            "\n"
            "== PXE Server Setup ==\n"
            "\n"
            "1. Place boot file on SD card:\n"
            "   /ext/apps_data/lan_tester/pxe/\n"
            "\n"
            "   Supported formats:\n"
            "   .kpxe  — Legacy BIOS (PXE)\n"
            "   .efi   — UEFI boot\n"
            "   .pxe   — PXE bootstrap\n"
            "   .0     — PXELinux\n"
            "\n"
            "   Recommended: undionly.kpxe (~70 KB)\n"
            "   You can download boot files from PXE\n"
            "   settings menu: Download Files.\n"
            "\n"
            "2. Connect W5500 SPI module to Flipper Zero\n"
            "3. Connect RJ45 cable to target machine\n"
            "4. Open LAN Tester > Utilities > PXE Server\n"
            "\n"
            "== PXE Modes ==\n"
            "\n"
            "DHCP ON (default):\n"
            "  Flipper assigns IP to client and provides\n"
            "  TFTP server address + boot filename via\n"
            "  DHCP options 66/67. Use with direct cable.\n"
            "\n"
            "DHCP OFF (TFTP only):\n"
            "  Flipper only serves files via TFTP.\n"
            "  Use when external DHCP is available or\n"
            "  target has a static IP configured.\n"
            "\n"
            "== Default Network ==\n"
            "\n"
            "Server IP:  192.168.77.1\n"
            "Client IP:  192.168.77.10\n"
            "Subnet:     255.255.255.0\n"
            "All addresses are configurable in settings.\n"
            "\n"
            "== Target BIOS/UEFI ==\n"
            "\n"
            "Enable Network/PXE Boot in BIOS settings.\n"
            "Set boot order to Network first.\n"
            "\n"
            "== TFTP Client ==\n"
            "\n"
            "Downloaded files are saved to this directory:\n"
            "/ext/apps_data/lan_tester/tftp/\n"
            "\n"
            "== Links ==\n"
            "\n"
            "Project:  https://github.com/dok2d/fz-W5500-lan-analyse\n"
            "iPXE:     https://boot.ipxe.org\n"
            "netboot:  https://boot.netboot.xyz\n"
            "W5500:    https://docs.wiznet.io/Product/iEthernet/W5500/overview\n";
        storage_file_write(file, help, sizeof(help) - 1);
        storage_file_close(file);
    }
    storage_file_free(file);
}

/* TFTP opcodes */
#define TFTP_OP_RRQ   1
#define TFTP_OP_DATA  3
#define TFTP_OP_ACK   4
#define TFTP_OP_ERROR 5

static void write_u16_be(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

static uint16_t read_u16_be(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

/**
 * Build TFTP Read Request (RRQ) packet.
 */
static uint16_t tftp_build_rrq(uint8_t* pkt, uint16_t pkt_size, const char* filename) {
    uint16_t fname_len = (uint16_t)strlen(filename);
    const char* mode = "octet";
    uint16_t mode_len = 5;

    uint16_t total = 2 + fname_len + 1 + mode_len + 1;
    if(total > pkt_size) return 0;

    uint16_t idx = 0;
    write_u16_be(&pkt[idx], TFTP_OP_RRQ);
    idx += 2;
    memcpy(&pkt[idx], filename, fname_len);
    idx += fname_len;
    pkt[idx++] = 0;
    memcpy(&pkt[idx], mode, mode_len);
    idx += mode_len;
    pkt[idx++] = 0;

    return idx;
}

/**
 * Build TFTP ACK packet.
 */
static uint16_t tftp_build_ack(uint8_t* pkt, uint16_t block_num) {
    write_u16_be(&pkt[0], TFTP_OP_ACK);
    write_u16_be(&pkt[2], block_num);
    return 4;
}

bool tftp_client_get(
    const uint8_t server_ip[4],
    const char* filename,
    const char* save_path,
    TftpClientResult* result,
    volatile bool* running) {
    memset(result, 0, sizeof(TftpClientResult));
    strncpy(result->save_path, save_path, sizeof(result->save_path) - 1);

    close(TFTP_SOCK);
    if(socket(TFTP_SOCK, Sn_MR_UDP, TFTP_LOCAL_PORT, 0) != TFTP_SOCK) {
        strncpy(result->error_msg, "Socket open failed", sizeof(result->error_msg));
        return false;
    }

    /* Open file for writing */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH("tftp"));
    tftp_ensure_help_file(storage);
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, save_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        strncpy(result->error_msg, "Cannot create file", sizeof(result->error_msg));
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        close(TFTP_SOCK);
        return false;
    }

    /* Send RRQ */
    uint8_t* pkt = malloc(600);
    if(!pkt) {
        strncpy(result->error_msg, "Memory alloc failed", sizeof(result->error_msg));
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        close(TFTP_SOCK);
        return false;
    }
    uint16_t pkt_len = tftp_build_rrq(pkt, 600, filename);
    if(pkt_len == 0) {
        strncpy(result->error_msg, "Filename too long", sizeof(result->error_msg));
        free(pkt);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        close(TFTP_SOCK);
        return false;
    }

    sendto(TFTP_SOCK, pkt, pkt_len, (uint8_t*)server_ip, TFTP_SERVER_PORT);

    /* Receive data blocks */
    uint16_t expected_block = 1;
    uint16_t server_tid = 0; /* server's transfer ID (ephemeral port) */
    bool first_block = true;

    while(*running) {
        uint32_t block_start = furi_get_tick();
        bool got_data = false;

        while((furi_get_tick() - block_start) < TFTP_TIMEOUT_MS && *running) {
            uint16_t rx_len = getSn_RX_RSR(TFTP_SOCK);
            if(rx_len > 0) {
                uint8_t from_ip[4];
                uint16_t from_port;
                int32_t recv_len = recvfrom(TFTP_SOCK, pkt, sizeof(pkt), from_ip, &from_port);
                if(recv_len < 4) continue;

                uint16_t opcode = read_u16_be(&pkt[0]);

                if(opcode == TFTP_OP_ERROR) {
                    uint16_t err_code = read_u16_be(&pkt[2]);
                    if(recv_len > 4) {
                        uint16_t msg_len = (uint16_t)(recv_len - 4);
                        if(msg_len > sizeof(result->error_msg) - 1)
                            msg_len = sizeof(result->error_msg) - 1;
                        memcpy(result->error_msg, &pkt[4], msg_len);
                        result->error_msg[msg_len] = '\0';
                    } else {
                        snprintf(
                            result->error_msg,
                            sizeof(result->error_msg),
                            "TFTP error %d",
                            err_code);
                    }
                    result->errors++;
                    goto done;
                }

                if(opcode == TFTP_OP_DATA) {
                    uint16_t block_num = read_u16_be(&pkt[2]);
                    if(first_block) {
                        server_tid = from_port;
                        first_block = false;
                    }

                    if(block_num == expected_block) {
                        uint16_t data_len = (uint16_t)(recv_len - 4);

                        /* Write to file */
                        if(data_len > 0) {
                            storage_file_write(file, &pkt[4], data_len);
                        }

                        result->bytes_received += data_len;
                        result->blocks_received++;

                        /* Send ACK */
                        uint16_t ack_len = tftp_build_ack(pkt, block_num);
                        sendto(TFTP_SOCK, pkt, ack_len, from_ip, server_tid);

                        expected_block++;
                        got_data = true;

                        /* Last block if data < 512 bytes */
                        if(data_len < TFTP_BLOCK_SIZE) {
                            result->success = true;
                            result->saved_to_sd = true;
                            goto done;
                        }
                        break; /* Wait for next block */
                    }
                }
            }
            furi_delay_ms(5);
        }

        if(!got_data) {
            strncpy(result->error_msg, "Timeout waiting for data", sizeof(result->error_msg));
            result->errors++;
            goto done;
        }
    }

done:
    free(pkt);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    close(TFTP_SOCK);

    if(!result->success && result->bytes_received == 0) {
        /* Clean up empty file */
        Storage* st = furi_record_open(RECORD_STORAGE);
        storage_simply_remove(st, save_path);
        furi_record_close(RECORD_STORAGE);
        result->saved_to_sd = false;
    }

    return result->success;
}
