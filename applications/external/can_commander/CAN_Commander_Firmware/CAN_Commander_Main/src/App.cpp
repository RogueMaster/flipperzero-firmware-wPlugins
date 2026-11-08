#include "App.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/ToolUtils.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <errno.h>
#include <lwip/sockets.h>
#endif

namespace {

static constexpr uint8_t kProtocolVersion = io::FlipperProtocol::kProtocolVersion;

// Output pacing (see App::flushOutputs).
static constexpr uint8_t kCliLinesPerLoop = 32;
static constexpr uint8_t kFlipperPacketsPerLoop = 64;
static constexpr uint32_t kUsbStallMs = 250;
static constexpr uint32_t kWifiStallMs = 5000;
static constexpr size_t kUsbUartTxBufferSize = 4096;
static constexpr size_t kFlipperUartTxBufferSize = 4096;
static constexpr size_t kFlipperUartRxBufferSize = 1024;

// USB output can be paced (checked with availableForWrite) on the hardware USB-CDC
// (HWCDC) and on UART0; TinyUSB CDC keeps the core's own write behavior.
#if defined(ARDUINO_ARCH_ESP32) && (!ARDUINO_USB_CDC_ON_BOOT || ARDUINO_USB_MODE)
#define CC_USB_OUTPUT_PACED 1
#else
#define CC_USB_OUTPUT_PACED 0
#endif

inline void writeU16LE(uint8_t* dst, uint16_t v) {
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
}

inline void writeU32LE(uint8_t* dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
    dst[2] = (uint8_t)((v >> 16) & 0xFF);
    dst[3] = (uint8_t)((v >> 24) & 0xFF);
}

inline uint16_t readU16LE(const uint8_t* src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

inline uint32_t readU32LE(const uint8_t* src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

inline uint64_t readU64LE(const uint8_t* src) {
    return ((uint64_t)src[0]) | ((uint64_t)src[1] << 8) | ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) | ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
}

inline float readFloatLE(const uint8_t* src) {
    uint32_t raw = readU32LE(src);
    float out = 0.0f;
    memcpy(&out, &raw, sizeof(float));
    return out;
}

const char* statusName(core::StatusCode status) {
    switch(status) {
    case core::StatusCode::OK:
        return "OK";
    case core::StatusCode::BAD_CMD:
        return "BAD_CMD (command not supported here)";
    case core::StatusCode::BAD_ARG:
        return "BAD_ARG (missing or invalid argument)";
    case core::StatusCode::BUS_ERR:
        return "BUS_ERR (hardware unavailable or request rejected)";
    case core::StatusCode::LOCKED:
        return "LOCKED";
    case core::StatusCode::NO_MEM:
        return "NO_MEM (storage full)";
    case core::StatusCode::CRC_FAIL:
        return "CRC_FAIL";
    case core::StatusCode::NOT_ACTIVE:
        return "NOT_ACTIVE (nothing to act on)";
    default:
        return "UNKNOWN";
    }
}

} // namespace

App::App()
    : flipper_uart_(1)
    ,
#if defined(ARDUINO_ARCH_ESP32)
    wifi_cli_server_(BoardConfig::WIFI_CLI_PORT)
    , wifi_ap_ready_(false)
    , wifi_autostart_(BoardConfig::WIFI_AP_AUTOSTART != 0)
    ,
#endif
    owner_(core::InterfaceOwner::NONE)
    , last_flipper_activity_ms_(0)
    , drop_event_dirty_(false)
    , last_drop_event_ms_(0) {
    usb_dropped_ = 0;
    usb_blocked_since_ms_ = 0;
#if defined(ARDUINO_ARCH_ESP32)
    wifi_ssid_[0] = '\0';
    wifi_password_[0] = '\0';
    wifi_pending_[0] = '\0';
    wifi_pending_len_ = 0;
    wifi_pending_off_ = 0;
    wifi_last_progress_ms_ = 0;
    wifi_dropped_ = 0;
#endif
}

void App::setup() {
#if defined(ARDUINO_ARCH_ESP32) && !ARDUINO_USB_CDC_ON_BOOT
    // CLI on UART0: buffer output so writes can be paced instead of blocking.
    Serial.setTxBufferSize(kUsbUartTxBufferSize);
#endif
    Serial.begin(BoardConfig::USB_CLI_BAUD);
    delay(50);

    sendCliLine("CAN Commander v%s booting...", BoardConfig::FIRMWARE_VERSION_STRING);

#if defined(ARDUINO_ARCH_ESP32)
    // Buffer sizes must be set before begin(). The TX ring lets flushFlipper() check
    // free space instead of blocking; the RX buffer absorbs command bursts.
    flipper_uart_.setRxBufferSize(kFlipperUartRxBufferSize);
    flipper_uart_.setTxBufferSize(kFlipperUartTxBufferSize);
    flipper_uart_.begin(
        BoardConfig::FLIPPER_UART_BAUD,
        SERIAL_8N1,
        BoardConfig::FLIPPER_UART_RX,
        BoardConfig::FLIPPER_UART_TX);
#else
    flipper_uart_.begin(BoardConfig::FLIPPER_UART_BAUD);
#endif

    flipper_proto_.begin(flipper_uart_);
    cli_.begin(Serial);
#if defined(ARDUINO_ARCH_ESP32)
    setupWifiAp();
#endif
    leds_.begin();

    (void)can_.begin();
    if(!can_.isBusAvailable(core::BusId::CAN0)) {
        sendCliLine("CAN0 init error: %s", can_.busError(core::BusId::CAN0));
    } else {
        sendCliLine(
            "CAN0 initialized; CAN1 backend=%s state=%s",
            can_.can1BackendName(),
            can_.isBusAvailable(core::BusId::CAN1) ? "ready" : "disabled");
    }
    if(BoardConfig::CAN1_ENABLED && !can_.isBusAvailable(core::BusId::CAN1)) {
        // A missing second controller is not fatal: CAN0 keeps working.
        sendCliLine(
            "WARN: CAN1 (%s) not available: %s",
            can_.can1BackendName(),
            can_.busError(core::BusId::CAN1));
        sendCliLine(
            "WARN: build with BOARDCFG_CAN1_DRIVER=0 if this board has no second CAN controller");
    }

    sendCliLine("Type 'help' for commands.");
}

void App::loop() {
    releaseFlipperOnTimeout();
    pollInputs();

    can_.poll();
    processCan();
    processTools();

    if(drop_event_dirty_ && (uint32_t)(millis() - last_drop_event_ms_) > 500) {
        uint8_t payload[8];
        writeU32LE(&payload[0], cli_counters_.dropped);
        writeU32LE(&payload[4], flipper_counters_.dropped);
        pushFlipperEvent((uint8_t)core::FlipperEventId::DROPS_EVT, payload, sizeof(payload));
        event_counters_.drops_evt++;
        sendCliLine(
            "drops: cli=%lu flipper=%lu",
            (unsigned long)cli_counters_.dropped,
            (unsigned long)flipper_counters_.dropped);
        drop_event_dirty_ = false;
        last_drop_event_ms_ = millis();
    }

    flushOutputs();
    // The error LED reflects CAN0 health, so it clears once CAN0 recovers.
    leds_.setError(!can_.isBusAvailable(core::BusId::CAN0));
#if defined(ARDUINO_ARCH_ESP32)
    leds_.setWifiApUp(wifi_ap_ready_);
    leds_.setWifiClientConnected(wifi_cli_client_ && wifi_cli_client_.connected());
#endif
    leds_.tick();
    yield();
}

uint32_t App::nowMs() const {
    return millis();
}

bool App::sendCanFrame(const core::CanFrame& frame) {
    const bool ok = can_.sendFrame(frame);
    if(ok) {
        leds_.noteTx();
    }
    return ok;
}

void App::emitCanFrameEvent(const core::CanFrame& frame, bool ascii_mode) {
    char line[BoardConfig::CLI_OUT_MAX];
    tools::formatFrameHex(frame, line, sizeof(line), ascii_mode);
    pushCliLineRaw(line);

    uint8_t payload[4 + 1 + 4 + 1 + 1 + 8] = {0};
    writeU32LE(&payload[0], frame.ts_ms);
    payload[4] = (uint8_t)frame.bus;
    writeU32LE(&payload[5], frame.id);
    payload[9] = (uint8_t)((frame.ext ? 0x01 : 0x00) | (frame.rtr ? 0x02 : 0x00));
    payload[10] = frame.dlc;
    memcpy(&payload[11], frame.data, 8);
    pushFlipperEvent((uint8_t)core::FlipperEventId::CAN_FRAME_EVT, payload, sizeof(payload));

    event_counters_.can_frame++;
}

void App::emitToolEvent(core::ToolId tool, core::ToolEventCode code, const char* text) {
    char line[BoardConfig::CLI_OUT_MAX];
    snprintf(
        line,
        sizeof(line),
        "[%s] %s",
        tools::ToolManager::toolName(tool),
        text != nullptr ? text : "");
    pushCliLineRaw(line);

    uint8_t payload[BoardConfig::MAX_FLIPPER_PAYLOAD] = {0};
    uint8_t text_len = 0;
    if(text != nullptr) {
        text_len = (uint8_t)strnlen(text, 120);
    }

    payload[0] = (uint8_t)tool;
    payload[1] = (uint8_t)code;
    payload[2] = text_len;
    if(text_len > 0) {
        memcpy(&payload[3], text, text_len);
    }
    pushFlipperEvent((uint8_t)core::FlipperEventId::TOOL_EVT, payload, (uint16_t)(3 + text_len));

    event_counters_.tool_evt++;
}

void App::emitDbcDecodeEvent(const core::DbcDecodedValue& value) {
    char line[BoardConfig::CLI_OUT_MAX];
    snprintf(
        line,
        sizeof(line),
        "dbc sid=%u bus=%s id=0x%lX raw=%lld value=%.4f %s %s",
        value.sid,
        core::busName(value.bus),
        (unsigned long)value.frame_id,
        (long long)value.raw,
        value.value,
        value.unit,
        value.in_range ? "ok" : "oor");
    pushCliLineRaw(line);

    uint8_t payload[2 + 1 + 4 + 8 + 4 + 1 + 12] = {0};
    writeU16LE(&payload[0], value.sid);
    payload[2] = (uint8_t)value.bus;
    writeU32LE(&payload[3], value.frame_id);
    memcpy(&payload[7], &value.raw, 8);
    memcpy(&payload[15], &value.value, 4);
    payload[19] = value.in_range ? 1 : 0;
    memcpy(&payload[20], value.unit, 12);
    pushFlipperEvent((uint8_t)core::FlipperEventId::DBC_DECODE_EVT, payload, sizeof(payload));

    event_counters_.dbc_evt++;
}

decode::DbcSignalStore& App::dbcStore() {
    return dbc_store_;
}

bool App::isBusAvailable(core::BusId bus) const {
    return can_.isBusAvailable(bus);
}

uint16_t App::outputHeadroom() const {
    const size_t cli_free = cli_tx_.freeSlots();
    const size_t flipper_free = flipper_tx_.freeSlots();
    return (uint16_t)(cli_free < flipper_free ? cli_free : flipper_free);
}

#if defined(ARDUINO_ARCH_ESP32)
void App::setupWifiAp() {
    if(!BoardConfig::WIFI_AP_ENABLED) {
        sendCliLine("WiFi AP support compiled out");
        return;
    }

    loadWifiSettings();
    if(!wifi_autostart_) {
        sendCliLine("WiFi AP autostart=off (use 'wifi start' to enable now)");
        return;
    }
    startWifiAp();
}

bool App::startWifiAp() {
    if(!BoardConfig::WIFI_AP_ENABLED) {
        return false;
    }
    if(wifi_ap_ready_) {
        return true;
    }

    const size_t ssid_len = strnlen(wifi_ssid_, sizeof(wifi_ssid_));
    const size_t pass_len = strnlen(wifi_password_, sizeof(wifi_password_));
    if(ssid_len == 0 || ssid_len > 32) {
        sendCliLine("WiFi AP start failed: invalid ssid");
        return false;
    }
    if(pass_len < 8 || pass_len > 63) {
        sendCliLine("WiFi AP start failed: password must be 8..63 chars");
        return false;
    }

    WiFi.mode(WIFI_AP);
    const bool ok = WiFi.softAP(
        wifi_ssid_,
        wifi_password_,
        BoardConfig::WIFI_AP_CHANNEL,
        0,
        BoardConfig::WIFI_AP_MAX_CLIENTS);
    if(!ok) {
        sendCliLine("WiFi AP start failed");
        return false;
    }

    wifi_cli_server_.begin();
    wifi_cli_server_.setNoDelay(true);
    wifi_ap_ready_ = true;

    const IPAddress ip = WiFi.softAPIP();
    sendCliLine(
        "WiFi AP ready ssid=%s ip=%u.%u.%u.%u cli_port=%u",
        wifi_ssid_,
        (unsigned)ip[0],
        (unsigned)ip[1],
        (unsigned)ip[2],
        (unsigned)ip[3],
        (unsigned)BoardConfig::WIFI_CLI_PORT);
    return true;
}

void App::stopWifiAp() {
    if(wifi_cli_client_) {
        wifi_cli_client_.stop();
    }
    wifi_cli_server_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    wifi_ap_ready_ = false;
}

void App::loadWifiSettings() {
    strncpy(wifi_ssid_, BoardConfig::WIFI_AP_SSID, sizeof(wifi_ssid_) - 1);
    wifi_ssid_[sizeof(wifi_ssid_) - 1] = '\0';
    strncpy(wifi_password_, BoardConfig::WIFI_AP_PASSWORD, sizeof(wifi_password_) - 1);
    wifi_password_[sizeof(wifi_password_) - 1] = '\0';
    wifi_autostart_ = (BoardConfig::WIFI_AP_AUTOSTART != 0);

    Preferences prefs;
    if(!prefs.begin("can_commander", true)) {
        return;
    }

    const String saved_ssid = prefs.getString("wifi_ssid", "");
    if(saved_ssid.length() > 0 && saved_ssid.length() <= 32) {
        saved_ssid.toCharArray(wifi_ssid_, sizeof(wifi_ssid_));
    }

    const String saved_pass = prefs.getString("wifi_pass", "");
    if(saved_pass.length() >= 8 && saved_pass.length() <= 63) {
        saved_pass.toCharArray(wifi_password_, sizeof(wifi_password_));
    }

    wifi_autostart_ = prefs.getBool("wifi_auto", wifi_autostart_);
    prefs.end();
}

bool App::saveWifiSettings() const {
    Preferences prefs;
    if(!prefs.begin("can_commander", false)) {
        return false;
    }

    const bool ok = prefs.putString("wifi_ssid", wifi_ssid_) == strlen(wifi_ssid_) &&
                    prefs.putString("wifi_pass", wifi_password_) == strlen(wifi_password_) &&
                    prefs.putBool("wifi_auto", wifi_autostart_) == 1;
    prefs.end();
    return ok;
}

void App::pollWifiCli() {
    if(!wifi_ap_ready_) {
        return;
    }

    if(wifi_cli_client_ && !wifi_cli_client_.connected()) {
        wifi_cli_client_.stop();
        sendCliLine("WiFi CLI client disconnected");
    }

    if(wifi_cli_server_.hasClient()) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        WiFiClient incoming = wifi_cli_server_.accept();
#else
        WiFiClient incoming = wifi_cli_server_.available();
#endif
        if(wifi_cli_client_ && wifi_cli_client_.connected()) {
            incoming.println("ERR: WiFi CLI busy");
            incoming.stop();
        } else {
            wifi_cli_client_ = incoming;
            wifi_pending_len_ = 0; // never send a previous client's partial line
            wifi_cli_.begin(wifi_cli_client_);
            sendCliLine("WiFi CLI client connected");
            sendCliLine(
                "WiFi CLI ready. Commands: help | inject slot=<1-5> [count=<n>] [interval_ms=<ms>]");
        }
    }

