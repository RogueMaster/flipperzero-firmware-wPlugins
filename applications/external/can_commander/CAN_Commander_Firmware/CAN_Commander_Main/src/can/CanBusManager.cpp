#include "CanBusManager.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace can {

CanBusManager* CanBusManager::instance_ = nullptr;

#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
static ACAN2517FDSettings::Oscillator can1OscillatorFromConfig() {
    switch(BoardConfig::CAN1_CRYSTAL_MHZ) {
    case 4:
        return ACAN2517FDSettings::OSC_4MHz10xPLL; // 40 MHz system clock
    case 40:
        return ACAN2517FDSettings::OSC_40MHz;
    case 20:
    default:
        return ACAN2517FDSettings::OSC_20MHz;
    }
}
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
static CAN_CLOCK can1ClockFromConfig() {
    switch(BoardConfig::CAN1_CRYSTAL_MHZ) {
    case 8:
        return MCP_8MHZ;
    case 16:
        return MCP_16MHZ;
    case 20:
    default:
        return MCP_20MHZ;
    }
}

// Bitrates autowp-mcp2515 implements for each crystal (MCP2515::setBitrate).
static bool can1BitrateToMcp2515Speed(uint32_t bitrate, CAN_SPEED& speed) {
    const uint8_t mhz = BoardConfig::CAN1_CRYSTAL_MHZ;
    switch(bitrate) {
    case 5000:
        speed = CAN_5KBPS;
        return mhz != 20;
    case 10000:
        speed = CAN_10KBPS;
        return mhz != 20;
    case 20000:
        speed = CAN_20KBPS;
        return mhz != 20;
    case 31250:
        speed = CAN_31K25BPS;
        return mhz == 8;
    case 33333:
        speed = CAN_33KBPS;
        return true;
    case 40000:
        speed = CAN_40KBPS;
        return true;
    case 50000:
        speed = CAN_50KBPS;
        return true;
    case 80000:
        speed = CAN_80KBPS;
        return true;
    case 83333:
        speed = CAN_83K3BPS;
        return mhz != 8;
    case 95000:
    case 95238:
        speed = CAN_95KBPS;
        return mhz == 16;
    case 100000:
        speed = CAN_100KBPS;
        return true;
    case 125000:
        speed = CAN_125KBPS;
        return true;
    case 200000:
        speed = CAN_200KBPS;
        return true;
    case 250000:
        speed = CAN_250KBPS;
        return true;
    case 500000:
        speed = CAN_500KBPS;
        return true;
    case 1000000:
        speed = CAN_1000KBPS;
        return true;
    default:
        return false;
    }
}
#endif

CanBusManager::CanBusManager()
// NOLINTNEXTLINE(readability-redundant-member-init)
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    : can1_(BoardConfig::SPI_CS_PIN, SPI, BoardConfig::CAN1_INT_PIN)
    , can1_begun_(false)
    ,
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    // Pass &SPI explicitly: without it the driver calls SPI.begin() with the
    // board's default pins, and the configured pins in initCan1() are ignored.
    : can1_(BoardConfig::SPI_CS_PIN, BoardConfig::CAN1_SPI_CLOCK_HZ, &SPI)
    ,
#else
    :
#endif
    can0_cfg_{BoardConfig::CAN0_BITRATE, false}
    , can1_cfg_{BoardConfig::CAN1_BITRATE, false}
    , can0_filter_{false, 0, 0, false, false}
    , can1_filter_{false, 0, 0, false, false}
    , can0_available_(false)
    , can1_available_(false)
    , rx_queue_dropped_(0)
    , can0_missed_accum_(0)
    , can0_overrun_accum_(0)
    , can0_missed_live_(0)
    , can0_overrun_live_(0)
    , can1_overflow_(0)
#if defined(ARDUINO_ARCH_ESP32) && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    , can0_handle_(nullptr)
    , can0_driver_installed_(false)
    , can0_last_recover_attempt_ms_(0)
