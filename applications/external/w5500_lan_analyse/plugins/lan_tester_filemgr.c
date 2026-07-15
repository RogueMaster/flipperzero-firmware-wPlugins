/*
 * File Manager category plugin — HTTP file server over W5500.
 *
 * Bundles protocols/file_manager.c (the HTTP server) so its ~5 KB of code is
 * resident in RAM only while the File Manager tool runs, instead of living in
 * the host binary permanently. The server reaches the W5500 through the socket
 * shim (api/lan_tester_ioshim.h) resolved by the host's API table.
 */

#include "../lan_tester_app.h"
#include "../lan_tester_plugin.h"
#include "../hal/w5500_hal.h"
#include "../protocols/file_manager.h"
#include <furi.h>
#include <notification/notification_messages.h>
#include <flipper_application/flipper_application.h>

static void lan_tester_do_file_manager(LanTesterApp* app) {
    FuriString* out = app->tool_text;
    furi_string_reset(out);

    /* Step 1: Init W5500 */
    if(!lan_tester_ensure_w5500(app)) {
        furi_string_cat(out, "[File Manager] W5500 Not Found!\nCheck SPI wiring.\n");
        return;
    }

    /* Step 2: Check link */
    if(!w5500_hal_get_link_status()) {
        furi_string_cat(out, "[File Manager] No LAN link!\nConnect Ethernet cable.\n");
        return;
    }

    /* Step 3: Run DHCP to get IP */
    furi_string_printf(out, "[File Manager]\n%s\n", lan_tester_net_acquire_msg(app));
    lan_tester_update_view(app->text_box_tool, out);

    if(!lan_tester_ensure_dhcp(app)) {
        furi_string_set(out, "[File Manager]\nDHCP failed!\n");
        return;
    }

    /* Step 5: Start HTTP server */
    FileManagerState fm_state;
    if(!file_manager_start(&fm_state)) {
        furi_string_cat(out, "[File Manager]\nFailed to start HTTP!\n");
        return;
    }

    /* Step 6: Show compact status with auth token */
    furi_string_printf(
        out,
        "[File Manager] Running\n"
        "http://%d.%d.%d.%d/?t=%s\n"
        "Req:0 Tx:0 Rx:0\n"
        "Press BACK to stop.",
        app->dhcp_ip[0],
        app->dhcp_ip[1],
        app->dhcp_ip[2],
        app->dhcp_ip[3],
        fm_state.auth_token);
    lan_tester_update_view(app->text_box_tool, out);

    /* Step 7: Main loop */
    uint32_t last_status = furi_get_tick();
    while(app->worker_running && fm_state.running) {
        file_manager_poll(&fm_state, app->frame_buf, 1024);

        /* Update status every 2 seconds */
        if(furi_get_tick() - last_status >= 2000) {
            last_status = furi_get_tick();
            furi_string_printf(
                out,
                "[File Manager] Running\n"
                "http://%d.%d.%d.%d/?t=%s\n"
                "Req:%lu Tx:%lu Rx:%lu\n"
                "%s\n"
                "Press BACK to stop.",
                app->dhcp_ip[0],
                app->dhcp_ip[1],
                app->dhcp_ip[2],
                app->dhcp_ip[3],
                fm_state.auth_token,
                (unsigned long)fm_state.requests_served,
                (unsigned long)fm_state.bytes_sent,
                (unsigned long)fm_state.bytes_received,
                fm_state.errors ? "Errors!" : "");
            lan_tester_update_view(app->text_box_tool, out);
        }
    }

    /* Cleanup */
    file_manager_stop(&fm_state);

    furi_string_printf(
        out,
        "[File Manager] Stopped\n"
        "Req:%lu Tx:%lu Rx:%lu",
        (unsigned long)fm_state.requests_served,
        (unsigned long)fm_state.bytes_sent,
        (unsigned long)fm_state.bytes_received);
    if(app->setting_sound) notification_message(app->notifications, &sequence_success);
}

static void filemgr_run(LanTesterApp* app, uint32_t op) {
    switch(op) {
    case LanTesterMenuItemFileManager:
        lan_tester_do_file_manager(app);
        break;
    default:
        break;
    }
}

static const LanTesterCategoryPlugin filemgr_plugin = {
    .name = "filemgr",
    .run = filemgr_run,
};

static const FlipperAppPluginDescriptor filemgr_plugin_descriptor = {
    .appid = LAN_TESTER_PLUGIN_APP_ID,
    .ep_api_version = LAN_TESTER_PLUGIN_API_VERSION,
    .entry_point = &filemgr_plugin,
};

const FlipperAppPluginDescriptor* lan_tester_filemgr_plugin_ep(void) {
    return &filemgr_plugin_descriptor;
}
