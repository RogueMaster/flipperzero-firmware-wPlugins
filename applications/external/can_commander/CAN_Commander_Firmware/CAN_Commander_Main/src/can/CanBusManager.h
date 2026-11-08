#pragma once

#include <SPI.h>

#include "../BoardConfig.h"

#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
#include <ACAN2517FD.h>
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
#include <mcp2515.h>
#endif

#if defined(ARDUINO_ARCH_ESP32)
#include <driver/twai.h>
#include <esp_idf_version.h>
#endif

#include "../core/RingQueue.h"
#include "../core/Types.h"

namespace can {

// Frames lost before reaching the tools, by cause. These are separate from output
// (CLI/Flipper) drops.
struct RxLossStats {
    uint32_t queue_dropped; // read from a controller, evicted from the 128-frame RX queue
    uint32_t can0_missed; // TWAI driver RX queue full
    uint32_t can0_overrun; // TWAI hardware FIFO overrun
    uint32_t can1_overflow; // CAN1 controller receive-buffer overflow events
};

class CanBusManager {
public:
    CanBusManager();

    // Initializes both buses. Returns true only if every configured bus came up;
    // check isBusAvailable()/busError() for each bus.
    bool begin();
    void poll();
    bool readFrame(core::CanFrame& out);

    bool sendFrame(const core::CanFrame& frame);
    bool isBusAvailable(core::BusId bus) const;
    const char* can1BackendName() const;

    // Validates the request for every affected bus before touching hardware. On a
    // failure while applying, the previous configuration is restored.
    bool setBusConfig(core::BusId bus, const core::BusConfig& cfg);
    core::BusConfig getBusConfig(core::BusId bus) const;

    bool setFilter(core::BusId bus, const core::BusFilter& filter);
    void clearFilter(core::BusId bus);
    core::BusFilter getFilter(core::BusId bus) const;

    RxLossStats rxLossStats() const;

    const char* lastError() const {
        return last_error_;
    }
    // Last initialization error for one bus ("" if it initialized).
    const char* busError(core::BusId bus) const;

private:
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    static void can1IsrThunk();
    void handleCan1Isr();
#endif

    bool initCan0();
    bool shutdownCan0();
    bool initCan1();
    void pollCan0();
    void pollCan1();
    bool can0BitrateSupported(uint32_t bitrate) const;
    bool can1BitrateSupported(uint32_t bitrate) const;
    bool framePassesFilter(const core::CanFrame& frame, const core::BusFilter& filter) const;
    void setError(const char* fmt, ...);
    void recordBusError(core::BusId bus, bool ok);

#if defined(ARDUINO_ARCH_ESP32)
    bool getCan0Timing(uint32_t bitrate, twai_timing_config_t& timing) const;
#endif

#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    ACAN2517FD can1_;
    // ACAN2517FD::begin() allocates and starts a task; it must be end()ed before the
    // next begin(), whether or not the previous begin() succeeded.
    bool can1_begun_;
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    MCP2515 can1_;
#endif
    core::BusConfig can0_cfg_;
    core::BusConfig can1_cfg_;
    core::BusFilter can0_filter_;
    core::BusFilter can1_filter_;
    bool can0_available_;
    bool can1_available_;

    core::RingQueue<core::CanFrame, 128> rx_queue_;
    void enqueueRx(const core::CanFrame& frame);
    uint32_t rx_queue_dropped_;
    uint32_t can0_missed_accum_;
    uint32_t can0_overrun_accum_;
    uint32_t can0_missed_live_;
    uint32_t can0_overrun_live_;
    uint32_t can1_overflow_;
    char last_error_[96];
    char can0_error_[96];
    char can1_error_[96];

#if defined(ARDUINO_ARCH_ESP32)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    twai_handle_t can0_handle_;
#endif
    bool can0_driver_installed_;
    uint32_t can0_last_recover_attempt_ms_;
#endif

    static CanBusManager* instance_;
};

} // namespace can