#elif defined(ARDUINO_ARCH_ESP32)
    , can0_driver_installed_(false)
    , can0_last_recover_attempt_ms_(0)
#endif
{
    last_error_[0] = '\0';
    can0_error_[0] = '\0';
    can1_error_[0] = '\0';
}

void CanBusManager::setError(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(last_error_, sizeof(last_error_), fmt, ap);
    va_end(ap);
}

void CanBusManager::recordBusError(core::BusId bus, bool ok) {
    char* dst = (bus == core::BusId::CAN1) ? can1_error_ : can0_error_;
    if(ok) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, last_error_, sizeof(can0_error_) - 1);
    dst[sizeof(can0_error_) - 1] = '\0';
}

const char* CanBusManager::busError(core::BusId bus) const {
    return (bus == core::BusId::CAN1) ? can1_error_ : can0_error_;
}

bool CanBusManager::begin() {
    instance_ = this;
    rx_queue_.clear();
    const bool can0_ok = initCan0();
    recordBusError(core::BusId::CAN0, can0_ok);
    const bool can1_ok = initCan1();
    recordBusError(core::BusId::CAN1, can1_ok);
    return can0_ok && can1_ok;
}

bool CanBusManager::can0BitrateSupported(uint32_t bitrate) const {
#if defined(ARDUINO_ARCH_ESP32)
    twai_timing_config_t timing{};
    return getCan0Timing(bitrate, timing);
#else
    (void)bitrate;
    return false;
#endif
}

bool CanBusManager::can1BitrateSupported(uint32_t bitrate) const {
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    ACAN2517FDSettings settings(can1OscillatorFromConfig(), bitrate, DataBitRateFactor::x1);
    return settings.mArbitrationBitRateClosedToDesiredRate &&
           settings.CANBitSettingConsistency() == 0;
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    CAN_SPEED speed = CAN_500KBPS;
    return can1BitrateToMcp2515Speed(bitrate, speed);
#else
    (void)bitrate;
    return false;
#endif
}

bool CanBusManager::initCan0() {
#if !defined(ARDUINO_ARCH_ESP32)
    can0_available_ = false;
    setError("CAN0 requires ESP32 TWAI support");
    return false;
#else
    twai_timing_config_t timing{};
    if(!getCan0Timing(can0_cfg_.bitrate, timing)) {
        setError("CAN0 unsupported bitrate %lu", (unsigned long)can0_cfg_.bitrate);
        return false;
    }

    if(!shutdownCan0()) {
        can0_available_ = false;
        return false;
    }

    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        BoardConfig::CAN0_TX_PIN,
        BoardConfig::CAN0_RX_PIN,
        can0_cfg_.listen_only ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
    general.tx_queue_len = 32;
    // Large enough to ride out a few milliseconds of main-loop work at full bus load.
    general.rx_queue_len = 128;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    general.controller_id = 0;
#endif

    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    esp_err_t err = twai_driver_install_v2(&general, &timing, &filter, &can0_handle_);
    if(err != ESP_OK) {
        can0_available_ = false;
        setError("CAN0 driver install failed (%d)", (int)err);
        can0_handle_ = nullptr;
        return false;
    }
    err = twai_start_v2(can0_handle_);
    if(err != ESP_OK) {
        can0_available_ = false;
        setError("CAN0 start failed (%d)", (int)err);
        (void)twai_driver_uninstall_v2(can0_handle_);
        can0_handle_ = nullptr;
        return false;
    }
#else
    esp_err_t err = twai_driver_install(&general, &timing, &filter);
    if(err != ESP_OK) {
        can0_available_ = false;
        setError("CAN0 driver install failed (%d)", (int)err);
        return false;
    }
    err = twai_start();
    if(err != ESP_OK) {
        can0_available_ = false;
        setError("CAN0 start failed (%d)", (int)err);
        (void)twai_driver_uninstall();
        return false;
    }
#endif

    can0_driver_installed_ = true;
    can0_available_ = true;
    return true;
#endif
}

