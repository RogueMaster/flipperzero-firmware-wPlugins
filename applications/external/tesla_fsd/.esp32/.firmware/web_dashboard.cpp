/*
 * web_dashboard.cpp — HTTP + WebSocket dashboard for Tesla FSD ESP32
 *
 * HTTP  :80  → serves the embedded HTML page
 * WS    :81  → pushes JSON state every 1 s; receives control commands
 *
 * All HTML/CSS/JS is embedded as a raw-string literal — no external CDN.
 * State is shared with the CAN task. Reads copy a locked snapshot; writes use
 * the same FreeRTOS critical section as the CAN/button side.
 */

#include "web_dashboard.h"
#include "can_dump.h"
#include "prefs.h"
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

// ── Module state ──────────────────────────────────────────────────────────────
static FSDState  *g_state = nullptr;   // shared with main
static CanDriver *g_can   = nullptr;   // for setListenOnly()
static portMUX_TYPE *g_state_mux = nullptr;

static WebServer        g_http(80);
static WebSocketsServer g_ws(81);

static uint32_t g_start_ms    = 0;
static uint32_t g_last_rx     = 0;
static uint32_t g_last_fps_ms = 0;
static uint32_t g_last_can_seen_ms = 0;
static float    g_fps         = 0.0f;

#define CAN_VEHICLE_ALIVE_MS 3000u
#define OTA_ESP32_IMAGE_MAGIC 0xE9u
#define OTA_AUTH_USER "admin"

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

// ── Embedded HTML/CSS/JS ──────────────────────────────────────────────────────
// Tesla dark theme; mobile-first (max 480 px); WebSocket on :81
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
  --border:#1e293b;--text:#e2e8f0;--text2:#94a3b8;--text3:#475569
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

/* ── Error banner ── */
.err{display:none;color:var(--red);text-align:center;font-size:.78em;padding:8px;
  background:rgba(255,107,107,.07);border-radius:10px;margin-bottom:10px;
  border:1px solid rgba(255,107,107,.18)}

/* ── Cards ── */
.card{background:var(--card);border-radius:16px;padding:16px;
  margin-bottom:12px;border:1px solid var(--border)}
.card-head{display:flex;align-items:center;gap:8px;margin-bottom:12px}
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

/* ── Controls ── */
.btn-main{width:100%;padding:14px;border:none;border-radius:12px;
  font-size:.95em;font-weight:700;cursor:pointer;letter-spacing:.04em;
  transition:opacity .2s;margin-bottom:10px}
.btn-main:active{opacity:.75}
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
</style>
</head>
<body>
<div class="wrap">

<!-- Header -->
<div class="hdr">
  <h1>Tesla FSD</h1>
  <div class="sub">ESP32 CAN Controller &middot; 192.168.4.1</div>
  <div class="cdot" id="dot"></div>
</div>
<div id="connErr" class="err">Connection lost &mdash; retrying&hellip;</div>

<div id="authPanel" class="auth-panel">
  <div class="auth-box">
    <h3>身份认证</h3>
    <div class="auth-msg">请输入管理员账号和 WiFi AP 密码</div>
    <label class="auth-field" for="authUser">用户名</label>
    <input id="authUser" class="auth-input" type="text" value="admin" autocomplete="username">
    <label class="auth-field" for="authPass">密码</label>
    <input id="authPass" class="auth-input" type="password" autocomplete="current-password">
    <div class="auth-actions">
      <button type="button" class="btn-main btn-stop" onclick="cancelAuth()">取消</button>
      <button type="button" class="btn-main btn-blue" onclick="submitAuth()">登录</button>
    </div>
  </div>
</div>

<div id="restartConfirmPanel" class="confirm-panel">
  <div class="auth-box">
    <h3>确认重启设备？</h3>
    <div class="auth-msg">设备将立即重启，网页连接会短暂中断。</div>
    <div class="auth-actions">
      <button type="button" class="btn-main btn-stop" onclick="cancelRestartConfirm()">否</button>
      <button type="button" class="btn-main btn-yellow" onclick="confirmRestart()">是</button>
    </div>
  </div>
