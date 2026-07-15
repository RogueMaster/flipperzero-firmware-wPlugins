/*
 * PXE category plugin — PXE boot server + iPXE binary downloader.
 *
 * Bundles protocols/pxe_server.c (DHCP/TFTP boot server) and
 * protocols/http_download.c (iPXE HTTP downloader) so their code is resident in
 * RAM only while a PXE tool runs, instead of living in the host binary. Both
 * reach the W5500 through the socket shim (api/lan_tester_ioshim.h).
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../protocols/pxe_server.h"
#include "../protocols/http_download.h"
#include "../protocols/dns_lookup.h" /* W5500_DNS_SOCKET */
#include <furi.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_application/flipper_application.h>

/* PXE download progress */
typedef struct {
    LanTesterApp* app;
    const char* filename;
    size_t base_len; /* length of tool_text before "downloading..." line */
} PxeDownloadCtx;

static void pxe_download_progress_cb(uint32_t bytes_received, void* ctx) {
    PxeDownloadCtx* pctx = ctx;
    /* Truncate back to base text, then append progress line */
    furi_string_left(pctx->app->tool_text, pctx->base_len);
    if(bytes_received < 1024) {
        furi_string_cat_printf(
            pctx->app->tool_text, "%s: %lu B\n", pctx->filename, (unsigned long)bytes_received);
    } else {
        furi_string_cat_printf(
            pctx->app->tool_text,
            "%s: %lu KB\n",
            pctx->filename,
            (unsigned long)(bytes_received / 1024));
    }
    lan_tester_update_view(pctx->app->text_box_tool, pctx->app->tool_text);
}