bool CanBusManager::shutdownCan0() {
#if !defined(ARDUINO_ARCH_ESP32)
    can0_available_ = false;
    return true;
#else
    if(!can0_driver_installed_) {
        can0_available_ = false;
        return true;
    }

    can0_missed_accum_ += can0_missed_live_;
    can0_overrun_accum_ += can0_overrun_live_;
    can0_missed_live_ = 0;
    can0_overrun_live_ = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    if(can0_handle_ != nullptr) {
        (void)twai_stop_v2(can0_handle_);
        esp_err_t err = twai_driver_uninstall_v2(can0_handle_);
        if(err != ESP_OK) {
            setError("CAN0 uninstall failed (%d)", (int)err);
            return false;
        }
        can0_handle_ = nullptr;
    }
#else
    (void)twai_stop();
    esp_err_t err = twai_driver_uninstall();
    if(err != ESP_OK) {
        setError("CAN0 uninstall failed (%d)", (int)err);
        return false;
    }
#endif
    can0_driver_installed_ = false;
    can0_available_ = false;
    return true;
#endif
}

#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
void CanBusManager::can1IsrThunk() {
    if(instance_ != nullptr) {
        instance_->handleCan1Isr();
    }
}

void CanBusManager::handleCan1Isr() {
    can1_.isr();
}
#endif

bool CanBusManager::initCan1() {
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_NONE
    can1_available_ = false;
    return true;
#else
    if(!can1BitrateSupported(can1_cfg_.bitrate)) {
        setError(
            "CAN1(%s) unsupported bitrate %lu at %u MHz",
            can1BackendName(),
            (unsigned long)can1_cfg_.bitrate,
            (unsigned)BoardConfig::CAN1_CRYSTAL_MHZ);
        return false;
    }

    // First call uses the configured pins; later calls are no-ops in the ESP32 core.
    SPI.begin(
        BoardConfig::SPI_SCK_PIN,
        BoardConfig::SPI_MISO_PIN,
        BoardConfig::SPI_MOSI_PIN,
        BoardConfig::SPI_CS_PIN);

#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    if(can1_begun_) {
        (void)can1_.end();
        can1_begun_ = false;
    }
    can1_available_ = false;

    ACAN2517FDSettings settings(
        can1OscillatorFromConfig(), can1_cfg_.bitrate, DataBitRateFactor::x1);
    settings.mRequestedMode = can1_cfg_.listen_only ? ACAN2517FDSettings::ListenOnly :
                                                      ACAN2517FDSettings::Normal20B;

    can1_begun_ = true;
    uint32_t error_code = 0;
    if(BoardConfig::CAN1_INT_PIN == 255) {
        error_code = can1_.begin(settings, nullptr);
    } else {
        error_code = can1_.begin(settings, CanBusManager::can1IsrThunk);
    }

    if(error_code != 0) {
        setError("CAN1 init error 0x%08lX", (unsigned long)error_code);
        return false;
    }
    can1_available_ = true;
    return true;
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    can1_available_ = false;
    const MCP2515::ERROR reset_err = can1_.reset();
    if(reset_err != MCP2515::ERROR_OK) {
        setError("CAN1(MCP2515) reset failed (%d)", (int)reset_err);
        return false;
    }

    CAN_SPEED speed = CAN_500KBPS;
    (void)can1BitrateToMcp2515Speed(can1_cfg_.bitrate, speed);

    const MCP2515::ERROR br_err = can1_.setBitrate(speed, can1ClockFromConfig());
    if(br_err != MCP2515::ERROR_OK) {
        setError("CAN1(MCP2515) bitrate set failed (%d)", (int)br_err);
        return false;
    }

    const MCP2515::ERROR mode_err = can1_cfg_.listen_only ? can1_.setListenOnlyMode() :
                                                            can1_.setNormalMode();
    if(mode_err != MCP2515::ERROR_OK) {
        setError("CAN1(MCP2515) mode set failed (%d)", (int)mode_err);
        return false;
    }

    can1_available_ = true;
    return true;