</div>

<!-- OTA Warning -->
<div id="otaBanner" class="ota">&#9888;&#xFE0F; OTA UPDATE IN PROGRESS &mdash; CAN TX SUSPENDED</div>

<!-- FSD Status -->
<div class="card">
  <div class="card-head"><div class="icon ic-s">S</div><h2>FSD Status</h2></div>
  <div class="row">
    <span class="lbl">FSD Active</span>
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
  <div class="row">
    <span class="lbl">NAG Killer</span>
    <span class="pill off" id="nagSt"><span class="pd"></span>--</span>
  </div>
  <div class="row">
    <span class="lbl">CAN Vehicle</span>
    <span class="pill off" id="canVeh"><span class="pd"></span>--</span>
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
        <span class="soc-lbl">SOC</span>
      </div>
    </div>
    <div class="hg">
      <div><div class="hv" id="bVolt">--</div><div class="hl">Voltage</div></div>
      <div><div class="hv" id="bCurr">--</div><div class="hl">Current</div></div>
      <div><div class="hv" id="bTemp">--</div><div class="hl">Temp</div></div>
    </div>
  </div>
</div>

<!-- CAN Stats -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">C</div><h2>CAN Bus</h2></div>
  <div class="sg">
    <div class="sb"><div class="sv" id="rxCnt">0</div><div class="sl">RX Frames</div></div>
    <div class="sb"><div class="sv" id="txCnt">0</div><div class="sl">TX Modified</div></div>
    <div class="sb"><div class="sv" id="crcErr">0</div><div class="sl">CRC Errors</div></div>
    <div class="sb"><div class="sv" id="fps">0.0</div><div class="sl">Frames/s</div></div>
  </div>
</div>

<!-- Controls -->
<div class="card">
  <div class="card-head"><div class="icon ic-c">C</div><h2>Controls</h2></div>
  <button id="btnMode" class="btn-main btn-act" onclick="toggleMode()">ACTIVATE FSD</button>
  <div class="row">
    <span class="lbl">NAG Killer</span>
    <label class="sw"><input type="checkbox" id="swNag" onchange="cmd('nag',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">BMS Display</span>
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
  <div class="row">
    <span class="lbl">Suppress Chime</span>
    <label class="sw"><input type="checkbox" id="swChime" onchange="cmd('suppress_speed_chime',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">TLSSC Restore</span>
    <label class="sw"><input type="checkbox" id="swTlssc" onchange="cmd('tlssc_restore',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">CAN Dump</span>
    <label class="sw"><input type="checkbox" id="swDump" onchange="cmd('dump',this.checked)"><span class="sl2"></span></label>
  </div>
  <div class="row">
    <span class="lbl">Deep Sleep (sec)</span>
    <input type="number" id="numSleep" min="10" max="3600" style="width:60px;background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px;border-radius:4px;text-align:right" onchange="cmd('sleep',parseInt(this.value)*1000)">
  </div>
</div>

<!-- WiFi Config -->
<div class="card">
  <div class="card-head"><div class="icon ic-c">W</div><h2>WiFi Configuration</h2></div>
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
  <button class="btn-main btn-stop" onclick="saveWifi()" style="margin-top:12px">SAVE & RESTART WIFI</button>
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
    OTA writes to the next app partition when available. Keep USB reflashing available as a recovery path.
  </div>
</div>

<!-- SD Card -->
<div class="card">
  <div class="card-head"><div class="icon ic-d">S</div><h2>SD Card</h2></div>
  <div class="row">
    <span class="lbl">Dump Status</span>
    <span class="pill off" id="dumpSt"><span class="pd"></span>Idle</span>
  </div>
  <button id="btnFmt" class="btn-main btn-stop" onclick="sdFormat()" style="margin-top:8px">FORMAT SD CARD</button>
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
    <span class="lbl">WiFi Clients</span>
    <span id="wifiCl">--</span>
  </div>
  <div class="row">
    <span class="lbl">OTA Partition</span>
    <span id="otaPartInfo" style="font-size:.78em;color:var(--text2)">--</span>
  </div>
  <button class="btn-main btn-yellow" onclick="restartDevice(this)" style="margin-top:12px">RESTART DEVICE</button>
