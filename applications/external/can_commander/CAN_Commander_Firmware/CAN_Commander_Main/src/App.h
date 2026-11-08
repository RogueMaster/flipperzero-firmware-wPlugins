#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#include <WiFi.h>
#endif

#include "BoardConfig.h"
#include "can/CanBusManager.h"
#include "cmd/CommandRouter.h"
#include "core/ParseUtils.h"
#include "core/RingQueue.h"
#include "core/Status.h"
#include "core/Types.h"
#include "decode/DbcSignalStore.h"
#include "io/CliShell.h"
#include "io/FlipperProtocol.h"
#include "tools/ToolBase.h"
#include "tools/ToolManager.h"
#include "ui/LedController.h"

class App : public tools::ToolHost {
public:
    App();

    void setup();
    void loop();

    // ToolHost implementation
    uint32_t nowMs() const override;
    bool sendCanFrame(const core::CanFrame& frame) override;
    void emitCanFrameEvent(const core::CanFrame& frame, bool ascii_mode) override;
    void emitToolEvent(core::ToolId tool, core::ToolEventCode code, const char* text) override;
    void emitDbcDecodeEvent(const core::DbcDecodedValue& value) override;
    decode::DbcSignalStore& dbcStore() override;
    bool isBusAvailable(core::BusId bus) const override;
    uint16_t outputHeadroom() const override;

private:
    struct FlipperOut {
        core::FlipperPacket packet;
    };

    void pollInputs();
    void processCan();
    void processTools();
    void flushOutputs();
    void flushCli();
    void flushFlipper();
    enum class UsbReady : uint8_t {
        WRITE,
        SKIP,
        WAIT,
        STALLED
    };
    UsbReady usbReady(size_t need, uint32_t now);
#if defined(ARDUINO_ARCH_ESP32)
    void setupWifiAp();
    bool startWifiAp();
    void stopWifiAp();
    void loadWifiSettings();
    bool saveWifiSettings() const;
    void pollWifiCli();
    void queueWifiLine(const char* line, size_t len, uint32_t now);
    void serviceWifiPending(uint32_t now);
#endif

    void handleRequest(const cmd::Request& req);

    bool isReadOnlyRequest(cmd::RequestType type) const;
    bool acquireOwnerForMutation(const cmd::Request& req);
    void releaseFlipperOnTimeout();

    void sendCliLine(const char* fmt, ...);
    void pushCliLineRaw(const char* line);

    void sendFlipperResponse(
        uint8_t cmd_id,
        uint8_t seq,
        core::StatusCode status,
        const uint8_t* payload = nullptr,
        uint16_t payload_len = 0);
    void pushFlipperPacket(const core::FlipperPacket& packet);
    void pushFlipperEvent(uint8_t event_id, const uint8_t* payload, uint16_t payload_len);
    void pushVersionStatusEvent();

    core::StatusCode cmdHelp(const cmd::Request& req);
    core::StatusCode cmdStatus(const cmd::Request& req);
    core::StatusCode cmdPing(const cmd::Request& req);
    core::StatusCode cmdGetInfo(const cmd::Request& req);
    core::StatusCode cmdVer(const cmd::Request& req);
    core::StatusCode cmdBusSet(const cmd::Request& req);
    core::StatusCode cmdBusShow(const cmd::Request& req);
    core::StatusCode cmdBusGetCfg(const cmd::Request& req);
    core::StatusCode cmdBusSetFilter(const cmd::Request& req);
    core::StatusCode cmdBusClearFilter(const cmd::Request& req);
    core::StatusCode cmdSendFrame(const cmd::Request& req);
    core::StatusCode cmdToolStart(const cmd::Request& req);
    core::StatusCode cmdToolStop(const cmd::Request& req);
    core::StatusCode cmdToolStatus(const cmd::Request& req);
    core::StatusCode cmdToolConfig(const cmd::Request& req);
    core::StatusCode cmdDbcClear(const cmd::Request& req);
    core::StatusCode cmdDbcAdd(const cmd::Request& req);
    core::StatusCode cmdDbcRemove(const cmd::Request& req);
    core::StatusCode cmdDbcList(const cmd::Request& req);
    core::StatusCode cmdStats(const cmd::Request& req);
    core::StatusCode cmdLedShow(const cmd::Request& req);
    core::StatusCode cmdLedSet(const cmd::Request& req);
    core::StatusCode cmdWifiShow(const cmd::Request& req);
    core::StatusCode cmdWifiConfig(const cmd::Request& req);

    bool decodeBusByte(uint8_t raw, core::BusId& bus) const;

    bool
        decodeFlipperBusSet(const cmd::Request& req, core::BusId& bus, core::BusConfig& cfg) const;
    bool decodeFlipperBusFilter(const cmd::Request& req, core::BusId& bus, core::BusFilter& filter)
        const;
    bool decodeFlipperSendFrame(const cmd::Request& req, core::CanFrame& frame) const;
    bool decodeFlipperToolStart(
        const cmd::Request& req,
        core::ToolId& tool_id,
        char* args,
        size_t args_len) const;
    bool decodeFlipperDbcAdd(const cmd::Request& req, core::DbcSignalDef& def) const;
    bool extractAsciiArgs(const cmd::Request& req, char* args, size_t args_len) const;
    bool isSharedCustomInjectControlRequest(const cmd::Request& req) const;

    bool parseCliSendFrame(const char* args, core::CanFrame& frame) const;
    bool parseCliDbcAdd(const char* args, core::DbcSignalDef& def) const;

    HardwareSerial flipper_uart_;
#if defined(ARDUINO_ARCH_ESP32)
    io::CliShell wifi_cli_;
    WiFiServer wifi_cli_server_;
    WiFiClient wifi_cli_client_;
    bool wifi_ap_ready_;
    bool wifi_autostart_;
    char wifi_ssid_[33];
    char wifi_password_[64];
#endif

    can::CanBusManager can_;
    io::FlipperProtocol flipper_proto_;
    io::CliShell cli_;
    cmd::CommandRouter router_;
    tools::ToolManager tools_;
    decode::DbcSignalStore dbc_store_;
    ui::LedController leds_;

    // The two output queues are ~105 KB together; heap storage keeps them out of the
    // static DRAM segment, which is too small for them on the classic ESP32.
    core::RingQueue<core::CliOut, BoardConfig::EVENT_QUEUE_DEPTH_PER_SINK, core::HeapStorage>
        cli_tx_;
    core::RingQueue<FlipperOut, BoardConfig::EVENT_QUEUE_DEPTH_PER_SINK, core::HeapStorage>
        flipper_tx_;
    // Command responses have their own queue and are sent before events, so a burst of
    // telemetry can never evict the reply the Flipper is waiting for.
    core::RingQueue<FlipperOut, 32> flipper_resp_tx_;

    uint32_t usb_dropped_;
    uint32_t usb_blocked_since_ms_;
#if defined(ARDUINO_ARCH_ESP32)
    // One CLI line in flight to the Wi-Fi client; written with non-blocking sends.
    char wifi_pending_[BoardConfig::CLI_OUT_MAX + 2];
    uint16_t wifi_pending_len_;
    uint16_t wifi_pending_off_;
    uint32_t wifi_last_progress_ms_;
    uint32_t wifi_dropped_;
#endif

    core::SinkCounters cli_counters_;
    core::SinkCounters flipper_counters_;
    core::EventCounters event_counters_;

    core::InterfaceOwner owner_;
    uint32_t last_flipper_activity_ms_;
    bool drop_event_dirty_;
    uint32_t last_drop_event_ms_;
};