#endif
#endif
}

bool CanBusManager::isBusAvailable(core::BusId bus) const {
    switch(bus) {
    case core::BusId::CAN0:
        return can0_available_;
    case core::BusId::CAN1:
        return can1_available_;
    case core::BusId::BOTH:
        return can0_available_ || can1_available_;
    default:
        return false;
    }
}

const char* CanBusManager::can1BackendName() const {
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_NONE
    return "disabled";
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    return "mcp2515";
#else
    return "mcp2517/2518";
#endif
}

bool CanBusManager::setBusConfig(core::BusId bus, const core::BusConfig& cfg) {
    if(!core::isValidBusId(bus)) {
        setError("invalid bus");
        return false;
    }
    if(bus == core::BusId::CAN1 && !BoardConfig::CAN1_ENABLED) {
        setError("CAN1 unavailable (disabled in BoardConfig)");
        return false;
    }

    const bool touch_can0 = (bus == core::BusId::CAN0 || bus == core::BusId::BOTH);
    // `both` applies only to buses that are present; an explicit can1 request always
    // retries initialization (e.g. after connecting the controller).
    const bool touch_can1 =
        BoardConfig::CAN1_ENABLED &&
        (bus == core::BusId::CAN1 || (bus == core::BusId::BOTH && can1_available_));

    // Validate everything before tearing anything down, so a bad request cannot
    // disable a working bus.
    if(touch_can0 && !can0BitrateSupported(cfg.bitrate)) {
        setError("CAN0 unsupported bitrate %lu", (unsigned long)cfg.bitrate);
        return false;
    }
    if(touch_can1 && !can1BitrateSupported(cfg.bitrate)) {
        setError(
            "CAN1(%s) unsupported bitrate %lu at %u MHz",
            can1BackendName(),
            (unsigned long)cfg.bitrate,
            (unsigned)BoardConfig::CAN1_CRYSTAL_MHZ);
        return false;
    }

    const core::BusConfig old_can0 = can0_cfg_;
    const core::BusConfig old_can1 = can1_cfg_;
    const bool can0_was_up = can0_available_;
    const bool can1_was_up = can1_available_;

    if(touch_can0) {
        can0_cfg_ = cfg;
        if(!initCan0()) {
            char err[sizeof(last_error_)];
            strncpy(err, last_error_, sizeof(err));
            err[sizeof(err) - 1] = '\0';
            can0_cfg_ = old_can0;
            if(can0_was_up) {
                (void)initCan0();
            }
            setError("%s (previous CAN0 config restored)", err);
            return false;
        }
        recordBusError(core::BusId::CAN0, true);
    }

    if(touch_can1) {
        can1_cfg_ = cfg;
        if(!initCan1()) {
            char err[sizeof(last_error_)];
            strncpy(err, last_error_, sizeof(err));
            err[sizeof(err) - 1] = '\0';
            can1_cfg_ = old_can1;
            if(can1_was_up) {
                (void)initCan1();
            }
            if(touch_can0) {
                can0_cfg_ = old_can0;
                if(can0_was_up) {
                    (void)initCan0();
                }
            }
            setError("%s (previous config restored)", err);
            recordBusError(core::BusId::CAN1, can1_available_);
            return false;
        }
        recordBusError(core::BusId::CAN1, true);
    }
    return true;
}

core::BusConfig CanBusManager::getBusConfig(core::BusId bus) const {
    if(bus == core::BusId::CAN1) {
        return can1_cfg_;
    }
    return can0_cfg_;
}

