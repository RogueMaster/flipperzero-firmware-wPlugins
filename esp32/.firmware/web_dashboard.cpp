/*
 * web_dashboard.cpp — HTTP + WebSocket dashboard for Tesla FSD ESP32
 *
 * HTTP  :80  → serves the embedded HTML page
 * WS    :81  → pushes JSON state every 1 s; receives control commands
 *
 * The WS is the primary link. GET /api/status and POST /api/cmd carry the same
 * state and commands over plain HTTP for clients whose WebSocket never
 * delivers (the iOS/macOS captive-portal sheet).
 *
 * All HTML/CSS/JS is embedded as a raw-string literal — no external CDN.
 * State is shared with the CAN task. Reads copy a locked snapshot; writes use
 * the same FreeRTOS critical section as the CAN/button side.
 */

#include "web_dashboard.h"
#include "can_dump.h"
#include "http_can_stream.h"
#include "blackbox.h"
#include "capability.h"
#include "profile_match.h"
#include "prefs.h"
#include "ota_verify.h"
#include "power_log.h"
#include "loop_health.h"
#include "captive_portal.h"
#include "web_cmd.h"
#include "web_origin.h"
#include "page_send.h"
#include "../../fsd_logic/fsd_cp_status.h"  // CP_status stale read-out (#228)
#include "../../fsd_logic/fsd_bms.h"        // 0x312 pack-temperature freshness
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <lwip/sockets.h>
#include <errno.h>

// ── Module state ──────────────────────────────────────────────────────────────
static FSDState  *g_state = nullptr;   // shared with main
static CanDriver **g_can_buses = nullptr; // mode switch, error split, pre-reboot quiesce
static uint8_t g_can_count = 0;
static portMUX_TYPE *g_state_mux = nullptr;

static WebServer        g_http(80);
static WebSocketsServer g_ws(81);

static uint32_t g_start_ms    = 0;
static uint32_t g_last_rx     = 0;
static uint32_t g_last_fps_ms = 0;
static uint32_t g_last_can_seen_ms = 0;
static float    g_fps         = 0.0f;
static bool     g_debug       = false;  // serial `debug`: per-push JSON size

// CAN Bus card reset (#252). The card shows each counter minus its value at
// the last reset; the counters themselves stay cumulative for everything else
// that reads them (black box, capture meta, Tap Check, power log, canstat).
struct CanCounts {
    uint32_t rx, tx, rx_missed, bus_error, tx_failed;
};
static CanCounts g_can_base[CAN_BUS_COUNT] = {};  // each driver at the reset
static uint32_t  g_rx_base      = 0;               // FSDState rx_count at the reset
static bool      g_can_reset    = false;
static uint32_t  g_can_reset_ms = 0;

#define CAN_VEHICLE_ALIVE_MS 3000u
#define OTA_ESP32_IMAGE_MAGIC 0xE9u
#define OTA_AUTH_USER "admin"
// Longest command is wifi_cfg: ~90 bytes of JSON around two SSIDs (32) and two
// passphrases (64) = ~282, up to ~480 when every character needs a backslash
// (\" or \\). The parser works in place, so this only bounds the work.
#define WEB_CMD_MAX_LEN 767u
// Page body limits (page_send.h). The gzip page is 8-9 send-buffer fills
// (5760 bytes each), one per round trip: 40-250 ms at the 5-30 ms of a phone
// on the SoftAP, over 1 s at the 100+ ms of in-car Wi-Fi in power save. A
// client still taking data goes on; one that took nothing for STALL_MS has
// gone (a round trip that long is not a working link). MAX_MS caps a client
// that only ever takes a trickle.
#define PAGE_SEND_STALL_MS 400u
#define PAGE_SEND_MAX_MS   3000u

static void state_enter() {
    if (g_state_mux) portENTER_CRITICAL(g_state_mux);
}

static void state_exit() {
    if (g_state_mux) portEXIT_CRITICAL(g_state_mux);
}

static bool state_copy(FSDState *out) {
    if (g_state == nullptr || out == nullptr) return false;
    state_enter();
    *out = *g_state;
    state_exit();
    return true;
}

static bool ap_has_password(const FSDState *state) {
    return state != nullptr && strlen(state->wifi_pass) >= 8;
}

static bool require_admin_auth(bool challenge_browser = false) {
    FSDState s;
    if (!state_copy(&s) || !ap_has_password(&s)) {
        g_http.send(403, "text/plain", "WiFi AP password required before OTA/restart");
        return false;
    }
    if (g_http.authenticate(OTA_AUTH_USER, s.wifi_pass)) return true;
    if (challenge_browser) {
        g_http.requestAuthentication(BASIC_AUTH, "Tesla-FSD");
    } else {
        g_http.send(401, "text/plain", "Authentication failed");
    }
    return false;
}

// Black-box captures hold recorded CAN (VIN, drive data), so gate the download
// endpoints behind admin auth — but only when an AP password is set. Open setups
// keep the one-click capture-download workflow; password-protected devices don't
// leak persistent captures to other hosts on a shared LAN. Softer than
// require_admin_auth (which hard-blocks when no password is set).
static bool download_auth_ok() {
    FSDState s;
    if (!state_copy(&s) || !ap_has_password(&s)) return true;   // open setup — allow
    if (g_http.authenticate(OTA_AUTH_USER, s.wifi_pass)) return true;
    g_http.requestAuthentication(BASIC_AUTH, "Tesla-FSD");
    return false;
}

// ── Embedded HTML/CSS/JS ──────────────────────────────────────────────────────
// Tesla dark theme; mobile-first (max 480 px); WebSocket on :81
// String literals and #if defined(...) blocks only: gen_web_page.py gzips the
// page at build time and stops the build on a macro in it (FW_VERSION and the
// like reach the page through the status JSON).
static const char WEB_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#0a0a1a">
<link rel="icon" href="data:,">
<title>Tesla FSD</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
:root{
  --bg:#0a0a1a;--card:#111827;--card2:#1a1f35;
  --accent:#00d4aa;--accent2:#00b894;
  --red:#ff6b6b;--yellow:#ffd93d;--blue:#4dabf7;
  --border:#1e293b;--text:#e2e8f0;--text2:#94a3b8;--text3:#475569;
  --muted:#7d8ba1  /* hint text: between text2 and text3, 4.7:1 on card2 */
}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:var(--bg);color:var(--text);min-height:100vh}
.wrap{max-width:480px;margin:0 auto;padding:16px 16px 40px}

/* ── Header ── */
.hdr{text-align:center;padding:20px 0 12px;position:relative}
.hdr h1{font-size:1.65em;font-weight:700;
  background:linear-gradient(135deg,var(--accent),var(--blue));
  -webkit-background-clip:text;-webkit-text-fill-color:transparent;
  letter-spacing:-.02em}
.hdr .sub{font-size:.68em;color:var(--text3);margin-top:3px;
  letter-spacing:.1em;text-transform:uppercase}
.cdot{position:absolute;right:0;top:26px;width:10px;height:10px;
  border-radius:50%;background:var(--accent);
  box-shadow:0 0 10px var(--accent);transition:.4s}
.cdot.off{background:var(--red);box-shadow:0 0 10px var(--red)}

/* ── OTA Warning ── */
.ota{display:none;background:rgba(255,107,107,.1);border:1px solid rgba(255,107,107,.4);
  border-radius:12px;padding:12px 16px;margin-bottom:12px;text-align:center;
  color:var(--red);font-weight:700;font-size:.9em;letter-spacing:.04em;
  animation:pulse 1s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.55}}

/* ── 14.x firmware warning (static, non-animated) ── */
.warn14x{display:none;background:rgba(252,196,25,.08);border:1px solid rgba(252,196,25,.45);
  border-radius:12px;padding:10px 14px;margin-bottom:12px;color:#fcc419;font-size:.82em;
  line-height:1.4;text-align:left}
.warn14x .w-row{display:flex;justify-content:space-between;align-items:center;gap:12px}
.warn14x .w-msg{flex:1}
.warn14x .w-dismiss{background:transparent;border:1px solid rgba(252,196,25,.5);color:#fcc419;
  padding:5px 10px;border-radius:6px;cursor:pointer;font-size:.8em;font-weight:600;
  white-space:nowrap}
.warn14x .w-dismiss:hover{background:rgba(252,196,25,.15)}

/* ── Error banner ── */
.err{display:none;color:var(--red);text-align:center;font-size:.78em;padding:8px;
  background:rgba(255,107,107,.07);border-radius:10px;margin-bottom:10px;
  border:1px solid rgba(255,107,107,.18)}

/* ── Cards ── */
.card{background:var(--card);border-radius:16px;padding:16px;
  margin-bottom:12px;border:1px solid var(--border)}
.card-head{display:flex;align-items:center;gap:8px;margin-bottom:12px}
.config-section{background:var(--card);border:1px solid var(--border);
  border-radius:16px;margin-bottom:12px;overflow:hidden}
.config-section summary{display:flex;align-items:center;gap:8px;list-style:none;
  padding:16px;cursor:pointer;user-select:none}
.config-section summary::-webkit-details-marker{display:none}
.config-section summary:after{content:"";margin-left:auto;width:9px;height:9px;
  border-right:2px solid var(--text2);border-bottom:2px solid var(--text2);
  transform:rotate(45deg);transition:transform .2s}
.config-section[open] summary:after{transform:rotate(225deg)}
.config-section .config-body{padding:0 12px 12px}
.config-section .card{border-radius:12px;margin-bottom:10px}
.config-section .card:last-child{margin-bottom:0}
.controls-fold{margin-top:10px;border-top:1px solid rgba(255,255,255,.04)}
.controls-fold summary{position:relative;display:block;list-style:none;
  padding:10px 28px 0 0;cursor:pointer;user-select:none;min-height:24px}
.controls-fold summary::-webkit-details-marker{display:none}
.controls-fold summary:after{content:"";position:absolute;right:4px;top:12px;width:9px;height:9px;
  border-right:2px solid var(--text2);border-bottom:2px solid var(--text2);
  transform:rotate(45deg);transition:transform .2s}
.controls-fold[open] summary:after{transform:rotate(225deg);top:16px}
.controls-fold[open] .control-summary{display:none}
.control-summary{color:var(--text2);font-size:.76em;
  display:block;max-width:calc(100% - 8px);white-space:normal;line-height:1.35;padding-right:8px}
.controls-body{padding-top:8px}
.icon{width:28px;height:28px;border-radius:8px;display:flex;
  align-items:center;justify-content:center;font-size:.85em;font-weight:700}
.ic-s{background:rgba(0,212,170,.14);color:var(--accent)}
.ic-b{background:rgba(77,171,247,.14);color:var(--blue)}
.ic-c{background:rgba(255,217,61,.14);color:var(--yellow)}
.ic-d{background:rgba(148,163,184,.14);color:var(--text2)}
.card-head h2{font-size:.78em;font-weight:600;color:var(--text2);
  text-transform:uppercase;letter-spacing:.07em}

/* ── Rows ── */
.row{display:flex;justify-content:space-between;align-items:center;padding:9px 0}
.row+.row{border-top:1px solid rgba(255,255,255,.04)}
.lbl{color:var(--text2);font-size:.85em}
.hint{display:block;color:var(--text2);opacity:.62;font-size:.82em;line-height:1.35;margin-top:2px;max-width:20em}
details select,select.sel{background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px 6px;border-radius:4px;font-size:.85em;flex:none}
details input{background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right;width:60px}
details input.cgn{width:38px;margin-left:4px}
details input:disabled{opacity:.45}

/* ── Pills ── */
.pill{display:inline-flex;align-items:center;gap:5px;
  padding:3px 10px;border-radius:20px;font-size:.8em;font-weight:600}
.pill.on{background:rgba(0,212,170,.14);color:var(--accent)}
.pill.off{background:rgba(71,85,105,.22);color:var(--text3)}
.pill.warn{background:rgba(255,107,107,.14);color:var(--red)}
.pd{width:6px;height:6px;border-radius:50%;flex-shrink:0;
  background:currentColor;box-shadow:0 0 5px currentColor}