</div>

<div class="foot">Tesla FSD ESP32 &middot; M5Stack ATOM Lite + ATOMIC CAN Base</div>
</div><!-- /wrap -->

<script>
var ws,rt,busy=0,wifiOnce=false,authHeader='',authAction=null,restartAnchor=null;
var HW=['Unknown','Legacy','HW3','HW4'];
var CIRC=326.73;

function initWifi(d){
  if(wifiOnce)return;
  wifiOnce=true;
  document.getElementById('wifiSsid').value=d.wifi_ssid||'';
  document.getElementById('wifiPass').value=d.wifi_pass||'';
  document.getElementById('swWifiHid').checked=!!d.wifi_hidden;
}

function fmt(s){
  var h=Math.floor(s/3600),m=Math.floor((s%3600)/60),sc=s%60;
  return h+':'+(m<10?'0':'')+m+':'+(sc<10?'0':'')+sc;
}
function socCol(p){return p>60?'var(--accent)':p>30?'var(--yellow)':'var(--red)';}
function pill(id,on,txt,warnClass){
  var e=document.getElementById(id);
  e.className='pill '+(warnClass||''+(on?'on':'off'));
  e.innerHTML='<span class="pd"></span>'+txt;
}
function ring(p){
  var b=document.getElementById('socBar');
  b.style.strokeDashoffset=CIRC-(CIRC*Math.min(p,100)/100);
  b.style.stroke=socCol(p);
}