    if(!(wifi_cli_client_ && wifi_cli_client_.connected())) {
        return;
    }

    wifi_cli_.poll();
    core::CliLine line;
    while(wifi_cli_.popLine(line)) {
        cmd::Request req;
        if(router_.parseCli(line.line, req)) {
            handleRequest(req);
        }
    }
    if(wifi_cli_.takeOverflow()) {
        sendCliLine(
            "ERR: line too long (max %u chars), ignored",
            (unsigned)(BoardConfig::CLI_LINE_MAX - 1));
    }
}

void App::queueWifiLine(const char* line, size_t len, uint32_t now) {
    if(!(wifi_cli_client_ && wifi_cli_client_.connected())) {
        wifi_pending_len_ = 0;
        return;
    }
    if(wifi_pending_len_ > 0) {
        // The previous line is still going out; drop this one for Wi-Fi only.
        wifi_dropped_++;
        return;
    }
    if(len > BoardConfig::CLI_OUT_MAX) {
        len = BoardConfig::CLI_OUT_MAX;
    }
    memcpy(wifi_pending_, line, len);
    wifi_pending_[len] = '\r';
    wifi_pending_[len + 1] = '\n';
    wifi_pending_len_ = (uint16_t)(len + 2);
    wifi_pending_off_ = 0;
    wifi_last_progress_ms_ = now;
    serviceWifiPending(now);
}