bool CanBusManager::setFilter(core::BusId bus, const core::BusFilter& filter) {
    switch(bus) {
    case core::BusId::CAN0:
        can0_filter_ = filter;
        return true;
    case core::BusId::CAN1:
        if(!can1_available_) {
            setError("CAN1 unavailable");
            return false;
        }
        can1_filter_ = filter;
        return true;
    case core::BusId::BOTH:
        can0_filter_ = filter;
        if(can1_available_) {
            can1_filter_ = filter;
        }
        return true;
    default:
        return false;
    }
}

void CanBusManager::clearFilter(core::BusId bus) {
    core::BusFilter clear{false, 0, 0, false, false};
    if(bus == core::BusId::CAN0 || bus == core::BusId::BOTH) {
        can0_filter_ = clear;
    }
    if((bus == core::BusId::CAN1 || bus == core::BusId::BOTH) && can1_available_) {
        can1_filter_ = clear;
    }
}

core::BusFilter CanBusManager::getFilter(core::BusId bus) const {
    if(bus == core::BusId::CAN1) {
        return can1_filter_;
    }
    return can0_filter_;
}

bool CanBusManager::framePassesFilter(const core::CanFrame& frame, const core::BusFilter& filter)
    const {
    if(!filter.enabled) {
        return true;
    }
    if(filter.require_ext_match && frame.ext != filter.ext) {
        return false;
    }
    return ((frame.id & filter.mask) == (filter.filter & filter.mask));
}

void CanBusManager::poll() {
    pollCan0();
    pollCan1();
}

void CanBusManager::pollCan0() {
#if defined(ARDUINO_ARCH_ESP32)
    if(!can0_available_) {
        return;
    }

    twai_status_info_t status{};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
    esp_err_t status_err = (can0_handle_ != nullptr) ?
                               twai_get_status_info_v2(can0_handle_, &status) :
                               ESP_ERR_INVALID_STATE;
#else
    esp_err_t status_err = twai_get_status_info(&status);
#endif
    if(status_err == ESP_OK) {
        can0_missed_live_ = status.rx_missed_count;
        can0_overrun_live_ = status.rx_overrun_count;
    }
    if(status_err == ESP_OK && status.state == TWAI_STATE_BUS_OFF) {
        // Only CAN0 waits for recovery; CAN1 keeps being serviced.
        const uint32_t now_ms = millis();
        if((uint32_t)(now_ms - can0_last_recover_attempt_ms_) >= 500) {
            can0_last_recover_attempt_ms_ = now_ms;
            setError("CAN0 bus-off detected, restarting driver");
            (void)initCan0();
        }
        return;
    }

    core::CanFrame out{};
    for(uint8_t loops = 0; loops < 32; ++loops) {
        twai_message_t can0_msg{};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        esp_err_t err = (can0_handle_ != nullptr) ? twai_receive_v2(can0_handle_, &can0_msg, 0) :
                                                    ESP_ERR_INVALID_STATE;
#else
        esp_err_t err = twai_receive(&can0_msg, 0);
#endif
        if(err != ESP_OK) {
            break;
        }

        out.ts_ms = millis();
        out.ts_us = micros();
        out.bus = core::BusId::CAN0;
        out.id = can0_msg.identifier;
        out.ext = can0_msg.extd != 0;
        out.rtr = can0_msg.rtr != 0;
        out.dlc = can0_msg.data_length_code > 8 ? 8 : can0_msg.data_length_code;
        memset(out.data, 0, sizeof(out.data));
        memcpy(out.data, can0_msg.data, out.dlc);

        if(framePassesFilter(out, can0_filter_)) {
            enqueueRx(out);
        }
    }
#endif
}