function upd(d){
  if(!d || Date.now() < busy) return;
  // Status
  pill('fsdSt', d.fsd_enabled, d.fsd_enabled?'Active':'Waiting');
  pill('opMode', d.op_mode===1, d.op_mode===1?'Active':'Listen-Only');

  var hwEl=document.getElementById('hwVer');
  if(hwEl){
    hwEl.className='pill '+(d.hw_version>0?'on':'off');
    hwEl.innerHTML='<span class="pd"></span>'+(HW[d.hw_version]||'?');
  }

  pill('nagSt', d.nag_killer, d.nag_killer?'ON':'OFF');
  pill('canVeh', d.can_vehicle_detected, d.can_vehicle_detected?'Detected':'No CAN Traffic');
  pill('bmsSt', d.bms && d.bms.seen, (d.bms && d.bms.seen)?'Live':'Waiting Frames');
  var bF=document.getElementById('bmsFrames');
  if(bF) bF.textContent='HV:'+(d.bms_hv_seen||0)+' SOC:'+(d.bms_soc_seen||0)+' TH:'+(d.bms_thermal_seen||0);

  // OTA banner
  var otaB=document.getElementById('otaBanner');
  if(otaB) otaB.style.display=d.ota?'block':'none';

  // Mode button
  var act=d.op_mode===1;
  var btn=document.getElementById('btnMode');
  if(btn){
    btn.textContent=act?'STOP FSD  \u2192  Listen-Only':'ACTIVATE FSD  \u2192  Active';
    btn.className='btn-main '+(act?'btn-stop':'btn-act');
  }

  // Switches sync
  if(document.getElementById('swNag')) document.getElementById('swNag').checked=d.nag_killer;
  if(document.getElementById('swBms')) document.getElementById('swBms').checked=d.bms_output;
  if(document.getElementById('swFsd')) document.getElementById('swFsd').checked=d.force_fsd;
  if(document.getElementById('swChina')) document.getElementById('swChina').checked=d.china_mode;
  if(document.getElementById('swChime')) document.getElementById('swChime').checked=d.suppress_speed_chime;
  if(document.getElementById('swTlssc')) document.getElementById('swTlssc').checked=d.tlssc_restore;
  if(document.getElementById('swDump')) document.getElementById('swDump').checked=!!d.can_dump;
  
  if(document.activeElement.id!=='numSleep' && document.getElementById('numSleep')) 
    document.getElementById('numSleep').value=Math.floor((d.sleep_ms||0)/1000);
  
  pill('dumpSt',d.can_dump,d.can_dump?'Recording':'Idle');

  // CAN stats
  if(document.getElementById('rxCnt')) document.getElementById('rxCnt').textContent=(d.rx_count||0).toLocaleString();
  if(document.getElementById('txCnt')) document.getElementById('txCnt').textContent=(d.tx_count||0).toLocaleString();
  if(document.getElementById('crcErr')) document.getElementById('crcErr').textContent=d.crc_errors||0;
  if(document.getElementById('fps')) document.getElementById('fps').textContent=(d.fps||0.0).toFixed(1);

  // Battery
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
    if(document.getElementById('bTemp')) document.getElementById('bTemp').textContent=d.bms.temp_min+'~'+d.bms.temp_max+'\u00b0C';
  }

  // Device
  if(document.getElementById('fwBuild')) document.getElementById('fwBuild').textContent=d.fw_build;
  if(document.getElementById('uptime')) document.getElementById('uptime').textContent=fmt(d.uptime_s||0);
  if(document.getElementById('wifiCl')) document.getElementById('wifiCl').textContent=d.wifi_clients||0;
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
  if(!file.name.endsWith('.bin')){alert('Error: Please select a .bin firmware file');input.value='';return;}
  var MAX_SIZE=16*1024*1024;
  if(file.size>MAX_SIZE){alert('Error: Firmware file too large (max 16MB)');input.value='';return;}
  if(file.size<32768 && !confirm('Warning: This file is very small ('+Math.round(file.size/1024)+' KB).\nAre you sure it is a valid ESP32 firmware?')){input.value='';return;}
  if(!confirm('Flash firmware: '+file.name+' ('+Math.round(file.size/1024)+' KB)?\n\nDevice will reboot after update.')){input.value='';return;}
  var prog=document.getElementById('otaProgress'),bar=document.getElementById('otaBar'),status=document.getElementById('otaStatus'),bytes=document.getElementById('otaBytes'),btn=document.getElementById('otaSelectBtn');
  prog.style.display='block';bar.style.width='0%';bar.style.background='var(--accent)';status.textContent='Uploading firmware...';status.style.color='var(--text2)';bytes.textContent='0 / '+Math.round(file.size/1024)+' KB';btn.disabled=true;btn.style.opacity='.5';
  var xhr=new XMLHttpRequest();
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
    alert('已触发设备重启');
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
    authFailed();
    if(p){p.value='';p.focus();}
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

function authFailed(){
  alert('Authentication failed');
  var input=document.getElementById('otaFile');
  if(input)input.value='';
}

function sdFormat(){
  if(!confirm('Format SD card? All data will be lost.'))return;
  var btn=document.getElementById('btnFmt');
  var out=document.getElementById('fmtOut');
  btn.disabled=true;btn.textContent='FORMATTING\u2026';
  fetch('/sdformat').then(function(r){return r.json();}).then(function(d){
    out.style.display='block';
    out.style.color=d.ok?'var(--accent)':'var(--red)';
    out.textContent=d.msg+(d.ok?' \u2014 '+d.free_mb+' MB free':'');
  }).catch(function(){
    out.style.display='block';out.style.color='var(--red)';out.textContent='Request failed';
  }).then(function(){
    btn.disabled=false;btn.textContent='FORMAT SD CARD';
  });
}
function saveWifi(){
  var s=document.getElementById('wifiSsid').value;
  var p=document.getElementById('wifiPass').value;
  var h=document.getElementById('swWifiHid').checked;
  if(s.length<1){alert('SSID required');return;}
  if(p.length>0 && p.length<8){alert('Password must be 8+ chars');return;}
  if(confirm('WiFi settings will be updated and the device will restart.')){
    var b=document.activeElement; if(b&&b.tagName==='BUTTON'){b.disabled=true;b.textContent='SAVING...';}
    cmd('wifi_cfg',{ssid:s,pass:p,hidden:h});
  }
}
function cmd(c,v){
  if(ws&&ws.readyState===1) {
    ws.send(JSON.stringify({cmd:c,value:v}));
    busy = Date.now() + 3000;
  }
}
function toggleMode(){ cmd('mode',null); }