void App::serviceWifiPending(uint32_t now) {
    if(wifi_pending_len_ == 0) {
        return;
    }
    if(!(wifi_cli_client_ && wifi_cli_client_.connected())) {
        wifi_pending_len_ = 0;
        return;
    }

    // Non-blocking send: WiFiClient::write() can block for seconds on a peer that
    // stopped reading, which would freeze CAN processing.
    const int fd = wifi_cli_client_.fd();
    const ssize_t n = send(
        fd,
        wifi_pending_ + wifi_pending_off_,
        (size_t)(wifi_pending_len_ - wifi_pending_off_),
        MSG_DONTWAIT);
    if(n > 0) {
        wifi_pending_off_ = (uint16_t)(wifi_pending_off_ + n);
        wifi_last_progress_ms_ = now;
        if(wifi_pending_off_ >= wifi_pending_len_) {
            wifi_pending_len_ = 0;
        }
        return;
    }
    if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        wifi_cli_client_.stop();
        wifi_pending_len_ = 0;
        return;
    }
    if((uint32_t)(now - wifi_last_progress_ms_) > kWifiStallMs) {
        wifi_cli_client_.stop();
        wifi_pending_len_ = 0;
        sendCliLine("WiFi CLI client stopped reading; disconnected");
    }
}
#endif

void App::pollInputs() {
    cli_.poll();
    core::CliLine line;
    while(cli_.popLine(line)) {
        cmd::Request req;
        if(router_.parseCli(line.line, req)) {
            handleRequest(req);
        }
    }
    if(cli_.takeOverflow()) {
        sendCliLine(
            "ERR: line too long (max %u chars), ignored",
            (unsigned)(BoardConfig::CLI_LINE_MAX - 1));
    }

#if defined(ARDUINO_ARCH_ESP32)
    pollWifiCli();
#endif

    core::FlipperPacket packet;
    while(flipper_proto_.pollPacket(packet)) {
        last_flipper_activity_ms_ = millis();
        cmd::Request req;
        if(router_.parseFlipper(packet, req)) {
            handleRequest(req);
        }
    }
}

void App::processCan() {
    core::CanFrame frame;
    while(can_.readFrame(frame)) {
        leds_.noteRx();
        tools_.onFrame(frame, *this);
    }
}

void App::processTools() {
    tools_.tick(*this);
    leds_.setToolActive(tools_.isActive());
}

// Output never blocks the main loop: CAN polling, timed transmissions and command
// handling must keep running whatever the consumers do.
// - Work per loop is bounded (kCliLinesPerLoop / kFlipperPacketsPerLoop).
// - USB and the Flipper UART are written only when their buffers have room; the
//   queues absorb short bursts (and drop the oldest entries when full).
// - A USB host that stays full for kUsbStallMs (terminal open but not reading) stops
//   receiving lines until it drains, so it cannot starve the Wi-Fi client.
// - The Wi-Fi client gets non-blocking sends; a client that makes no progress for
//   kWifiStallMs is disconnected.
void App::flushOutputs() {
    flushCli();
    flushFlipper();
}

App::UsbReady App::usbReady(size_t need, uint32_t now) {
#if CC_USB_OUTPUT_PACED
    if(!Serial) {
        return UsbReady::SKIP; // no USB host attached
    }
    const int room = Serial.availableForWrite();
    if(room >= (int)need) {
        usb_blocked_since_ms_ = 0;
        return UsbReady::WRITE;
    }
    if(usb_blocked_since_ms_ == 0) {
        usb_blocked_since_ms_ = now | 1U;
    }
    if((uint32_t)(now - usb_blocked_since_ms_) >= kUsbStallMs) {
        return UsbReady::STALLED;
    }
    return UsbReady::WAIT;
#else
    (void)need;
    (void)now;
    return UsbReady::WRITE;
#endif
}

void App::flushCli() {
    const uint32_t now = millis();
#if defined(ARDUINO_ARCH_ESP32)
    serviceWifiPending(now);
#endif

    for(uint8_t n = 0; n < kCliLinesPerLoop; n++) {
        const core::CliOut* next = cli_tx_.front();
        if(next == nullptr) {
            break;
        }
        const size_t len = strnlen(next->line, sizeof(next->line));
        const UsbReady usb = usbReady(len + 2, now);
        if(usb == UsbReady::WAIT) {
            break; // USB is draining; keep the line queued and retry next loop
        }

        core::CliOut msg;
        cli_tx_.pop(msg);
        if(usb == UsbReady::WRITE) {
            Serial.write((const uint8_t*)msg.line, len);
            Serial.write((const uint8_t*)"\r\n", 2);
        } else if(usb == UsbReady::STALLED) {
            usb_dropped_++;
        }
#if defined(ARDUINO_ARCH_ESP32)
        queueWifiLine(msg.line, len, now);
#endif
        cli_counters_.sent++;
    }
}

void App::flushFlipper() {
    for(uint8_t n = 0; n < kFlipperPacketsPerLoop; n++) {
        core::RingQueue<FlipperOut, 32>& resp = flipper_resp_tx_;
        const bool from_resp = !resp.empty();
        const FlipperOut* next = from_resp ? resp.front() : flipper_tx_.front();
        if(next == nullptr) {
            break;
        }
#if defined(ARDUINO_ARCH_ESP32)
        if(flipper_uart_.availableForWrite() <
           (int)io::FlipperProtocol::maxEncodedSize(next->packet.len)) {
            break; // the UART drains at line rate; retry next loop
        }
#endif
        FlipperOut msg;
        if(from_resp) {
            resp.pop(msg);
        } else {
            flipper_tx_.pop(msg);
        }
        if(flipper_proto_.sendPacket(msg.packet)) {
            flipper_counters_.sent++;
        }
    }
}

bool App::isReadOnlyRequest(cmd::RequestType type) const {
    switch(type) {
    case cmd::RequestType::HELP:
    case cmd::RequestType::STATUS:
    case cmd::RequestType::PING:
    case cmd::RequestType::GET_INFO:
    case cmd::RequestType::VER:
    case cmd::RequestType::BUS_SHOW:
    case cmd::RequestType::BUS_GET_CFG:
    case cmd::RequestType::TOOL_STATUS:
    case cmd::RequestType::DBC_LIST:
    case cmd::RequestType::STATS:
    case cmd::RequestType::LED_SHOW:
    case cmd::RequestType::WIFI_SHOW:
        return true;
    default:
        return false;
    }
}

bool App::extractAsciiArgs(const cmd::Request& req, char* args, size_t args_len) const {
    if(args == nullptr || args_len == 0) {
        return false;
    }

    args[0] = '\0';
    if(req.source == core::Source::CLI) {
        const size_t len = strnlen(req.args, sizeof(req.args));
        if(len >= args_len) {
            return false;
        }
        memcpy(args, req.args, len);
        args[len] = '\0';
        return true;
    }

    // Never truncate: a shortened argument string can silently change a value.
    if(req.payload_len > args_len - 1) {
        return false;
    }
    if(req.payload_len > 0) {
        memcpy(args, req.payload, req.payload_len);
    }
    args[req.payload_len] = '\0';
    return true;
}

bool App::isSharedCustomInjectControlRequest(const cmd::Request& req) const {
    if(req.type != cmd::RequestType::TOOL_CONFIG) {
        return false;
    }
    if(tools_.activeId() != core::ToolId::CUSTOM_INJECT) {
        return false;
    }

    char args[BoardConfig::ASCII_ARGS_MAX] = {0};
    if(!extractAsciiArgs(req, args, sizeof(args))) {
        return false;
    }

    char cmd_name[16] = {0};
    if(!core::parse::parseString(args, "cmd", cmd_name, sizeof(cmd_name))) {
        return false;
    }

    return strcmp(cmd_name, "inject") == 0 || strcmp(cmd_name, "cancel") == 0 ||
           strcmp(cmd_name, "list") == 0;
}

bool App::acquireOwnerForMutation(const cmd::Request& req) {
    if(req.source == core::Source::CLI) {
        if(owner_ == core::InterfaceOwner::FLIPPER) {
            if(isSharedCustomInjectControlRequest(req)) {
                return true;
            }
            sendCliLine("LOCKED: Flipper owns active session");
            return false;
        }
        if(owner_ == core::InterfaceOwner::NONE) {
            owner_ = core::InterfaceOwner::CLI;
        }
        return true;
    }

    // Flipper mutation request: preempt CLI owner.
    if(owner_ == core::InterfaceOwner::CLI) {
        if(tools_.isActive()) {
            tools_.stop(*this);
            sendCliLine("Flipper preempted active CLI tool");
        }
    }
    owner_ = core::InterfaceOwner::FLIPPER;
    last_flipper_activity_ms_ = millis();
    return true;
}

void App::releaseFlipperOnTimeout() {
    if(owner_ != core::InterfaceOwner::FLIPPER) {
        return;
    }
    if((uint32_t)(millis() - last_flipper_activity_ms_) > BoardConfig::FLIPPER_OWNER_TIMEOUT_MS) {
        owner_ = core::InterfaceOwner::NONE;
        sendCliLine("Flipper owner timeout; ownership released");
    }
}