void CanBusManager::pollCan1() {
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    if(!can1_available_) {
        return;
    }
    if(BoardConfig::CAN1_INT_PIN == 255) {
        can1_.poll();
    }

    core::CanFrame out{};
    CANFDMessage can1_msg;
    uint8_t can1_loops = 0;
    while(can1_.available() && can1_loops < 32) {
        ++can1_loops;
        if(!can1_.receive(can1_msg)) {
            break;
        }

        out.ts_ms = millis();
        out.ts_us = micros();
        out.bus = core::BusId::CAN1;
        out.id = can1_msg.id;
        out.ext = can1_msg.ext;
        out.rtr = (can1_msg.type == CANFDMessage::CAN_REMOTE);
        out.dlc = can1_msg.len > 8 ? 8 : can1_msg.len;
        memset(out.data, 0, sizeof(out.data));
        memcpy(out.data, can1_msg.data, out.dlc);

        if(framePassesFilter(out, can1_filter_)) {
            enqueueRx(out);
        }
    }
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    if(!can1_available_) {
        return;
    }

    const uint8_t eflg = can1_.getErrorFlags();
    if((eflg & (MCP2515::EFLG_RX0OVR | MCP2515::EFLG_RX1OVR)) != 0) {
        can1_overflow_++;
        can1_.clearRXnOVRFlags();
    }

    core::CanFrame out{};
    struct can_frame can1_msg {};
    for(uint8_t loops = 0; loops < 32; ++loops) {
        const MCP2515::ERROR err = can1_.readMessage(&can1_msg);
        if(err != MCP2515::ERROR_OK) {
            break;
        }

        out.ts_ms = millis();
        out.ts_us = micros();
        out.bus = core::BusId::CAN1;
        out.ext = (can1_msg.can_id & CAN_EFF_FLAG) != 0;
        out.rtr = (can1_msg.can_id & CAN_RTR_FLAG) != 0;
        out.id = out.ext ? (can1_msg.can_id & CAN_EFF_MASK) : (can1_msg.can_id & CAN_SFF_MASK);
        out.dlc = can1_msg.can_dlc > 8 ? 8 : can1_msg.can_dlc;
        memset(out.data, 0, sizeof(out.data));
        memcpy(out.data, can1_msg.data, out.dlc);

        if(framePassesFilter(out, can1_filter_)) {
            enqueueRx(out);
        }
    }
#endif
}

bool CanBusManager::readFrame(core::CanFrame& out) {
    return rx_queue_.pop(out);
}

void CanBusManager::enqueueRx(const core::CanFrame& frame) {
    bool dropped = false;
    rx_queue_.pushDropOldest(frame, &dropped);
    if(dropped) {
        rx_queue_dropped_++;
    }
}

RxLossStats CanBusManager::rxLossStats() const {
    RxLossStats s{};
    s.queue_dropped = rx_queue_dropped_;
    s.can0_missed = can0_missed_accum_ + can0_missed_live_;
    s.can0_overrun = can0_overrun_accum_ + can0_overrun_live_;
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    s.can1_overflow = can1_.hardwareReceiveBufferOverflowCount();
#else
    s.can1_overflow = can1_overflow_;
#endif
    return s;
}