/* ── Battery Hero ── */
.hero{text-align:center;padding-bottom:4px}
.soc-ring{width:120px;height:120px;margin:0 auto 14px;position:relative}
.soc-ring svg{transform:rotate(-90deg)}
.trk{fill:none;stroke:#1e293b;stroke-width:8}
.bar{fill:none;stroke:var(--accent);stroke-width:8;stroke-linecap:round;
  transition:stroke-dashoffset .8s ease,stroke .5s}
.soc-val{position:absolute;inset:0;display:flex;flex-direction:column;
  align-items:center;justify-content:center}
.soc-num{font-size:2em;font-weight:700;line-height:1;
  font-variant-numeric:tabular-nums}
.soc-lbl{font-size:.6em;color:var(--text3);margin-top:3px;text-transform:uppercase}
.hg{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;text-align:center}
.hg .hv{font-size:1.1em;font-weight:600;font-variant-numeric:tabular-nums}
.hg .hl{font-size:.65em;color:var(--text3);margin-top:2px}

/* ── CAN stat grid ── */
.sg{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}
.sb{background:var(--card2);border-radius:10px;padding:10px 12px}
.sb .sv{font-size:1.15em;font-weight:700;font-variant-numeric:tabular-nums}
.sb .sl{font-size:.64em;color:var(--text3);margin-top:2px}
.tg{display:grid;grid-template-columns:auto repeat(4,1fr);gap:6px 10px;align-items:baseline;
  text-align:right;font-variant-numeric:tabular-nums}
.tg .tu,.tg .th{color:var(--text3);font-size:.72em}
.tg .tu{text-align:left}
.tg.stale{color:var(--text3)}
.btn-mini{background:var(--card2);border:1px solid var(--border);color:var(--text2);
  padding:4px 10px;border-radius:6px;font-size:.72em;font-weight:600;cursor:pointer}
.btn-mini:active{opacity:.75}

/* ── Controls ── */
.btn-main{width:100%;padding:14px;border:none;border-radius:12px;
  font-size:.95em;font-weight:700;cursor:pointer;letter-spacing:.04em;
  transition:opacity .2s;margin-bottom:10px}
.btn-main:active{opacity:.75}
.btn-main:disabled{opacity:.45;cursor:default}
.btn-act{background:linear-gradient(135deg,var(--accent),var(--accent2));color:#000}
.btn-stop{background:rgba(255,107,107,.14);color:var(--red);
  border:1px solid rgba(255,107,107,.3)}
.sw{position:relative;width:44px;height:24px;flex-shrink:0}
.sw input{opacity:0;width:0;height:0}
.sl2{position:absolute;cursor:pointer;inset:0;background:#2a2a3e;
  border-radius:24px;transition:.3s}
.sl2:before{content:"";position:absolute;height:18px;width:18px;
  left:3px;bottom:3px;background:#555;border-radius:50%;transition:.3s}
input:checked+.sl2{background:var(--accent)}
input:checked+.sl2:before{transform:translateX(20px);background:#fff}

/* ── OTA firmware update ── */
.ota-file{display:none}.ota-progress{display:none;margin-top:12px}
.ota-track{background:var(--card2);border-radius:8px;height:10px;overflow:hidden}
.ota-bar{background:var(--accent);height:100%;width:0%;transition:width .2s}
.ota-status{text-align:center;margin-top:8px;font-size:.85em;color:var(--text2)}
.ota-bytes{text-align:center;margin-top:4px;font-size:.72em;color:var(--text3)}
.ota-info{margin-top:10px;padding:10px 12px;background:rgba(77,171,247,.07);
  border-radius:8px;border:1px solid rgba(77,171,247,.15);font-size:.72em;color:var(--text3);line-height:1.4}
.btn-blue{background:rgba(77,171,247,.14);color:var(--blue);border:1px solid rgba(77,171,247,.3)}
.btn-yellow{background:rgba(255,217,61,.14);color:var(--yellow);border:1px solid rgba(255,217,61,.3)}

/* ── Footer ── */
.foot{text-align:center;padding:16px 0 0;font-size:.64em;color:var(--text3)}

/* ── Auth panel ── */
.auth-panel,.confirm-panel{display:none;background:var(--card);border:1px solid var(--border);
  border-radius:12px;padding:16px;margin-bottom:12px}
.auth-panel.show,.confirm-panel.show{display:block}
.auth-box{width:100%;background:transparent;border:0;
  border-radius:0;padding:0;box-shadow:none}
.auth-box h3{font-size:1.15em;text-align:center;margin-bottom:12px}
.auth-msg{font-size:.82em;color:var(--text2);line-height:1.4;margin-bottom:14px}
.auth-field{display:block;font-size:.75em;color:var(--text2);margin:10px 0 5px}
.auth-input{width:100%;background:var(--card2);border:1px solid var(--border);
  color:var(--text);border-radius:8px;padding:11px;font-size:1em}
.auth-actions{display:flex;gap:10px;margin-top:16px}
.auth-actions button{flex:1;margin:0}
.log-info{font-size:.74em;color:var(--text2);line-height:1.45;margin-top:8px}
.log-actions{display:flex;gap:10px;margin-top:12px}
.log-actions button{flex:1;margin:0}
.log-filter{width:210px;max-width:60%;background:var(--card2);border:1px solid var(--border);
  color:var(--text);border-radius:6px;padding:6px 8px;font-size:.8em;text-align:right}
.log-filter::placeholder{color:var(--text3)}
.action-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-top:10px}
.action-dot{display:inline-flex;align-items:center;gap:7px;color:var(--text2);font-size:.78em}
.blinkdot{width:10px;height:10px;border-radius:50%;background:var(--text3);box-shadow:none;transition:.15s}
.blinkdot.hit{background:var(--accent);box-shadow:0 0 10px var(--accent)}
.test-btn{padding:10px 8px;border-radius:10px;border:1px solid rgba(77,171,247,.3);
  background:rgba(77,171,247,.12);color:var(--blue);font-size:.74em;font-weight:700}
.test-btn:disabled{opacity:.4;color:var(--text3);border-color:var(--border);background:var(--card2)}
.metric{font-size:.95em;font-weight:700;font-variant-numeric:tabular-nums}
.metric small{font-size:.72em;color:var(--text3);font-weight:600}

/* ── In-page dialog + toast ── */
.dlg{display:none;position:fixed;top:0;left:0;right:0;bottom:0;z-index:100;
  align-items:center;justify-content:center;padding:16px;background:rgba(5,6,16,.8)}
.dlg.show{display:flex}
.dlg-box{width:100%;max-width:400px;max-height:100%;overflow:auto;background:var(--card);
  border:1px solid var(--border);border-radius:16px;padding:18px 16px 16px;
  box-shadow:0 12px 40px rgba(0,0,0,.55)}
.dlg-msg{font-size:.9em;line-height:1.45;overflow-wrap:anywhere;word-break:break-word}
.dlg-msg div+div{margin-top:8px}
.toast{display:none;position:fixed;left:16px;right:16px;bottom:16px;z-index:90;
  max-width:448px;margin:0 auto;padding:11px 14px;border-radius:12px;text-align:center;
  font-size:.8em;line-height:1.4;color:var(--red);background:var(--card2);
  border:1px solid rgba(255,107,107,.45);box-shadow:0 8px 28px rgba(0,0,0,.5)}
.toast.show{display:block}
</style>
</head>
<body>
<div class="wrap">

<!-- Header -->
<div class="hdr">
  <h1>Tesla FSD</h1>
  <div class="sub">ESP32 CAN Controller &middot; <span id="deviceHost">device.local</span></div>
  <div class="cdot" id="dot"></div>
</div>
<div id="connErr" class="err">Connection lost &mdash; retrying&hellip;</div>

<div id="authPanel" class="auth-panel">
  <div class="auth-box">
    <h3>Authentication Required</h3>
    <div class="auth-msg">Enter the admin username and the WiFi AP password.</div>
    <label class="auth-field" for="authUser">Username</label>
    <input id="authUser" class="auth-input" type="text" value="admin" autocomplete="username">
    <label class="auth-field" for="authPass">Password</label>
    <input id="authPass" class="auth-input" type="password" autocomplete="current-password">
    <div class="auth-actions">
      <button type="button" class="btn-main btn-stop" onclick="cancelAuth()">Cancel</button>
      <button type="button" class="btn-main btn-blue" onclick="submitAuth()">Sign In</button>
    </div>
  </div>
</div>

<div id="restartConfirmPanel" class="confirm-panel">
  <div class="auth-box">
    <h3>Restart device?</h3>
    <div class="auth-msg">The device will reboot immediately and the web connection will drop briefly.</div>
    <div class="auth-actions">
      <button type="button" class="btn-main btn-stop" onclick="cancelRestartConfirm()">No</button>
      <button type="button" class="btn-main btn-yellow" onclick="confirmRestart()">Yes</button>
    </div>
  </div>
</div>

<!-- OTA Warning -->
<div id="otaBanner" class="ota">&#9888;&#xFE0F; OTA UPDATE IN PROGRESS &mdash; CAN TX SUSPENDED</div>

<!-- Autopark Warning (#180) -->
<div id="autoparkBanner" class="ota">&#9888;&#xFE0F; IN-CAR AUTOPARK &mdash; CAN TX PAUSED</div>

<!-- Signal Map watchdog (#100) -->
<div id="sigmapWarn" class="warn14x"><div class="w-row"><div class="w-msg">
  <strong>&#9888;&#xFE0F; Signal Map DAS id not seen on this bus.</strong>
  The configured DAS id isn't arriving, so AP-state can't be read and the nag
  killer is paused. Set <b>DAS id 0</b> for auto, or fix the mapping / tap.
</div></div></div>

<!-- 2026.14.x Firmware Warning -->
<div id="warn14x" class="warn14x">
  <div class="w-row">
    <div class="w-msg">
      <strong>&#9888;&#xFE0F; 2026.14.x firmware enforcement active.</strong>
      Tesla added a preflight check in 2026.14.x that disables autosteer
      the moment any CAN frame touches <code>0x3FD</code>. Symptom on
      the dash: <em>"Autopilot turning off"</em> appears within a second
      of stalk engagement, then AP immediately disengages. Listen-Only
      mode is safe. <strong>AP-First</strong> (delay injection until AP is
      engaged) is on the Flipper build only right now — on the ESP32, engage
      AP from the stalk first, then turn on injection. Dismiss if you're on
      pre-14.x firmware.
    </div>
    <button class="w-dismiss" onclick="cmd('14x_warning',false)">Dismiss</button>
  </div>
</div>

<!-- FSD Status -->
<div class="card">
  <div class="card-head"><div class="icon ic-s">S</div><h2>FSD Status</h2></div>
  <div class="row">
    <span class="lbl">AP Status</span>
    <span class="pill off" id="fsdSt"><span class="pd"></span>--</span>
  </div>
  <div class="row">
    <span class="lbl">Mode</span>
    <span class="pill off" id="opMode"><span class="pd"></span>--</span>
  </div>
  <div class="row">
    <span class="lbl">Hardware</span>
    <span class="pill off" id="hwVer"><span class="pd"></span>--</span>
  </div>
  <div class="row" id="rowHw3Spd" style="display:none">
    <span class="lbl">HW3 Speed<span class="hint">car &rarr; sent</span></span>
    <span id="hw3Spd" style="font-size:.8em;color:var(--text2)">--</span>
  </div>
  <div class="row">
    <span class="lbl">NAG Killer</span>
    <span class="pill off" id="nagSt"><span class="pd"></span>--</span>
  </div>
  <div class="row">
    <span class="lbl">CAN Vehicle</span>
    <span class="pill off" id="canVeh"><span class="pd"></span>--</span>
  </div>
  <div class="row">
    <span class="lbl">Charge Port<span class="hint">0x25D CP_status &mdash; Vehicle CAN only</span></span>
    <span style="font-size:.8em;color:var(--text2)"><span id="cpStale" style="display:none;color:var(--yellow);margin-right:6px">Stale</span><span id="cpType">--</span></span>
  </div>
</div>

<!-- Battery -->
<div class="card">
  <div class="card-head"><div class="icon ic-b">B</div><h2>Battery</h2></div>
  <div class="row">
    <span class="lbl">BMS Status</span>
    <span class="pill off" id="bmsSt"><span class="pd"></span>Waiting Frames</span>
  </div>
  <div class="row">
    <span class="lbl">BMS Frames</span>
    <span id="bmsFrames" style="font-size:.8em;color:var(--text2)">HV:0 SOC:0 TH:0</span>
  </div>
  <div class="hero">
    <div class="soc-ring">
      <svg viewBox="0 0 120 120" width="120" height="120">
        <circle class="trk" cx="60" cy="60" r="52"/>
        <circle class="bar" id="socBar" cx="60" cy="60" r="52"
          stroke-dasharray="326.73" stroke-dashoffset="326.73"/>
      </svg>
      <div class="soc-val">
        <span class="soc-num" id="bSoc">--</span>
        <span class="soc-lbl">Pack SOC</span>
      </div>
    </div>
    <div class="hg">
      <div><div class="hv" id="bVolt">--</div><div class="hl">Voltage</div></div>
      <div><div class="hv" id="bCurr">--</div><div class="hl">Current</div></div>
      <div><div class="hv" id="bTemp">--</div><div class="hl">Pack temp min~max</div></div>
    </div>
  </div>
  <div class="row" id="rowCarSoc" style="display:none">
    <span class="lbl">Car SOC<br><small style="color:var(--muted)">the % on the car's screen (0x33A)</small></span>
    <span id="bCarSoc" style="font-variant-numeric:tabular-nums">--</span>
  </div>
</div>

<!-- Tyre pressure (read-only, 0x219); shown once the frame reaches this tap.
     The switch folds it, per browser like Performance (#237). -->
<div class="card" id="tpmsCard" style="display:none">
  <!-- Too narrow for title, pill and switch (320 px): the pill and switch
       move to a line of their own instead of the title breaking. -->
  <div class="card-head" style="flex-wrap:wrap"><div class="icon ic-b">T</div><h2 style="white-space:nowrap">Tyre Pressure</h2>
    <span style="margin-left:auto;display:flex;align-items:center;gap:8px">
      <span class="pill off" id="tpmsSt"><span class="pd"></span>--</span>
      <label class="sw"><input type="checkbox" id="swTpms" onchange="tpToggle(this.checked)"><span class="sl2"></span></label>
    </span>
  </div>
  <div id="tpOff" class="log-info" style="display:none;margin-bottom:0">
    Read-only read-out of the tyre pressures the car sends on 0x219 &mdash; it transmits
    nothing. Turn on to show them.
  </div>
  <div id="tpBody">
    <div class="tg" id="tpGrid"></div>
    <div class="row" id="rowTpAge" style="display:none">
      <span class="lbl">Since last reading</span>
      <span id="tpAge" style="font-variant-numeric:tabular-nums">--</span>
    </div>
    <div class="row" id="rowTpTgt" style="display:none">
      <span class="lbl">Car's target</span>
      <span id="tpTgt" style="font-size:.8em;color:var(--text2)">--</span>
    </div>
    <div class="row">
      <span class="lbl">Unit</span>
      <select id="selTpUnit" class="sel" onchange="tpSetUnit(this.value)">
        <option value="bar">bar</option>
        <option value="psi">psi</option>
      </select>
    </div>
    <div class="log-info" id="tpOrder" style="display:none">Columns follow the order the car sends them. Which wheel each one belongs to is not confirmed yet.</div>
    <div class="log-info" id="tpNone" style="display:none">0x219 is arriving on this tap, but the car is not reporting a pressure in it.</div>
    <div class="log-info" id="tpForm" style="display:none">This 0x219 has a length this firmware does not read yet. A capture of it would help.</div>
  </div>
</div>

<!-- Performance (read-only, opt-in) -->
<div class="card">
  <div class="card-head"><div class="icon ic-b">P</div><h2>Performance</h2>
    <label class="sw" style="margin-left:auto"><input type="checkbox" id="swPerf" onchange="perfToggle(this.checked)"><span class="sl2"></span></label>
  </div>
  <div id="perfOff" class="log-info" style="margin-bottom:0">
    Read-only read-out derived from frames already on the bus &mdash; it transmits
    nothing. Enable to show acceleration / braking timers, an estimated G (no IMU),
    wheel slip and pack temperature / power. Speed &amp; est. G need any tap
    carrying 0x257; wheel slip needs Party CAN (0x175); temp &amp; power need a
    Vehicle-CAN tap (BMS).
  </div>
  <div id="perfBody" style="display:none">
    <div class="row"><span class="lbl">Vehicle speed</span><span id="pfSpeed" style="color:var(--text2)">--</span></div>
    <div class="sg">
      <div class="sb"><div class="sv" id="pf050">--</div><div class="sl">0-50 km/h</div><div class="sl"><span>best</span> <span id="pf050b">--</span></div></div>
      <div class="sb"><div class="sv" id="pf060mph">--</div><div class="sl">0-60 mph</div><div class="sl"><span>best</span> <span id="pf060mphb">--</span></div></div>
      <div class="sb"><div class="sv" id="pf0100">--</div><div class="sl">0-100 km/h</div><div class="sl"><span>best</span> <span id="pf0100b">--</span></div></div>
      <div class="sb"><div class="sv" id="pf1000">--</div><div class="sl">100-0 km/h brake</div><div class="sl"><span>best</span> <span id="pf1000b">--</span></div></div>
    </div>
    <div class="sg">
      <div class="sb"><div class="sv" id="pfG">--</div><div class="sl">Est. long. G</div><div class="sl"><span>peak</span> <span id="pfGpk">--</span></div></div>
      <div class="sb"><div class="sv" id="pfSlip">--</div><div class="sl">Wheel slip</div></div>
      <div class="sb"><div class="sv" id="pfPow">--</div><div class="sl">Pack power</div></div>
      <div class="sb"><div class="sv" id="pfTemp">--</div><div class="sl">Pack temp</div><div class="sl">unconfirmed on some cars</div></div>
    </div>
    <div class="row" style="gap:12px"><span class="lbl" style="white-space:nowrap">Battery advisory</span><span id="pfAdv" style="font-size:.8em;color:var(--text2);text-align:right">--</span></div>
    <div class="log-info" style="margin-top:8px;font-size:.76em">
      G is estimated from dv/dt of vehicle speed (no IMU). Timers auto-arm leaving a
      standstill, lock at the target, and reset at the next standstill; best time is
      kept per session. Temp / power need a Vehicle-CAN tap; wheel slip needs 0x175.
    </div>
  </div>
</div>

<!-- CAN Stats -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">C</div><h2>CAN Bus</h2>
    <button type="button" class="btn-mini" style="margin-left:auto" onclick="canReset()">Reset</button>
  </div>
  <div class="sg">
    <div class="sb"><div class="sv" id="rxCnt">0</div><div class="sl">RX Frames</div></div>
    <div class="sb"><div class="sv" id="txCnt">0</div><div class="sl">TX Frames</div></div>
    <div class="sb"><div class="sv" id="crcErr">0</div><div class="sl">CAN Errors</div><div class="sl" id="crcSplit"><span style="white-space:nowrap">RX missed</span>&nbsp;0 &middot; <span style="white-space:nowrap">bus</span>&nbsp;0 &middot; <span style="white-space:nowrap">TX fail</span>&nbsp;0</div></div>
    <div class="sb"><div class="sv" id="fps">0.0</div><div class="sl">Frames/s</div></div>
  </div>
  <!-- Per bus, on boards with two controllers (#253) -->
  <div class="tg" id="canBuses" style="display:none;margin-top:12px"></div>
  <div class="row" style="padding-bottom:0;margin-top:4px">
    <span class="lbl" id="canSinceL">Since boot</span>
    <span id="canSince" style="font-size:.8em;color:var(--text2);font-variant-numeric:tabular-nums">--</span>
  </div>
</div>

<!-- Controls -->
<div class="card controls-section">
  <div class="card-head"><div class="icon ic-c">C</div><h2>Controls</h2></div>
  <button id="btnMode" class="btn-main btn-act" onclick="toggleMode()">Activate</button>
<details class="controls-fold">
  <summary><span id="controlsSummary" class="control-summary">...</span></summary>
  <div class="controls-body">
  <div class="row">
    <span class="lbl">Hardware<br><span class="hint">Auto-detect needs 0x398 &mdash; many Model 3/Y never send it. Pick your car if detection is wrong.</span></span>
    <select id="selHwOverride" onchange="cmd('hw_override',parseInt(this.value,10))">
      <option value="0">Auto-detect</option>
      <option value="3">Force HW4</option>
      <option value="2">Force HW3</option>
      <option value="1">Force Legacy</option>
    </select>
  </div>
  <div class="row">
    <span class="lbl">Ignore OTA</span>
    <label class="sw"><input type="checkbox" id="swIgnoreOta" onchange="cmd('ignore_ota',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">FSD Unlock</span>
    <label class="sw"><input type="checkbox" id="swFsdUnlock" onchange="cmd('fsd_unlock',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">HW3 Speed Override (legacy)<br><small style="color:var(--muted)">Writes the follow-distance profile and Autopilot offset into the FSD speed fields. Off = the car's own FSD offset and profile are left alone.</small></span>
    <label class="sw"><input type="checkbox" id="swHw3Spd" onchange="cmd('hw3_speed_override',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">NAG Killer</span>
    <label class="sw"><input type="checkbox" id="swNag" onchange="cmd('nag',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Continuous AP</span>
    <label class="sw"><input type="checkbox" id="swContinuousAp" onchange="cmd('continuous_ap',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">AP-First (14.x)</span>
    <label class="sw"><input type="checkbox" id="swApFirst" onchange="cmd('ap_first',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Instant Engage (exp.)</span>
    <label class="sw"><input type="checkbox" id="swApFe" onchange="cmd('ap_first_edge',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Minimal Inject (exp.)</span>
    <label class="sw"><input type="checkbox" id="swApMi" onchange="cmd('ap_first_minimal',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Nag EPAS-faithful (14.x, exp.)</span>
    <label class="sw"><input type="checkbox" id="swNagF" onchange="cmd('nag_faithful',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Soft Engage (14.x, exp.)</span>
    <label class="sw"><input type="checkbox" id="swSoft" onchange="cmd('soft_engage',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Nag Burst (14.x, exp.)</span>
    <label class="sw"><input type="checkbox" id="swNagB" onchange="cmd('nag_burst',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Abort Guard (14.x, exp.)</span>
    <label class="sw"><input type="checkbox" id="swAbrt" onchange="cmd('abort_guard',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">BMS Display<span class="hint">Prints voltage, current, power, SoC and pack temperature to the USB serial log once a second. The dashboard shows them either way.</span></span>
    <label class="sw"><input type="checkbox" id="swBms" onchange="cmd('bms',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Force FSD</span>
    <label class="sw"><input type="checkbox" id="swFsd" onchange="cmd('force_fsd',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">China Mode</span>
    <label class="sw"><input type="checkbox" id="swChina" onchange="cmd('china_mode',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row" id="rowChime">
    <span class="lbl">Suppress Chime</span>
    <label class="sw"><input type="checkbox" id="swChime" onchange="cmd('suppress_speed_chime',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Precondition<br><small style="color:var(--muted)">battery preheat trigger (0x082)</small></span>
    <label class="sw"><input type="checkbox" id="swPrecond" onchange="cmd('precondition',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">TLSSC Restore</span>
    <label class="sw"><input type="checkbox" id="swTlssc" onchange="cmd('tlssc_restore',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Summon EU Unlock</span>
    <label class="sw"><input type="checkbox" id="swSummon" onchange="cmd('summon_unlock',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Continue on Green<br><small style="color:var(--muted)">pairs with TLSSC</small></span>
    <label class="sw"><input type="checkbox" id="swCog" onchange="cmd('continue_on_green',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">TLSSC bit38</span>
    <label class="sw"><input type="checkbox" id="swTlssc38" onchange="cmd('assist_tlssc_bit38',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Right-Hand Drive (RHD)<br><small style="color:var(--red)">RHD markets only — do NOT enable while driving on the right.</small></span>
    <label class="sw"><input type="checkbox" id="swRhd" onchange="cmd('assist_rhd_override',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Telemetry Off (experimental)<br><small style="color:var(--muted)">Experimental &amp; unverified — clears reachable telemetry flags only (not the Vehicle-bus ECU log-upload). Does NOT guarantee reduced detection.</small></span>
    <label class="sw"><input type="checkbox" id="swTelOff" onchange="cmd('assist_telemetry_off',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">AP Branch/Tier (experimental)<br><small style="color:var(--muted)">Experimental &amp; non-persistent — injects a UI branch/tier hint only, reverts when injection stops; unverified and may be a ban signal. Off by default.</small></span>
    <select id="selApmv3" onchange="cmd('apmv3_branch',parseInt(this.value,10))">
      <option value="255">Off</option>
      <option value="0">Live</option>
      <option value="1">Stage</option>
      <option value="2">Dev</option>
      <option value="3">Stage2</option>
      <option value="4">EAP</option>
      <option value="5">Demo</option>
    </select>
  </div>
  <div class="row" style="display:block">
    <div style="display:flex;align-items:center;justify-content:space-between">
      <span class="lbl">Acceleration Mode (experimental)<br><small style="color:var(--muted)">Rewrites the pedal map on 0x334. The touchscreen keeps showing your own setting; a new mode only switches at standstill.</small></span>
      <select id="selAccel" onchange="cmd('accel_mode',parseInt(this.value,10))">
        <option value="0">Off</option>
        <option value="1" translate="no">Chill</option>
        <option value="2" translate="no">Standard</option>
        <option value="3" translate="no">Performance</option>
      </select>
    </div>
    <div id="accelSt" style="font-size:12px;color:var(--muted);margin-top:4px">--</div>
  </div>
  <div class="row" style="display:block">
    <div style="display:flex;align-items:center;justify-content:space-between">
      <span class="lbl">EU Charge-Port Type (experimental)<br><small style="color:var(--muted)">Re-sends 0x25D on the Vehicle bus with CP_type set to IEC CCS so EU Superchargers show in nav. Held while a cable is plugged in or charging. Experimental.</small></span>
      <label class="sw"><input type="checkbox" id="swCpType" onchange="cmd('cp_type_eu_rewrite',this.checked)"><span class="sl2"></span></label>
    </div>
    <div id="cpTypeSt" style="font-size:12px;color:var(--muted);margin-top:4px">--</div>
  </div>
  <div class="row" style="display:block">
    <div style="display:flex;align-items:center;justify-content:space-between">
      <span class="lbl">Track Mode (experimental)<br><small style="color:var(--muted)">Experimental &mdash; Vehicle-bus; not car-validated. Defaults to rear-biased (rotation 100) + 30% stability &mdash; fun with a safety margin. Raise stability for stock feel.</small></span>
      <label class="sw"><input type="checkbox" id="swTrkMode" onchange="cmd('track_mode_inject',this.checked)"><span class="sl2"></span></label>
    </div>
    <div style="margin-top:8px">
      <label style="display:flex;justify-content:space-between;font-size:12px;color:var(--muted)"><span>Handling Balance <small>(stable &rarr; rotation)</small></span><span id="trkRotV">100</span></label>
      <input type="range" id="trkRot" min="0" max="100" style="width:100%" oninput="document.getElementById('trkRotV').textContent=this.value" onchange="cmd('track_rotation_pct',parseInt(this.value,10))">
    </div>
    <div style="margin-top:6px">
      <label style="display:flex;justify-content:space-between;font-size:12px;color:var(--muted)"><span>Stability Assist</span><span id="trkStabV">30</span></label>
      <input type="range" id="trkStab" min="0" max="100" style="width:100%" oninput="document.getElementById('trkStabV').textContent=this.value" onchange="cmd('track_stability_pct',parseInt(this.value,10))">
    </div>
    <div class="row" style="padding:6px 0 0">
      <span class="lbl">Post-drive Cooling</span>
      <label class="sw"><input type="checkbox" id="swTrkPC" onchange="cmd('track_post_cooling',this.checked)"><span class="sl2"></span></label>
    </div>
    <div class="row" style="padding:0">
      <span class="lbl">Compressor Overclock<br><small style="color:var(--muted)">max cooling</small></span>
      <label class="sw"><input type="checkbox" id="swTrkCO" onchange="cmd('track_cmp_overclock',this.checked)"><span class="sl2"></span></label>
    </div>
  </div>
  <div class="row" style="display:block">
    <div id="pmSuggest" style="display:none;margin:0 0 8px;padding:8px 10px;border:1px solid var(--accent);border-radius:6px;background:var(--card2)">
      <div style="font-size:12px;color:var(--text)">Looks like variant <b id="pmName">?</b> &mdash; the standard parser can't read AP-state on this bus.</div>
      <button type="button" id="pmApply" onclick="pmApply()" style="margin-top:6px;background:var(--accent);color:#000;border:0;padding:6px 12px;border-radius:4px;cursor:pointer">Apply this profile</button>
    </div>
    <details>
      <summary class="lbl" style="cursor:pointer">Signal Map (advanced, 14.x)</summary>
      <div style="font-size:11px;color:var(--muted);margin:6px 0 8px">Override where the nag killer reads AP-state / hands-on / steering. Leave DAS id <b>0</b> for auto-detect. byte 0-7, shift 0-7, mask hex.</div>
      <div style="display:grid;grid-template-columns:minmax(0,1fr) auto;gap:6px 10px;align-items:center;font-size:12px">
        <span>DAS id (0x..)</span><input id="cgDid" placeholder="0">
        <span>AP-state byte/sh/mask</span><span style="white-space:nowrap"><input id="cgApB" class="cgn"><input id="cgApS" class="cgn"><input id="cgApM" class="cgn" placeholder="0xF"></span>
        <span>Hands-on byte/sh/mask</span><span style="white-space:nowrap"><input id="cgHoB" class="cgn"><input id="cgHoS" class="cgn"><input id="cgHoM" class="cgn" placeholder="0xF"></span>
        <span>Steer id (0x..)</span><input id="cgSid" placeholder="0" oninput="sigSteerUi()">
        <span>Steer hi/lo byte</span><span style="white-space:nowrap"><input id="cgSHi" class="cgn"><input id="cgSLo" class="cgn"></span>
      </div>
      <div style="font-size:11px;color:var(--muted);margin-top:6px">Steer id 0 or 0x129: the car's 0x129 steering angle, read with its own layout and only while valid; hi/lo are not used. Another id: a signed 16-bit angle at the hi/lo bytes, 0.1&deg; per bit; hi = lo maps nothing.</div>
      <button onclick="saveSigCfg()" style="margin-top:8px;background:var(--accent);color:#000;border:0;padding:6px 12px;border-radius:4px;cursor:pointer">Save mapping</button>
    </details>
  </div>
)rawliteral"
#if defined(BOARD_TTGO_DISPLAY)
R"rawliteral(
  <div class="row">
    <span class="lbl">TTGO Display</span>
    <label class="sw"><input type="checkbox" id="swDisp" onchange="cmd('disp',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Display Brightness (%)</span>
    <input type="number" id="dispBr" min="0" max="100" style="width:60px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right" onchange="cmd('disp_br',parseInt(this.value))">
  </div>
  <div class="row">
    <span class="lbl">Display Timeout (s)</span>
    <input type="number" id="dispTo" min="0" max="3600" style="width:60px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right" onchange="cmd('disp_to',parseInt(this.value))">
  </div>
)rawliteral"
#endif
R"rawliteral(
  <div class="row">
    <span class="lbl">CAN Dump</span>
    <label class="sw"><input type="checkbox" id="swDump" onchange="cmd('dump',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row" id="mcpRxRow" style="display:none">
    <div style="display:flex;align-items:center;justify-content:space-between">
      <span class="lbl">MCP2515 RX task (experimental)<br><small style="color:var(--muted)">Reads the MCP2515 from its own task into a 256-frame queue, so a slow loop pass no longer loses frames from its two receive buffers.</small></span>
      <label class="sw"><input type="checkbox" id="swMcpRx" onchange="cmd('mcp_rx_task',this.checked)"><span class="sl2"></span></label>
    </div>
    <div id="mcpRxSt" style="font-size:12px;color:var(--muted);margin-top:4px">--</div>
  </div>
)rawliteral"
#if defined(BOARD_LILYGO)
R"rawliteral(
  <div class="row">
    <span class="lbl">Deep Sleep (sec)</span>
    <input type="number" id="numSleep" min="10" max="3600" style="width:60px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right" onchange="cmd('sleep',parseInt(this.value)*1000)">
  </div>
)rawliteral"
#endif
R"rawliteral(
  </div>
</details>
</div>

<!-- Tap capability checker (#125) -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">T</div><h2>Tap Check</h2>
    <span id="capSt" class="pill off" style="margin-left:auto"><span class="pd"></span>Idle</span></div>
  <div class="log-info" style="margin-bottom:8px">
    Listens a few seconds and reports whether each feature can work on the bus this
    device is tapped into. Pure read-only &mdash; nothing is transmitted.
  </div>
  <div id="capBody"></div>
  <button id="capBtn" type="button" class="btn-main btn-blue" onclick="cmd('capability_recheck',true)" style="margin-top:8px">RE-CHECK</button>
</div>

<!-- Black-box incident recorder (#124) -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">R</div><h2>Black-box</h2>
    <span id="bbBadge" class="pill on" style="display:none;margin-left:auto">NEW</span></div>
  <div id="bbNote" class="log-info" style="margin-bottom:10px;display:none">
    Records the key diagnostic CAN IDs around anomalies (aborts, bus-off, manual
    marks) to the device only &mdash; never uploaded. Use the toggle below to enable or disable.
    <button type="button" onclick="bbDismiss()" style="margin-left:6px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:2px 8px;border-radius:4px;cursor:pointer">Got it</button>
  </div>
  <div class="row">
    <span class="lbl">Auto-record</span>
    <label class="sw"><input type="checkbox" id="swBlackbox" onchange="cmd('blackbox_enable',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Status</span>
    <span class="pill off" id="bbSt"><span class="pd"></span>Idle</span>
  </div>
  <div class="row" style="gap:12px">
    <span class="lbl" style="white-space:nowrap">Storage</span>
    <span id="bbStore" style="font-size:.78em;color:var(--text2);text-align:right">--</span>
  </div>
  <div id="bbVolatileWarn" class="log-info" style="display:none;margin:8px 0;color:var(--yellow)">
    &#9888;&#xFE0F; Volatile storage &mdash; download events before power-off; they are lost on reboot.
  </div>
  <div id="bbSaveWarn" class="log-info" style="display:none;margin:8px 0;color:var(--red)">
    &#9888;&#xFE0F; Last save failed &mdash; low memory. The recorded event was dropped.
  </div>
  <button id="bbMark" type="button" class="btn-main btn-blue" onclick="cmd('blackbox_mark',true)" style="margin:8px 0">&#9873; MARK NOW</button>
  <div id="bbList" style="margin-top:4px"></div>
  <button id="bbDelAll" type="button" class="btn-main btn-stop" onclick="bbDeleteAll()" style="margin-top:10px;display:none">DELETE ALL EVENTS</button>
</div>

<!-- Administration -->
<details class="config-section">
  <summary><div class="icon ic-c">A</div><div class="card-head" style="margin:0"><h2>Administration</h2></div></summary>
  <div class="config-body">

<!-- HTTP CAN Log -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">L</div><h2>HTTP CAN Log</h2></div>
  <div class="row">
    <span class="lbl">Stream</span>
    <span class="pill off" id="httpLogSt"><span class="pd"></span>Idle</span>
  </div>
  <div class="row">
    <span class="lbl">Filter IDs</span>
    <input id="httpLogFilter" class="log-filter" type="text" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="0x370, 0x3FD">
  </div>
  <div class="row">
    <span class="lbl">Buffered</span>
    <span id="httpLogBuf" style="font-size:.8em;color:var(--text2)">0 frames</span>
  </div>
  <div class="row">
    <span class="lbl">Dropped</span>
    <span id="httpLogDrop" style="font-size:.8em;color:var(--text2)">0 frames</span>
  </div>
  <div class="row">
    <span class="lbl">Filtered</span>
    <span id="httpLogFiltered" style="font-size:.8em;color:var(--text2)">0 frames</span>
  </div>
  <div id="httpLogInfo" class="log-info">Ready to collect a candump file in this browser.</div>
  <div class="log-actions">
    <button id="btnHttpLog" type="button" class="btn-main btn-blue" onclick="toggleHttpLog()">STREAM LOG AND SAVE</button>
  </div>
</div>

<!-- WiFi Config -->
<div class="card">
  <div class="card-head"><div class="icon ic-c">W</div><h2>WiFi Configuration</h2></div>
  <div class="log-info" style="margin-bottom:10px">
    The device starts its own access point by default. Optionally set a network below; when a network name is set, the device tries to connect to it on boot and starts its own access point if it cannot connect.
  </div>
  <div class="row">
    <span class="lbl">Access Point</span>
    <span style="font-size:.72em;color:var(--text3)">default</span>
  </div>
  <div class="row">
    <span class="lbl">SSID</span>
    <input type="text" id="wifiSsid" maxlength="32" style="width:140px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right">
  </div>
  <div class="row">
    <span class="lbl">Password</span>
    <input type="password" id="wifiPass" maxlength="64" style="width:140px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right">
  </div>
  <div class="row">
    <span class="lbl">Stealth Mode (Hidden)</span>
    <label class="sw"><input type="checkbox" id="swWifiHid"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Connect to WiFi</span>
    <span style="font-size:.72em;color:var(--text3)">optional</span>
  </div>
  <div class="row">
    <span class="lbl">Network Name</span>
    <input type="text" id="wifiStaSsid" maxlength="32" style="width:140px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right">
  </div>
  <div class="row">
    <span class="lbl">Network Password</span>
    <input type="password" id="wifiStaPass" maxlength="64" style="width:140px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right">
  </div>
  <div class="row">
    <span class="lbl">Hostname<br><small style="color:var(--muted)">name shown on your router</small></span>
    <input type="text" id="wifiHost" maxlength="31" placeholder="tesla-fsd" autocomplete="off" autocapitalize="off" spellcheck="false" style="width:140px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right">
  </div>
  <button class="btn-main btn-stop" onclick="saveWifi(this)" style="margin-top:12px">SAVE & RESTART WIFI</button>
</div>

<!-- OTA Update -->
<div class="card">
  <div class="card-head"><div class="icon ic-c">U</div><h2>OTA Firmware Update</h2></div>
  <div style="font-size:.75em;color:var(--text3);margin-bottom:12px;line-height:1.5">
    Upload a .bin firmware file. Device will reboot after a successful update.
  </div>
  <form id="otaForm" enctype="multipart/form-data" style="margin:0">
    <input type="file" id="otaFile" class="ota-file" accept=".bin" onchange="uploadFirmware()">
    <button type="button" class="btn-main btn-blue" id="otaSelectBtn" onclick="selectFirmware(this)">
      SELECT FIRMWARE (.bin)
    </button>
  </form>
  <div id="otaProgress" class="ota-progress">
    <div class="ota-track"><div id="otaBar" class="ota-bar"></div></div>
    <div id="otaStatus" class="ota-status">Preparing...</div>
    <div id="otaBytes" class="ota-bytes"></div>
  </div>
  <div id="otaRollbackInfo" class="ota-info">
    <b style="color:var(--blue)">Partition Safety</b><br>
    OTA writes to the next app partition when available. The new image is kept only after 15 s of runtime; a crash or power cut before that restores the previous firmware. Keep USB reflashing available as a recovery path.
  </div>
</div>

<!-- SD Card -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">S</div><h2>SD Card</h2></div>
  <div class="row">
    <span class="lbl">Dump Status</span>
    <span class="pill off" id="dumpSt"><span class="pd"></span>Idle</span>
  </div>
  <button id="btnFmt" class="btn-main btn-stop" onclick="sdFormat(this)" style="margin-top:8px">FORMAT SD CARD</button>
  <div id="fmtOut" style="font-size:.75em;color:var(--text2);margin-top:8px;display:none"></div>
</div>

<!-- Device Info -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">D</div><h2>Device</h2></div>
  <div class="row">
    <span class="lbl">Firmware</span>
    <span id="fwBuild" style="font-size:.8em;color:var(--text2)">--</span>
  </div>
  <div class="row">
    <span class="lbl">Uptime</span>
    <span id="uptime" style="font-variant-numeric:tabular-nums">--</span>
  </div>
  <div class="row">
    <span class="lbl">Power</span>
    <span id="pwrInfo" style="font-size:.78em;color:var(--text2);text-align:right">--</span>
  </div>
  <div class="row">
    <span class="lbl">WiFi Clients</span>
    <span id="wifiCl">--</span>
  </div>
  <div class="row">
    <span class="lbl">Memory</span>
    <span id="memInfo" style="font-size:.78em;color:var(--text2)">--</span>
  </div>
  <div class="row">
    <span class="lbl">Loop</span>
    <span id="loopInfo" style="font-size:.78em;color:var(--text2);text-align:right">--</span>
  </div>
  <div class="row">
    <span class="lbl">OTA Partition</span>
    <span id="otaPartInfo" style="font-size:.78em;color:var(--text2)">--</span>
  </div>
  <button class="btn-main btn-yellow" onclick="restartDevice(this)" style="margin-top:12px">RESTART DEVICE</button>
</div>

  </div>
</details>

<div class="foot">Tesla FSD ESP32<span id="footId"></span></div>
<div class="foot">Free &amp; open source &middot; <a href="https://fsd.fkey.id/" target="_blank" rel="noopener">support the research</a></div>
</div><!-- /wrap -->

<!-- In-page alert / confirm + command-error toast -->
<div id="uiDlg" class="dlg" role="alertdialog" aria-modal="true" aria-labelledby="uiDlgMsg">
  <div class="dlg-box">
    <div id="uiDlgMsg" class="dlg-msg"></div>
    <div class="auth-actions">
      <button type="button" id="uiDlgNo" class="btn-main btn-stop" onclick="dlgDone(false)">Cancel</button>
      <button type="button" id="uiDlgYes" class="btn-main btn-blue" onclick="dlgDone(true)">OK</button>
    </div>
  </div>
</div>
<div id="uiToast" class="toast" role="alert"></div>

<script>
var ws,rt,busy=0,wifiOnce=false,authHeader='',authAction=null,restartAnchor=null;
var wsRx=0,httpUp=true,stBusy=false,otaBusy=false;
var httpLogAbort=null,httpLogReader=null,httpLogParts=[],httpLogBytes=0,httpLogStarted=0,httpLogRunning=false;
var httpLogName='',httpLogReady=false,httpLogSaveUrl='';
var httpLogAllowed=true;
var HW=['Unknown','Legacy','HW3','HW4'];
var CPT=['US Tesla','EU IEC','GB','IEC CCS'];   // CP_type (0x25D, #228)
var CIRC=326.73;
document.getElementById('deviceHost').textContent=location.host||location.hostname||'192.168.4.1';

// ── In-page dialogs ──
// Captive-portal sheets (iOS/macOS) ignore the native alert / confirm dialogs
// (a confirm just returns false), so prompts are drawn in the page. One at a
// time; later calls queue.
var dlgQ=[],dlgCur=null,dlgPrev=null;
function dlgShow(){
  if(dlgCur||!dlgQ.length)return;
  dlgCur=dlgQ.shift();
  var m=document.getElementById('uiDlgMsg'),no=document.getElementById('uiDlgNo');
  m.textContent='';
  // One block per line: keeps the breaks and lets each line translate on its own.
  String(dlgCur.msg).split('\n').forEach(function(l){
    if(!l)return;
    var p=document.createElement('div');p.textContent=l;m.appendChild(p);
  });
  no.style.display=dlgCur.ask?'':'none';
  dlgPrev=document.activeElement;
  document.getElementById('uiDlg').className='dlg show';
  // Confirms open on Cancel: each one guards a destructive action.
  (dlgCur.ask?no:document.getElementById('uiDlgYes')).focus();
}
function dlgDone(ok){
  var c=dlgCur;if(!c)return;
  dlgCur=null;
  document.getElementById('uiDlg').className='dlg';
  try{if(dlgPrev&&dlgPrev.focus)dlgPrev.focus();}catch(e){}
  dlgPrev=null;
  var f=ok?c.yes:c.no;
  try{if(f)f();}finally{dlgShow();}
}
document.addEventListener('keydown',function(e){
  if(!dlgCur)return;
  var k=e.key||e.keyCode;
  if(k==='Escape'||k==='Esc'||k===27){e.preventDefault();dlgDone(!dlgCur.ask);}
  else if(k==='Tab'||k===9){
    // Keep focus inside the dialog.
    e.preventDefault();
    var no=document.getElementById('uiDlgNo'),yes=document.getElementById('uiDlgYes');
    ((dlgCur.ask&&document.activeElement===yes)?no:yes).focus();
  }
},true);
function uiAlert(msg,done){dlgQ.push({msg:msg,ask:false,yes:done,no:done});dlgShow();}
function uiConfirm(msg,onYes,onNo){dlgQ.push({msg:msg,ask:true,yes:onYes,no:onNo});dlgShow();}
var toastT=0;
function uiToast(msg){
  var t=document.getElementById('uiToast');if(!t)return;
  t.textContent=msg;t.className='toast show';
  clearTimeout(toastT);toastT=setTimeout(function(){t.className='toast';},6000);
}

function initWifi(d){
  if(wifiOnce||d.wifi_ssid===undefined)return;
  wifiOnce=true;
  document.getElementById('wifiSsid').value=d.wifi_ssid||'';
  document.getElementById('wifiPass').value=d.wifi_pass||'';
  document.getElementById('swWifiHid').checked=!!d.wifi_hidden;
  document.getElementById('wifiStaSsid').value=d.wifi_sta_ssid||'';
  document.getElementById('wifiStaPass').value=d.wifi_sta_pass||'';
  document.getElementById('wifiHost').value=d.wifi_hostname||'';
}

function fmt(s){
  var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sc=s%60;
  return h+':'+(m<10?'0':'')+m+':'+(sc<10?'0':'')+sc;
}
function socCol(p){return p>60?'var(--accent)':p>30?'var(--yellow)':'var(--red)';}
// Performance card (read-only, opt-in). Shown state persists per browser.
var perfShow=false;
try{perfShow=localStorage.getItem('perfShow')==='1'}catch(e){}
function perfToggle(on){
  perfShow=!!on;
  try{localStorage.setItem('perfShow',perfShow?'1':'0')}catch(e){}
  var b=document.getElementById('perfBody'),o=document.getElementById('perfOff');
  if(b)b.style.display=perfShow?'':'none';
  if(o)o.style.display=perfShow?'none':'';
}
function perfInit(){
  var sw=document.getElementById('swPerf');
  if(sw)sw.checked=perfShow;
  perfToggle(perfShow);
}
function pfT(ms){return ms>0?(ms/1000).toFixed(2)+'s':'--';}
function setTxt(id,t){var e=document.getElementById(id);if(e)e.textContent=t;}
function renderPerf(d){
  if(!perfShow||!d.perf)return;
  var p=d.perf;
  setTxt('pfSpeed',p.speed_ok?(p.speed||0).toFixed(1)+' km/h':'--');
  // Timers: locked result if present, else the live run time while armed.
  // Without a valid speed (SNA / stale) the run's progress is unknown: no
  // live time and no "braking" then, only the locked results.
  function cell(id,bid,t,best){
    var e=document.getElementById(id);
    setTxt(bid,pfT(best));
    if(!e)return;
    if(t>0){e.textContent=pfT(t);e.style.color='var(--accent)';}
    else if(p.speed_ok&&p.armed&&p.run_ms>0){e.textContent=pfT(p.run_ms);e.style.color='var(--yellow)';}
    else{e.textContent='--';e.style.color='';}
  }
  cell('pf050','pf050b',p.t50,p.best50);
  cell('pf060mph','pf060mphb',p.t60mph,p.best60mph);
  cell('pf0100','pf0100b',p.t100,p.best100);
  var e1=document.getElementById('pf1000');
  if(e1){
    if(p.t1000>0){e1.textContent=pfT(p.t1000);e1.style.color='var(--accent)';}
    else if(p.speed_ok&&p.brake_armed){e1.textContent='braking';e1.style.color='var(--yellow)';}
    else{e1.textContent='--';e1.style.color='';}
  }
  setTxt('pf1000b',pfT(p.best1000));
  // Estimated longitudinal G (no IMU).
  setTxt('pfG',(p.g>=0?'+':'')+(p.g||0).toFixed(2)+'g');
  setTxt('pfGpk','+'+(p.gpk||0).toFixed(2)+' / '+(p.gpkb||0).toFixed(2));
  // Wheel slip (0x175, Party CAN).
  var se=document.getElementById('pfSlip');
  if(se){
    if(p.slip_ok){se.textContent=(p.slip>=0?'+':'')+p.slip.toFixed(1)+'%';se.style.color=Math.abs(p.slip)>5?'var(--red)':'var(--accent)';}
    else if(!p.wheel_seen){se.textContent='n/a';se.style.color='var(--text3)';}
    else{se.textContent='--';se.style.color='';}
  }
  // Power + pack temp (BMS, Vehicle-CAN tap).
  if(d.bms&&d.bms.seen){
    setTxt('pfPow',(d.bms.voltage*d.bms.current/1000).toFixed(1)+' kW');
    // Pack temp + advisory only from a fresh, plausible 0x312 reading (temp_ok).
    var te=document.getElementById('pfTemp');
    if(d.bms.temp_ok){
      var tmax=d.bms.temp_max,tmin=d.bms.temp_min;
      if(te){
        te.textContent=tmin+'~'+tmax+'°C';
        te.style.color=tmax>=45?'var(--red)':tmax>=35?'var(--yellow)':tmin<=5?'var(--yellow)':'var(--accent)';
      }
      setTxt('pfAdv',tmin<=5?'Cold pack — reduced regen and power until warmed':tmax>=45?'Hot pack — power may be limited':'Normal');
    }else{
      if(te){te.textContent='--';te.style.color='';}
      setTxt('pfAdv','--');
    }
  }else{
    setTxt('pfPow','--');setTxt('pfTemp','--');setTxt('pfAdv','No BMS frames (needs Vehicle-CAN tap)');
  }
}
function pill(id,on,txt,warnClass){
  var e=document.getElementById(id);
  if(!e)return;
  e.className='pill '+(warnClass||''+(on?'on':'off'));
  e.innerHTML='<span class="pd"></span>'+txt;
}
function dot(id,on){
  var e=document.getElementById(id);
  if(e)e.className='blinkdot'+(on?' hit':'');
}
function speedText(v,seen,source){
  if(!seen)return '--';
  var src=source?'<small> '+source+'</small>':'';
  return Math.round(v)+' km/h'+src;
}
function updateControlsSummary(d){
  var e=document.getElementById('controlsSummary');
  if(!e)return;
  var items=[];
  if(d.op_mode===1)items.push('Active');
  if(d.ignore_ota)items.push('Ignore OTA');
  if(d.fsd_unlock)items.push('FSD Unlock');
  if(d.hw3_speed_override)items.push('HW3 Speed Override');
  if(d.nag_killer)items.push('NAG Killer');
  if(d.continuous_ap)items.push('Continuous AP');
  if(d.ap_first)items.push('AP-First');
  if(d.bms_output)items.push('BMS');
  if(d.force_fsd)items.push('Force FSD');
  if(d.china_mode)items.push('China');
  if(d.isa_speed_enabled&&d.suppress_speed_chime)items.push('Chime');
  if(d.tlssc_restore)items.push('TLSSC');
  if(d.precondition)items.push('Precond');
  if(d.assist_tlssc_bit38)items.push('TLSSC bit38');
  if(d.accel_mode>0)items.push('Accel '+(['','Chill','Standard','Performance'][d.accel_mode]||'?'));
  if(d.cp_type_eu_rewrite)items.push('EU CP Type');
  if(d.display_enabled)items.push('Display');
  if(d.can_dump)items.push('CAN Dump');
  if(d.mcp_rx_task&&d.mcp_rx_task.on)items.push('MCP2515 RX task');
  e.textContent=items.length?items.join(', '):'Expand to setup';
  e.title=e.textContent;
}
// ── Black-box incident recorder (#124) ──
var bbCaptures=-1,bbNew=false,bbNoteDismissed=false;
try{bbNoteDismissed=localStorage.getItem('bbNote')==='1'}catch(e){}
function bbEsc(s){return String(s==null?'':s).replace(/[<>&"]/g,function(c){return{'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]})}
// The fixed words of a composed line in a text node of their own, which the
// zh layer can translate (it matches whole text nodes).
function sp(t){return '<span>'+t+'</span>';}
function bbDismiss(){bbNoteDismissed=true;try{localStorage.setItem('bbNote','1')}catch(e){}var n=document.getElementById('bbNote');if(n)n.style.display='none';}
function bbSeen(){bbNew=false;var bd=document.getElementById('bbBadge');if(bd)bd.style.display='none';}
function bbSync(d){
  var b=d.blackbox;if(!b)return;
  var sw=document.getElementById('swBlackbox');if(sw&&sw!==document.activeElement)sw.checked=!!b.enabled;
  pill('bbSt',b.enabled,b.armed?'Capturing':(b.enabled?'Armed':'Off'));
  var st=document.getElementById('bbStore');
  if(st)st.innerHTML=bbEsc(b.backend||'?')+' · '+(b.psram?'PSRAM':sp('internal RAM'))+' · '+((b.cap||0).toLocaleString())+' '+sp('frames')+' · '+(b.events||0)+' '+sp('saved');
  var vw=document.getElementById('bbVolatileWarn');if(vw)vw.style.display=b.volatile?'block':'none';
  var sw2=document.getElementById('bbSaveWarn');if(sw2)sw2.style.display=b.save_lowmem?'block':'none';
  var note=document.getElementById('bbNote');if(note)note.style.display=bbNoteDismissed?'none':'block';
  var da=document.getElementById('bbDelAll');if(da)da.style.display=(b.events>0)?'block':'none';
  if(bbCaptures<0){bbCaptures=b.captures;bbRefreshList();}
  else if(b.captures!==bbCaptures){bbCaptures=b.captures;bbNew=true;bbRefreshList();}
  var bd=document.getElementById('bbBadge');if(bd)bd.style.display=bbNew?'inline-block':'none';
}
function bbRefreshList(){
  fetch('/blackbox/list').then(function(r){return r.json()}).then(function(a){
    var el=document.getElementById('bbList');if(!el)return;
    if(!a||!a.length){el.innerHTML='<div style="font-size:.78em;color:var(--text3);padding:6px 0">No events recorded yet.</div>';return;}
    // Newest first: by event number (0 for names from before it existed).
    a.sort(function(x,y){return ((y.seq||0)-(x.seq||0))||(y.name||'').localeCompare(x.name||'')});
    var h='';
    a.forEach(function(ev){
      var s=ev.summary||{},nm=encodeURIComponent(ev.name);
      // Events saved by older firmware name a manual mark as a transition that
      // did not happen ("MANUAL 2->2 @ t=..."): show the state once.
      var det=String(s.detail||s.trigger||'event').replace(/^(\S+) (\d+)->\2 @/,'$1 AP state $2 @');
      h+='<div style="border-top:1px solid var(--border);padding:8px 0">';
      h+='<div style="font-size:.8em;color:var(--text);font-family:monospace">'+bbEsc(det)+'</div>';
      h+='<div style="font-size:.72em;color:var(--text3);margin:2px 0">'+bbEsc(s.hw||'')+' · '+(s.frames||0)+' '+sp('frames')+' · '+sp((s.buses&&s.buses.dual_can)?'dual-CAN':'single')+'</div>';
      h+='<div style="display:flex;gap:6px;margin-top:4px">';
      h+='<a class="btn-main btn-blue" style="padding:4px 10px;font-size:.7em;flex:0" href="/blackbox/get?name='+nm+'&type=log" onclick="bbSeen()">.log</a>';
      h+='<a class="btn-main btn-blue" style="padding:4px 10px;font-size:.7em;flex:0" href="/blackbox/get?name='+nm+'&type=json" onclick="bbSeen()">.json</a>';
      h+='<button type="button" class="btn-main btn-stop" style="padding:4px 10px;font-size:.7em;flex:none;width:auto;margin-left:auto" onclick="bbDelete(\''+bbEsc(ev.name)+'\')">del</button>';
      h+='</div></div>';
    });
    el.innerHTML=h;
  }).catch(function(){});
}
function bbDelete(n){cmd('blackbox_delete',n);setTimeout(bbRefreshList,400);}
function bbDeleteAll(){uiConfirm('Delete all recorded events from the device?',function(){cmd('blackbox_delete_all',true);bbSeen();setTimeout(bbRefreshList,400);});}
// ── Tap capability checker (#125) ──
// Per verdict (FSDCapVerdict): OK, dual-CAN, missing, present but nothing valid.
var CAP_MSG={
  nag_killer:['0x370 + DAS state present — gates correctly',
              '0x370 here but no DAS state — dual-CAN recommended (read DAS on a second tap)',
              'no 0x370 on this tap — wrong bus for the nag killer'],
  ap_first:['DAS state readable',
            'dual-CAN',
            'no DAS state here (need 0x399/0x39B)'],
  fsd_activation:['AP control frame present (0x3FD/0x3EE)',
                  '',
                  'no AP control frame to modify here'],
  soft_engage:['0x129 steering angle present',
               '',
               'no 0x129 — degrades to AP-First-only',
               '0x129 present, no valid angle yet'],
  body_control:['Vehicle/body bus reachable — mirror/window/lights frames present (RX only; not proof of actuation)',
                '',
                'not on this tap — needs the X179 A-pillar (Vehicle-bus) tap']};
function capRow(name,key,v){
  var col=v===0?'var(--accent)':v===1?'var(--yellow)':v===3?'var(--text2)':'var(--red)';
  var sym=v===0?'✓':v===1?'⚠':v===3?'–':'✗';
  var msg=(CAP_MSG[key]||[])[v]||'';
  return '<div style="display:flex;gap:8px;padding:5px 0;border-top:1px solid var(--border)">'
    +'<span style="color:'+col+';font-weight:700;flex:0 0 14px">'+sym+'</span>'
    +'<span style="flex:0 0 112px;white-space:nowrap;font-size:.8em;color:var(--text)">'+name+'</span>'
    +'<span style="font-size:.73em;color:var(--text3);line-height:1.3">'+msg+'</span></div>';
}
// bus_hint (FSDCapBusHint), the same as the firmware's English "hint" text.
var CAP_BUS_HINT=['','looks Party-like','looks Chassis/Vehicle-like'];
// vehicle_hint (0x3C2) is a hint about the bus, not a pass/fail, so no ✓/✗.
var CAP_VEH_HINT=['no 0x3C2 and no body frames — no hint',
  '0x3C2 seen — likely the Vehicle CAN pair (a hint, not a verdict)',
  'body frames without 0x3C2 — likely a forwarded subset, not the Vehicle CAN pair (a hint, not a verdict)'];
function capHintRow(name,v){
  return '<div style="display:flex;gap:8px;padding:5px 0;border-top:1px solid var(--border)">'
    +'<span style="color:var(--text3);font-weight:700;flex:0 0 14px">i</span>'
    +'<span style="flex:0 0 112px;white-space:nowrap;font-size:.8em;color:var(--text)">'+name+'</span>'
    +'<span style="font-size:.73em;color:var(--text3);line-height:1.3">'+(CAP_VEH_HINT[v]||'')+'</span></div>';
}
function capSync(d){
  var c=d.capability;if(!c)return;
  pill('capSt',c.state===2,c.state===1?'Listening…':(c.state===2?'Done':'Idle'));
  var btn=document.getElementById('capBtn');if(btn)btn.disabled=(c.state===1);
  var el=document.getElementById('capBody');if(!el)return;
  if(c.state===0){el.innerHTML='<div style="font-size:.78em;color:var(--text3);padding:6px 0">Connect to run a check, or press Re-check.</div>';return;}
  if(c.state===1){el.innerHTML='<div style="font-size:.8em;color:var(--text2);padding:6px 0">Listening on the bus… '+Math.ceil((c.ms_left||0)/1000)+'s</div>';return;}
  var buses=c.buses||[];
  if(!buses.length){el.innerHTML='<div style="font-size:.8em;color:var(--yellow);padding:6px 0">No frames seen — check wiring / that the car is awake.</div>';return;}
  var dual=buses.length>1;
  var h='';
  buses.forEach(function(b){
    h+='<div style="margin-top:8px">';
    if(dual)h+='<div style="font-size:.78em;color:var(--text2);font-weight:600;margin-bottom:2px">'+bbEsc(b.bus)+' &middot; '+(b.frames||0)+' '+sp('frames')+'</div>';
    h+=capRow('Nag killer','nag_killer',b.nag_killer);
    h+=capRow('AP-First','ap_first',b.ap_first);
    h+=capRow('FSD activate','fsd_activation',b.fsd_activation);
    h+=capRow('Soft Engage','soft_engage',b.soft_engage);
    h+=capRow('Body/comfort bus','body_control',b.body_control);
    if(b.body_control===0){
      // Which frames were seen comes per bus under "ids" (capability_status_json()).
      var ids=b.ids||{},seen=[];
      if(ids.body_ui)seen.push('mirror/lock/horn');
      if(ids.body_window)seen.push('windows');
      if(ids.body_lights)seen.push('lights/turn');
      if(ids.body_door)seen.push('mirror read-back');
      if(seen.length)h+='<div style="font-size:.7em;color:var(--text3);padding-top:3px">'+sp('Reachable frames:')+' '+seen.map(sp).join(sp(', '))+' '+sp('(presence only — not proof injection actuates them).')+'</div>';
    }
    if(typeof b.vehicle_hint==='number')h+=capHintRow('Bus hint',b.vehicle_hint);
    var bh=CAP_BUS_HINT[b.bus_hint]||'';
    var hint=bh?(sp('Best guess:')+' '+sp(bh)+' '+sp("— confirm with your car's wiring diagram (Service Mode → CAN Port where available)")):'';
    if(hint)h+='<div style="font-size:.7em;color:var(--text3);padding-top:5px">'+hint+'</div>';
    if(b.hw_unconfirmed)h+='<div style="font-size:.7em;color:var(--yellow);padding-top:3px">HW unconfirmed — 0x399 reading assumed; verdict may change once HW is detected.</div>';
    h+='</div>';
  });
  el.innerHTML=h;
}
function ring(p){
  var b=document.getElementById('socBar');
  b.style.strokeDashoffset=CIRC-(CIRC*Math.min(p,100)/100);
  b.style.stroke=socCol(p);
}

function focusId(){var a=document.activeElement;return a?a.id:'';}
// Each section is guarded on its own, so a bad field in one cannot blank the
// rest of the page. Device goes first: it has nothing to wait for.
function sect(name,fn,d){try{fn(d);}catch(x){console.error('upd '+name,x);}}
function upd(d){
  if(!d || Date.now() < busy) return;
  sect('device',updDevice,d);
  sect('status',updStatus,d);
  sect('controls',updControls,d);
  sect('can',updCan,d);
  sect('battery',updBattery,d);
  sect('tpms',updTpms,d);      // read-only
  sect('perf',renderPerf,d);   // read-only, opt-in
}
function updStatus(d){
  // Status
  var apActive=!!d.ap_active;
  pill('fsdSt', apActive, apActive?'Active':'Waiting');
  pill('opMode', d.op_mode===1, d.op_mode===1?'Active':'Listen-Only');

  var hwEl=document.getElementById('hwVer');
  if(hwEl){
    hwEl.className='pill '+(d.hw_version>0?'on':'off');
    hwEl.innerHTML='<span class="pd"></span>'+(HW[d.hw_version]||'?');
  }

  // HW3 speed read-out (#209): car value -> value forwarded on 0x3FD; -1 = not seen
  var h3r=document.getElementById('rowHw3Spd');
  if(h3r){
    h3r.style.display=d.hw_version===2?'flex':'none';
    var h3v=function(c,o){return (c===undefined||c<0)?'--':c+'\u2192'+o;};
    var h3t=document.getElementById('hw3Spd');
    if(h3t)h3t.textContent='Profile '+h3v(d.hw3_car_profile,d.hw3_sent_profile)+' \u00b7 Offset '+h3v(d.hw3_car_offset,d.hw3_sent_offset)+' \u00b7 Follow '+((d.follow_distance===undefined||d.follow_distance<0)?'--':d.follow_distance);
  }

  pill('nagSt', d.nag_killer, d.nag_killer?'ON':'OFF');
  pill('canVeh', d.can_vehicle_detected, d.can_vehicle_detected?'Detected':'No CAN Traffic');
  // Charge-port type read-out (0x25D, #228): the car's own type, -- until seen,
  // marked stale once CP_status has stopped arriving.
  var cpEl=document.getElementById('cpType');
  if(cpEl) cpEl.textContent=(d.cp_type===undefined||d.cp_type<0)?'--':(CPT[d.cp_type]||('type '+d.cp_type));
  var cpSx=document.getElementById('cpStale');
  if(cpSx) cpSx.style.display=(d.cp_stale&&d.cp_type>=0)?'inline':'none';
  pill('bmsSt', d.bms && d.bms.seen, (d.bms && d.bms.seen)?'Live':'Waiting Frames');
  var bF=document.getElementById('bmsFrames');
  if(bF) bF.textContent='HV:'+(d.bms_hv_seen||0)+' SOC:'+(d.bms_soc_seen||0)+' TH:'+(d.bms_thermal_seen||0);

  // OTA banner
  var otaB=document.getElementById('otaBanner');
  if(otaB){
    otaB.style.display=d.ota?'block':'none';
    if(d.ota) otaB.innerHTML=d.ignore_ota?'&#9888;&#xFE0F; OTA UPDATE IN PROGRESS &mdash; TX ALLOWED BY IGNORE OTA':'&#9888;&#xFE0F; OTA UPDATE IN PROGRESS &mdash; CAN TX SUSPENDED';
  }

  // Autopark banner (#180) — TX paused during an in-car Autopark episode
  var apB=document.getElementById('autoparkBanner');
  if(apB) apB.style.display=d.autopark_block?'block':'none';

  // Signal Map watchdog banner (#100)
  var smW=document.getElementById('sigmapWarn');
  if(smW) smW.style.display=d.signal_map_das_missing?'block':'none';

  // 14.x firmware warning banner
  var w14x=document.getElementById('warn14x');
  if(w14x) w14x.style.display=d.firmware_14x_warning?'block':'none';
}
function updControls(d){
  // Mode button
  var act=d.op_mode===1;
  var btn=document.getElementById('btnMode');
  if(btn){
    btn.textContent=act?'Deactivate':'Activate';
    btn.className='btn-main '+(act?'btn-stop':'btn-act');
  }

  // Switches sync
  if(document.getElementById('swIgnoreOta')) document.getElementById('swIgnoreOta').checked=d.ignore_ota;
  // Manual HW selection (#110) — don't fight the user while the menu is open.
  var hwSel=document.getElementById('selHwOverride');
  if(hwSel && d.hw_override!==undefined && document.activeElement!==hwSel) hwSel.value=String(d.hw_override);
  if(document.getElementById('swFsdUnlock')) document.getElementById('swFsdUnlock').checked=d.fsd_unlock;
  if(document.getElementById('swHw3Spd')) document.getElementById('swHw3Spd').checked=d.hw3_speed_override;
  if(document.getElementById('swNag')) document.getElementById('swNag').checked=d.nag_killer;
  if(document.getElementById('swContinuousAp')) document.getElementById('swContinuousAp').checked=d.continuous_ap;
  if(document.getElementById('swApFirst')) document.getElementById('swApFirst').checked=d.ap_first;
  if(document.getElementById('swApFe')) document.getElementById('swApFe').checked=d.ap_first_edge;
  if(document.getElementById('swApMi')) document.getElementById('swApMi').checked=d.ap_first_minimal;
  if(document.getElementById('swNagF')) document.getElementById('swNagF').checked=d.nag_faithful;
  if(document.getElementById('swSoft')) document.getElementById('swSoft').checked=d.soft_engage;
  if(document.getElementById('swNagB')) document.getElementById('swNagB').checked=d.nag_burst;
  if(document.getElementById('swAbrt')) document.getElementById('swAbrt').checked=d.abort_guard;
  if(d.cfg_das_id!==undefined) setSig(d);
  if(document.getElementById('swBms')) document.getElementById('swBms').checked=d.bms_output;
  if(document.getElementById('swFsd')) document.getElementById('swFsd').checked=d.force_fsd;
  if(document.getElementById('swChina')) document.getElementById('swChina').checked=d.china_mode;
  if(document.getElementById('swChime')) document.getElementById('swChime').checked=d.suppress_speed_chime;
  if(document.getElementById('swPrecond')) document.getElementById('swPrecond').checked=d.precondition;
  if(document.getElementById('rowChime')) document.getElementById('rowChime').style.display=d.isa_speed_enabled?'flex':'none';
  if(document.getElementById('swTlssc')) document.getElementById('swTlssc').checked=d.tlssc_restore;
  if(document.getElementById('swSummon')) document.getElementById('swSummon').checked=d.summon_unlock;
  if(document.getElementById('swCog')) document.getElementById('swCog').checked=d.continue_on_green;
  if(document.getElementById('swTlssc38')) document.getElementById('swTlssc38').checked=d.assist_tlssc_bit38;
  if(document.getElementById('swRhd')) document.getElementById('swRhd').checked=d.assist_rhd_override;
  if(document.getElementById('swTelOff')) document.getElementById('swTelOff').checked=d.assist_telemetry_off;
  var apmv3Sel=document.getElementById('selApmv3');
  if(apmv3Sel && d.apmv3_branch!==undefined && document.activeElement!==apmv3Sel) apmv3Sel.value=String(d.apmv3_branch);
  // Acceleration Mode (#211): setting + car map -> map going out. Mode names as
  // the car's own UI has them: pedal map 1 is Standard (SPORT in older signal
  // lists). The fixed words sit in spans of their own for the i18n layer.
  var acSel=document.getElementById('selAccel');
  if(acSel && d.accel_mode!==undefined && document.activeElement!==acSel) acSel.value=String(d.accel_mode);
  var acSt=document.getElementById('accelSt');
  if(acSt && d.accel_car_map!==undefined){
    var ACM=['CHILL','STANDARD','PERFORMANCE'], at;
    if(d.accel_car_map<0) at=d.accel_bad>0?'0x334 not recognised ('+d.accel_bad+' rejected)':'no 0x334 on this bus yet';
    else if(d.accel_sent_map===d.accel_car_map) at=sp('Car')+' '+ACM[d.accel_car_map]+' \u00b7 '+sp('pass-through');
    else at=sp('Car')+' '+ACM[d.accel_car_map]+' \u2192 '+sp('sent')+' '+ACM[d.accel_sent_map];
    // A new mode waits for standstill while the latched one keeps going out:
    // name the waiting mode, so it is not read as the one being sent.
    if(d.accel_pending) at+=' \u00b7 '+(ACM[d.accel_mode-1]||'?')+' '+sp('pending, applies at standstill');
    else if(d.accel_mode>0 && !d.tx_allowed) at+=' \u00b7 '+sp('TX off');
    acSt.innerHTML=at;
  }
  // EU Charge-Port Type (0x25D, #228): toggle + what the car reports and whether
  // the rewrite is active or held off (and by which guard).
  if(document.getElementById('swCpType')) document.getElementById('swCpType').checked=d.cp_type_eu_rewrite;
  var cpSt=document.getElementById('cpTypeSt');
  if(cpSt){
    var ct;
    if(d.cp_type===undefined||d.cp_type<0) ct='no CP_status on this tap (the 8-byte 0x25D, Vehicle CAN only)';
    else if(d.cp_stale) ct='Stale: no CP_status from the car for '+(d.cp_age_s||0)+' s';
    else{
      ct='Car '+(CPT[d.cp_type]||('type '+d.cp_type));
      if(d.cp_type_eu_rewrite){
        if(d.cp_type!==0) ct+=' \u00b7 already EU, not rewritten';
        else if(!d.tx_allowed) ct+=' \u00b7 TX off';
        else if(d.cp_plug===1) ct+=' \u00b7 held \u2014 cable plugged in';
        else if(d.cp_plug===2) ct+=' \u00b7 held \u2014 plug state unknown';
        else ct+=' \u2192 IEC CCS';
        if(d.cp_type_tx>0) ct+=' ('+d.cp_type_tx+' sent)';
      }
    }
    cpSt.textContent=ct;
  }
  // MCP2515 receive task (opt-in): queue depth reached, frames lost on a full
  // queue, and the chip's RX overflows the task saw. Only builds with an
  // MCP2515 send mcp_rx_task; the row stays hidden on the others.
  var mr=d.mcp_rx_task,mrSt=document.getElementById('mcpRxSt');
  var mrRow=document.getElementById('mcpRxRow');
  if(mrRow) mrRow.style.display=mr?'block':'none';
  if(mr&&document.getElementById('swMcpRx')) document.getElementById('swMcpRx').checked=mr.on;
  if(mr&&mrSt){
    var mw=function(t){return '<span style="white-space:nowrap">'+t+'</span>';};
    if(mr.run) mrSt.innerHTML=mw(mr.int?'Running, INT pin':'Running, 1 ms poll')+' \u00b7 '+mw('queue max')+'\u00a0'+mr.max+'/'+mr.len+' \u00b7 '+mw('lost on a full queue')+'\u00a0'+mr.drop+' \u00b7 '+mw('chip overflows')+'\u00a0'+mr.ovf;
    else mrSt.innerHTML=mw(mr.on?'Waiting for the MCP2515':'Off (read from the loop)');
  }
  if(document.getElementById('swTrkMode')) document.getElementById('swTrkMode').checked=d.track_mode_inject;
  if(document.getElementById('trkRot')&&focusId()!=='trkRot'&&d.track_rotation_pct!==undefined){document.getElementById('trkRot').value=d.track_rotation_pct;var _tr=document.getElementById('trkRotV');if(_tr)_tr.textContent=d.track_rotation_pct;}
  if(document.getElementById('trkStab')&&focusId()!=='trkStab'&&d.track_stability_pct!==undefined){document.getElementById('trkStab').value=d.track_stability_pct;var _ts=document.getElementById('trkStabV');if(_ts)_ts.textContent=d.track_stability_pct;}
  if(document.getElementById('swTrkPC')) document.getElementById('swTrkPC').checked=d.track_post_cooling;
  if(document.getElementById('swTrkCO')) document.getElementById('swTrkCO').checked=d.track_cmp_overclock;
  if(document.getElementById('swDisp')) document.getElementById('swDisp').checked=!!d.display_enabled;
  if(focusId()!=='dispBr' && document.getElementById('dispBr'))
    document.getElementById('dispBr').value=d.display_brightness!==undefined?d.display_brightness:50;
  if(focusId()!=='dispTo' && document.getElementById('dispTo'))
    document.getElementById('dispTo').value=d.display_timeout_s!==undefined?d.display_timeout_s:60;
  if(document.getElementById('swDump')) document.getElementById('swDump').checked=!!d.can_dump;

  if(focusId()!=='numSleep' && document.getElementById('numSleep'))
    document.getElementById('numSleep').value=Math.floor((d.sleep_ms||0)/1000);

  updateControlsSummary(d);
  pill('dumpSt',d.can_dump,d.can_dump?'Recording':'Idle');
}
// CAN Bus card. The counts are since the last reset (#252); the device keeps
// the baseline, so every open dashboard shows the same reset.
var canLast=null;
function canStats(d){
  if(document.getElementById('rxCnt')) document.getElementById('rxCnt').textContent=(d.rx_count||0).toLocaleString();
  if(document.getElementById('txCnt')) document.getElementById('txCnt').textContent=(d.tx_count||0).toLocaleString();
  if(document.getElementById('crcErr')) document.getElementById('crcErr').textContent=d.crc_errors||0;
  // One span per label, so each one translates on its own (and stays whole).
  var cs=document.getElementById('crcSplit'),nw=function(t){return '<span style="white-space:nowrap">'+t+'</span>';};
  if(cs) cs.innerHTML=nw('RX missed')+'\u00a0'+(d.rx_missed_count||0)+' · '+nw('bus')+'\u00a0'+(d.bus_error_count||0)+' · '+nw('TX fail')+'\u00a0'+(d.tx_failed_count||0);
  if(document.getElementById('fps')) document.getElementById('fps').textContent=(d.fps||0.0).toFixed(1);
  var rs=d.can_reset_s;
  setTxt('canSinceL',rs>=0?'Since reset':'Since boot');
  setTxt('canSince',fmt(rs>=0?rs:(d.uptime_s||0)));
  // Per bus (#253): only boards with two controllers send can_buses.
  var g=document.getElementById('canBuses'),b=d.can_buses;
  if(!g)return;
  if(!b||b.length<2){g.style.display='none';return;}
  var n=function(x,k){return x.up?(x[k]||0).toLocaleString():'--';};
  var rows=[['RX Frames','rx'],['TX Frames','tx'],['RX missed','rx_missed'],['Bus errors','bus_error'],['TX fail','tx_failed']];
  var h='<span></span>';
  b.forEach(function(x){
    h+='<span class="th">'+bbEsc(x.name)+' · '+bbEsc(x.drv)
      +(x.up?'':'<br><span style="color:var(--yellow)">not detected</span>')+'</span>';
  });
  rows.forEach(function(r){
    h+='<span class="tu">'+r[0]+'</span>';
    b.forEach(function(x){h+='<span>'+n(x,r[1])+'</span>';});
  });
  g.style.gridTemplateColumns='auto repeat('+b.length+',1fr)';
  g.innerHTML=h;
  g.style.display='';
}
function canReset(){
  uiConfirm('Reset the CAN Bus counters?\nRX, TX, the error counts and frames/s start again from zero on every open dashboard. Black-box, CAN captures and Tap Check keep their own counts.',function(){
    cmd('can_counters_reset',true);
    // Pushes are held for a moment after a command: show the zeros now.
    if(canLast)canStats({can_reset_s:0,can_buses:(canLast.can_buses||[]).map(function(x){
      return {name:x.name,drv:x.drv,up:x.up};
    })});
  });
}
function updCan(d){
  canLast=d;
  canStats(d);
  httpLogAllowed=true; // capture works in both modes — needed to log through an Activate (#108)
  if(!httpLogRunning)setHttpLogUi(false);
  if(!httpLogRunning){
    if(httpLogReady){
      pill('httpLogSt',true,'Ready');
    }else{
      pill('httpLogSt', d.http_can_stream && d.http_can_stream.active,
        httpLogAllowed?((d.http_can_stream && d.http_can_stream.active)?'Streaming':'Idle'):'Disabled');
    }
  }
  if(!httpLogRunning&&!httpLogReady&&document.getElementById('httpLogInfo')&&!httpLogAllowed)
    logInfo('HTTP CAN log is available only in Listen-Only mode.','var(--yellow)');
  if(document.getElementById('httpLogBuf'))
    document.getElementById('httpLogBuf').textContent=((d.http_can_stream&&d.http_can_stream.buffered)||0)+' frames';
  if(document.getElementById('httpLogDrop'))
    document.getElementById('httpLogDrop').textContent=((d.http_can_stream&&d.http_can_stream.dropped)||0)+' frames / rx-missed '+((d.http_can_stream&&d.http_can_stream.rx_missed)||0);
  if(document.getElementById('httpLogFiltered'))
    document.getElementById('httpLogFiltered').textContent=((d.http_can_stream&&d.http_can_stream.filtered)||0)+' frames';
}
function updBattery(d){
  // Battery
  // Car-displayed SOC (0x33A) can arrive without the BMS frames; shown when decoded.
  var cs=d.bms&&d.bms.car_soc!=null?d.bms.car_soc:null;
  var cr=document.getElementById('rowCarSoc');
  if(cr) cr.style.display=cs==null?'none':'';
  setTxt('bCarSoc',cs==null?'--':cs+'%');
  if(d.bms && d.bms.seen){
    var sn=document.getElementById('bSoc');
    if(sn){
      sn.textContent=d.bms.soc.toFixed(0)+'%';
      sn.style.color=socCol(d.bms.soc);
    }
    ring(d.bms.soc);
    if(document.getElementById('bVolt')) document.getElementById('bVolt').textContent=d.bms.voltage.toFixed(0)+'V';
    var ce=document.getElementById('bCurr');
    if(ce){
      ce.textContent=(d.bms.current>=0?'+':'')+d.bms.current.toFixed(1)+'A';
      ce.style.color=d.bms.current>=0?'var(--accent)':'var(--red)';
    }
    if(document.getElementById('bTemp')) document.getElementById('bTemp').textContent=d.bms.temp_ok?d.bms.temp_min+'~'+d.bms.temp_max+'\u00b0C':'--';
  }
}
// Tyre pressure (read-only, 0x219). Columns are named as corners when every
// slot has its own: the 5-byte form's location field, or the 7-byte form's
// fixed FL, FR, RL, RR (#237). Otherwise they stay in the order the car sends.
// The card shows up once 0x219 has been seen; its switch and the unit are kept
// per browser, as the Performance card's switch is (#237).
var TPOS=['FL','FR','RL','RR'];
var tpShow=true,tpUnit='bar',tpLast=null;
try{tpShow=localStorage.getItem('tpmsShow')!=='0';tpUnit=localStorage.getItem('tpmsUnit')==='psi'?'psi':'bar';}catch(e){}
function tpToggle(on){
  tpShow=!!on;
  try{localStorage.setItem('tpmsShow',tpShow?'1':'0')}catch(e){}
  var b=document.getElementById('tpBody'),o=document.getElementById('tpOff');
  if(b)b.style.display=tpShow?'':'none';
  if(o)o.style.display=tpShow?'none':'';
}
function tpSetUnit(u){
  tpUnit=u==='psi'?'psi':'bar';
  try{localStorage.setItem('tpmsUnit',tpUnit)}catch(e){}
  if(tpLast)updTpms(tpLast);
}
function tpInit(){
  var sw=document.getElementById('swTpms'),su=document.getElementById('selTpUnit');
  if(sw)sw.checked=tpShow;
  if(su)su.value=tpUnit;
  tpToggle(tpShow);
}
// Age of a reading: seconds, then minutes, then h:mm:ss.
function ageTxt(s){s=Math.max(0,Math.floor(s||0));return s<60?s+' s':s<3600?Math.floor(s/60)+' min':fmt(s);}
function updTpms(d){
  var c=document.getElementById('tpmsCard'),t=d.tpms,i;
  if(!c)return;
  if(!t||!t.seen){c.style.display='none';return;}
  c.style.display='';
  tpLast=d;
  var mb=t.mbar||[],tc=t.temp||[],ps=t.pos||[],have=false,anyT=false;
  for(i=0;i<4;i++){if(mb[i]!=null)have=true;if(tc[i]!=null)anyT=true;}
  var known=t.dlc===5||t.dlc===7,stale=have&&!!t.stale;
  pill('tpmsSt',have&&!stale,!have?'No reading':stale?'Stale':'Receiving');
  var ord=[],used={};
  for(i=0;i<4;i++){var p=ps[i];if(p==null||p<0||p>3||used[p])break;used[p]=1;ord[p]=i;}
  var named=i===4;
  if(!named)ord=[0,1,2,3];
  var bar=function(m){return m==null?'--':(Math.round(m/10)/100).toFixed(2);};
  var psi=function(m){return m==null?'--':(m*0.0145038).toFixed(1);};
  var u=tpUnit==='psi'?psi:bar;
  var h='<span></span>',ru='<span class="tu">'+tpUnit+'</span>',rt='<span class="tu">\u00b0C</span>';
  ord.forEach(function(k,n){
    h+='<span class="th">'+(named?TPOS[n]:String(n+1))+'</span>';
    ru+='<span>'+u(mb[k])+'</span>';
    rt+='<span>'+(tc[k]==null?'--':tc[k])+'</span>';
  });
  var g=document.getElementById('tpGrid');
  if(g){g.innerHTML=h+ru+(anyT?rt:'');g.className='tg'+(stale?' stale':'');}
  // When the shown values were last updated, yellow once they count as stale.
  var ar=document.getElementById('rowTpAge'),ae=document.getElementById('tpAge');
  if(ar)ar.style.display=have?'flex':'none';
  if(have&&ae){ae.textContent=ageTxt(t.age_s);ae.style.color=stale?'var(--yellow)':'';}
  var tg=(t.target||[]).filter(function(v){return v!=null;});
  if(tg.length===2&&tg[0]===tg[1])tg=[tg[0]];
  var tr=document.getElementById('rowTpTgt');
  if(tr)tr.style.display=tg.length?'flex':'none';
  if(tg.length)setTxt('tpTgt',tg.map(u).join(' / ')+' '+tpUnit);
  var show=function(id,on){var e=document.getElementById(id);if(e)e.style.display=on?'':'none';};
  show('tpOrder',have&&!named);
  show('tpNone',!have&&known);
  show('tpForm',!known);
}
function updDevice(d){
  // Device
  if(document.getElementById('fwBuild') && d.fw_build) document.getElementById('fwBuild').textContent=(d.fw_version?d.fw_version+' \u00b7 ':'')+d.fw_build;
  if(d.board) setTxt('footId',' \u00b7 '+d.board+(d.fw_version?' \u00b7 '+d.fw_version:''));
  if(document.getElementById('uptime')) document.getElementById('uptime').textContent=fmt(d.uptime_s||0);
  if(document.getElementById('wifiCl')) document.getElementById('wifiCl').textContent=d.wifi_clients||0;
  // Lowest free loop-task stack since boot + heap now / lowest. Red under 1 KB of stack.
  var me=document.getElementById('memInfo');
  if(me && d.stack_free!==undefined){
    var kb=function(b){return ((b||0)/1024).toFixed(1)+' KB';};
    me.textContent='stack '+kb(d.stack_free)+' \u00b7 heap '+kb(d.heap_free)+' (min '+kb(d.heap_min)+')';
    me.style.color=d.stack_free<1024?'var(--red)':'var(--text2)';
  }
  // Longest loop() pass, the stage that took most of it, and passes over
  // 25 ms: since the CAN Bus card's reset when there was one, else since boot.
  var lo=document.getElementById('loopInfo'),lp=d.loop;
  if(lo && lp){
    var lr=d.can_reset_s>=0,lm=lr?lp.r_max_us:lp.max_us,ls=function(t){return '<span>'+t+'</span>';};
    lo.innerHTML=ls('longest')+' '+(lm/1000).toFixed(1)+' ms ('+(lr?lp.r_stage:lp.stage)+') \u00b7 '+(lr?lp.r_slow:lp.slow)+' '+ls('over 25 ms')+(lr?' \u00b7 '+ls('since CAN reset'):'');
    lo.style.color=lm>25000?'var(--yellow)':'var(--text2)';
  }
  // Power / reset log (#218). Codes mirror power_log.h (reset) and FsdPwrVerdict
  // (12V). Each phrase sits in its own span so the i18n layer can translate it.
  var pe=document.getElementById('pwrInfo');
  if(pe && d.reset_reason!==undefined){
    var RST=['Unknown reset','Power-on','Brownout','Panic','Watchdog','Software restart','Deep-sleep wake','External reset','USB reset'];
    var PV=['Not known yet','Inconclusive (car never slept)','Switched (dies with the car)','Always-on (outlives the car)','Brownout (check wiring)','Crash (supply not implicated)','Inconclusive (USB powered)'];
    var PVCOL=['var(--text2)','var(--text2)','var(--accent)','var(--yellow)','var(--red)','var(--text2)','var(--text2)'];
    var rc=d.reset_reason,vc=d.power_verdict;
    var h='<span>'+(RST[rc]||'?')+'</span> · 12V: <span style="color:'
      +(PVCOL[vc]||'var(--text2)')+'">'+(PV[vc]||'?')+'</span>';
    if(d.restart_count!==undefined) h+=' · <span>restarts since power-on</span> '+d.restart_count;
    pe.innerHTML=h;
  }
  var partEl=document.getElementById('otaPartInfo');
  if(partEl && d.ota_partition){
    var p=d.ota_partition;
    var stateStr=(p.state===0)?'New':(p.state===1)?'Pending':(p.state===2)?'Valid':(p.state===3)?'Invalid':'State '+p.state;
    partEl.textContent=p.running+' ('+stateStr+') - '+(p.has_ota?'OTA capable':'No OTA partition');
    var info=document.getElementById('otaRollbackInfo');
    if(info && !p.has_ota){
      info.innerHTML='<b style="color:var(--red)">No OTA Partition</b><br>This build appears to be running from a factory/single app partition. Use an OTA partition table before relying on Web updates.';
    }
  }
}

function uploadFirmware(){
  var input=document.getElementById('otaFile');
  var file=input.files[0];
  if(!file)return;
  function drop(){input.value='';}
  if(!file.name.endsWith('.bin')){drop();uiAlert('Error: Please select a .bin firmware file');return;}
  var MAX_SIZE=16*1024*1024;
  if(file.size>MAX_SIZE){drop();uiAlert('Error: Firmware file too large (max 16MB)');return;}
  var kb=Math.round(file.size/1024);
  function ask(){uiConfirm('Flash firmware: '+file.name+' ('+kb+' KB)?\n\nDevice will reboot after update.',function(){otaSend(input,file);},drop);}
  if(file.size<32768)uiConfirm('Warning: This file is very small ('+kb+' KB).\nAre you sure it is a valid ESP32 firmware?',ask,drop);
  else ask();
}
function otaSend(input,file){
  var prog=document.getElementById('otaProgress'),bar=document.getElementById('otaBar'),status=document.getElementById('otaStatus'),bytes=document.getElementById('otaBytes'),btn=document.getElementById('otaSelectBtn');
  prog.style.display='block';bar.style.width='0%';bar.style.background='var(--accent)';status.textContent='Uploading firmware...';status.style.color='var(--text2)';bytes.textContent='0 / '+Math.round(file.size/1024)+' KB';btn.disabled=true;btn.style.opacity='.5';
  otaBusy=true;
  var xhr=new XMLHttpRequest();
  xhr.addEventListener('loadend',function(){otaBusy=false;});
  xhr.upload.addEventListener('progress',function(e){if(e.lengthComputable){var pct=Math.round((e.loaded/e.total)*100);bar.style.width=pct+'%';status.textContent='Uploading: '+pct+'%';bytes.textContent=Math.round(e.loaded/1024)+' / '+Math.round(e.total/1024)+' KB';}});
  xhr.addEventListener('load',function(){btn.disabled=false;btn.style.opacity='1';if(xhr.status===200&&xhr.responseText==='OK'){bar.style.width='100%';status.textContent='Upload complete - rebooting...';status.style.color='var(--accent)';var c=8;var t=setInterval(function(){c--;bytes.textContent='Reconnecting in '+c+'s...';if(c<=0){clearInterval(t);location.reload();}},1000);}else{bar.style.background='var(--red)';status.textContent='Update failed';status.style.color='var(--red)';bytes.textContent='Server response: '+(xhr.responseText||xhr.statusText||'Unknown error');input.value='';}});
  xhr.addEventListener('error',function(){btn.disabled=false;btn.style.opacity='1';bar.style.background='var(--red)';status.textContent='Connection lost during upload';status.style.color='var(--red)';bytes.textContent='Check WiFi connection and try again';input.value='';});
  xhr.addEventListener('timeout',function(){btn.disabled=false;btn.style.opacity='1';bar.style.background='var(--yellow)';status.textContent='Upload timed out';status.style.color='var(--yellow)';bytes.textContent='The device may have rebooted - check if new firmware is running';input.value='';});
  var fd=new FormData();fd.append('firmware',file);xhr.open('POST','/update',true);xhr.setRequestHeader('Authorization',authHeader);xhr.timeout=120000;xhr.send(fd);
}

function selectFirmware(el){
  requireAuth(function(){document.getElementById('otaFile').click();},el);
}

function restartDevice(el){
  restartAnchor=el;
  requireAuth(function(){showRestartConfirm(el);},el);
}

function movePanelNear(panel,anchor){
  if(!panel || !anchor)return;
  var card=anchor.closest?anchor.closest('.card'):null;
  if(card && panel.parentNode!==card)card.appendChild(panel);
}

function showRestartConfirm(anchor){
  var p=document.getElementById('restartConfirmPanel');
  movePanelNear(p,anchor);
  if(p)p.className='confirm-panel show';
}

function cancelRestartConfirm(){
  var p=document.getElementById('restartConfirmPanel');
  if(p)p.className='confirm-panel';
}

function confirmRestart(){
  cancelRestartConfirm();
  requestRestart();
}

function requestRestart(){
  fetch('/restart',{headers:{Authorization:authHeader}}).then(function(r){
    if(!r.ok){authHeader='';requireAuth(function(){showRestartConfirm(restartAnchor);},restartAnchor);return;}
    uiAlert('Device restart triggered');
    setTimeout(function(){location.reload();},8000);
  }).catch(function(){setTimeout(function(){location.reload();},8000);});
}

function requireAuth(action,anchor){
  if(authHeader && checkAuth()){action();return;}
  authHeader='';
  authAction=action;
  showAuth(anchor);
}

function showAuth(anchor){
  var m=document.getElementById('authPanel');
  var u=document.getElementById('authUser');
  var p=document.getElementById('authPass');
  movePanelNear(m,anchor);
  if(u)u.value='admin';
  if(p)p.value='';
  if(m)m.className='auth-panel show';
  setTimeout(function(){if(u)u.focus();},0);
}

function cancelAuth(){
  authAction=null;
  var m=document.getElementById('authPanel');
  if(m)m.className='auth-panel';
}

function submitAuth(){
  var u=document.getElementById('authUser');
  var p=document.getElementById('authPass');
  var user=u?u.value:'';
  var pass=p?p.value:'';
  if(!user || !pass)return;
  authHeader='Basic '+btoa(user+':'+pass);
  if(!checkAuth()){
    authHeader='';
    if(p)p.value='';
    authFailed(p);
    return;
  }
  var action=authAction;
  cancelAuth();
  if(action)action();
}

function checkAuth(){
  try{
    var xhr=new XMLHttpRequest();
    xhr.open('GET','/auth',false);
    xhr.setRequestHeader('Authorization',authHeader);
    xhr.send(null);
    return xhr.status===200;
  }catch(e){
    return false;
  }
}

function authFailed(el){
  var input=document.getElementById('otaFile');
  if(input)input.value='';
  uiAlert('Authentication failed',function(){if(el)el.focus();});
}

// The device asks for the admin password before it wipes the card.
function sdFormat(el){
  requireAuth(function(){uiConfirm('Format SD card? All data will be lost.',sdFormatRun);},el);
}
function sdFormatRun(){
  var btn=document.getElementById('btnFmt');
  var out=document.getElementById('fmtOut');
  btn.disabled=true;btn.textContent='FORMATTING\u2026';
  fetch('/sdformat',{method:'POST',headers:{Authorization:authHeader}}).then(function(r){
    if(r.status===401){authHeader='';requireAuth(sdFormatRun,btn);return null;}
    return r.json();
  }).then(function(d){
    if(!d)return;
    if(d.err==='origin'&&d.url){location.replace(d.url);return;}
    out.style.display='block';
    var part=d.ok&&d.left>0;   // folders nested too deep, or deletes the card refused
    out.style.color=part?'var(--yellow)':d.ok?'var(--accent)':'var(--red)';
    out.textContent=(part?'Some files or folders could not be deleted (nested too deep, or refused by the card)':d.msg)
      +(d.ok?' \u2014 '+d.free_mb+' MB free':'');
  }).catch(function(){
    out.style.display='block';out.style.color='var(--red)';out.textContent='Request failed';
  }).then(function(){
    btn.disabled=false;btn.textContent='FORMAT SD CARD';
  });
}
function saveWifi(b){
  var s=document.getElementById('wifiSsid').value;
  var p=document.getElementById('wifiPass').value;
  var h=document.getElementById('swWifiHid').checked;
  var ss=document.getElementById('wifiStaSsid').value;
  var sp=document.getElementById('wifiStaPass').value;
  var hn=document.getElementById('wifiHost').value.trim();
  if(s.length<1){uiAlert('SSID required');return;}
  if(p!=='***' && p.length>0 && p.length<8){uiAlert('Password must be empty or 8+ chars');return;}
  if(sp!=='***' && ss.length>0 && sp.length>0 && sp.length<8){uiAlert('Network password must be empty or 8+ chars');return;}
  // 64 characters are the key itself, in hex (web_wifi_pass_ok() in web_cmd.h).
  if(p!=='***' && p.length===64 && !WIFI_HEX64.test(p)){uiAlert(WIFI_ERR.pass);return;}
  if(sp!=='***' && ss.length>0 && sp.length===64 && !WIFI_HEX64.test(sp)){uiAlert(WIFI_ERR.sta_pass);return;}
  if(hn && !/^[A-Za-z0-9]([A-Za-z0-9-]{0,29}[A-Za-z0-9])?$/.test(hn)){uiAlert(WIFI_ERR.host);return;}
  uiConfirm('WiFi settings will be updated and the device will restart.',function(){
    var t=b?b.textContent:'';
    if(b){b.disabled=true;b.textContent='SAVING...';}
    cmd('wifi_cfg',{ssid:s,pass:p,hidden:h,sta_ssid:ss,sta_pass:sp,host:hn},function(){
      if(b){b.disabled=false;b.textContent=t;}
    });
  });
}
// fetch() with a deadline, so a dead link fails in seconds instead of hanging.
// Resolves with the body text; rejects on a network error, timeout or non-2xx
// (with the reply body as .body).
function fetchT(url,opt,ms){
  var ac=window.AbortController?new AbortController():null,t=0;
  if(ac){opt.signal=ac.signal;t=setTimeout(function(){ac.abort();},ms);}
  return fetch(url,opt).then(function(r){
    return r.text().then(function(s){
      if(!r.ok){var x=new Error('HTTP '+r.status);x.body=s;throw x;}
      return s;
    });
  }).then(function(s){clearTimeout(t);return s;},function(x){clearTimeout(t);throw x;});
}
// The WS only counts while it is delivering: the device pushes state at 1 Hz,
// so an open socket that has gone quiet is treated like a closed one.
function wsLive(){return !!ws&&ws.readyState===1&&Date.now()-wsRx<4000;}
// Commands go over the WS when it is live, else as the same JSON over HTTP.
// A command the device refuses is answered with {ok:false,err,cmd,field}: as
// the HTTP reply, or as a WS frame of its own.
var cmdFail={};
function cmd(c,v,fail){
  var m=JSON.stringify({cmd:c,value:v});
  busy = Date.now() + 3000;
  cmdFail[c]=fail;
  if(wsLive()){ws.send(m);return;}
  fetchT('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:m},5000).catch(function(x){
    console.error('cmd '+c,x);
    var d=null;
    try{d=JSON.parse(x.body);}catch(e){}
    cmdRefused(d&&d.ok===false?d:{cmd:c});
  });
}
var WIFI_BYTES='\nCharacters outside plain ASCII take 2 to 4 bytes each; control characters are not accepted.';
var WIFI_HEX64=/^[0-9A-Fa-f]{64}$/;
var WIFI_ERR={
  ssid:'SSID must be 1 to 32 bytes.'+WIFI_BYTES,
  pass:'Password must be empty, 8 to 63 bytes, or exactly 64 hex digits (0-9, a-f), which are the key itself.'+WIFI_BYTES,
  sta_ssid:'Network name must be at most 32 bytes.'+WIFI_BYTES,
  sta_pass:'Network password must be empty, 8 to 63 bytes, or exactly 64 hex digits (0-9, a-f), which are the key itself.'+WIFI_BYTES,
  host:'Hostname must be 1 to 31 letters, digits or hyphens, and cannot start or end with a hyphen.'};
// A command that did not take, refused by the device (d.err) or never
// delivered: say so and undo what the page did for it.
function cmdRefused(d){
  busy=0;
  // Loaded under a name the device does not take commands from (renamed
  // since): reload from the address it gives.
  if(d.err==='origin'&&d.url){location.replace(d.url);return;}
  var fail=cmdFail[d.cmd];
  if(d.cmd==='wifi_cfg'&&d.err){
    var why=WIFI_ERR[d.field];
    uiAlert('WiFi settings were not saved.'+(why?'\n'+why:''));
  }else{
    uiToast('Command failed — the device did not accept it. Check the connection and try again.');
  }
  if(fail)fail();
}
function toggleMode(){ cmd('mode',null); }
function gv(id){ var e=document.getElementById(id); return (e&&e.value.trim()!=='')?e.value.trim():'0'; }
function saveSigCfg(){
  var csv=[gv('cgDid'),gv('cgApB'),gv('cgApS'),gv('cgApM'),
           gv('cgHoB'),gv('cgHoS'),gv('cgHoM'),
           gv('cgSid'),gv('cgSHi'),gv('cgSLo')].join(',');
  cmd('sig_cfg',csv);
}
function sv(id,val){ var e=document.getElementById(id); if(e&&e!==document.activeElement) e.value=val; }
function hx(n){ return n?('0x'+n.toString(16).toUpperCase()):'0'; }
// ── Built-in variant-profile auto-suggest (#126) ──
var PM_SUG=null;
function pmSync(d){
  var p=d.profile;var el=document.getElementById('pmSuggest');if(!el)return;
  if(p&&p.suggest){
    PM_SUG=p;
    var nm=document.getElementById('pmName');if(nm)nm.textContent=p.name||'?';
    el.style.display='block';
  } else {
    PM_SUG=null;el.style.display='none';
  }
}
function sf(id,val){ var e=document.getElementById(id); if(e) e.value=val; }
function pmApply(){
  var p=PM_SUG;if(!p)return;
  // One-tap confirm: fill the Signal Map fields from the suggested profile and
  // apply via the existing sig_cfg path. Steer mapping is left as-is (0 unless
  // the user already set it). Never applied without this tap.
  sf('cgDid',hx(p.das_id));
  sf('cgApB',p.apb); sf('cgApS',p.aps); sf('cgApM',hx(p.apm));
  sf('cgHoB',p.hob); sf('cgHoS',p.hos); sf('cgHoM',hx(p.hom));
  saveSigCfg();
}
function setSig(d){
  sv('cgDid',hx(d.cfg_das_id)); sv('cgApB',d.cfg_apb); sv('cgApS',d.cfg_aps); sv('cgApM',hx(d.cfg_apm));
  sv('cgHoB',d.cfg_hob); sv('cgHoS',d.cfg_hos); sv('cgHoM',hx(d.cfg_hom));
  sv('cgSid',hx(d.cfg_steer_id)); sv('cgSHi',d.cfg_shi); sv('cgSLo',d.cfg_slo);
  sigSteerUi();
}
// Steer hi/lo are for a steering id other than 0x129; 0 (auto) and 0x129 read
// 0x129 by its own layout (fsd_apply_signal_config()).
function sigSteerUi(){
  var id=parseInt(gv('cgSid'))||0,off=(id===0||id===0x129);
  ['cgSHi','cgSLo'].forEach(function(n){var e=document.getElementById(n);if(e)e.disabled=off;});
}

function logInfo(text,color){
  var e=document.getElementById('httpLogInfo');
  if(!e)return;
  e.textContent=text;
  e.style.color=color||'var(--text2)';
}

function setHttpLogUi(running){
  var btn=document.getElementById('btnHttpLog');
  if(btn){
    btn.disabled=!httpLogAllowed&&!httpLogReady;
    btn.style.opacity=btn.disabled?'.45':'1';
    if(running){
      btn.textContent='STOP COLLECTING';
      btn.className='btn-main btn-stop';
    }else if(httpLogReady){
      btn.textContent='SAVE LOG FILE';
      btn.className='btn-main btn-blue';
    }else if(!httpLogAllowed){
      btn.textContent='LISTEN-ONLY REQUIRED';
      btn.className='btn-main btn-blue';
    }else{
      btn.textContent='STREAM LOG AND SAVE';
      btn.className='btn-main btn-blue';
    }
  }
  var filterEl=document.getElementById('httpLogFilter');
  if(filterEl)filterEl.disabled=running;
  pill('httpLogSt',running||httpLogReady,running?'Collecting':(httpLogReady?'Ready':'Idle'));
}

function formatBytes(n){
  if(n<1024)return n+' B';
  if(n<1048576)return (n/1024).toFixed(1)+' KB';
  return (n/1048576).toFixed(1)+' MB';
}

function logFileName(){
  var d=new Date();
  function p(n){return n<10?'0'+n:''+n;}
  return 'tesla_can_'+d.getFullYear()+p(d.getMonth()+1)+p(d.getDate())+'_'
    +p(d.getHours())+p(d.getMinutes())+p(d.getSeconds())+'.dump';
}

function clearHttpLogBlob(){
  if(httpLogSaveUrl)URL.revokeObjectURL(httpLogSaveUrl);
  httpLogSaveUrl='';
  httpLogName='';
  httpLogReady=false;
}

function prepareHttpLogFile(){
  clearHttpLogBlob();
  if(httpLogBytes===0){
    logInfo('No CAN frames collected. Nothing saved.','var(--yellow)');
    httpLogParts=[];
    setHttpLogUi(false);
    return;
  }
  httpLogName=logFileName();
  httpLogReady=true;
  setHttpLogUi(false);
  logInfo('Log ready in phone memory: '+httpLogName+' ('+formatBytes(httpLogBytes)+'). Tap SAVE LOG FILE.','var(--accent)');
}

function savePreparedHttpLog(){
  if(!httpLogReady||httpLogBytes===0)return;
  var blob=new Blob(httpLogParts,{type:'text/plain;charset=utf-8'});
  if(blob.size===0){
    logInfo('No CAN frames collected. Nothing saved.','var(--yellow)');
    return;
  }
  if(window.File&&navigator.canShare&&navigator.share){
    var file=new File([blob],httpLogName,{type:'text/plain'});
    if(navigator.canShare({files:[file]})){
      navigator.share({files:[file],title:httpLogName}).then(function(){
        logInfo('Save/share requested for '+httpLogName+'.','var(--accent)');
      }).catch(function(err){
        logInfo('Share cancelled or failed: '+(err&&err.message?err.message:'unknown')+'. Trying download link...','var(--yellow)');
        downloadHttpLogBlob(blob);
      });
      return;
    }
  }
  downloadHttpLogBlob(blob);
}

function downloadHttpLogBlob(blob){
  if(httpLogSaveUrl)URL.revokeObjectURL(httpLogSaveUrl);
  httpLogSaveUrl=URL.createObjectURL(blob);
  var a=document.createElement('a');
  a.href=httpLogSaveUrl;
  a.download=httpLogName;
  a.rel='noopener';
  a.textContent=httpLogName;
  a.style.position='fixed';
  a.style.left='0';
  a.style.bottom='0';
  a.style.opacity='0.01';
  document.body.appendChild(a);
  a.click();
  setTimeout(function(){a.remove();},1000);
  logInfo('Save requested for '+httpLogName+'. If no file appears, tap SAVE LOG FILE again or use another browser.','var(--accent)');
}

function stopHttpLog(reason){
  if(!httpLogRunning)return;
  httpLogRunning=false;
  if(httpLogReader)httpLogReader.cancel().catch(function(){});
  if(httpLogAbort)httpLogAbort.abort();
  httpLogReader=null;
  httpLogAbort=null;
  prepareHttpLogFile();
  if(reason){
    if(httpLogReady){
      logInfo(reason+' Log ready in phone memory: '+httpLogName+' ('+formatBytes(httpLogBytes)+'). Tap SAVE LOG FILE.','var(--yellow)');
    }else{
      logInfo(reason,'var(--yellow)');
    }
  }
}

function startHttpLog(){
  if(httpLogRunning)return;
  if(!httpLogAllowed){
    logInfo('Switch to Listen-Only mode before starting HTTP CAN log.','var(--yellow)');
    setHttpLogUi(false);
    return;
  }
  if(!window.ReadableStream){
    uiAlert('This browser does not support HTTP stream collection.');
    return;
  }
  httpLogParts=[];
  httpLogBytes=0;
  clearHttpLogBlob();
  httpLogStarted=Date.now();
  httpLogRunning=true;
  httpLogAbort=new AbortController();
  setHttpLogUi(true);
  logInfo('Connecting to HTTP stream...');

  var streamUrl='http://'+location.hostname+':82/stream';
  var filterEl=document.getElementById('httpLogFilter');
  var filter=(filterEl&&filterEl.value)?filterEl.value.trim():'';
  if(filter)streamUrl+='?ids='+encodeURIComponent(filter);

  fetch(streamUrl,{cache:'no-store',signal:httpLogAbort.signal})
    .then(function(r){
      if(!r.ok)throw new Error('HTTP '+r.status);
      if(!r.body)throw new Error('Readable stream unavailable');
      logInfo('Collecting 0 B...');
      var reader=r.body.getReader();
      httpLogReader=reader;
      function pump(){
        return reader.read().then(function(result){
          if(result.done)return;
          if(result.value&&result.value.length){
            httpLogParts.push(result.value);
            httpLogBytes+=result.value.length;
            var secs=Math.max(1,Math.round((Date.now()-httpLogStarted)/1000));
            logInfo('Collecting '+formatBytes(httpLogBytes)+' for '+secs+'s...');
          }
          return pump();
        });
      }
      return pump();
    })
    .catch(function(err){
      if(!httpLogRunning)return;
      httpLogRunning=false;
      httpLogReader=null;
      httpLogAbort=null;
      setHttpLogUi(false);
      logInfo('Stream stopped: '+(err&&err.message?err.message:'connection closed'),'var(--yellow)');
      if(httpLogBytes>0)prepareHttpLogFile();
    });
}

function toggleHttpLog(){
  if(httpLogRunning)stopHttpLog();
  else if(httpLogReady)savePreparedHttpLog();
  else startHttpLog();
}

// ── Aux status poll (#124) ──
// blackbox/capability/profile are served on demand from /api/aux, NOT in the
// hot 1 Hz WS state push. Poll them on a slower timer so the black-box card,
// Tap Check card and profile suggestion stay live without bloating each push.
var auxBusy=false;
function auxPoll(){
  if(auxBusy)return;
  auxBusy=true;
  fetch('/api/aux').then(function(r){return r.json()}).then(function(a){
    if(a){bbSync(a);capSync(a);pmSync(a);}
  }).catch(function(){}).then(function(){auxBusy=false;});
}
setInterval(auxPoll,2500);

function applyState(d){
  if(!d)return;
  try{initWifi(d);}catch(x){console.error('initWifi',x);}
  upd(d);
}
// Connected = an open socket or a status poll that just succeeded.
function link(){
  var up=(!!ws&&ws.readyState===1)||httpUp;
  document.getElementById('dot').className=up?'cdot':'cdot off';
  document.getElementById('connErr').style.display=up?'none':'block';
}

// ── Status over HTTP ──
// The WS push is the primary feed. GET /api/status covers the first paint and
// any client where :81 never delivers (captive-portal sheets): polled only
// while the socket is not live, so a healthy WS is never applied twice. Paused
// during a firmware upload — the server is busy with it anyway.
function statusPoll(){
  if(stBusy||otaBusy||wsLive())return;
  stBusy=true;
  fetchT('/api/status',{cache:'no-store'},4000).then(function(s){
    var d=JSON.parse(s);
    httpUp=true;
    if(!wsLive())applyState(d);
  }).catch(function(){httpUp=false;}).then(function(){stBusy=false;link();});
}

function conn(){
  // Some embedded browsers refuse to construct a WebSocket at all; HTTP carries on.
  try{ws=new WebSocket('ws://'+location.hostname+':81/');}catch(x){ws=null;return;}
  ws.onopen=function(){
    wsRx=0;   // not live until this socket delivers its first frame
    link();
    clearTimeout(rt);
    auxPoll();
  };
  ws.onmessage=function(e){
    wsRx=Date.now();
    httpUp=false;   // polling stops while the WS delivers, so HTTP is unknown again
    var d;
    try{d=JSON.parse(e.data);}catch(x){console.error('ws frame',x);return;}
    if(d&&d.ok===false){cmdRefused(d);return;}   // the answer to a command we sent
    applyState(d);
  };
  ws.onclose=function(){
    link();
    // A feed that was live just stopped: take over now, not at the next tick.
    if(Date.now()-wsRx<4000)statusPoll();
    rt=setTimeout(conn,2000);
  };
  ws.onerror=function(){ ws.close(); };
}
perfInit();
tpInit();
conn();
statusPoll();
setInterval(statusPoll,2000);
</script>
)rawliteral"
#include "web_i18n.inc"
R"rawliteral(
</body>
</html>
)rawliteral";

// The same page gzip-compressed, written at build time by esp32/gen_web_page.py
// from the literals above (into the build directory, not the source tree).
#include "web_page_gz.h"
static_assert(sizeof(WEB_HTML) - 1 == WEB_HTML_GZ_SRC_LEN,
              "web_page_gz.h was made from a different page: see esp32/gen_web_page.py");
// Set at init when WEB_HTML_GZ inflates to WEB_HTML (its gzip trailer: CRC-32
// and length of the input); otherwise every client gets WEB_HTML.
static bool g_page_gz_ok = false;

// ── JSON helpers ──────────────────────────────────────────────────────────────
static String json_escape(const char *s) {
    String out;
    for (; *s; ++s) {
        if (*s == '"')       out += "\\\"";
        else if (*s == '\\') out += "\\\\";
        else                 out += *s;
    }
    return out;
}

// ── CAN counters since the last reset (#252, #253) ────────────────────────────
// Controller behind each bus, as can_driver_create(CanBusId) picks it.
static const char *can_driver_kind(uint8_t index) {
#if defined(CAN_DRIVER_T2CAN_DUAL) && defined(BOARD_LILYGO_T2CAN_FD)
    return index == 1 ? "MCP2518FD" : "TWAI";
#elif defined(CAN_DRIVER_T2CAN_DUAL)
    return index == 1 ? "MCP2515" : "TWAI";
#elif defined(CAN_DRIVER_MCP2515)
    (void)index;
    return "MCP2515";
#else
    (void)index;
    return "TWAI";
#endif
}

// Up = the chip answered and begin() got it running ("down" is what every
// driver reports for its filter mode while it is not installed).
static bool can_driver_up(CanDriver *d) {
    return d && d->hardwarePresent() && strcmp(d->acceptanceFilterMode(), "down") != 0;
}

static CanCounts can_counts(CanDriver *d) {
    CanCounts c = {};
    if (!d) return c;
    c.rx        = d->rxCount();
    c.tx        = d->txCount();
    c.rx_missed = d->rxMissedCount();
    c.bus_error = d->busErrorCount();
    c.tx_failed = d->txFailedCount();
    return c;
}

// The drivers' counters are cumulative: TWAI's carry across the reinstalls for
// a mode switch or a capture filter (can_counters.h). Should one ever read
// below its baseline it has restarted, and everything it holds is newer than
// the reset.
static uint32_t since_reset(uint32_t now, uint32_t &base) {
    if (now < base) base = 0;
    return now - base;
}

static CanCounts can_counts_since_reset(uint8_t i) {
    CanCounts c = can_counts(g_can_buses[i]);
    CanCounts &b = g_can_base[i];
    c.rx        = since_reset(c.rx, b.rx);
    c.tx        = since_reset(c.tx, b.tx);
    c.rx_missed = since_reset(c.rx_missed, b.rx_missed);
    c.bus_error = since_reset(c.bus_error, b.bus_error);
    c.tx_failed = since_reset(c.tx_failed, b.tx_failed);
    return c;
}

static void can_counters_reset() {
    for (uint8_t i = 0; i < g_can_count && i < CAN_BUS_COUNT; i++)
        g_can_base[i] = can_counts(g_can_buses[i]);
    state_enter();
    uint32_t rx = g_state->rx_count;
    state_exit();
    g_rx_base = rx;
    // Frames/s is a one-second rate: start its window again.
    g_last_rx     = rx;
    g_last_fps_ms = millis();
    g_fps         = 0.0f;
    g_can_reset    = true;
    g_can_reset_ms = millis();
    loop_health_reset_window();
}

// ── JSON builder ──────────────────────────────────────────────────────────────
static String build_json() {
    FSDState state;
    if (!state_copy(&state)) return "{}";

    uint32_t uptime_s = (millis() - g_start_ms) / 1000;
    bool can_vehicle_detected = false;
    if (state.rx_count > 0) {
        can_vehicle_detected = (millis() - g_last_can_seen_ms) <= CAN_VEHICLE_ALIVE_MS;
    }

    // BMS sub-object. Pack temperature only from a fresh, plausible 0x312
    // reading (fsd_bms.h): temp_ok false -> null, the dashboard shows "--" and
    // no advisory. temp_bad counts the 0x312 frames the plausibility gate refused.
    // car_soc: the % on the car's screen (0x33A, #251), null until decoded; it can
    // arrive without the BMS frames.
    char bms[208];
    char car_soc[8];
    if (state.ui_soc_seen) snprintf(car_soc, sizeof(car_soc), "%u", (unsigned)state.ui_soc);
    else strcpy(car_soc, "null");
    if (state.bms_seen) {
        bool temp_ok = fsd_bms_temp_fresh(&state, millis());
        char temp[64];
        if (temp_ok)
            snprintf(temp, sizeof(temp), "\"temp_min\":%d,\"temp_max\":%d,\"temp_age_s\":%lu",
                     (int)state.batt_temp_min_c, (int)state.batt_temp_max_c,
                     (unsigned long)((millis() - state.batt_temp_ms) / 1000u));
        else
            strcpy(temp, "\"temp_min\":null,\"temp_max\":null");
        snprintf(bms, sizeof(bms),
            "{\"seen\":true,\"voltage\":%.1f,\"current\":%.1f,"
            "\"soc\":%.1f,\"car_soc\":%s,\"temp_ok\":%s,%s,\"temp_bad\":%lu}",
            state.pack_voltage_v,
            state.pack_current_a,
            state.soc_percent,
            car_soc, temp_ok ? "true" : "false", temp,
            (unsigned long)state.batt_temp_rejected);
    } else {
        snprintf(bms, sizeof(bms), "{\"seen\":false,\"car_soc\":%s}", car_soc);
    }

    // Performance read-out sub-object (read-only; see fsd_logic/fsd_perf.h).
    // Times in ms (0 = not captured this run / no best yet). Everything here is
    // derived from frames already received (0x257 speed, 0x175 wheels, 0x312
    // batt temp) — nothing is transmitted.
    char perf[384];
    {
        const FSDPerf *p = &state.perf;
        uint32_t run_ms = fsd_perf_accel_running_ms(p, millis());
        float slip = 0.0f;
        bool slip_ok = state.wheel_speed_seen &&
            fsd_perf_wheel_slip_pct(state.wheel_speed_fl_kph, state.wheel_speed_fr_kph,
                                    state.wheel_speed_rl_kph, state.wheel_speed_rr_kph, &slip);
        snprintf(perf, sizeof(perf),
            "{\"speed\":%.1f,\"speed_ok\":%s,\"armed\":%s,\"run_ms\":%lu,"
            "\"t50\":%lu,\"t60mph\":%lu,\"t100\":%lu,"
            "\"best50\":%lu,\"best60mph\":%lu,\"best100\":%lu,"
            "\"brake_armed\":%s,\"t1000\":%lu,\"best1000\":%lu,"
            "\"g\":%.2f,\"gpk\":%.2f,\"gpkb\":%.2f,"
            "\"wheel_seen\":%s,\"slip_ok\":%s,\"slip\":%.1f}",
            state.vehicle_speed_kph, fsd_speed_valid(&state, millis()) ? "true" : "false",
            p->accel_armed ? "true" : "false", (unsigned long)run_ms,
            (unsigned long)p->t_0_50_ms, (unsigned long)p->t_0_60mph_ms, (unsigned long)p->t_0_100_ms,
            (unsigned long)p->best_0_50_ms, (unsigned long)p->best_0_60mph_ms, (unsigned long)p->best_0_100_ms,
            p->brake_armed ? "true" : "false", (unsigned long)p->t_100_0_ms, (unsigned long)p->best_100_0_ms,
            (double)p->g_est, (double)p->g_peak, (double)p->g_peak_brake,
            state.wheel_speed_seen ? "true" : "false", slip_ok ? "true" : "false", (double)slip);
    }

    char ota_part[128] = {};
    {
        const esp_partition_t *running = esp_ota_get_running_partition();
        const char *running_label = running ? running->label : "unknown";
        esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
        if (running) esp_ota_get_state_partition(running, &ota_state);
        bool has_ota = (running &&
            (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
             running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1));
        snprintf(ota_part, sizeof(ota_part),
            "{\"running\":\"%s\",\"state\":%d,\"has_ota\":%s}",
            running_label, (int)ota_state, has_ota ? "true" : "false");
    }

    // fps as fixed-point string
    char fps_s[12];
    snprintf(fps_s, sizeof(fps_s), "%.1f", g_fps);

    // CAN counters since the last reset (#252), per bus and summed. The
    // combined error count (legacy crc_errors key) and its per-cause split come
    // from one read, so the dashboard tile always equals its breakdown. On a
    // busy bus it is mostly rx_missed (controller RX-queue drops).
    CanCounts bus[CAN_BUS_COUNT] = {};
    CanCounts sum = {};
    for (uint8_t i = 0; i < g_can_count && i < CAN_BUS_COUNT; i++) {
        bus[i] = can_counts_since_reset(i);
        sum.tx        += bus[i].tx;
        sum.rx_missed += bus[i].rx_missed;
        sum.bus_error += bus[i].bus_error;
        sum.tx_failed += bus[i].tx_failed;
    }
    uint32_t err_total = sum.rx_missed + sum.bus_error + sum.tx_failed;

    String j;
    bool isa_speed_enabled = state.hw_version == TeslaHW_HW4;
    const char *ap_das_profile =
        (state.hw_version == TeslaHW_HW4) ? "HW4: DAS 0x39B + ISA 0x399" :
        (state.hw_version == TeslaHW_HW3) ? "HW3: DAS 0x399" :
        (state.hw_version == TeslaHW_Legacy) ? "Legacy: DAS 0x399" :
        "Waiting for HW detection";
    // About 3.1 KB on a T-2CAN in use: the scalar fields, the bms/perf/tpms/
    // ota/http_can_stream/loop objects, can_buses and mcp_rx_task. The heavy
    // blackbox/capability/profile blocks come from /api/aux (#124). With every
    // counter at 4294967295 and both SSIDs at 32 escaped characters it is
    // 3.9 KB (3.7 KB on the TTGO). Past the reserve the String reallocs every
    // 16 bytes, and build_json() runs on every WS push.
    j.reserve(4096);
    j  = "{";
    j += "\"fsd_enabled\":";   j += state.fsd_enabled                 ? "true" : "false"; j += ',';
    j += "\"ap_active\":";     j += state.ap_active                   ? "true" : "false"; j += ',';
    j += "\"op_mode\":";       j += (int)state.op_mode;                j += ',';
    j += "\"hw_override\":";   j += (int)state.hw_override;            j += ',';
    j += "\"hw_version\":";    j += (int)state.hw_version;             j += ',';
    j += "\"ota\":";           j += state.tesla_ota_in_progress        ? "true" : "false"; j += ',';
    j += "\"autopark_block\":"; j += state.autopark_tx_block            ? "true" : "false"; j += ',';
    j += "\"signal_map_das_missing\":"; j += state.signal_map_das_missing ? "true" : "false"; j += ',';
    j += "\"ap_das_profile\":\""; j += ap_das_profile;                 j += "\",";
    j += "\"isa_speed_enabled\":"; j += isa_speed_enabled              ? "true" : "false"; j += ',';
    j += "\"ignore_ota\":";    j += state.ignore_ota                   ? "true" : "false"; j += ',';
    j += "\"fsd_unlock\":";    j += state.fsd_unlock                   ? "true" : "false"; j += ',';
    j += "\"hw3_speed_override\":"; j += state.hw3_speed_override       ? "true" : "false"; j += ',';
    // HW3 0x3FD speed read-out (#209): car value vs value forwarded, -1 = not seen yet
    j += "\"hw3_car_profile\":";  j += state.hw3_profile_seen ? (int)state.hw3_car_profile  : -1; j += ',';
    j += "\"hw3_sent_profile\":"; j += state.hw3_profile_seen ? (int)state.hw3_sent_profile : -1; j += ',';
    j += "\"hw3_car_offset\":";   j += state.hw3_offset_seen  ? (int)state.hw3_car_offset   : -1; j += ',';
    j += "\"hw3_sent_offset\":";  j += state.hw3_offset_seen  ? (int)state.hw3_sent_offset  : -1; j += ',';
    j += "\"follow_distance\":";  j += state.follow_distance_seen ? (int)state.follow_distance : -1; j += ',';
    j += "\"nag_killer\":";    j += state.nag_killer                   ? "true" : "false"; j += ',';
    j += "\"continuous_ap\":"; j += state.continuous_ap                 ? "true" : "false"; j += ',';
    j += "\"ap_first\":";      j += state.ap_first                      ? "true" : "false"; j += ',';
    j += "\"ap_first_edge\":"; j += state.ap_first_edge                 ? "true" : "false"; j += ',';
    j += "\"ap_first_minimal\":"; j += state.ap_first_minimal           ? "true" : "false"; j += ',';
    j += "\"nag_faithful\":";  j += state.nag_epas_faithful             ? "true" : "false"; j += ',';
    j += "\"soft_engage\":";   j += state.soft_engage                  ? "true" : "false"; j += ',';
    j += "\"nag_burst\":";     j += state.nag_burst                    ? "true" : "false"; j += ',';
    j += "\"abort_guard\":";   j += state.abort_guard                  ? "true" : "false"; j += ',';
    j += "\"cfg_das_id\":";    j += state.cfg_das_id;       j += ',';
    j += "\"cfg_apb\":";       j += state.cfg_apstate_byte;  j += ',';
    j += "\"cfg_aps\":";       j += state.cfg_apstate_shift; j += ',';
    j += "\"cfg_apm\":";       j += state.cfg_apstate_mask;  j += ',';
    j += "\"cfg_hob\":";       j += state.cfg_handson_byte;  j += ',';
    j += "\"cfg_hos\":";       j += state.cfg_handson_shift; j += ',';
    j += "\"cfg_hom\":";       j += state.cfg_handson_mask;  j += ',';
    j += "\"cfg_steer_id\":";  j += state.cfg_steer_id;      j += ',';
    j += "\"cfg_shi\":";       j += state.cfg_steer_hi;      j += ',';
    j += "\"cfg_slo\":";       j += state.cfg_steer_lo;      j += ',';
    j += "\"bms_output\":";    j += state.bms_output                   ? "true" : "false"; j += ',';
    j += "\"force_fsd\":";     j += state.force_fsd                    ? "true" : "false"; j += ',';
    j += "\"china_mode\":";    j += state.china_mode                   ? "true" : "false"; j += ',';
    j += "\"suppress_speed_chime\":"; j += state.suppress_speed_chime  ? "true" : "false"; j += ',';
    j += "\"precondition\":";  j += state.precondition                ? "true" : "false"; j += ',';
    j += "\"tlssc_restore\":"; j += state.tlssc_restore                ? "true" : "false"; j += ',';
    j += "\"summon_unlock\":"; j += state.summon_unlock                ? "true" : "false"; j += ',';
    j += "\"continue_on_green\":"; j += state.continue_on_green         ? "true" : "false"; j += ',';
    j += "\"assist_tlssc_bit38\":"; j += state.assist_tlssc_bit38       ? "true" : "false"; j += ',';
    j += "\"assist_rhd_override\":"; j += state.assist_rhd_override      ? "true" : "false"; j += ',';
    j += "\"assist_telemetry_off\":"; j += state.assist_telemetry_off    ? "true" : "false"; j += ',';
    j += "\"apmv3_branch\":";  j += (int)state.apmv3_branch;             j += ',';
    j += "\"track_mode_inject\":"; j += state.track_mode_inject         ? "true" : "false"; j += ',';
    j += "\"track_rotation_pct\":";  j += (int)state.track_rotation_pct;   j += ',';
    j += "\"track_stability_pct\":"; j += (int)state.track_stability_pct;  j += ',';
    j += "\"track_post_cooling\":"; j += state.track_post_cooling        ? "true" : "false"; j += ',';
    j += "\"track_cmp_overclock\":"; j += state.track_cmp_overclock       ? "true" : "false"; j += ',';
    // Acceleration Mode (#211): setting, car's own 0x334 pedal map and the map
    // going out (-1 = no valid 0x334 yet), pending = waiting for standstill.
    j += "\"accel_mode\":";     j += (int)state.accel_mode;               j += ',';
    j += "\"accel_car_map\":";  j += state.accel_car_seen ? (int)state.accel_car_map : -1; j += ',';
    j += "\"accel_sent_map\":"; j += fsd_accel_sent_map(&state);           j += ',';
    j += "\"accel_pending\":";  j += state.accel_mode_pending            ? "true" : "false"; j += ',';
    j += "\"accel_bad\":";      j += state.accel_bad_frames;              j += ',';
    // Charge-port type (0x25D, #228): what the car reports (-1 = no 0x25D on
    // this tap yet), the plug-guard read-out, the opt-in rewrite + copies sent.
    // cp_stale: no CP_status for a few seconds, cp_type / cp_plug are its last.
    j += "\"cp_type\":";        j += state.cp_status_seen ? (int)state.cp_type : -1; j += ',';
    j += "\"cp_plug\":";        j += (int)state.cp_plug_state;            j += ',';
    {
        uint32_t now_ms = millis();
        j += "\"cp_stale\":";   j += fsd_cp_status_stale(state.cp_status_seen, state.cp_status_ms, now_ms)
                                     ? "true" : "false"; j += ',';
        j += "\"cp_age_s\":";   j += state.cp_status_seen ? (now_ms - state.cp_status_ms) / 1000u : 0u; j += ',';
    }
    j += "\"cp_type_eu_rewrite\":"; j += state.cp_type_eu_rewrite        ? "true" : "false"; j += ',';
    j += "\"cp_type_tx\":";     j += state.cp_type_tx_count;              j += ',';
    j += "\"tx_allowed\":";     j += fsd_can_transmit(&state)            ? "true" : "false"; j += ',';
    j += "\"firmware_14x_warning\":"; j += state.firmware_14x_warning  ? "true" : "false"; j += ',';
#if defined(BOARD_TTGO_DISPLAY)
    j += "\"display_enabled\":"; j += state.display_enabled             ? "true" : "false"; j += ',';
    j += "\"display_brightness\":"; j += state.display_brightness;      j += ',';
    j += "\"display_timeout_s\":";  j += state.display_timeout_s;       j += ',';
#endif
    j += "\"can_vehicle_detected\":"; j += can_vehicle_detected       ? "true" : "false"; j += ',';
    j += "\"bms_hv_seen\":";   j += state.seen_bms_hv;                 j += ',';
    j += "\"bms_soc_seen\":";  j += state.seen_bms_soc;                j += ',';
    j += "\"bms_thermal_seen\":"; j += state.seen_bms_thermal;          j += ',';
    j += "\"rx_count\":";      j += since_reset(state.rx_count, g_rx_base); j += ',';
    j += "\"tx_count\":";      j += sum.tx;                            j += ',';
    j += "\"tx_modified\":";   j += state.frames_modified;             j += ',';
    j += "\"crc_errors\":";    j += err_total;                         j += ',';
    j += "\"rx_missed_count\":"; j += sum.rx_missed;                   j += ',';
    j += "\"bus_error_count\":"; j += sum.bus_error;                   j += ',';
    j += "\"tx_failed_count\":"; j += sum.tx_failed;                   j += ',';
    j += "\"fps\":";           j += fps_s;                             j += ',';
    // Seconds since the CAN Bus card was reset, -1 = counting since boot.
    j += "\"can_reset_s\":";   j += g_can_reset ? (long)((millis() - g_can_reset_ms) / 1000u) : -1L; j += ',';
    // Per bus (#253), on boards with two controllers; same counts as above.
    if (g_can_count > 1) {
        j += "\"can_buses\":[";
        for (uint8_t i = 0; i < g_can_count && i < CAN_BUS_COUNT; i++) {
            if (i) j += ',';
            j += "{\"name\":\"";      j += can_bus_name((CanBusId)i);
            j += "\",\"drv\":\"";     j += can_driver_kind(i);
            j += "\",\"up\":";        j += can_driver_up(g_can_buses[i]) ? "true" : "false";
            j += ",\"rx\":";          j += bus[i].rx;
            j += ",\"tx\":";          j += bus[i].tx;
            j += ",\"rx_missed\":";   j += bus[i].rx_missed;
            j += ",\"bus_error\":";   j += bus[i].bus_error;
            j += ",\"tx_failed\":";   j += bus[i].tx_failed;
            j += '}';
        }
        j += "],";
    }
    // MCP2515 receive task (opt-in), on the builds with one; one per board.
    for (uint8_t i = 0; i < g_can_count && i < CAN_BUS_COUNT; i++) {
        CanRxTaskInfo t;
        if (!g_can_buses[i] || !g_can_buses[i]->rxTaskInfo(t)) continue;
        j += "\"mcp_rx_task\":{\"on\":"; j += t.wanted ? "true" : "false";
        j += ",\"run\":";   j += t.running ? "true" : "false";
        j += ",\"int\":";   j += t.int_pin ? "true" : "false";
        j += ",\"len\":";   j += t.queue_len;
        j += ",\"max\":";   j += t.high_water;
        j += ",\"drop\":";  j += t.dropped;
        j += ",\"ovf\":";   j += t.overflows;
        j += ",\"stack\":"; j += t.stack_free;
        j += "},";
        break;
    }
    j += "\"bms\":";           j += bms;                               j += ',';
    // Tyre pressures (read-only 0x219, fsd_logic/fsd_tpms.h). Four slots in the
    // order the car fills them: mbar, temp in deg C, pos 0 FL 1 FR 2 RL 3 RR;
    // target = the car's own pair. null = not known.
    {
        const FSDTpms *t = &state.tpms;
        j += "\"tpms\":{\"seen\":"; j += fsd_tpms_seen(t) ? "true" : "false";
        if (fsd_tpms_seen(t)) {
            uint32_t now_ms = millis();
            j += ",\"src\":";   j += (int)t->source;
            j += ",\"dlc\":";   j += (int)t->frame_dlc;
            j += ",\"age_s\":"; j += fsd_tpms_age_ms(t, now_ms) / 1000u;
            j += ",\"stale\":"; j += fsd_tpms_stale(t, now_ms) ? "true" : "false";
            j += ",\"mbar\":[";
            for (int i = 0; i < 4; i++) {
                if (i) j += ',';
                if (t->pressure[i] == FSD_TPMS_NA) j += "null";
                else j += fsd_tpms_mbar(t->pressure[i]);
            }
            j += "],\"temp\":[";
            for (int i = 0; i < 4; i++) {
                if (i) j += ',';
                if (t->temp[i] == FSD_TPMS_NA) j += "null";
                else j += fsd_tpms_temp_c(t->temp[i]);
            }
            j += "],\"pos\":[";
            for (int i = 0; i < 4; i++) {
                if (i) j += ',';
                if (t->pos[i] > FSD_TPMS_POS_RR) j += "null";
                else j += (int)t->pos[i];
            }
            j += "],\"target\":[";
            for (int i = 0; i < 2; i++) {
                if (i) j += ',';
                if (t->target[i] == FSD_TPMS_NA) j += "null";
                else j += fsd_tpms_mbar(t->target[i]);
            }
            j += ']';
        }
        j += "},";
    }
    j += "\"perf\":";          j += perf;                              j += ',';
    j += "\"uptime_s\":";      j += uptime_s;                          j += ',';
    // Power / reset log (#218): why the board last restarted, the 12V-feed
    // verdict, and the restart count since power-on. Codes; the page maps them.
    j += "\"reset_reason\":";  j += power_log_reset_code();            j += ',';
    j += "\"power_verdict\":"; j += power_log_verdict_code();          j += ',';
    j += "\"restart_count\":"; j += power_log_restart_count();         j += ',';
    // Loop-task stack low-water mark (this runs on it) and heap, for field reports.
    j += "\"stack_free\":";    j += (uint32_t)uxTaskGetStackHighWaterMark(nullptr); j += ',';
    j += "\"heap_free\":";     j += (uint32_t)ESP.getFreeHeap();       j += ',';
    j += "\"heap_min\":";      j += (uint32_t)ESP.getMinFreeHeap();    j += ',';
    // Longest loop() pass, its slowest stage and passes over 25 ms
    // (loop_health.h): since boot, and since the CAN Bus card's reset (r_*).
    {
        const LoopHealth &lh = loop_health();
        j += "\"loop\":{\"max_us\":"; j += lh.boot.max_us;
        j += ",\"stage\":\"";    j += loop_stage_name(lh.boot.max_stage);
        j += "\",\"slow\":";     j += lh.boot.slow;
        j += ",\"r_max_us\":";   j += lh.since.max_us;
        j += ",\"r_stage\":\"";  j += loop_stage_name(lh.since.max_stage);
        j += "\",\"r_slow\":";   j += lh.since.slow;
        j += "},";
    }
    j += "\"fw_version\":\""   FW_VERSION "\",";
    j += "\"board\":\""        BOARD_NAME "\",";
    j += "\"fw_build\":\"";    j += __DATE__;  j += ' '; j += __TIME__; j += "\",";
    j += "\"can_dump\":";      j += can_dump_active()                 ? "true" : "false"; j += ',';
    // blackbox/capability/profile moved OFF the hot WS state push (#124): they
    // are large and blackbox_status_json() scans the LittleFS dir every call.
    // The dashboard polls them on a slower timer via GET /api/aux instead.
    j += "\"sleep_ms\":";     j += state.sleep_idle_ms;               j += ',';
    j += "\"wifi_ssid\":\"";  j += json_escape(state.wifi_ssid);      j += "\",";
    j += "\"wifi_pass\":\"";  j += state.wifi_pass[0] ? "***" : "";  j += "\",";
    j += "\"wifi_hidden\":";  j += state.wifi_hidden                  ? "true" : "false"; j += ',';
    j += "\"wifi_sta_ssid\":\""; j += json_escape(state.wifi_sta_ssid); j += "\",";
    j += "\"wifi_sta_pass\":\""; j += state.wifi_sta_pass[0] ? "***" : ""; j += "\",";
    j += "\"wifi_hostname\":\""; j += json_escape(state.wifi_hostname); j += "\",";
    j += "\"wifi_clients\":";  j += (int)WiFi.softAPgetStationNum();   j += ',';
    j += "\"http_can_stream\":{";
    j += "\"active\":";       j += http_can_stream_active()           ? "true" : "false"; j += ',';
    j += "\"sent\":";         j += http_can_stream_frames_sent();      j += ',';
    j += "\"dropped\":";      j += http_can_stream_frames_dropped();   j += ',';
    j += "\"rx_missed\":";    j += http_can_stream_rx_missed();        j += ',';
    j += "\"filtered\":";     j += http_can_stream_frames_filtered();  j += ',';
    j += "\"buffered\":";     j += http_can_stream_buffered_frames();  j += "},";
    j += "\"ota_partition\":"; j += ota_part;
    j += '}';
    return j;
}

// Heavy/auxiliary status blocks, served on demand via GET /api/aux and polled
// by the dashboard on a slow timer — kept OUT of the 1 Hz WS state push (#124)
// so the hot path stays small and never touches the filesystem. Each helper is
// self-guarding and always returns a valid JSON object (never an empty string).
static String build_aux_json() {
    String j;
    j.reserve(1536);
    j  = "{";
    j += "\"blackbox\":";   j += blackbox_status_json();   j += ',';
    j += "\"capability\":"; j += capability_status_json(); j += ',';
    j += "\"profile\":";    j += profile_match_json();
    j += '}';
    return j;
}

// ── Command dispatcher ────────────────────────────────────────────────────────
// Runs one dashboard command, {"cmd":"xxx","value":yyy}. The single command
// table for both transports: WS TEXT frames and POST /api/cmd. The message is
// parsed as JSON (web_cmd.h): a branch is picked by the value of "cmd" alone
// and reads its value with cmd.boolean() / integer() / string(), which refuse
// a value of the wrong type. The caller sends the reply and, for
// WEB_CMD_RESTART, restarts the device afterwards.
static WebCmdResult web_command(WebCmd &cmd, const uint8_t *payload, size_t length)
{
    if (g_state == nullptr || length == 0) return WEB_CMD_BAD_JSON;
    if (length > WEB_CMD_MAX_LEN) return WEB_CMD_TOO_LONG;
    if (!web_cmd_parse((const char *)payload, length, &cmd)) return WEB_CMD_BAD_JSON;

    if (cmd.is("mode")) {
        FSDState saved;
        bool active = false;
        state_enter();
        if (g_state->op_mode == OpMode_ListenOnly) {
            g_state->op_mode = OpMode_Active;
            active = true;
        } else {
            g_state->op_mode = OpMode_ListenOnly;
        }
        saved = *g_state;
        state_exit();
        for (uint8_t i = 0; i < g_can_count; i++) {
            if (g_can_buses[i]) g_can_buses[i]->setListenOnly(!active);
        }
        http_can_stream_set_enabled(true);  // capture works in both modes now (#108)
        Serial.println(active ? "[Web] → Active mode" : "[Web] → Listen-Only mode");
        prefs_save(&saved);
    } else if (cmd.is("hw_override")) {
        // Manual HW selection (#110): 0 = auto-detect, else pin the version.
        int sel;
        if (cmd.integer(&sel)) {
            if (sel >= (int)TeslaHW_Unknown && sel <= (int)TeslaHW_HW4) {
                TeslaHWVersion want = (TeslaHWVersion)sel;
                FSDState saved;
                state_enter();
                g_state->hw_override = want;
                // Apply at once so the right handlers are live immediately; on
                // "auto" leave the current detection in place and let the normal
                // detectors take over again from the next frames.
                if (want != TeslaHW_Unknown) fsd_apply_hw_version(g_state, want);
                saved = *g_state;
                state_exit();
                Serial.printf("[Web] HW override: %s\n",
                              (want == TeslaHW_HW4)    ? "HW4" :
                              (want == TeslaHW_HW3)    ? "HW3" :
                              (want == TeslaHW_Legacy) ? "Legacy" : "Auto");
                prefs_save(&saved);
            } else {
                cmd.bad_value = true;
            }
        }
    } else if (cmd.is("ignore_ota")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->ignore_ota = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Ignore OTA: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("fsd_unlock")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->fsd_unlock = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] FSD Unlock: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("hw3_speed_override")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->hw3_speed_override = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] HW3 Speed Override: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("nag")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->nag_killer = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] NAG Killer: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("continuous_ap")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->continuous_ap = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Continuous AP: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("ap_first")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->ap_first = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] AP-First: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("ap_first_edge")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->ap_first_edge = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Instant Engage: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("ap_first_minimal")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->ap_first_minimal = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Minimal Inject: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("nag_faithful")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->nag_epas_faithful = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Nag EPAS-faithful: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("soft_engage")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->soft_engage = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Soft Engage: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("nag_burst")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->nag_burst = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Nag Burst: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("abort_guard")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->abort_guard = enabled;
            if (!enabled) g_state->abort_guard_latched = false;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Abort Guard: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("sig_cfg")) {
        // value is a 10-field CSV: das_id,apB,apS,apM,hoB,hoS,hoM,steer_id,sHi,sLo
        // ids accept 0x.. ; byte/shift 0-7, mask hex. (#122)
        char csv[128];
        if (cmd.string(csv, sizeof(csv))) {
            long v[10]; int got = 0;
            char *tok = strtok(csv, ",");
            while (tok && got < 10) { v[got++] = strtol(tok, NULL, 0); tok = strtok(NULL, ","); }
            if (got == 10) {
                FSDState saved;
                state_enter();
                g_state->cfg_das_id        = (uint16_t)v[0];
                g_state->cfg_apstate_byte  = (uint8_t)(v[1] & 7);
                g_state->cfg_apstate_shift = (uint8_t)(v[2] & 7);
                g_state->cfg_apstate_mask  = (uint8_t)v[3];
                g_state->cfg_handson_byte  = (uint8_t)(v[4] & 7);
                g_state->cfg_handson_shift = (uint8_t)(v[5] & 7);
                g_state->cfg_handson_mask  = (uint8_t)v[6];
                g_state->cfg_steer_id      = (uint16_t)v[7];
                g_state->cfg_steer_hi      = (uint8_t)(v[8] & 7);
                g_state->cfg_steer_lo      = (uint8_t)(v[9] & 7);
                saved = *g_state;
                state_exit();
                Serial.printf("[Web] Signal map: das=0x%X ap=%ld/%ld/0x%lX ho=%ld/%ld/0x%lX steer=0x%X %ld/%ld\n",
                              (uint16_t)v[0], v[1], v[2], v[3], v[4], v[5], v[6],
                              (uint16_t)v[7], v[8], v[9]);
                prefs_save(&saved);
            } else {
                cmd.bad_value = true;
            }
        }
    } else if (cmd.is("blackbox_enable")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            blackbox_set_enabled(enabled);          // locks the state mux itself
            FSDState saved;
            state_enter();
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Black-box: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("capability_recheck")) {
        capability_start(millis());                 // re-run the tap check (#125)
        Serial.println("[Web] Capability: re-check");
    } else if (cmd.is("can_counters_reset")) {
        can_counters_reset();                       // CAN Bus card only (#252)
        Serial.println("[Web] CAN counters reset");
    } else if (cmd.is("blackbox_mark")) {
        blackbox_mark(millis());                    // inject EVT_MANUAL + arm
        Serial.println("[Web] Black-box: manual mark");
    } else if (cmd.is("blackbox_delete_all")) {
        blackbox_delete_all();
        Serial.println("[Web] Black-box: delete all");
    } else if (cmd.is("blackbox_delete")) {
        char name[40];
        if (cmd.string(name, sizeof(name))) {
            if (name[0]) { blackbox_delete(name); Serial.printf("[Web] Black-box: delete %s\n", name); }
        }
    }
#if defined(BOARD_TTGO_DISPLAY)
    else if (cmd.is("disp")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->display_enabled = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Display: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("disp_br")) {
        int n;
        if (cmd.integer_capped(&n, 0, 100)) {
            uint8_t val = (uint8_t)n;
            FSDState saved;
            state_enter();
            g_state->display_brightness = val;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Display Brightness: %u\n", val);
            prefs_save(&saved);
        }
    } else if (cmd.is("disp_to")) {
        // 0 = never off; capped where the page's field stops, well short of
        // the seconds * 1000 overflow in the timeout check.
        int n;
        if (cmd.integer_capped(&n, 0, 3600)) {
            uint32_t val = (uint32_t)n;
            FSDState saved;
            state_enter();
            g_state->display_timeout_s = val;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Display Timeout: %u s\n", val);
            prefs_save(&saved);
        }
    }
#endif
    else if (cmd.is("bms")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->bms_output = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] BMS output: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("tlssc_restore")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->tlssc_restore = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] TLSSC Restore: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("14x_warning")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->firmware_14x_warning = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] 14.x Warning: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("force_fsd")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->force_fsd = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Force FSD: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("china_mode")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->china_mode = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] China Mode: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("suppress_speed_chime")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->suppress_speed_chime = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Suppress Speed Chime: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("precondition")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->precondition = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Precondition: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("summon_unlock")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->summon_unlock = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Summon EU Unlock: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("continue_on_green")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->continue_on_green = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Continue on Green: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("assist_tlssc_bit38")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->assist_tlssc_bit38 = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] TLSSC bit38: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("assist_rhd_override")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->assist_rhd_override = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] RHD Override: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("assist_telemetry_off")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->assist_telemetry_off = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Telemetry Off: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("apmv3_branch")) {
        // AP branch/tier selector (experimental, non-persistent): 0-5 select a
        // UI_apmv3Branch value, any other value (255 = Off) stores the 0xFF
        // sentinel so the handler leaves the frame untouched.
        int sel;
        if (cmd.integer(&sel)) {
            uint8_t want = (sel >= 0 && sel <= 5) ? (uint8_t)sel : 0xFF;
            FSDState saved;
            state_enter();
            g_state->apmv3_branch = want;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] AP Branch/Tier: %d\n", want);
            prefs_save(&saved);
        }
    } else if (cmd.is("accel_mode")) {
        // Acceleration Mode override (#211): 1 Chill / 2 Standard (ACCEL_MODE_SPORT)
        // / 3 Performance, anything else = Off (pass-through). The handler
        // latches a new value only at standstill; Off applies on the next 0x334.
        int sel;
        if (cmd.integer(&sel)) {
            uint8_t want = (sel >= (int)ACCEL_MODE_CHILL && sel <= (int)ACCEL_MODE_PERFORMANCE)
                               ? (uint8_t)sel : (uint8_t)ACCEL_MODE_OFF;
            FSDState saved;
            state_enter();
            g_state->accel_mode = want;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Acceleration Mode: %u\n", (unsigned)want);
            prefs_save(&saved);
        }
    } else if (cmd.is("cp_type_eu_rewrite")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->cp_type_eu_rewrite = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] EU CP Type rewrite: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("track_mode_inject")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->track_mode_inject = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Track Mode inject: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("track_rotation_pct")) {
        int n;
        if (cmd.integer_capped(&n, 0, 100)) {
            uint8_t val = (uint8_t)n;
            FSDState saved;
            state_enter();
            g_state->track_rotation_pct = val;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Track Handling Balance: %u\n", val);
            prefs_save(&saved);
        }
    } else if (cmd.is("track_stability_pct")) {
        int n;
        if (cmd.integer_capped(&n, 0, 100)) {
            uint8_t val = (uint8_t)n;
            FSDState saved;
            state_enter();
            g_state->track_stability_pct = val;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Track Stability Assist: %u\n", val);
            prefs_save(&saved);
        }
    } else if (cmd.is("track_post_cooling")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->track_post_cooling = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Track Post-drive Cooling: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("track_cmp_overclock")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->track_cmp_overclock = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Track Compressor Overclock: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("dump")) {
        bool want;
        if (cmd.boolean(&want)) {
            if (want) can_dump_start();
            else      can_dump_stop();
            Serial.printf("[Web] CAN Dump: %s\n", want ? "START" : "STOP");
        }
    } else if (cmd.is("mcp_rx_task")) {
        bool enabled;
        if (cmd.boolean(&enabled)) {
            FSDState saved;
            state_enter();
            g_state->mcp_rx_task = enabled;
            saved = *g_state;
            state_exit();
            // Starts or stops at once; drivers without the task ignore it.
            for (uint8_t i = 0; i < g_can_count; i++)
                if (g_can_buses[i]) g_can_buses[i]->setRxTask(enabled);
            Serial.printf("[Web] MCP2515 RX task: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (cmd.is("sleep")) {
        int n;
        if (cmd.integer_capped(&n, 10000, INT32_MAX)) {   // minimum 10 s
            uint32_t val = (uint32_t)n;
            FSDState saved;
            state_enter();
            g_state->sleep_idle_ms = val;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Sleep timeout: %u ms\n", val);
            prefs_save(&saved);
        }
    } else if (cmd.is("wifi_cfg")) {
        // All or nothing: one field that does not validate rejects the whole
        // message, nothing is saved and the device stays up. cmd.field names
        // the field for the reply.
        WebWifiCfg cfg;
        cmd.field = web_wifi_parse(cmd.value, &cfg);
        if (cmd.field) {
            Serial.printf("[Web] WiFi config rejected: bad %s\n", cmd.field);
            return WEB_CMD_BAD_VALUE;
        }
        FSDState saved;
        state_enter();
        if (cfg.has_ssid)     strlcpy(g_state->wifi_ssid, cfg.ssid, sizeof(g_state->wifi_ssid));
        if (cfg.has_pass)     strlcpy(g_state->wifi_pass, cfg.pass, sizeof(g_state->wifi_pass));
        if (cfg.has_hidden)   g_state->wifi_hidden = cfg.hidden;
        if (cfg.has_sta_ssid) strlcpy(g_state->wifi_sta_ssid, cfg.sta_ssid, sizeof(g_state->wifi_sta_ssid));
        if (cfg.has_sta_pass) strlcpy(g_state->wifi_sta_pass, cfg.sta_pass, sizeof(g_state->wifi_sta_pass));
        if (cfg.has_host)     strlcpy(g_state->wifi_hostname, cfg.host[0] ? cfg.host : WIFI_HOSTNAME_DEFAULT,
                                      sizeof(g_state->wifi_hostname));
        saved = *g_state;
        state_exit();
        Serial.printf("[Web] WiFi config: AP=\"%s\" STA=\"%s\" PASS=*** HIDDEN=%d HOST=%s\n",
            saved.wifi_ssid, saved.wifi_sta_ssid, saved.wifi_hidden, saved.wifi_hostname);
        prefs_save(&saved);
        return WEB_CMD_RESTART;
    } else {
        return WEB_CMD_UNKNOWN;
    }
    return cmd.bad_value ? WEB_CMD_BAD_VALUE : WEB_CMD_OK;
}

// The reply to a refused command, the same on both transports, e.g.
// {"ok":false,"err":"bad_value","cmd":"wifi_cfg","field":"pass"}. "cmd" only
// once the name matched one of the literals above, "field" only from
// wifi_cfg; a value is never echoed, it may be a password.
static String cmd_refusal(const WebCmd &cmd, WebCmdResult res) {
    String j = "{\"ok\":false,\"err\":\"";
    j += web_cmd_error(res);
    j += '"';
    if (res == WEB_CMD_BAD_VALUE) { j += ",\"cmd\":\""; j += cmd.name; j += '"'; }
    if (cmd.field) { j += ",\"field\":\""; j += cmd.field; j += '"'; }
    j += '}';
    Serial.printf("[Web] command refused: %s\n", j.c_str());
    return j;
}

// New WiFi settings are saved: restart to apply them. Called after the reply.
static void restart_for_wifi() {
    ota_verify_confirm("WiFi config restart");
    can_shutdown_all(g_can_buses, g_can_count);
    delay(500);
    ESP.restart();
}

// ── Whose commands are taken (web_origin.h) ──────────────────────────────────
// Text form of the names in WebOwnNames, filled in by own_names().
struct OwnNamesText {
    char ap[16];
    char sta[16];
    char host[sizeof(FSDState::wifi_hostname)];
};

// The addresses and hostname the device has right now.
static WebOwnNames own_names(OwnNamesText &t) {
    auto text = [](IPAddress ip, char *out) {
        snprintf(out, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    };
    t.ap[0] = t.sta[0] = '\0';
    wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) text(WiFi.softAPIP(), t.ap);
    if ((mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) && WiFi.status() == WL_CONNECTED) {
        text(WiFi.localIP(), t.sta);
    }
    state_enter();
    memcpy(t.host, g_state->wifi_hostname, sizeof(t.host));
    state_exit();
    t.host[sizeof(t.host) - 1] = '\0';
    return WebOwnNames{t.ap, t.sta, t.host};
}

// The dashboard at the address this client reached us on.
static String home_url() {
    return String("http://") + g_http.client().localIP().toString() + "/";
}

WebStreamAccess web_dashboard_stream_access(const char *origin, const char *host) {
    if (g_state == nullptr) return WEB_STREAM_REFUSE;
    OwnNamesText t;
    return web_stream_access(origin, host, own_names(t));
}

// GETs that hand out state (status, aux, black-box captures) answer only
// under our own names: a page on a rebound name is same-origin with them and
// could read them. The page itself is sent to our address under any other
// name (handle_root()), so the dashboard never gets this 403; like the one
// from /api/cmd it names the address to use.
static bool host_ours(const char *what) {
    String host = g_http.hostHeader();
    OwnNamesText t;
    if (web_host_header_ours(host.c_str(), own_names(t))) return true;
    Serial.printf("[Web] %s refused: Host %.64s\n", what, host.c_str());
    g_http.send(403, "application/json",
                String("{\"ok\":false,\"err\":\"host\",\"url\":\"") + home_url() + "\"}");
    return false;
}

// WebSocket handshake, once for each request header the library does not
// handle itself: a browser page that is not the dashboard is refused (the
// library answers 400 and the page falls back to HTTP polling).
static bool ws_header_ok(String name, String value) {
    if (!name.equalsIgnoreCase("Origin")) return true;
    OwnNamesText t;
    if (web_ws_origin_ok(value.c_str(), own_names(t))) return true;
    Serial.printf("[WS] connection refused: Origin %.64s\n", value.c_str());
    return false;
}

// ── WebSocket event handler ───────────────────────────────────────────────────
static void ws_event(uint8_t num, WStype_t type,
                     uint8_t *payload, size_t length)
{
    if (type == WStype_CONNECTED) {
        // Auto-run the tap capability check on connect (#125): the first few
        // seconds answer "will the nag killer work on this tap?" before any
        // guesswork. Pure RX — counting only.
        capability_start(millis());
        // Push current state immediately on connect
        String json = build_json();
        g_ws.sendTXT(num, json.c_str(), json.length());
        return;
    }

    if (type == WStype_TEXT) {
        // A refusal goes back to the sender; success needs no reply, the next
        // state push shows it.
        WebCmd cmd;
        WebCmdResult res = web_command(cmd, payload, length);
        if (res == WEB_CMD_RESTART) {
            restart_for_wifi();
        } else if (res != WEB_CMD_OK) {
            String reply = cmd_refusal(cmd, res);
            g_ws.sendTXT(num, reply.c_str(), reply.length());
        }
    }
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
// The page body, after the headers. The loop task services no CAN while this
// runs, so the socket is written directly and non-blocking (page_send.h);
// WiFiClient::write() would wait up to 10 s per call for a client that has
// gone. A client that takes nothing for PAGE_SEND_STALL_MS, or is not done in
// PAGE_SEND_MAX_MS, gets a cut page and the connection is dropped (WebServer
// lets go of it once this handler returns).
static void send_page_body(const uint8_t *ptr, size_t len) {
    WiFiClient client = g_http.client();
    int fd = client.fd();
    if (fd < 0) return;
    size_t sent = page_send(ptr, len, PAGE_SEND_STALL_MS, PAGE_SEND_MAX_MS,
        [fd](const uint8_t *p, size_t n) -> long {
            int r = send(fd, p, n, MSG_DONTWAIT);
            if (r >= 0) return r;
            return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
        },
        [fd](uint32_t ms) {
            fd_set w;
            FD_ZERO(&w);
            FD_SET(fd, &w);
            struct timeval tv = { (time_t)(ms / 1000u), (suseconds_t)((ms % 1000u) * 1000u) };
            return select(fd + 1, nullptr, &w, nullptr, &tv) >= 0;
        },
        []() { return (uint32_t)millis(); });
    if (sent < len) {
        Serial.printf("[Web] page cut at %u of %u bytes: client too slow or gone\n",
                      (unsigned)sent, (unsigned)len);
        client.stop();
    }
}

static bool client_takes_gzip() {
    String enc = g_http.header("Accept-Encoding");
    enc.toLowerCase();
    return enc.indexOf("gzip") >= 0;
}

static void handle_root() {
    // The page is served under our own names only: its origin is what the
    // WebSocket and /api/cmd check, and on the SoftAP every name resolves
    // here (captive portal). Under any other name it is sent to our address.
    OwnNamesText t;
    if (!web_host_header_ours(g_http.hostHeader().c_str(), own_names(t))) {
        g_http.sendHeader("Location", home_url(), true);
        g_http.send(302, "text/plain", "");
        return;
    }
    captive_portal_note_dashboard(g_http);   // portal "login" for Apple's probe (SoftAP only)
    // Every browser sends Accept-Encoding: gzip and gets WEB_HTML_GZ, under a
    // third of the bytes; any other client gets the page as it is.
    bool gz = g_page_gz_ok && client_takes_gzip();
    size_t len = gz ? sizeof(WEB_HTML_GZ) : sizeof(WEB_HTML) - 1;
    g_http.sendHeader("Vary", "Accept-Encoding");
    if (gz) g_http.sendHeader("Content-Encoding", "gzip");
    g_http.setContentLength(len);
    g_http.send(200, "text/html", "");
    send_page_body(gz ? WEB_HTML_GZ : (const uint8_t *)WEB_HTML, len);
}

static void handle_status() {
    if (g_state == nullptr) { g_http.send(503, "application/json", "{}"); return; }
    if (!host_ours("status")) return;
    g_http.sendHeader("Cache-Control", "no-store");
    g_http.send(200, "application/json", build_json());
}

// A POST that changes something has to come from the dashboard's own page, as
// on the WebSocket (web_origin.h); that also stops a rebound name, where the
// request is not cross-site at all. Otherwise this sends a 403 that names the
// address to load the dashboard from, and returns false.
static bool post_from_dashboard(const char *what) {
    String origin = g_http.header("Origin");
    String host = g_http.hostHeader();
    OwnNamesText t;
    if (web_post_origin_ok(origin.c_str(), own_names(t))) return true;
    Serial.printf("[Web] %s refused: Origin %.64s Host %.64s\n", what, origin.c_str(), host.c_str());
    g_http.send(403, "application/json",
                String("{\"ok\":false,\"err\":\"origin\",\"url\":\"") + home_url() + "\"}");
    return false;
}

// Commands over HTTP, for clients without a working WebSocket. The body is the
// same JSON text a WS frame carries and goes through the same dispatcher.
// Content-Type must be application/json: a cross-site form or no-cors fetch
// cannot send that without a CORS preflight, which this server never grants.
// The Origin has to be the dashboard's own page (post_from_dashboard()).
// Otherwise the reply is {"ok":true}, or a 400 with the reason the command
// was refused.
static void handle_cmd() {
    if (g_state == nullptr) { g_http.send(503, "application/json", "{}"); return; }
    if (!post_from_dashboard("command")) return;
    String ctype = g_http.header("Content-Type");
    ctype.toLowerCase();
    if (!ctype.startsWith("application/json")) {
        g_http.send(415, "text/plain", "Content-Type must be application/json");
        return;
    }
    String body = g_http.arg("plain");
    if (body.length() == 0) { g_http.send(400, "text/plain", "empty body"); return; }
    if (body.length() > WEB_CMD_MAX_LEN) {
        g_http.send(413, "text/plain", "command too long");
        return;
    }
    // The reply carries the outcome, so the command runs first; a wifi_cfg
    // restart waits until the reply has gone out.
    WebCmd cmd;
    WebCmdResult res = web_command(cmd, (const uint8_t *)body.c_str(), body.length());
    if (res != WEB_CMD_OK && res != WEB_CMD_RESTART) {
        g_http.send(400, "application/json", cmd_refusal(cmd, res));
        return;
    }
    g_http.send(200, "application/json", "{\"ok\":true}");
    if (res == WEB_CMD_RESTART) restart_for_wifi();
}

// On-demand aux status (blackbox/capability/profile). Polled by the dashboard
// on a slow timer so these heavy blocks stay off the hot WS state push (#124).
static void handle_aux() {
    if (g_state == nullptr) { g_http.send(503, "application/json", "{}"); return; }
    if (!host_ours("aux")) return;
    g_http.sendHeader("Cache-Control", "no-store");
    g_http.send(200, "application/json", build_aux_json());
}

static void handle_auth() {
    if (!require_admin_auth()) return;
    g_http.send(200, "text/plain", "OK");
}

// Wipes the card, so it takes what the other destructive routes take: a POST
// from the dashboard's own page (a GET could be set off by an <img> on any
// page) and the admin password.
static void handle_sdformat() {
    if (!post_from_dashboard("SD format")) return;
    if (!require_admin_auth()) return;
    String result = sd_format_card();
    g_http.send(200, "application/json", result);
}

// ── Black-box (#124) ──────────────────────────────────────────────────────────
static void handle_blackbox_list() {
    if (!host_ours("black-box list")) return;
    if (!download_auth_ok()) return;
    g_http.sendHeader("Cache-Control", "no-store");
    g_http.send(200, "application/json", blackbox_list_json());
}

static void handle_blackbox_get() {
    if (!host_ours("black-box download")) return;
    if (!download_auth_ok()) return;
    String name = g_http.arg("name");
    bool json = (g_http.arg("type") == "json");
    if (name.length() == 0 || name.length() >= 40) {
        g_http.send(400, "text/plain", "bad name");
        return;
    }
    size_t size = 0;
    if (!blackbox_file_size(name.c_str(), json, &size)) {
        g_http.send(404, "text/plain", "no such event");
        return;
    }
    String fname = name + (json ? ".json" : ".log");
    g_http.setContentLength(size);
    g_http.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
    g_http.sendHeader("Cache-Control", "no-store");
    g_http.send(200, json ? "application/json" : "text/plain", "");
    WiFiClient client = g_http.client();
    blackbox_stream_body(client, name.c_str(), json);
    client.flush();
}

static void handle_restart() {
    if (!require_admin_auth()) return;
    g_http.send(200, "text/plain", "OK");
    ota_verify_confirm("web restart");
    can_shutdown_all(g_can_buses, g_can_count);
    delay(500);
    ESP.restart();
}

// ── OTA Update handlers ───────────────────────────────────────────────────────
static size_t ota_total_size = 0;
static size_t ota_max_size = 0;
static bool ota_error_flag = false;
static bool ota_magic_checked = false;
static const char *ota_error_msg = nullptr;

static void handle_ota_upload() {
    HTTPUpload& upload = g_http.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] Start: %s\n", upload.filename.c_str());
        if (!require_admin_auth()) {
            ota_error_flag = true;
            ota_error_msg = "Authentication required";
            return;
        }
        ota_error_flag = false;
        ota_error_msg = nullptr;
        ota_magic_checked = false;
        ota_total_size = 0;
        ota_max_size = 0;

        if (!upload.filename.endsWith(".bin")) {
            Serial.println("[OTA] ERROR: File must be .bin");
            ota_error_flag = true;
            ota_error_msg = "File must be .bin";
            return;
        }

        size_t max_size = UPDATE_SIZE_UNKNOWN;
        const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
        if (partition != NULL) {
            max_size = partition->size;
            ota_max_size = partition->size;
            Serial.printf("[OTA] Target partition: %s, size: %u bytes\n",
                partition->label, (unsigned)max_size);
        } else {
            Serial.println("[OTA] ERROR: No OTA partition available");
            ota_error_flag = true;
            ota_error_msg = "No OTA partition available";
            return;
        }

        // esp_ota_begin() refuses while the running image is still unconfirmed.
        ota_verify_confirm("next web OTA");
        if (!Update.begin(max_size, U_FLASH)) {
            Update.printError(Serial);
            Serial.println("[OTA] ERROR: Update.begin() failed");
            ota_error_flag = true;
            ota_error_msg = "Update.begin() failed";
            return;
        }

        Serial.println("[OTA] Update started successfully");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (ota_error_flag) return;
        if (!ota_magic_checked) {
            if (upload.currentSize == 0 || upload.buf[0] != OTA_ESP32_IMAGE_MAGIC) {
                Serial.println("[OTA] ERROR: Invalid ESP32 image magic byte");
                ota_error_flag = true;
                ota_error_msg = "Invalid ESP32 image magic byte";
                Update.abort();
                return;
            }
            ota_magic_checked = true;
        }
        if (ota_max_size > 0 && (ota_total_size + upload.currentSize) > ota_max_size) {
            Serial.println("[OTA] ERROR: Firmware exceeds OTA partition size");
            ota_error_flag = true;
            ota_error_msg = "Firmware exceeds OTA partition size";
            Update.abort();
            return;
        }

        size_t written = Update.write(upload.buf, upload.currentSize);
        if (written != upload.currentSize) {
            Update.printError(Serial);
            Serial.printf("[OTA] ERROR: Write failed, expected %u, wrote %u\n",
                upload.currentSize, (unsigned)written);
            ota_error_flag = true;
            ota_error_msg = "Flash write failed";
            return;
        }

        ota_total_size += upload.currentSize;
        if (ota_total_size % 65536 == 0) {
            Serial.printf("[OTA] Progress: %u bytes\n", (unsigned)ota_total_size);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (ota_error_flag) {
            Serial.println("[OTA] Upload aborted due to previous error");
            Update.abort();
            return;
        }

        if (Update.end(true)) {
            Serial.printf("[OTA] Success: %u bytes total\n", (unsigned)ota_total_size);
            if (!Update.isFinished()) {
                Serial.println("[OTA] ERROR: Update not finished properly");
                ota_error_flag = true;
                ota_error_msg = "Update not finished properly";
            }
        } else {
            Update.printError(Serial);
            Serial.println("[OTA] ERROR: Update.end() failed");
            ota_error_flag = true;
            ota_error_msg = "Update.end() failed";
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Serial.println("[OTA] Upload aborted by client");
        Update.abort();
        ota_error_flag = true;
        ota_error_msg = "Upload aborted";
    }
}

static void handle_ota_done() {
    if (!require_admin_auth()) return;
    if (ota_error_flag || Update.hasError()) {
        String error_msg = "FAIL: ";
        if (Update.hasError()) {
            error_msg += "Error code " + String(Update.getError());
        } else if (ota_error_msg != nullptr) {
            error_msg += ota_error_msg;
        } else {
            error_msg += "Upload error";
        }

        Serial.printf("[OTA] %s\n", error_msg.c_str());
        g_http.send(500, "text/plain", error_msg);
        Update.abort();
        return;
    }

    g_http.send(200, "text/plain", "OK");

    Serial.println("[OTA] Firmware update successful!");
    Serial.println("[OTA] Rebooting in 2 seconds...");

    can_shutdown_all(g_can_buses, g_can_count);
    delay(2000);
    ESP.restart();
}

// ── Public API ────────────────────────────────────────────────────────────────
void web_dashboard_init(FSDState *state,
                        CanDriver **can_buses,
                        uint8_t can_count,
                        portMUX_TYPE *state_mux) {
    g_state       = state;
    g_can_buses   = can_buses;
    g_can_count   = can_count;
    g_state_mux   = state_mux;
    g_start_ms    = millis();
    g_last_fps_ms = millis();
    g_last_rx     = state ? state->rx_count : 0;
    g_last_can_seen_ms = (state && state->rx_count > 0) ? millis() : 0;

    // The gzip trailer holds the CRC-32 of what was compressed.
    const uint8_t *gz_end = WEB_HTML_GZ + sizeof(WEB_HTML_GZ);
    uint32_t gz_crc = (uint32_t)gz_end[-8] | ((uint32_t)gz_end[-7] << 8) |
                      ((uint32_t)gz_end[-6] << 16) | ((uint32_t)gz_end[-5] << 24);
    g_page_gz_ok = esp_rom_crc32_le(0, (const uint8_t *)WEB_HTML, sizeof(WEB_HTML) - 1) == gz_crc;
    Serial.printf("[Web] page %u bytes, gzip %u bytes%s\n", (unsigned)(sizeof(WEB_HTML) - 1),
                  (unsigned)sizeof(WEB_HTML_GZ), g_page_gz_ok ? "" : " (CRC mismatch, not sent)");

    g_http.on("/",           HTTP_GET,  handle_root);
    g_http.on("/api/status", HTTP_GET,  handle_status);
    g_http.on("/api/cmd",    HTTP_POST, handle_cmd);
    g_http.on("/api/aux",    HTTP_GET,  handle_aux);
    g_http.on("/auth",       HTTP_GET,  handle_auth);
    g_http.on("/sdformat",   HTTP_POST, handle_sdformat);
    g_http.on("/blackbox/list", HTTP_GET, handle_blackbox_list);
    g_http.on("/blackbox/get",  HTTP_GET, handle_blackbox_get);
    g_http.on("/restart",    HTTP_GET,  handle_restart);
    g_http.on("/update",     HTTP_POST, handle_ota_done, handle_ota_upload);
    // Captive portal: OS probe routes + catch-all onNotFound (SoftAP only).
    // Registered last so the explicit routes above take precedence.
    captive_portal_register_routes(g_http);
    // WebServer keeps only the request headers named here (plus Authorization),
    // and collectHeaders() replaces the list instead of adding to it. This is
    // the last call before begin(), so it names every header a handler reads:
    // Content-Type and Origin for /api/cmd, Origin for /sdformat, User-Agent
    // for the captive portal's probes, Accept-Encoding for the page.
    const char *collect[] = { "Content-Type", "Origin", "User-Agent", "Accept-Encoding" };
    g_http.collectHeaders(collect, sizeof(collect) / sizeof(collect[0]));
    g_http.begin();
    captive_portal_begin();
    http_can_stream_init();

    g_ws.begin();
    g_ws.onEvent(ws_event);
    g_ws.onValidateHttpHeader(ws_header_ok, nullptr, 0);
    // Drop a client that vanished without closing (phone WiFi off, out of
    // range) in ~10 s — ping every 2 s, disconnect after 3 missed 3 s pongs.
    // Otherwise every 1 Hz broadcast to it blocks loop() for up to
    // WEBSOCKETS_TCP_TIMEOUT (5 s), starving CAN RX and the nag echo (#270).
    g_ws.enableHeartbeat(2000, 3000, 3);

    Serial.println("[Web] HTTP :80  WS :81 — ready");
}

void web_dashboard_set_debug(bool on) { g_debug = on; }

void web_dashboard_update() {
    if (g_state == nullptr) return;   // init was never called (WiFi failed)

    captive_portal_process();
    g_http.handleClient();
    g_ws.loop();
    http_can_stream_update();

    // FPS calculation + 1 Hz WebSocket broadcast
    uint32_t now = millis();
    if ((now - g_last_fps_ms) >= 1000u) {
        FSDState state;
        if (!state_copy(&state)) return;
        uint32_t rx = state.rx_count;
        float    dt = (now - g_last_fps_ms) / 1000.0f;
        if (rx != g_last_rx) g_last_can_seen_ms = now;
        g_fps        = (float)(rx - g_last_rx) / dt;
        g_last_rx    = rx;
        g_last_fps_ms = now;

        // Nobody on the WebSocket: skip the push. /api/status builds its own.
        if (g_ws.connectedClients() == 0) return;
        String json = build_json();
        if (g_debug) Serial.printf("[WS] state json=%u bytes\n", (unsigned)json.length());
        g_ws.broadcastTXT(json.c_str(), json.length());
    }
}