void App::sendCliLine(const char* fmt, ...) {
    char line[BoardConfig::CLI_OUT_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pushCliLineRaw(line);
}

void App::pushCliLineRaw(const char* line) {
    if(line == nullptr) {
        return;
    }
    core::CliOut out{};
    strncpy(out.line, line, sizeof(out.line) - 1);
    out.line[sizeof(out.line) - 1] = '\0';

    bool dropped = false;
    cli_tx_.pushDropOldest(out, &dropped);
    cli_counters_.produced++;
    if(dropped) {
        cli_counters_.dropped++;
        drop_event_dirty_ = true;
        leds_.noteDrop();
    }
}

void App::pushFlipperPacket(const core::FlipperPacket& packet) {
    FlipperOut out{};
    out.packet = packet;

    bool dropped = false;
    if(packet.kind == core::FlipperPacketKind::RESP) {
        flipper_resp_tx_.pushDropOldest(out, &dropped);
    } else {
        flipper_tx_.pushDropOldest(out, &dropped);
    }
    flipper_counters_.produced++;
    if(dropped) {
        flipper_counters_.dropped++;
        drop_event_dirty_ = true;
        leds_.noteDrop();
    }
}

void App::sendFlipperResponse(
    uint8_t cmd_id,
    uint8_t seq,
    core::StatusCode status,
    const uint8_t* payload,
    uint16_t payload_len) {
    core::FlipperPacket pkt{};
    pkt.ver = kProtocolVersion;
    pkt.kind = core::FlipperPacketKind::RESP;
    pkt.id = cmd_id;
    pkt.seq = seq;

    if(payload_len > (BoardConfig::MAX_FLIPPER_PAYLOAD - 1)) {
        payload_len = BoardConfig::MAX_FLIPPER_PAYLOAD - 1;
    }

    pkt.len = (uint16_t)(payload_len + 1);
    pkt.payload[0] = (uint8_t)status;
    if(payload_len > 0 && payload != nullptr) {
        memcpy(&pkt.payload[1], payload, payload_len);
    }

    pushFlipperPacket(pkt);
}

void App::pushFlipperEvent(uint8_t event_id, const uint8_t* payload, uint16_t payload_len) {
    if(payload_len > BoardConfig::MAX_FLIPPER_PAYLOAD) {
        payload_len = BoardConfig::MAX_FLIPPER_PAYLOAD;
    }

    core::FlipperPacket pkt{};
    pkt.ver = kProtocolVersion;
    pkt.kind = core::FlipperPacketKind::EVENT;
    pkt.id = event_id;
    pkt.seq = 0;
    pkt.len = payload_len;
    if(payload_len > 0 && payload != nullptr) {
        memcpy(pkt.payload, payload, payload_len);
    }
    pushFlipperPacket(pkt);
}

void App::handleRequest(const cmd::Request& req) {
    if(req.source == core::Source::FLIPPER) {
        last_flipper_activity_ms_ = millis();
    }

    if(req.type == cmd::RequestType::INVALID) {
        if(req.source == core::Source::CLI) {
            sendCliLine("ERR: unknown command");
        } else {
            sendFlipperResponse(req.flipper_cmd_id, req.flipper_seq, core::StatusCode::BAD_CMD);
        }
        return;
    }

    if(!isReadOnlyRequest(req.type) && !acquireOwnerForMutation(req)) {
        if(req.source == core::Source::FLIPPER) {
            sendFlipperResponse(req.flipper_cmd_id, req.flipper_seq, core::StatusCode::LOCKED);
        }
        return;
    }

    core::StatusCode status = core::StatusCode::BAD_CMD;
    switch(req.type) {
    case cmd::RequestType::HELP:
        status = cmdHelp(req);
        break;
    case cmd::RequestType::STATUS:
        status = cmdStatus(req);
        break;
    case cmd::RequestType::PING:
        status = cmdPing(req);
        break;
    case cmd::RequestType::GET_INFO:
        status = cmdGetInfo(req);
        break;
    case cmd::RequestType::VER:
        status = cmdVer(req);
        break;
    case cmd::RequestType::BUS_SET:
        status = cmdBusSet(req);
        break;
    case cmd::RequestType::BUS_SHOW:
        status = cmdBusShow(req);
        break;
    case cmd::RequestType::BUS_GET_CFG:
        status = cmdBusGetCfg(req);
        break;
    case cmd::RequestType::BUS_SET_FILTER:
        status = cmdBusSetFilter(req);
        break;
    case cmd::RequestType::BUS_CLEAR_FILTER:
        status = cmdBusClearFilter(req);
        break;
    case cmd::RequestType::SEND_FRAME:
        status = cmdSendFrame(req);
        break;
    case cmd::RequestType::TOOL_START:
        status = cmdToolStart(req);
        break;
    case cmd::RequestType::TOOL_STOP:
        status = cmdToolStop(req);
        break;
    case cmd::RequestType::TOOL_STATUS:
        status = cmdToolStatus(req);
        break;
    case cmd::RequestType::TOOL_CONFIG:
        status = cmdToolConfig(req);
        break;
    case cmd::RequestType::DBC_CLEAR:
        status = cmdDbcClear(req);
        break;
    case cmd::RequestType::DBC_ADD:
        status = cmdDbcAdd(req);
        break;
    case cmd::RequestType::DBC_REMOVE:
        status = cmdDbcRemove(req);
        break;
    case cmd::RequestType::DBC_LIST:
        status = cmdDbcList(req);
        break;
    case cmd::RequestType::STATS:
        status = cmdStats(req);
        break;
    case cmd::RequestType::LED_SHOW:
        status = cmdLedShow(req);
        break;
    case cmd::RequestType::LED_SET:
        status = cmdLedSet(req);
        break;
    case cmd::RequestType::WIFI_SHOW:
        status = cmdWifiShow(req);
        break;
    case cmd::RequestType::WIFI_CONFIG:
        status = cmdWifiConfig(req);
        break;
    default:
        status = core::StatusCode::BAD_CMD;
        break;
    }

    if(req.source == core::Source::FLIPPER) {
        sendFlipperResponse(req.flipper_cmd_id, req.flipper_seq, status);
    } else if(status != core::StatusCode::OK) {
        sendCliLine("ERR: %s", statusName(status));
    }
}

core::StatusCode App::cmdHelp(const cmd::Request& req) {
    (void)req;
    sendCliLine("help");
    sendCliLine("status");
    sendCliLine("ver");
    sendCliLine("bus set bus=<can0|can1> bitrate=<int> mode=<normal|listen>");
    sendCliLine("bus show");
    sendCliLine(
        "bus filter set bus=<can0|can1> mask=<hex> filter=<hex> ext_match=<0|1> ext=<0|1>");
    sendCliLine("bus filter clear bus=<can0|can1>");
    sendCliLine("send bus=<can0|can1> id=<hex> ext=<0|1> rtr=<0|1> dlc=<0-8> data=<hexbytes>");
    sendCliLine(
        "tool start <read_all|filtered|write|speed|valtrack|unique_ids|bittrack|reverse|obd_pid|dbc_decode|custom_inject|replay> ...");
    sendCliLine("tool config <args>");
    sendCliLine(
        "inject slot=<1-5> [count=<int>] [interval_ms=<int>] | inject list | inject cancel");
    sendCliLine("wifi show | wifi start | wifi stop");
    sendCliLine("wifi set ssid=<text> pass=<text> [autostart=<0|1>] [ap=<0|1>] (primary)");
    sendCliLine("wifi config ... (alias of 'wifi set')");
    sendCliLine("tool stop | tool status");
    sendCliLine("dbc add ... | dbc remove sid=<int> | dbc clear | dbc list");
    sendCliLine("led show | led set brightness=<1-10>");
    sendCliLine("stats");
    return core::StatusCode::OK;
}

core::StatusCode App::cmdStatus(const cmd::Request& req) {
    (void)req;
    const char* owner = "none";
    if(owner_ == core::InterfaceOwner::CLI) {
        owner = "cli";
    } else if(owner_ == core::InterfaceOwner::FLIPPER) {
        owner = "flipper";
    }

    sendCliLine(
        "owner=%s active_tool=%s dbc_signals=%u", owner, tools_.activeName(), dbc_store_.count());
    sendCliLine(
        "can1 backend=%s state=%s",
        can_.can1BackendName(),
        can_.isBusAvailable(core::BusId::CAN1) ? "ready" : "disabled");
#if defined(ARDUINO_ARCH_ESP32)
    if(BoardConfig::WIFI_AP_ENABLED) {
        sendCliLine(
            "wifi ap=%s autostart=%u ssid=%s clients=%u cli_port=%u",
            wifi_ap_ready_ ? "up" : "down",
            wifi_autostart_ ? 1 : 0,
            wifi_ssid_,
            (unsigned)WiFi.softAPgetStationNum(),
            (unsigned)BoardConfig::WIFI_CLI_PORT);
    }
#endif

    uint8_t payload[4] = {
        (uint8_t)owner_,
        (uint8_t)tools_.activeId(),
        (uint8_t)(dbc_store_.count() & 0xFF),
        (uint8_t)((dbc_store_.count() >> 8) & 0xFF),
    };
    pushFlipperEvent((uint8_t)core::FlipperEventId::STATUS_EVT, payload, sizeof(payload));
    event_counters_.status_evt++;
    return core::StatusCode::OK;
}

core::StatusCode App::cmdPing(const cmd::Request& req) {
    (void)req;
    return core::StatusCode::OK;
}

// STATUS_EVT version payload: [major, minor, len, version string].
void App::pushVersionStatusEvent() {
    static constexpr size_t kVersionLen = sizeof(BOARDCFG_FIRMWARE_VERSION_STRING) - 1;
    static_assert(kVersionLen <= 16, "firmware version string too long for STATUS_EVT");
    uint8_t payload[3 + kVersionLen] = {0};
    payload[0] = BoardConfig::FIRMWARE_VERSION_MAJOR;
    payload[1] = BoardConfig::FIRMWARE_VERSION_MINOR;
    payload[2] = (uint8_t)kVersionLen;
    memcpy(&payload[3], BoardConfig::FIRMWARE_VERSION_STRING, kVersionLen);
    pushFlipperEvent(
        (uint8_t)core::FlipperEventId::STATUS_EVT, payload, (uint16_t)sizeof(payload));
    event_counters_.status_evt++;
}

core::StatusCode App::cmdGetInfo(const cmd::Request& req) {
#if defined(CONFIG_IDF_TARGET)
    const char* target = CONFIG_IDF_TARGET;
#else
    const char* target = "unknown";
#endif
    sendCliLine(
        "CAN Commander v%s | %s | CAN0 TWAI + CAN1 %s | Flipper UART + USB/WiFi CLI",
        BoardConfig::FIRMWARE_VERSION_STRING,
        target,
        can_.can1BackendName());

    if(req.source == core::Source::FLIPPER) {
        pushVersionStatusEvent();
    }
    return core::StatusCode::OK;
}

core::StatusCode App::cmdVer(const cmd::Request& req) {
    sendCliLine("CAN Commander v%s", BoardConfig::FIRMWARE_VERSION_STRING);

    if(req.source == core::Source::FLIPPER) {
        pushVersionStatusEvent();
    }
    return core::StatusCode::OK;
}

bool App::decodeBusByte(uint8_t raw, core::BusId& bus) const {
    if(raw == 0) {
        bus = core::BusId::CAN0;
        return true;
    }
    if(raw == 1) {
        bus = core::BusId::CAN1;
        return true;
    }
    if(raw == 2) {
        bus = core::BusId::BOTH;
        return true;
    }
    return false;
}

bool App::decodeFlipperBusSet(const cmd::Request& req, core::BusId& bus, core::BusConfig& cfg)
    const {
    if(req.payload_len < 6) {
        return false;
    }
    if(!decodeBusByte(req.payload[0], bus)) {
        return false;
    }
    cfg.bitrate = readU32LE(&req.payload[1]);
    cfg.listen_only = req.payload[5] != 0;
    return true;
}

core::StatusCode App::cmdBusSet(const cmd::Request& req) {
    core::BusId bus = core::BusId::CAN0;
    core::BusConfig cfg{500000, false};

    if(req.source == core::Source::CLI) {
        if(!core::parse::parseBus(req.args, "bus", bus)) {
            return core::StatusCode::BAD_ARG;
        }
        if(!core::parse::parseU32(req.args, "bitrate", cfg.bitrate, 10)) {
            return core::StatusCode::BAD_ARG;
        }

        char mode[12] = {0};
        if(!core::parse::parseOptString(req.args, "mode", mode, sizeof(mode))) {
            return core::StatusCode::BAD_ARG;
        }
        if(strcmp(mode, "listen") == 0) {
            cfg.listen_only = true;
        } else if(mode[0] == '\0' || strcmp(mode, "normal") == 0) {
            cfg.listen_only = false;
        } else {
            return core::StatusCode::BAD_ARG;
        }
    } else {
        if(!decodeFlipperBusSet(req, bus, cfg)) {
            return core::StatusCode::BAD_ARG;
        }
    }

    if(!can_.setBusConfig(bus, cfg)) {
        sendCliLine("bus set failed: %s", can_.lastError());
        return core::StatusCode::BUS_ERR;
    }

    sendCliLine(
        "bus set %s bitrate=%lu mode=%s",
        core::busName(bus),
        (unsigned long)cfg.bitrate,
        cfg.listen_only ? "listen" : "normal");
    return core::StatusCode::OK;
}

core::StatusCode App::cmdBusShow(const cmd::Request& req) {
    (void)req;
    const core::BusConfig c0 = can_.getBusConfig(core::BusId::CAN0);
    const core::BusConfig c1 = can_.getBusConfig(core::BusId::CAN1);
    const core::BusFilter f0 = can_.getFilter(core::BusId::CAN0);
    const core::BusFilter f1 = can_.getFilter(core::BusId::CAN1);

    sendCliLine(
        "can0 bitrate=%lu mode=%s filter=%u mask=0x%lX value=0x%lX",
        (unsigned long)c0.bitrate,
        c0.listen_only ? "listen" : "normal",
        f0.enabled ? 1 : 0,
        (unsigned long)f0.mask,
        (unsigned long)f0.filter);
    sendCliLine(
        "can1 bitrate=%lu mode=%s filter=%u mask=0x%lX value=0x%lX",
        (unsigned long)c1.bitrate,
        c1.listen_only ? "listen" : "normal",
        f1.enabled ? 1 : 0,
        (unsigned long)f1.mask,
        (unsigned long)f1.filter);
    sendCliLine(
        "can1 backend=%s state=%s",
        can_.can1BackendName(),
        can_.isBusAvailable(core::BusId::CAN1) ? "ready" : "disabled");
    return core::StatusCode::OK;
}

core::StatusCode App::cmdBusGetCfg(const cmd::Request& req) {
    core::BusId bus = core::BusId::CAN0;
    if(req.source == core::Source::FLIPPER) {
        if(req.payload_len < 1 || !decodeBusByte(req.payload[0], bus)) {
            return core::StatusCode::BAD_ARG;
        }
    } else {
        if(!core::parse::parseOptBus(req.args, "bus", bus)) {
            return core::StatusCode::BAD_ARG;
        }
    }

    if(bus == core::BusId::CAN1 && !can_.isBusAvailable(core::BusId::CAN1)) {
        sendCliLine("bus unavailable: can1");
        return core::StatusCode::BUS_ERR;
    }

    const core::BusConfig cfg = can_.getBusConfig(bus);
    const core::BusFilter filter = can_.getFilter(bus);

    sendCliLine(
        "%s bitrate=%lu mode=%s filter=%u mask=0x%lX value=0x%lX",
        core::busName(bus),
        (unsigned long)cfg.bitrate,
        cfg.listen_only ? "listen" : "normal",
        filter.enabled ? 1 : 0,
        (unsigned long)filter.mask,
        (unsigned long)filter.filter);

    uint8_t payload[16] = {0};
    payload[0] = (uint8_t)bus;
    writeU32LE(&payload[1], cfg.bitrate);
    payload[5] = cfg.listen_only ? 1 : 0;
    payload[6] = filter.enabled ? 1 : 0;
    writeU32LE(&payload[7], filter.mask);
    writeU32LE(&payload[11], filter.filter);
    payload[15] = filter.ext ? 1 : 0;
    pushFlipperEvent((uint8_t)core::FlipperEventId::STATUS_EVT, payload, sizeof(payload));
    event_counters_.status_evt++;

    return core::StatusCode::OK;
}

bool App::decodeFlipperBusFilter(
    const cmd::Request& req,
    core::BusId& bus,
    core::BusFilter& filter) const {
    if(req.payload_len < 11) {
        return false;
    }
    if(!decodeBusByte(req.payload[0], bus)) {
        return false;
    }
    filter.enabled = true;
    filter.mask = readU32LE(&req.payload[1]);
    filter.filter = readU32LE(&req.payload[5]);
    filter.require_ext_match = req.payload[9] != 0;
    filter.ext = req.payload[10] != 0;
    return true;
}

core::StatusCode App::cmdBusSetFilter(const cmd::Request& req) {
    core::BusId bus = core::BusId::CAN0;
    core::BusFilter filter{true, 0x7FF, 0, false, false};

    if(req.source == core::Source::CLI) {
        if(!core::parse::parseBus(req.args, "bus", bus)) {
            return core::StatusCode::BAD_ARG;
        }
        if(!core::parse::parseOptU32(req.args, "mask", filter.mask, 16) ||
           !core::parse::parseOptU32(req.args, "filter", filter.filter, 16) ||
           !core::parse::parseOptBool01(req.args, "ext_match", filter.require_ext_match) ||
           !core::parse::parseOptBool01(req.args, "ext", filter.ext)) {
            return core::StatusCode::BAD_ARG;
        }
    } else {
        if(!decodeFlipperBusFilter(req, bus, filter)) {
            return core::StatusCode::BAD_ARG;
        }
    }

    if(bus == core::BusId::CAN1 && !can_.isBusAvailable(core::BusId::CAN1)) {
        sendCliLine("bus unavailable: can1");
        return core::StatusCode::BUS_ERR;
    }

    if(!can_.setFilter(bus, filter)) {
        sendCliLine("filter set failed: %s", can_.lastError());
        return core::StatusCode::BUS_ERR;
    }

    sendCliLine(
        "filter set %s mask=0x%lX filter=0x%lX",
        core::busName(bus),
        (unsigned long)filter.mask,
        (unsigned long)filter.filter);
    return core::StatusCode::OK;
}

core::StatusCode App::cmdBusClearFilter(const cmd::Request& req) {
    core::BusId bus = core::BusId::CAN0;

    if(req.source == core::Source::CLI) {
        if(!core::parse::parseBus(req.args, "bus", bus)) {
            return core::StatusCode::BAD_ARG;
        }
    } else {
        if(req.payload_len < 1 || !decodeBusByte(req.payload[0], bus)) {
            return core::StatusCode::BAD_ARG;
        }
    }

    if(bus == core::BusId::CAN1 && !can_.isBusAvailable(core::BusId::CAN1)) {
        sendCliLine("bus unavailable: can1");
        return core::StatusCode::BUS_ERR;
    }

    can_.clearFilter(bus);
    sendCliLine("filter cleared %s", core::busName(bus));
    return core::StatusCode::OK;
}

bool App::decodeFlipperSendFrame(const cmd::Request& req, core::CanFrame& frame) const {
    if(req.payload_len < 15) {
        return false;
    }
    if(!decodeBusByte(req.payload[0], frame.bus)) {
        return false;
    }
    frame.ts_ms = millis();
    frame.id = readU32LE(&req.payload[1]);
    const uint8_t flags = req.payload[5];
    frame.ext = (flags & 0x01) != 0;
    frame.rtr = (flags & 0x02) != 0;
    frame.dlc = req.payload[6];
    if(frame.dlc > 8 || !core::isValidCanId(frame.id, frame.ext)) {
        return false;
    }
    memset(frame.data, 0, sizeof(frame.data));
    memcpy(frame.data, &req.payload[7], 8);
    return true;
}

bool App::parseCliSendFrame(const char* args, core::CanFrame& frame) const {
    memset(&frame, 0, sizeof(frame));
    frame.ts_ms = millis();

    if(!core::parse::parseBus(args, "bus", frame.bus)) {
        return false;
    }
    if(!core::parse::parseU32(args, "id", frame.id, 16) ||
       !core::parse::parseOptBool01(args, "ext", frame.ext) ||
       !core::parse::parseOptBool01(args, "rtr", frame.rtr) ||
       !core::isValidCanId(frame.id, frame.ext)) {
        return false;
    }

    uint32_t dlc = 0;
    if(!core::parse::parseU32(args, "dlc", dlc, 10) || dlc > 8) {
        return false;
    }
    frame.dlc = (uint8_t)dlc;

    char data_hex[17] = {0};
    if(!core::parse::parseOptString(args, "data", data_hex, sizeof(data_hex))) {
        return false;
    }
    if(data_hex[0] != '\0') {
        uint8_t parsed_len = 0;
        if(!core::parse::parseHexBytes(data_hex, frame.data, 8, parsed_len) ||
           parsed_len > frame.dlc) {
            return false;
        }
    }

    return true;
}

core::StatusCode App::cmdSendFrame(const cmd::Request& req) {
    core::CanFrame frame{};
    const bool ok = (req.source == core::Source::CLI) ? parseCliSendFrame(req.args, frame) :
                                                        decodeFlipperSendFrame(req, frame);
    if(!ok) {
        return core::StatusCode::BAD_ARG;
    }

    if(!sendCanFrame(frame)) {
        return core::StatusCode::BUS_ERR;
    }

    sendCliLine("tx ok bus=%s id=0x%lX", core::busName(frame.bus), (unsigned long)frame.id);
    return core::StatusCode::OK;
}

bool App::decodeFlipperToolStart(
    const cmd::Request& req,
    core::ToolId& tool_id,
    char* args,
    size_t args_len) const {
    if(req.payload_len < 1) {
        return false;
    }
    tool_id = (core::ToolId)req.payload[0];
    if(tool_id == core::ToolId::NONE) {
        return false;
    }

    if(args != nullptr && args_len > 0) {
        args[0] = '\0';
        const uint16_t arg_len = req.payload_len - 1;
        if(arg_len > args_len - 1) {
            return false;
        }
        if(arg_len > 0) {
            memcpy(args, &req.payload[1], arg_len);
        }
        args[arg_len] = '\0';
    }
    return true;
}

core::StatusCode App::cmdToolStart(const cmd::Request& req) {
    core::ToolId started = core::ToolId::NONE;

    if(req.source == core::Source::CLI) {
        char tool_name[24] = {0};
        const char* p = req.args;
        while(*p == ' ') {
            ++p;
        }
        size_t n = 0;
        while(*p != '\0' && *p != ' ' && n < (sizeof(tool_name) - 1)) {
            tool_name[n++] = *p++;
        }
        tool_name[n] = '\0';
        while(*p == ' ') {
            ++p;
        }

        if(tool_name[0] == '\0') {
            return core::StatusCode::BAD_ARG;
        }

        if(!tools_.startByName(tool_name, p, *this, started)) {
            return core::StatusCode::BAD_ARG;
        }
    } else {
        core::ToolId id = core::ToolId::NONE;
        char args[BoardConfig::ASCII_ARGS_MAX] = {0};
        if(!decodeFlipperToolStart(req, id, args, sizeof(args))) {
            return core::StatusCode::BAD_ARG;
        }
        if(!tools_.startById(id, args, *this)) {
            return core::StatusCode::BAD_ARG;
        }
        started = id;
    }

    leds_.setToolActive(true);
    sendCliLine("tool started: %s", tools::ToolManager::toolName(started));
    return core::StatusCode::OK;
}

core::StatusCode App::cmdToolStop(const cmd::Request& req) {
    (void)req;
    if(!tools_.stop(*this)) {
        return core::StatusCode::NOT_ACTIVE;
    }
    leds_.setToolActive(false);
    sendCliLine("tool stopped");
    return core::StatusCode::OK;
}

core::StatusCode App::cmdToolStatus(const cmd::Request& req) {
    (void)req;
    char desc[160];
    tools_.describeActive(desc, sizeof(desc));
    sendCliLine("%s", desc);
    return core::StatusCode::OK;
}

core::StatusCode App::cmdToolConfig(const cmd::Request& req) {
    char args[BoardConfig::ASCII_ARGS_MAX] = {0};
    if(!extractAsciiArgs(req, args, sizeof(args))) {
        return core::StatusCode::BAD_ARG;
    }

    char cmd_name[16] = {0};
    const bool has_cmd = core::parse::parseString(args, "cmd", cmd_name, sizeof(cmd_name));
    const bool is_inject_control = has_cmd && (strcmp(cmd_name, "inject") == 0 ||
                                               strcmp(cmd_name, "cancel") == 0 ||
                                               strcmp(cmd_name, "list") == 0);
    if(is_inject_control && tools_.activeId() != core::ToolId::CUSTOM_INJECT) {
        sendCliLine("ERR: inject requires active custom_inject tool");
        return core::StatusCode::NOT_ACTIVE;
    }

    if(!tools_.configureActive(args, *this)) {
        if(is_inject_control) {
            sendCliLine("ERR: inject command failed (check slot/latest frame/args)");
        }
        return core::StatusCode::BAD_ARG;
    }
    return core::StatusCode::OK;
}

core::StatusCode App::cmdDbcClear(const cmd::Request& req) {
    (void)req;
    dbc_store_.clear();
    sendCliLine("dbc cleared");
    return core::StatusCode::OK;
}

bool App::decodeFlipperDbcAdd(const cmd::Request& req, core::DbcSignalDef& def) const {
    if(req.payload_len < 40) {
        return false;
    }

    core::BusId bus;
    if(!decodeBusByte(req.payload[0], bus)) {
        return false;
    }

    memset(&def, 0, sizeof(def));
    def.used = true;
    def.bus = bus;
    def.id = readU32LE(&req.payload[1]);
    def.ext = req.payload[5] != 0;
    def.start_bit = req.payload[6];
    def.bit_len = req.payload[7];
    if(req.payload[8] > 1 || req.payload[9] > 1) {
        return false;
    }
    def.order = req.payload[8] == 0 ? core::ByteOrder::INTEL : core::ByteOrder::MOTOROLA;
    def.sign = req.payload[9] == 0 ? core::SignMode::UNSIGNED : core::SignMode::SIGNED;
    def.factor = readFloatLE(&req.payload[10]);
    def.offset = readFloatLE(&req.payload[14]);
    def.min = readFloatLE(&req.payload[18]);
    def.max = readFloatLE(&req.payload[22]);
    memcpy(def.unit, &req.payload[26], 12);
    def.unit[11] = '\0';
    def.sid = readU16LE(&req.payload[38]);
    def.mux_enabled = false;
    def.mux_start_bit = 0;
    def.mux_bit_len = 0;
    def.mux_value = 0;

    // Optional mux extension payload:
    // [40] mux_enabled:u8
    // [41] mux_start_bit:u8
    // [42] mux_bit_len:u8
    // [43..50] mux_value:u64
    if(req.payload_len >= 51) {
        def.mux_enabled = req.payload[40] != 0;
        def.mux_start_bit = req.payload[41];
        def.mux_bit_len = req.payload[42];
        def.mux_value = readU64LE(&req.payload[43]);
    }

    return true;
}

bool App::parseCliDbcAdd(const char* args, core::DbcSignalDef& def) const {
    memset(&def, 0, sizeof(def));
    def.used = true;

    if(!core::parse::parseBus(args, "bus", def.bus)) {
        return false;
    }
    if(!core::parse::parseU32(args, "id", def.id, 16) ||
       !core::parse::parseOptBool01(args, "ext", def.ext)) {
        return false;
    }

    uint32_t tmp = 0;
    if(!core::parse::parseU32(args, "start", tmp, 10) || tmp > 63) {
        return false;
    }
    def.start_bit = (uint8_t)tmp;

    if(!core::parse::parseU32(args, "len", tmp, 10) || tmp == 0 || tmp > 64) {
        return false;
    }
    def.bit_len = (uint8_t)tmp;

    char order[16] = {0};
    if(!core::parse::parseString(args, "order", order, sizeof(order))) {
        return false;
    }
    if(strcmp(order, "intel") == 0) {
        def.order = core::ByteOrder::INTEL;
    } else if(strcmp(order, "motorola") == 0) {
        def.order = core::ByteOrder::MOTOROLA;
    } else {
        return false;
    }

    char sign[8] = {0};
    if(!core::parse::parseString(args, "sign", sign, sizeof(sign))) {
        return false;
    }
    if(strcmp(sign, "u") == 0) {
        def.sign = core::SignMode::UNSIGNED;
    } else if(strcmp(sign, "s") == 0) {
        def.sign = core::SignMode::SIGNED;
    } else {
        return false;
    }

    if(!core::parse::parseFloat(args, "factor", def.factor)) {
        return false;
    }
    if(!core::parse::parseFloat(args, "offset", def.offset)) {
        return false;
    }
    if(!core::parse::parseFloat(args, "min", def.min)) {
        return false;
    }
    if(!core::parse::parseFloat(args, "max", def.max)) {
        return false;
    }

    // The unit is display text; shorten it rather than reject the signal.
    def.unit[0] = '\0';
    core::parse::parseStringTruncate(args, "unit", def.unit, sizeof(def.unit));

    if(!core::parse::parseU32(args, "sid", tmp, 10) || tmp > 0xFFFF) {
        return false;
    }
    def.sid = (uint16_t)tmp;

    def.mux_enabled = false;
    def.mux_start_bit = 0;
    def.mux_bit_len = 0;
    def.mux_value = 0;

    const bool mux_start_present = core::parse::hasKey(args, "mux_start");
    const bool mux_len_present = core::parse::hasKey(args, "mux_len");
    const bool mux_value_present = core::parse::hasKey(args, "mux_value");

    if(mux_start_present || mux_len_present || mux_value_present) {
        uint32_t mux_start = 0;
        uint32_t mux_len = 0;
        uint64_t mux_value = 0;
        if(!core::parse::parseU32(args, "mux_start", mux_start, 10) ||
           !core::parse::parseU32(args, "mux_len", mux_len, 10) ||
           !core::parse::parseU64(args, "mux_value", mux_value, 0) || mux_start > 63 ||
           mux_len == 0 || mux_len > 64) {
            return false;
        }

        // Geometry and value range are checked by DbcSignalStore::isValid using the
        // signal's byte order.
        def.mux_enabled = true;
        def.mux_start_bit = (uint8_t)mux_start;
        def.mux_bit_len = (uint8_t)mux_len;
        def.mux_value = mux_value;
    }

    return true;
}

core::StatusCode App::cmdDbcAdd(const cmd::Request& req) {
    core::DbcSignalDef def{};
    const bool ok = (req.source == core::Source::CLI) ? parseCliDbcAdd(req.args, def) :
                                                        decodeFlipperDbcAdd(req, def);
    if(!ok || !decode::DbcSignalStore::isValid(def)) {
        return core::StatusCode::BAD_ARG;
    }

    if(!dbc_store_.add(def)) {
        return core::StatusCode::NO_MEM;
    }

    if(def.mux_enabled) {
        sendCliLine(
            "dbc add sid=%u bus=%s id=0x%lX mux=[start=%u len=%u val=0x%llX]",
            def.sid,
            core::busName(def.bus),
            (unsigned long)def.id,
            (unsigned)def.mux_start_bit,
            (unsigned)def.mux_bit_len,
            (unsigned long long)def.mux_value);
    } else {
        sendCliLine(
            "dbc add sid=%u bus=%s id=0x%lX",
            def.sid,
            core::busName(def.bus),
            (unsigned long)def.id);
    }
    return core::StatusCode::OK;
}

core::StatusCode App::cmdDbcRemove(const cmd::Request& req) {
    uint16_t sid = 0;
    if(req.source == core::Source::CLI) {
        uint32_t tmp = 0;
        if(!core::parse::parseU32(req.args, "sid", tmp, 10) || tmp > 0xFFFF) {
            return core::StatusCode::BAD_ARG;
        }
        sid = (uint16_t)tmp;
    } else {
        if(req.payload_len < 2) {
            return core::StatusCode::BAD_ARG;
        }
        sid = readU16LE(req.payload);
    }

    if(!dbc_store_.removeBySid(sid)) {
        return core::StatusCode::NOT_ACTIVE;
    }

    sendCliLine("dbc removed sid=%u", sid);
    return core::StatusCode::OK;
}

core::StatusCode App::cmdDbcList(const cmd::Request& req) {
    const uint16_t count = dbc_store_.count();
    sendCliLine("dbc count=%u", count);

    for(uint16_t i = 0; i < count; i++) {
        core::DbcSignalDef def{};
        if(!dbc_store_.getByIndex(i, def)) {
            continue;
        }
        if(def.mux_enabled) {
            sendCliLine(
                "sid=%u bus=%s id=0x%lX %s start=%u len=%u order=%s sign=%s factor=%.4f offset=%.4f mux=[start=%u len=%u val=0x%llX]",
                def.sid,
                core::busName(def.bus),
                (unsigned long)def.id,
                def.ext ? "ext" : "std",
                def.start_bit,
                def.bit_len,
                def.order == core::ByteOrder::INTEL ? "intel" : "motorola",
                def.sign == core::SignMode::SIGNED ? "s" : "u",
                def.factor,
                def.offset,
                (unsigned)def.mux_start_bit,
                (unsigned)def.mux_bit_len,
                (unsigned long long)def.mux_value);
        } else {
            sendCliLine(
                "sid=%u bus=%s id=0x%lX %s start=%u len=%u order=%s sign=%s factor=%.4f offset=%.4f",
                def.sid,
                core::busName(def.bus),
                (unsigned long)def.id,
                def.ext ? "ext" : "std",
                def.start_bit,
                def.bit_len,
                def.order == core::ByteOrder::INTEL ? "intel" : "motorola",
                def.sign == core::SignMode::SIGNED ? "s" : "u",
                def.factor,
                def.offset);
        }
    }

    if(req.source == core::Source::FLIPPER) {
        uint8_t payload[2];
        writeU16LE(payload, count);
        pushFlipperEvent((uint8_t)core::FlipperEventId::STATUS_EVT, payload, sizeof(payload));
        event_counters_.status_evt++;
    }

    return core::StatusCode::OK;
}

core::StatusCode App::cmdStats(const cmd::Request& req) {
    (void)req;
    sendCliLine(
        "cli produced=%lu sent=%lu dropped=%lu",
        (unsigned long)cli_counters_.produced,
        (unsigned long)cli_counters_.sent,
        (unsigned long)cli_counters_.dropped);
    sendCliLine(
        "flipper produced=%lu sent=%lu dropped=%lu",
        (unsigned long)flipper_counters_.produced,
        (unsigned long)flipper_counters_.sent,
        (unsigned long)flipper_counters_.dropped);
    const can::RxLossStats rx = can_.rxLossStats();
    sendCliLine(
        "rx lost: queue=%lu can0_missed=%lu can0_overrun=%lu can1_overflow=%lu",
        (unsigned long)rx.queue_dropped,
        (unsigned long)rx.can0_missed,
        (unsigned long)rx.can0_overrun,
        (unsigned long)rx.can1_overflow);
#if defined(ARDUINO_ARCH_ESP32)
    sendCliLine(
        "output skipped: usb_stalled=%lu wifi_busy=%lu flipper_rx_errors=%lu",
        (unsigned long)usb_dropped_,
        (unsigned long)wifi_dropped_,
        (unsigned long)flipper_proto_.rxErrors());
    sendCliLine(
        "heap free=%lu min_free=%lu",
        (unsigned long)ESP.getFreeHeap(),
        (unsigned long)ESP.getMinFreeHeap());
#endif
    sendCliLine(
        "events can=%lu tool=%lu dbc=%lu status=%lu drops=%lu",
        (unsigned long)event_counters_.can_frame,
        (unsigned long)event_counters_.tool_evt,
        (unsigned long)event_counters_.dbc_evt,
        (unsigned long)event_counters_.status_evt,
        (unsigned long)event_counters_.drops_evt);

    uint8_t payload[44] = {0};
    writeU32LE(&payload[0], cli_counters_.produced);
    writeU32LE(&payload[4], cli_counters_.sent);
    writeU32LE(&payload[8], cli_counters_.dropped);
    writeU32LE(&payload[12], flipper_counters_.produced);
    writeU32LE(&payload[16], flipper_counters_.sent);
    writeU32LE(&payload[20], flipper_counters_.dropped);
    writeU32LE(&payload[24], event_counters_.can_frame);
    writeU32LE(&payload[28], event_counters_.tool_evt);
    writeU32LE(&payload[32], event_counters_.dbc_evt);
    writeU32LE(&payload[36], event_counters_.status_evt);
    writeU32LE(&payload[40], event_counters_.drops_evt);
    pushFlipperEvent((uint8_t)core::FlipperEventId::STATUS_EVT, payload, sizeof(payload));
    event_counters_.status_evt++;

    return core::StatusCode::OK;
}

core::StatusCode App::cmdLedShow(const cmd::Request& req) {
    (void)req;
    sendCliLine(
        "led enabled=%u tool_active=%u error=%u brightness=%u",
        BoardConfig::NEOPIXEL_ENABLED ? 1 : 0,
        leds_.isActive() ? 1 : 0,
        leds_.isError() ? 1 : 0,
        (unsigned)leds_.brightness());
    return core::StatusCode::OK;
}

core::StatusCode App::cmdLedSet(const cmd::Request& req) {
    uint32_t level = 0;

    if(req.source == core::Source::CLI) {
        if(!core::parse::parseU32(req.args, "brightness", level, 10)) {
            sendCliLine("usage: led set brightness=<1-10>");
            return core::StatusCode::BAD_ARG;
        }
    } else {
        if(req.payload_len < 1) {
            return core::StatusCode::BAD_ARG;
        }
        level = req.payload[0];
    }

    if(level < 1 || level > 10) {
        sendCliLine("ERR: brightness must be 1-10");
        return core::StatusCode::BAD_ARG;
    }

    leds_.setBrightness((uint8_t)level);
    sendCliLine("led brightness=%u", (unsigned)level);
    return core::StatusCode::OK;
}

core::StatusCode App::cmdWifiShow(const cmd::Request& req) {
    (void)req;
#if defined(ARDUINO_ARCH_ESP32)
    if(!BoardConfig::WIFI_AP_ENABLED) {
        sendCliLine("wifi support disabled at compile time");
        return core::StatusCode::BAD_CMD;
    }

    sendCliLine(
        "wifi ap=%s autostart=%u ssid=%s pass_len=%u cli_port=%u clients=%u",
        wifi_ap_ready_ ? "up" : "down",
        wifi_autostart_ ? 1 : 0,
        wifi_ssid_,
        (unsigned)strnlen(wifi_password_, sizeof(wifi_password_)),
        (unsigned)BoardConfig::WIFI_CLI_PORT,
        (unsigned)WiFi.softAPgetStationNum());
    return core::StatusCode::OK;
#else
    sendCliLine("wifi unsupported on this target");
    return core::StatusCode::BAD_CMD;
#endif
}

core::StatusCode App::cmdWifiConfig(const cmd::Request& req) {
#if defined(ARDUINO_ARCH_ESP32)
    if(!BoardConfig::WIFI_AP_ENABLED) {
        sendCliLine("wifi support disabled at compile time");
        return core::StatusCode::BAD_CMD;
    }

    char args[BoardConfig::ASCII_ARGS_MAX] = {0};
    if(!extractAsciiArgs(req, args, sizeof(args))) {
        return core::StatusCode::BAD_ARG;
    }

    // Validate every field before changing anything, so a bad password cannot leave a
    // half-applied SSID behind. Over-long values are rejected, never truncated.
    const char* pass_key = core::parse::hasKey(args, "pass") ? "pass" : "password";
    const bool ssid_present = core::parse::hasKey(args, "ssid");
    const bool pass_present = core::parse::hasKey(args, pass_key);
    const bool autostart_present = core::parse::hasKey(args, "autostart");
    const bool ap_present = core::parse::hasKey(args, "ap");

    char ssid[33] = {0};
    char pass[64] = {0};
    bool autostart = false;
    bool ap_on = false;
    if(!core::parse::parseOptString(args, "ssid", ssid, sizeof(ssid)) ||
       !core::parse::parseOptString(args, pass_key, pass, sizeof(pass)) ||
       !core::parse::parseOptBool01(args, "autostart", autostart) ||
       !core::parse::parseOptBool01(args, "ap", ap_on)) {
        sendCliLine("wifi config rejected: ssid 1-32 chars, pass 8-63 chars, autostart/ap 0|1");
        return core::StatusCode::BAD_ARG;
    }

    if(!ssid_present && !pass_present && !autostart_present && !ap_present) {
        return core::StatusCode::BAD_ARG;
    }
    if(ssid_present && strnlen(ssid, sizeof(ssid)) == 0) {
        return core::StatusCode::BAD_ARG;
    }
    if(pass_present) {
        const size_t len = strnlen(pass, sizeof(pass));
        if(len < 8 || len > 63) {
            sendCliLine("wifi config rejected: password must be 8-63 chars");
            return core::StatusCode::BAD_ARG;
        }
    }

    if(ssid_present) {
        strncpy(wifi_ssid_, ssid, sizeof(wifi_ssid_) - 1);
        wifi_ssid_[sizeof(wifi_ssid_) - 1] = '\0';
    }
    if(pass_present) {
        strncpy(wifi_password_, pass, sizeof(wifi_password_) - 1);
        wifi_password_[sizeof(wifi_password_) - 1] = '\0';
    }
    if(autostart_present) {
        wifi_autostart_ = autostart;
    }

    const bool settings_changed = ssid_present || pass_present || autostart_present;
    const bool creds_changed = ssid_present || pass_present;
    if(settings_changed && !saveWifiSettings()) {
        sendCliLine("wifi config save failed");
        return core::StatusCode::NO_MEM;
    }

    if(ap_present) {
        if(ap_on) {
            if(wifi_ap_ready_ && creds_changed) {
                stopWifiAp();
            }
            if(!startWifiAp()) {
                return core::StatusCode::BUS_ERR;
            }
        } else {
            if(wifi_ap_ready_) {
                stopWifiAp();
                sendCliLine("WiFi AP stopped");
            }
        }
    } else if(wifi_ap_ready_ && creds_changed) {
        stopWifiAp();
        if(!startWifiAp()) {
            return core::StatusCode::BUS_ERR;
        }
    }

    if(settings_changed) {
        sendCliLine(
            "wifi config saved autostart=%u ap=%u ssid=%s",
            wifi_autostart_ ? 1 : 0,
            wifi_ap_ready_ ? 1 : 0,
            wifi_ssid_);
    } else {
        sendCliLine(
            "wifi ap updated autostart=%u ap=%u ssid=%s",
            wifi_autostart_ ? 1 : 0,
            wifi_ap_ready_ ? 1 : 0,
            wifi_ssid_);
    }
    return core::StatusCode::OK;
#else
    (void)req;
    sendCliLine("wifi unsupported on this target");
    return core::StatusCode::BAD_CMD;
#endif
}