static void lan_tester_do_pxe_server(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    /* Step 1: Init W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_cat(out, "[PXE] W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        furi_string_cat(out, "[PXE] No LAN link!\nConnect Ethernet cable.\n");
        return;
    }

    /* Step 3: Use boot file selected in settings (already scanned on entry) */
    PxeServerState state;
    memset(&state, 0, sizeof(state));

    if(!app->pxe_scan.boot_file_found) {
        furi_string_printf(
            out,
            "[PXE] No boot file!\n"
            "Place .kpxe or .efi in:\n"
            "%s/\n"
            "Recommended:\n"
            "undionly.kpxe from\n"
            "netboot.xyz (~70KB)\n",
            PXE_BOOT_DIR);
        return;
    }

    /* Copy selected boot file info */
    strncpy(state.boot_filename, app->pxe_scan.boot_filename, sizeof(state.boot_filename) - 1);
    state.boot_file_size = app->pxe_scan.boot_file_size;
    state.boot_file_found = true;

    /* Step 4: Build config from settings */
    state.config.dhcp_enabled = app->pxe_dhcp_enabled;
    memcpy(state.config.client_ip, app->pxe_client_ip, 4);
    memcpy(state.config.subnet, app->pxe_subnet, 4);

    /* Step 5: When built-in DHCP is OFF and we already have a DHCP lease,
     * use the real IP so the network can reach our TFTP server. */
    if(!state.config.dhcp_enabled && app->dhcp_valid) {
        memcpy(state.config.server_ip, app->dhcp_ip, 4);
        memcpy(state.config.subnet, app->dhcp_mask, 4);
        w5500_hal_set_net_info(app->dhcp_ip, app->dhcp_mask, app->dhcp_gw, app->dhcp_dns);
    } else {
        memcpy(state.config.server_ip, app->pxe_server_ip, 4);
        w5500_hal_set_net_info(
            state.config.server_ip,
            state.config.subnet,
            state.config.server_ip,
            state.config.server_ip);
    }

    /* Step 6: Open sockets */
    if(!pxe_server_start(&state)) {
        furi_string_cat(out, "\n[PXE] Failed to open sockets!\n");
        lan_tester_update_view(app->text_box_tool, out);
        return;
    }

    /* Step 7: Initial status */
    furi_string_printf(
        out,
        "[PXE Server]\n"
        "IP: %d.%d.%d.%d\n"
        "DHCP: %s  Files: %d\n"
        "Waiting for client...\n",
        state.config.server_ip[0],
        state.config.server_ip[1],
        state.config.server_ip[2],
        state.config.server_ip[3],
        state.config.dhcp_enabled ? "ON" : "OFF",
        state.boot_file_count);
    lan_tester_update_view(app->text_box_tool, out);

    /* Step 8: Main loop */
    state.running = true;
    PxeState prev_state = PxeStateIdle;
    uint32_t prev_blocks = 0;

    while(app->worker_running && state.running) {
        pxe_server_poll(&state, app->frame_buf, 1024);

        /* Update UI on state change or every 16 blocks */
        bool need_update = (state.state != prev_state) ||
                           (state.tftp_blocks_sent - prev_blocks >= 16);

        if(need_update) {
            prev_state = state.state;
            prev_blocks = state.tftp_blocks_sent;

            furi_string_reset(out);
            furi_string_printf(
                out,
                "[PXE Server]\nIP: %d.%d.%d.%d  DHCP:%s\n",
                state.config.server_ip[0],
                state.config.server_ip[1],
                state.config.server_ip[2],
                state.config.server_ip[3],
                state.config.dhcp_enabled ? "ON" : "OFF");
            if(state.boot_file_count > 1) {
                furi_string_cat_printf(out, "Boot: auto (%d files)\n", state.boot_file_count);
            } else {
                furi_string_cat_printf(out, "Boot: %s\n", state.boot_filename);
            }

            switch(state.state) {
            case PxeStateIdle:
                furi_string_cat(out, "Waiting for client...\n");
                break;
            case PxeStateDhcpOfferSent:
            case PxeStateDhcpAckSent:
                furi_string_cat_printf(
                    out,
                    "Client: %02X:%02X:%02X:%02X:%02X:%02X\n"
                    "DHCP handshake...\n",
                    state.client_mac[0],
                    state.client_mac[1],
                    state.client_mac[2],
                    state.client_mac[3],
                    state.client_mac[4],
                    state.client_mac[5]);
                break;
            case PxeStateTftpTransfer: {
                uint8_t pct = state.boot_file_size ?
                                  (uint8_t)((state.tftp.bytes_sent * 100) / state.boot_file_size) :
                                  0;
                uint8_t filled = pct / 5;
                char bar[23];
                bar[0] = '[';
                for(int i = 0; i < 20; i++)
                    bar[i + 1] = (i < filled) ? '#' : '.';
                bar[21] = ']';
                bar[22] = 0;
                furi_string_cat_printf(out, "%s %d%%\n", bar, pct);
                furi_string_cat_printf(
                    out,
                    "Blk %d/%d (%lu/%lu B)\n",
                    state.tftp.block_num,
                    (uint16_t)((state.boot_file_size + TFTP_BLOCK_SIZE - 1) / TFTP_BLOCK_SIZE),
                    state.tftp.bytes_sent,
                    state.boot_file_size);
                break;
            }
            case PxeStateDone:
                furi_string_cat_printf(
                    out,
                    "COMPLETE! %lu B in %lu blk\n",
                    state.tftp.bytes_sent,
                    state.tftp_blocks_sent);
                break;
            case PxeStateError:
                furi_string_cat_printf(out, "ERROR! Errs: %lu\n", state.tftp_errors);
                break;
            }
            lan_tester_update_view(app->text_box_tool, out);
        }

        /* After Done → reset to Idle for next client */
        if(state.state == PxeStateDone) {
            furi_delay_ms(2000); /* Show "COMPLETE" for 2 sec */
            state.state = PxeStateIdle;
            state.client_seen = false;
        }

        furi_delay_ms(10);
    }

    /* Cleanup */
    pxe_server_stop(&state);

    furi_string_printf(
        out,
        "[PXE Stopped]\nDHCP: %lu disc, %lu req\nTFTP: %lu req, %lu blk\nErr: %lu\n",
        state.dhcp_discovers,
        state.dhcp_requests,
        state.tftp_requests,
        state.tftp_blocks_sent,
        state.tftp_errors);
    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

static void lan_tester_do_pxe_download(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    /* Step 1: Init W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_set(out, "[PXE Download] W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        furi_string_set(out, "[PXE Download] No LAN link!\nConnect Ethernet cable.\n");
        return;
    }

    /* Step 3: Run DHCP */
    furi_string_printf(out, "[PXE Download]\n%s\n", lan_tester_net_acquire_msg(app));
    lan_tester_update_view(app->text_box_tool, out);

    if(!lan_tester_ensure_dhcp(app)) {
        furi_string_set(out, "[PXE Download]\nDHCP failed!\n");
        return;
    }

    /* Step 4: Create pxe directory */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, PXE_BOOT_DIR);
    furi_record_close(RECORD_STORAGE);

    /* Step 5: Download each boot file.
     * ipxe.pxe = native driver (best for Legacy BIOS)
     * undionly.kpxe = UNDI fallback (Legacy BIOS)
     * snponly.efi = UEFI (small, uses firmware SNP)
     * ipxe.efi = UEFI (full native drivers) */
    static const char* filenames[] = {"ipxe.pxe", "undionly.kpxe", "snponly.efi", "ipxe.efi"};
    static const char* url_paths[] = {
        "/ipxe.pxe",
        "/undionly.kpxe",
        "/x86_64-efi/snponly.efi",
        "/x86_64-efi/ipxe.efi",
    };
    static const uint8_t file_count = 4;
    uint8_t ok_count = 0;
    uint8_t skip_count = 0;

    furi_string_set(out, "[PXE Download]\n");
    lan_tester_update_view(app->text_box_tool, out);

    for(uint8_t i = 0; i < file_count && app->worker_running; i++) {
        /* Static to avoid 128B stack usage; worker is single-threaded */
        static char save_path[128];
        snprintf(save_path, sizeof(save_path), PXE_BOOT_DIR "/%s", filenames[i]);

        /* Check if file already exists */
        Storage* st = furi_record_open(RECORD_STORAGE);
        bool exists = (storage_common_stat(st, save_path, NULL) == FSE_OK);
        furi_record_close(RECORD_STORAGE);

        if(exists) {
            furi_string_cat_printf(out, "%s: exists\n", filenames[i]);
            lan_tester_update_view(app->text_box_tool, out);
            skip_count++;
            continue;
        }

        /* Set up progress callback */
        PxeDownloadCtx pctx = {
            .app = app,
            .filename = filenames[i],
            .base_len = furi_string_size(out),
        };
        furi_string_cat_printf(out, "%s: connecting...\n", filenames[i]);
        lan_tester_update_view(app->text_box_tool, out);

        HttpDownloadResult result;
        bool ok = http_download_file(
            W5500_DNS_SOCKET,
            HTTP_CLIENT_SOCKET,
            app->dhcp_dns,
            "boot.ipxe.org",
            url_paths[i],
            save_path,
            app->frame_buf,
            1024,
            &result,
            &app->worker_running,
            pxe_download_progress_cb,
            &pctx);

        /* Replace progress line with final status */
        furi_string_left(out, pctx.base_len);
        if(ok) {
            if(result.bytes_received < 1024) {
                furi_string_cat_printf(
                    out, "%s: OK %lu B\n", filenames[i], (unsigned long)result.bytes_received);
            } else {
                furi_string_cat_printf(
                    out,
                    "%s: OK %lu KB\n",
                    filenames[i],
                    (unsigned long)(result.bytes_received / 1024));
            }
            ok_count++;
        } else {
            furi_string_cat_printf(out, "%s: %s\n", filenames[i], result.error_msg);
        }
        lan_tester_update_view(app->text_box_tool, out);
    }

    furi_string_cat_printf(out, "Done: %d OK, %d skipped\n", ok_count, skip_count);

    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

static void pxe_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemPxeServer:
        lan_tester_do_pxe_server(app);
        break;
    case LanTesterMenuItemPxeDownload:
        lan_tester_do_pxe_download(app);
        break;
    default:
        break;
    }
}

static const LanTesterCategoryPlugin pxe_plugin = {
    .name = "pxe",
    .run = pxe_run,
};

static const FlipperAppPluginDescriptor pxe_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &pxe_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_pxe_plugin_ep(void) {
    return &pxe_plugin_descriptor;
}