function conn(){
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.onopen=function(){
    document.getElementById('dot').className='cdot';
    document.getElementById('connErr').style.display='none';
    clearTimeout(rt);
  };
  ws.onmessage=function(e){ try{var d=JSON.parse(e.data);initWifi(d);upd(d);}catch(x){} };
  ws.onclose=function(){
    document.getElementById('dot').className='cdot off';
    document.getElementById('connErr').style.display='block';
    rt=setTimeout(conn,2000);
  };
  ws.onerror=function(){ ws.close(); };
}
conn();
</script>
</body>
</html>
)rawliteral";

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

// ── JSON builder ──────────────────────────────────────────────────────────────
static String build_json() {
    FSDState state;
    if (!state_copy(&state)) return "{}";

    uint32_t uptime_s = (millis() - g_start_ms) / 1000;
    bool can_vehicle_detected = false;
    if (state.rx_count > 0) {
        can_vehicle_detected = (millis() - g_last_can_seen_ms) <= CAN_VEHICLE_ALIVE_MS;
    }

    // BMS sub-object
    char bms[128];
    if (state.bms_seen) {
        snprintf(bms, sizeof(bms),
            "{\"seen\":true,\"voltage\":%.1f,\"current\":%.1f,"
            "\"soc\":%.1f,\"temp_min\":%d,\"temp_max\":%d}",
            state.pack_voltage_v,
            state.pack_current_a,
            state.soc_percent,
            (int)state.batt_temp_min_c,
            (int)state.batt_temp_max_c);
    } else {
        strcpy(bms, "{\"seen\":false}");
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

    String j;
    j.reserve(768);
    j  = "{";
    j += "\"fsd_enabled\":";   j += state.fsd_enabled                 ? "true" : "false"; j += ',';
    j += "\"op_mode\":";       j += (int)state.op_mode;                j += ',';
    j += "\"hw_version\":";    j += (int)state.hw_version;             j += ',';
    j += "\"ota\":";           j += state.tesla_ota_in_progress        ? "true" : "false"; j += ',';
    j += "\"nag_killer\":";    j += state.nag_killer                   ? "true" : "false"; j += ',';
    j += "\"bms_output\":";    j += state.bms_output                   ? "true" : "false"; j += ',';
    j += "\"force_fsd\":";     j += state.force_fsd                    ? "true" : "false"; j += ',';
    j += "\"china_mode\":";    j += state.china_mode                   ? "true" : "false"; j += ',';
    j += "\"suppress_speed_chime\":"; j += state.suppress_speed_chime  ? "true" : "false"; j += ',';
    j += "\"tlssc_restore\":"; j += state.tlssc_restore                ? "true" : "false"; j += ',';
    j += "\"can_vehicle_detected\":"; j += can_vehicle_detected       ? "true" : "false"; j += ',';
    j += "\"bms_hv_seen\":";   j += state.seen_bms_hv;                 j += ',';
    j += "\"bms_soc_seen\":";  j += state.seen_bms_soc;                j += ',';
    j += "\"bms_thermal_seen\":"; j += state.seen_bms_thermal;          j += ',';
    j += "\"rx_count\":";      j += state.rx_count;                    j += ',';
    j += "\"tx_count\":";      j += state.frames_modified;             j += ',';
    j += "\"crc_errors\":";    j += state.crc_err_count;               j += ',';
    j += "\"fps\":";           j += fps_s;                             j += ',';
    j += "\"bms\":";           j += bms;                               j += ',';
    j += "\"uptime_s\":";      j += uptime_s;                          j += ',';
    j += "\"fw_build\":\"";    j += __DATE__;  j += ' '; j += __TIME__; j += "\",";
    j += "\"can_dump\":";      j += can_dump_active()                 ? "true" : "false"; j += ',';
    j += "\"sleep_ms\":";     j += state.sleep_idle_ms;               j += ',';
    j += "\"wifi_ssid\":\"";  j += json_escape(state.wifi_ssid);      j += "\",";
    j += "\"wifi_pass\":\"***\",";
    j += "\"wifi_hidden\":";  j += state.wifi_hidden                  ? "true" : "false"; j += ',';
    j += "\"wifi_clients\":";  j += (int)WiFi.softAPgetStationNum();   j += ',';
    j += "\"ota_partition\":"; j += ota_part;
    j += '}';
    return j;
}

// ── WebSocket event handler ───────────────────────────────────────────────────
static void ws_event(uint8_t num, WStype_t type,
                     uint8_t *payload, size_t length)
{
    if (type == WStype_CONNECTED) {
        // Push current state immediately on connect
        String json = build_json();
        g_ws.sendTXT(num, json.c_str(), json.length());
        return;
    }

    if (type != WStype_TEXT || g_state == nullptr || length == 0) return;

    // Use a slightly more robust way to find the value after the second colon
    char buf[256] = {};
    size_t n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
    memcpy(buf, payload, n);

    // Find the "value" part of {"cmd":"xxx","value":yyy}
    const char *vptr = strstr(buf, "\"value\":");
    if (vptr) vptr = strstr(vptr, ":") + 1;

    if (strstr(buf, "\"mode\"")) {
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
        if (g_can) g_can->setListenOnly(!active);
        Serial.println(active ? "[Web] → Active mode" : "[Web] → Listen-Only mode");
        prefs_save(&saved);
    } else if (strstr(buf, "\"nag\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool enabled = (strncmp(vptr, "true", 4) == 0);
            FSDState saved;
            state_enter();
            g_state->nag_killer = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] NAG Killer: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (strstr(buf, "\"bms\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool enabled = (strncmp(vptr, "true", 4) == 0);
            FSDState saved;
            state_enter();
            g_state->bms_output = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] BMS output: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (strstr(buf, "\"tlssc_restore\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool enabled = (strncmp(vptr, "true", 4) == 0);
            FSDState saved;
            state_enter();
            g_state->tlssc_restore = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] TLSSC Restore: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (strstr(buf, "\"force_fsd\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool enabled = (strncmp(vptr, "true", 4) == 0);
            FSDState saved;
            state_enter();
            g_state->force_fsd = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Force FSD: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (strstr(buf, "\"china_mode\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool enabled = (strncmp(vptr, "true", 4) == 0);
            FSDState saved;
            state_enter();
            g_state->china_mode = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] China Mode: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (strstr(buf, "\"suppress_speed_chime\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool enabled = (strncmp(vptr, "true", 4) == 0);
            FSDState saved;
            state_enter();
            g_state->suppress_speed_chime = enabled;
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] Suppress Speed Chime: %s\n", enabled ? "ON" : "OFF");
            prefs_save(&saved);
        }
    } else if (strstr(buf, "\"dump\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            bool want = (strncmp(vptr, "true", 4) == 0);
            if (want) can_dump_start();
            else      can_dump_stop();
            Serial.printf("[Web] CAN Dump: %s\n", want ? "START" : "STOP");
        }
    } else if (strstr(buf, "\"sleep\"")) {
        if (vptr) {
            while (*vptr == ' ' || *vptr == ':') vptr++;
            uint32_t val = (uint32_t)atoi(vptr);
            if (val >= 10000) { // minimum 10s
                FSDState saved;
                state_enter();
                g_state->sleep_idle_ms = val;
                saved = *g_state;
                state_exit();
                Serial.printf("[Web] Sleep timeout: %u ms\n", val);
                prefs_save(&saved);
            }
        }
    } else if (strstr(buf, "\"wifi_cfg\"")) {
        // Find the "value":{ object start
        const char *vobj = strstr(buf, "\"value\":");
        if (vobj) {
            FSDState saved;
            state_enter();
            char *s = strstr(vobj, "\"ssid\":\"");
            char *p = strstr(vobj, "\"pass\":\"");
            char *h = strstr(vobj, "\"hidden\":");
            if (s) {
                s += 8;
                char *end = strchr(s, '\"');
                if (end) {
                    int len = end - s;
                    if (len > 32) len = 32;
                    if (memchr(s, '\\', len) == nullptr) {
                        memcpy(g_state->wifi_ssid, s, len);
                        g_state->wifi_ssid[len] = '\0';
                    }
                }
            }
            if (p) {
                p += 8;
                char *end = strchr(p, '\"');
                if (end) {
                    int len = end - p;
                    if (len > 64) len = 64;
                    if (memchr(p, '\\', len) == nullptr &&
                        !(len == 3 && memcmp(p, "***", 3) == 0)) {
                        memcpy(g_state->wifi_pass, p, len);
                        g_state->wifi_pass[len] = '\0';
                    }
                }
            }
            if (h) {
                h += 9;
                while (*h == ' ' || *h == ':') h++;
                if (strncmp(h, "true", 4) == 0) g_state->wifi_hidden = true;
                else if (strncmp(h, "false", 5) == 0) g_state->wifi_hidden = false;
            }
            saved = *g_state;
            state_exit();
            Serial.printf("[Web] WiFi config: SSID=\"%s\" PASS=*** HIDDEN=%d\n",
                saved.wifi_ssid, saved.wifi_hidden);
            prefs_save(&saved);
            delay(500);
            ESP.restart();
        }
    }
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
static void handle_root() {
    g_http.send_P(200, "text/html", WEB_HTML);
}

static void handle_status() {
    if (g_state == nullptr) { g_http.send(503, "application/json", "{}"); return; }
    g_http.send(200, "application/json", build_json());
}

static void handle_auth() {
    if (!require_admin_auth()) return;
    g_http.send(200, "text/plain", "OK");
}

static void handle_sdformat() {
    String result = sd_format_card();
    g_http.send(200, "application/json", result);
}

static void handle_restart() {
    if (!require_admin_auth()) return;
    g_http.send(200, "text/plain", "OK");
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

    delay(2000);
    ESP.restart();
}

// ── Public API ────────────────────────────────────────────────────────────────
void web_dashboard_init(FSDState *state, CanDriver *can, portMUX_TYPE *state_mux) {
    g_state       = state;
    g_can         = can;
    g_state_mux   = state_mux;
    g_start_ms    = millis();
    g_last_fps_ms = millis();
    g_last_rx     = state ? state->rx_count : 0;
    g_last_can_seen_ms = (state && state->rx_count > 0) ? millis() : 0;

    g_http.on("/",           HTTP_GET,  handle_root);
    g_http.on("/api/status", HTTP_GET,  handle_status);
    g_http.on("/auth",       HTTP_GET,  handle_auth);
    g_http.on("/sdformat",   HTTP_GET,  handle_sdformat);
    g_http.on("/restart",    HTTP_GET,  handle_restart);
    g_http.on("/update",     HTTP_POST, handle_ota_done, handle_ota_upload);
    g_http.begin();

    g_ws.begin();
    g_ws.onEvent(ws_event);

    Serial.println("[Web] HTTP :80  WS :81 — ready");
}

void web_dashboard_update() {
    if (g_state == nullptr) return;   // init was never called (WiFi failed)

    g_http.handleClient();
    g_ws.loop();

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

        String json = build_json();
        g_ws.broadcastTXT(json.c_str(), json.length());
    }
}