bool CanBusManager::sendFrame(const core::CanFrame& frame) {
    // Every backend masks out-of-range IDs (an 11-bit 0x800 would go out as 0x000), so
    // reject invalid frames here, before any driver sees them.
    if(!core::isConcreteBus(frame.bus) || frame.dlc > 8 ||
       !core::isValidCanId(frame.id, frame.ext)) {
        return false;
    }

    if(frame.bus == core::BusId::CAN0) {
#if !defined(ARDUINO_ARCH_ESP32)
        return false;
#else
        if(!can0_available_) {
            return false;
        }
        twai_message_t out{};
        out.identifier = frame.id;
        out.extd = frame.ext ? 1 : 0;
        out.rtr = frame.rtr ? 1 : 0;
        out.data_length_code = frame.dlc;
        if(!frame.rtr) {
            memcpy(out.data, frame.data, out.data_length_code);
        }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
        if(can0_handle_ == nullptr) {
            return false;
        }
        return twai_transmit_v2(can0_handle_, &out, 0) == ESP_OK;
#else
        return twai_transmit(&out, 0) == ESP_OK;
#endif
#endif
    }

    // frame.bus == CAN1
#if BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2517FD
    if(!can1_available_) {
        return false;
    }
    CANFDMessage out;
    out.id = frame.id;
    out.ext = frame.ext;
    out.type = frame.rtr ? CANFDMessage::CAN_REMOTE : CANFDMessage::CAN_DATA;
    out.len = frame.dlc;
    memset(out.data, 0, sizeof(out.data));
    memcpy(out.data, frame.data, out.len);
    return can1_.tryToSend(out);
#elif BOARDCFG_CAN1_DRIVER == BOARDCFG_CAN1_DRIVER_MCP2515
    if(!can1_available_) {
        return false;
    }

    struct can_frame out {};
    uint32_t can_id = frame.id;
    if(frame.ext) {
        can_id |= CAN_EFF_FLAG;
    }
    if(frame.rtr) {
        can_id |= CAN_RTR_FLAG;
    }
    out.can_id = can_id;
    out.can_dlc = frame.dlc;
    memset(out.data, 0, sizeof(out.data));
    if(!frame.rtr) {
        memcpy(out.data, frame.data, out.can_dlc);
    }
    return can1_.sendMessage(&out) == MCP2515::ERROR_OK;
#else
    return false;
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
// Custom rates use quanta resolutions that divide the 80 MHz TWAI clock exactly
// (ESP32 and ESP32-S3), with a sample point of 75-80 %.
static twai_timing_config_t customTiming(uint32_t quanta_hz, uint8_t tseg_1, uint8_t tseg_2) {
    twai_timing_config_t t{};
    t.quanta_resolution_hz = quanta_hz;
    t.tseg_1 = tseg_1;
    t.tseg_2 = tseg_2;
    t.sjw = 3;
    t.triple_sampling = false;
    return t;
}

bool CanBusManager::getCan0Timing(uint32_t bitrate, twai_timing_config_t& timing) const {
    switch(bitrate) {
    case 1000000:
        timing = TWAI_TIMING_CONFIG_1MBITS();
        return true;
    case 800000:
        timing = TWAI_TIMING_CONFIG_800KBITS();
        return true;
    case 500000:
        timing = TWAI_TIMING_CONFIG_500KBITS();
        return true;
    case 250000:
        timing = TWAI_TIMING_CONFIG_250KBITS();
        return true;
    case 200000:
        timing = customTiming(4000000, 15, 4);
        return true; // 20 tq
    case 125000:
        timing = TWAI_TIMING_CONFIG_125KBITS();
        return true;
    case 100000:
        timing = TWAI_TIMING_CONFIG_100KBITS();
        return true;
    case 83333:
        timing = customTiming(1250000, 11, 3);
        return true; // 15 tq
    case 80000:
        timing = customTiming(1600000, 15, 4);
        return true; // 20 tq
    case 50000:
        timing = TWAI_TIMING_CONFIG_50KBITS();
        return true;
    case 40000:
        timing = customTiming(800000, 15, 4);
        return true; // 20 tq
    case 33333:
        timing = customTiming(500000, 11, 3);
        return true; // 15 tq
    case 31250:
        timing = customTiming(625000, 15, 4);
        return true; // 20 tq
    case 25000:
        timing = TWAI_TIMING_CONFIG_25KBITS();
        return true;
#ifdef TWAI_TIMING_CONFIG_20KBITS
    case 20000:
        timing = TWAI_TIMING_CONFIG_20KBITS();
        return true;
#endif
#ifdef TWAI_TIMING_CONFIG_10KBITS
    case 10000:
        timing = TWAI_TIMING_CONFIG_10KBITS();
        return true;
#endif
#ifdef TWAI_TIMING_CONFIG_5KBITS
    case 5000:
        timing = TWAI_TIMING_CONFIG_5KBITS();
        return true;
#endif
    default:
        return false;
    }
}
#endif

} // namespace can
