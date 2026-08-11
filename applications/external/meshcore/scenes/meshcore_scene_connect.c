/*
 * scene_connect — open the UART, handshake with the node and show what it is.
 *
 * The protocol calls block, so they run on a worker thread; the GUI thread is
 * only woken by a custom event when the worker is done. Nothing below the
 * scene layer ever touches a view.
 */
#include "../detect/meshcore_detect.h"
#include "../meshcore_cfg.h"

/* Offset well clear of the menu's item indices: a completion event that lands
 * just after the user backs out would otherwise be routed to scene_menu and
 * move its cursor. */
#define MESHCORE_CONNECT_EVENT_DONE 0x100u
#define MESHCORE_CONNECT_WORKER_STACK 2048u

static void meshcore_connect_copy_self_info(MeshCoreNodeInfo* node, const mc_self_info_t* info) {
    node->role_type = info->type;
    node->tx_power = info->tx_power;
    node->max_tx_power = info->max_tx_power;
    node->freq_khz = info->radio_freq;
    node->bw_hz = info->radio_bw;
    node->sf = info->radio_sf;
    node->cr = info->radio_cr;
    memcpy(node->public_key, info->public_key, sizeof(node->public_key));
    snprintf(node->name, sizeof(node->name), "%s", info->name);
}

/* Turn a failed request into something worth reading on a 128x64 screen. */
static const char* meshcore_connect_diagnose(MeshCoreApp* app, const mc_event_t* ev) {
    if(ev->code == MC_RESP_ERR) return "Node refused the handshake.";
    if(meshcore_session_rx_errors(app->session) > 0)
        return "Noise on the line.\nWrong baud, or TX/RX swapped?";
    return "No reply from the node.\nIs it on a hardware UART?";
}

/* Runs on the worker thread. Returns NULL on success, or a literal to show. */
static const char* meshcore_connect_run(MeshCoreApp* app) {
    MeshCoreNodeInfo* node = &app->node;
    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t ev;
    size_t len;

    node->valid = false;

    if(!meshcore_session_start(app->session)) {
        return "Cannot take the USART.\nAnother app is holding it.";
    }

    /* APP_START identifies us and comes back with the node's current radio
     * settings, which is also the config the editors will work on. */
    len = mc_cmd_app_start(payload, sizeof(payload), MESHCORE_LINK_APP_NAME);
    if(len == 0) return "Internal error: APP_START.";
    if(!meshcore_session_request(
           app->session, payload, len, MC_RESP_SELF_INFO, &ev, MESHCORE_LINK_TIMEOUT_MS)) {
        return meshcore_connect_diagnose(app, &ev);
    }
    meshcore_connect_copy_self_info(node, &ev.u.self_info);

    if(app->worker_stop) return "Cancelled.";

    /* DEVICE_QUERY gives model and firmware version. Older firmware may not
     * answer it — that is not fatal, we already have the settings. */
    len = mc_cmd_device_query(payload, sizeof(payload), MESHCORE_LINK_PROTO_VER);
    if(len != 0 && meshcore_session_request(
                       app->session, payload, len, MC_RESP_DEVICE_INFO, &ev,
                       MESHCORE_LINK_TIMEOUT_MS)) {
        snprintf(node->model, sizeof(node->model), "%s", ev.u.device_info.model);
        snprintf(node->fw_ver, sizeof(node->fw_ver), "%s", ev.u.device_info.ver);
        node->fw_ver_code = ev.u.device_info.fw_ver;
    } else {
        snprintf(node->model, sizeof(node->model), "MeshCore node");
        snprintf(node->fw_ver, sizeof(node->fw_ver), "?");
        node->fw_ver_code = 0;
        meshcore_log_printf(app->log, "no DEVICE_INFO, using SELF_INFO only");
    }

    node->valid = true;
    return NULL;
}

/* Shared entry point for scenes that need the link up but do not want to make
 * the user press Connect. Idempotent: if the session is already pumping and the
 * handshake has filled node, there is nothing to do. */
const char* meshcore_connect_ensure(MeshCoreApp* app) {
    if(meshcore_session_is_running(app->session) && app->node.valid) return NULL;
    return meshcore_connect_run(app);
}

static int32_t meshcore_connect_worker(void* context) {
    MeshCoreApp* app = context;
    app->worker_error = meshcore_connect_run(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_CONNECT_EVENT_DONE);
    return 0;
}

static void meshcore_scene_connect_show(MeshCoreApp* app, bool done) {
    /* Widget elements copy the text they are given, so a stack buffer is fine.
     * Every %s below is length-capped: the node name alone can be 184 bytes,
     * which neither the screen nor this buffer has any use for. */
    char text[512];

    widget_reset(app->widget);

    if(!done) {
        /* Debug mode owns PA13, which is the block A detect line. Saying so
         * here is cheaper than debugging a node that came up on radio because
         * it never saw the detect signal. */
        MeshCoreDetectState detect = meshcore_detect_state();
        snprintf(
            text,
            sizeof(text),
            "\e#Connecting...\n\n"
            "block A: 11 GND 12 DET\n"
            "         13 TX  14 RX\n"
            "115200 8N1\n"
            "detect A%s B%s%s",
            detect.block_a_high ? "+" : "-",
            detect.block_b_high ? "+" : "-",
            detect.debug_mode ? "\nturn Debug OFF: it owns pin 12" : "");
    } else if(app->worker_error) {
        snprintf(
            text,
            sizeof(text),
            "\e#Not connected\n"
            "%s\n\n"
            "13=TX 14=RX 18=GND, 115200\n"
            "line errors: %lu",
            app->worker_error,
            (unsigned long)meshcore_session_rx_errors(app->session));
    } else {
        const MeshCoreNodeInfo* n = &app->node;
        snprintf(
            text,
            sizeof(text),
            "\e#%.32s\n"
            "fw %.20s (v%d)\n"
            "name: %.48s\n"
            "freq: %lu.%03lu MHz\n"
            "bw %lu kHz  sf %u  cr %u\n" /* bw arrives in Hz */
            "tx %u dBm (max %u)",
            n->model,
            n->fw_ver,
            (int)n->fw_ver_code,
            n->name,
            (unsigned long)(n->freq_khz / 1000u),
            (unsigned long)(n->freq_khz % 1000u),
            (unsigned long)(n->bw_hz / 1000u),
            (unsigned)n->sf,
            (unsigned)n->cr,
            (unsigned)n->tx_power,
            (unsigned)n->max_tx_power);
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
}

void meshcore_scene_connect_on_enter(void* context) {
    MeshCoreApp* app = context;

    app->worker_stop = false;
    app->worker_error = NULL;

    meshcore_scene_connect_show(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreConnect", MESHCORE_CONNECT_WORKER_STACK, meshcore_connect_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_connect_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == MESHCORE_CONNECT_EVENT_DONE) {
        meshcore_scene_connect_show(app, true);
        return true;
    }

    return false;
}

void meshcore_scene_connect_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        /* Requests time out in MESHCORE_LINK_TIMEOUT_MS, so the join is
         * bounded by roughly that even when the node is silent. */
        app->worker_stop = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }

    widget_reset(app->widget);
}
