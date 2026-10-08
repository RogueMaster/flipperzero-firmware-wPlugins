/*
 * can_driver.cpp
 *
 * CAN driver abstraction: compile-time selection between
 *   CAN_DRIVER_TWAI        — ESP32 built-in TWAI peripheral
 *   CAN_DRIVER_MCP2515     — SPI-attached MCP2515
 *   CAN_DRIVER_T2CAN_DUAL  — LilyGO T-2CAN: TWAI can0 + MCP2515 can1
 *
 * Both drivers implement the CanDriver interface from can_driver.h.
 */

#include "can_driver.h"
#include "can_capture_filter.h"
#include "config.h"
#include "../../fsd_logic/fsd_can_ops.h"  // TESLA_CAN_MAX_DLC / tesla_can_tx_valid
#include <Arduino.h>
#include <string.h>

// DLC>8 frames one receive() call may drop before it reports "queue empty".
#define CAN_RX_REJECT_BURST_MAX 8u

// ── TWAI driver ───────────────────────────────────────────────────────────────
#if defined(CAN_DRIVER_TWAI) || defined(CAN_DRIVER_T2CAN_DUAL)

#include "driver/twai.h"

class TwaiDriver : public CanDriver {
    const char *label_;
    int      tx_pin_;
    int      rx_pin_;
    bool     listen_only_ = false;
    bool     installed_   = false;
    uint32_t tx_count_    = 0;
    uint32_t rx_count_    = 0;
    uint8_t  filter_count_ = 0;
    uint32_t filter_ids_[CAN_CAPTURE_MAX_IDS] = {};
    bool     recovering_    = false;  // true while a bus-off recovery is in flight
    bool     busoff_event_  = false;  // consume-on-read edge: recovery just started
    uint32_t rx_rejected_   = 0;      // DLC>8 frames dropped

    bool install_and_start(bool listen_only) {
#ifdef SNIFFER_ONLY
        // Research sniffer build: hardware-forced Listen-Only. TWAI_MODE_LISTEN_ONLY
        // never emits dominant bits (not even ACKs), so the node is physically
        // incapable of transmitting — zero ban risk from injected frames.
        listen_only = true;
#endif
        twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
            (gpio_num_t)tx_pin_,
            (gpio_num_t)rx_pin_,
            listen_only ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
        // Queue depths: 64 RX (busy Vehicle CAN can deliver thousands of
        // frames/s; a deeper queue cuts controller-level drops), 5 TX.
        g.rx_queue_len = 64;
        g.tx_queue_len = 5;

        twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f;
        if (filter_count_ > 0) {
            // Standard-frame hardware prefilter. TWAI has one acceptance
            // code/mask, so an exact whitelist is only possible for one ID.
            // For multiple IDs, match the common ID bits and leave differing
            // bits as don't-care; software filtering still enforces the final
            // list, while hardware drops part of unrelated traffic.
            CanTwaiPrefilter prefilter = can_capture_twai_prefilter(filter_ids_, filter_count_);
            f.acceptance_code = prefilter.code;
            f.acceptance_mask = prefilter.mask;
            f.single_filter   = true;
        } else {
            f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        }

        if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
        if (twai_start() != ESP_OK) {
            twai_driver_uninstall();
            return false;
        }
        installed_    = true;
        listen_only_  = listen_only;
        recovering_   = false;  // fresh controller, no recovery pending
        return true;
    }

    void stop_and_uninstall() {
        if (!installed_) return;
        twai_stop();
        twai_driver_uninstall();
        installed_ = false;
        recovering_ = false;
    }

public:
    TwaiDriver(const char *label, int tx_pin, int rx_pin)
        : label_(label), tx_pin_(tx_pin), rx_pin_(rx_pin) {}

    bool begin(bool listen_only) override {
        bool ok = install_and_start(listen_only);
        Serial.printf("[CAN] %s TWAI %s @ 500 kbps (TX=%d RX=%d)\n",
                      label_, ok ? (listen_only ? "Listen-Only" : "Normal") : "FAILED",
                      tx_pin_, rx_pin_);
        return ok;
    }

    bool send(const CanFrame &frame) override {
        if (listen_only_) return false;
        if (!tesla_can_tx_valid(frame.id, frame.dlc)) return false;
        twai_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.identifier       = frame.id;
        msg.data_length_code = frame.dlc;
        memcpy(msg.data, frame.data, frame.dlc);
        // 5 ms TX timeout — short enough to not stall the main loop
        if (twai_transmit(&msg, pdMS_TO_TICKS(5)) != ESP_OK) return false;
        tx_count_++;
        return true;
    }

    bool receive(CanFrame &frame) override {
        for (uint8_t i = 0; i < CAN_RX_REJECT_BURST_MAX; i++) {
            twai_message_t msg;
            // Non-blocking receive (timeout = 0)
            if (twai_receive(&msg, 0) != ESP_OK) return false;
            // TWAI hands DLC 9..15 up as-is while msg.data stays 8 bytes; a
            // plain memcpy of dlc bytes overran both buffers. Drop those.
            if (msg.data_length_code > TESLA_CAN_MAX_DLC) {
                rx_rejected_++;
                continue;
            }
            memset(&frame, 0, sizeof(frame));
            frame.id  = msg.identifier;
            frame.ext = msg.extd ? 1u : 0u;   // process_frame() records these
            frame.req = msg.rtr  ? 1u : 0u;   // but never hands them to a handler
            frame.dlc = msg.data_length_code;
            memcpy(frame.data, msg.data, frame.dlc);
            rx_count_++;
            return true;
        }
        return false;
    }

    uint32_t errorCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.rx_missed_count + info.bus_error_count + info.tx_failed_count;
    }

    // Bus-off auto-recovery. Without this, once TX errors push the TWAI TEC past
    // the limit the controller goes BUS_OFF and twai_receive() stops returning
    // frames forever (RX frames/s drops to 0) until a manual mode toggle reinstalls
    // the driver (#108). Acts only in the BUS_OFF/RECOVERING states; a healthy bus
    // hits the first early-return and this is a no-op.
    void serviceHealth() override {
        if (!installed_) return;
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return;
        if (info.state == TWAI_STATE_BUS_OFF && !recovering_) {
            // BUS_OFF -> RECOVERING; controller auto-advances to STOPPED after
            // observing 128 occurrences of 11 consecutive recessive bits.
            if (twai_initiate_recovery() == ESP_OK) {
                recovering_ = true;
                busoff_event_ = true;  // edge for the black-box EVT_BUSOFF trigger
                Serial.printf("[CAN] %s bus-off detected — initiating recovery\n", label_);
            }
        } else if (recovering_ && info.state == TWAI_STATE_STOPPED) {
            // Recovery complete — restart so RX/TX resume with the existing
            // filter and listen-only configuration.
            if (twai_start() == ESP_OK) {
                recovering_ = false;
                Serial.printf("[CAN] %s bus recovered — restarted\n", label_);
            }
        }
    }

    bool busOffEvent() override {
        bool e = busoff_event_;
        busoff_event_ = false;
        return e;
    }

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

    uint32_t rxRejectedCount() override { return rx_rejected_; }

    uint32_t rxMissedCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.rx_missed_count;
    }

    uint32_t busErrorCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.bus_error_count;
    }

    uint32_t txFailedCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.tx_failed_count;
    }

    void setListenOnly(bool enable) override {
#ifdef SNIFFER_ONLY
        (void)enable;   // sniffer build is permanently Listen-Only; ignore mode switches
        return;
#endif
        if (listen_only_ == enable) return;
        stop_and_uninstall();
        if (!install_and_start(enable)) {
            Serial.printf("[CAN] %s TWAI mode switch FAILED\n", label_);
        }
    }

    const char *acceptanceFilterMode() override {
        if (!installed_) return "down";
        return filter_count_ == 0 ? "accept-all" : filter_count_ == 1 ? "exact" : "prefilter";
    }

    void setAcceptanceFilter(bool single, uint32_t id) override {
        if (single) setAcceptanceFilters(&id, 1);
        else setAcceptanceFilters(nullptr, 0);
    }

    void setAcceptanceFilters(const uint32_t *ids, uint8_t count) override {
        if (ids == nullptr) count = 0;
        if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;

        bool same = filter_count_ == count;
        for (uint8_t i = 0; same && i < count; i++) {
            same = ((filter_ids_[i] & 0x7FFu) == (ids[i] & 0x7FFu));
        }
        if (same) return;

        filter_count_ = count;
        for (uint8_t i = 0; i < count; i++) {
            filter_ids_[i] = ids[i] & 0x7FFu;
        }
        if (!installed_) return;  // begin() will pick up the new filter
        bool lo = listen_only_;
        stop_and_uninstall();
        if (!install_and_start(lo)) {
            Serial.printf("[CAN] %s TWAI filter switch FAILED\n", label_);
        } else if (count > 0) {
            Serial.printf("[CAN] %s TWAI hardware filter -> %u id prefilter\n",
                          label_, (unsigned)count);
        } else {
            Serial.printf("[CAN] %s TWAI hardware filter -> accept all\n", label_);
        }
    }

    // Pre-reboot quiesce: stop the controller (no TX, no ACK), then take the TX
    // pad back from the TWAI peripheral and drive it recessive until the chip
    // resets. The level is latched high before the pad becomes an output (and
    // set again after) so the handover itself can't blip dominant on the bus.
    void shutdown() override {
        stop_and_uninstall();
        if (tx_pin_ >= 0) {
            digitalWrite((uint8_t)tx_pin_, HIGH);
            pinMode((uint8_t)tx_pin_, OUTPUT);
            digitalWrite((uint8_t)tx_pin_, HIGH);
        }
    }
};
#endif

// ── MCP2515 driver ────────────────────────────────────────────────────────────
#if defined(CAN_DRIVER_MCP2515) || defined(CAN_DRIVER_T2CAN_DUAL)

#include <SPI.h>
#include <mcp2515.h>   // autowp/autowp-mcp2515

static const char *mcp_clock_name(CAN_CLOCK clock) {
    switch (clock) {
        case MCP_20MHZ: return "20 MHz";
        case MCP_16MHZ: return "16 MHz";
        case MCP_8MHZ:  return "8 MHz";
        default:        return "?";
    }
}

class Mcp2515Driver : public CanDriver {
#if defined(BOARD_TTGO_DISPLAY)
    SPIClass spi_;
    bool     spi_begun_    = false;
#endif
    MCP2515  mcp_;
    const char *label_;
    int      cs_pin_;
    int      sck_pin_;
    int      miso_pin_;
    int      mosi_pin_;
    int      rst_pin_;
    bool     listen_only_  = false;
    bool     installed_    = false;
    bool     chip_detected_ = false;
    const char *filter_mode_ = "unknown";
    uint32_t err_count_    = 0;
    uint32_t tx_count_     = 0;
    uint32_t rx_count_     = 0;
    uint32_t rx_rejected_  = 0;   // DLC>8 frames dropped (readMessage FAIL)

public:
#if defined(BOARD_TTGO_DISPLAY)
    Mcp2515Driver(const char *label, int cs_pin, int sck_pin, int miso_pin, int mosi_pin, int rst_pin)
        : spi_(HSPI),
          mcp_(cs_pin, 10000000, &spi_),
          label_(label),
          cs_pin_(cs_pin),
          sck_pin_(sck_pin),
          miso_pin_(miso_pin),
          mosi_pin_(mosi_pin),
          rst_pin_(rst_pin) {}
#else
    Mcp2515Driver(const char *label, int cs_pin, int sck_pin, int miso_pin, int mosi_pin, int rst_pin)
        : mcp_(cs_pin),
          label_(label),
          cs_pin_(cs_pin),
          sck_pin_(sck_pin),
          miso_pin_(miso_pin),
          mosi_pin_(mosi_pin),
          rst_pin_(rst_pin) {}
#endif

    bool begin(bool listen_only) override {
        if (rst_pin_ >= 0) {
            pinMode((uint8_t)rst_pin_, OUTPUT);
            digitalWrite((uint8_t)rst_pin_, LOW);
            delay(2);
            digitalWrite((uint8_t)rst_pin_, HIGH);
            delay(10);
        }

#if defined(BOARD_TTGO_DISPLAY)
        // Keep MCP2515 on HSPI so TFT_eSPI can own the T-Display LCD SPI bus.
        if (!spi_begun_) {
            spi_.begin(sck_pin_, miso_pin_, mosi_pin_, cs_pin_);
            spi_.setFrequency(8000000);
            spi_begun_ = true;
        }
#else
        static bool s_spi_begun = false;
        if (!s_spi_begun) {
            SPI.begin(sck_pin_, miso_pin_, mosi_pin_, cs_pin_);
            SPI.setFrequency(8000000);
            s_spi_begun = true;
        }
#endif

        filter_mode_ = mcp_.reset() == MCP2515::ERROR_OK ? "accept-all" : "unknown";

        // setBitrate() internally enters CONFIG mode and verifies the mode
        // change via an SPI register read-back. If the MCP2515 isn't wired
        // up / powered / responding on SPI, this is the call that fails first.
        // Failure here means SPI/chip presence problem (entering CONFIG mode
        // does not require any CAN bus traffic).
        if (mcp_.setBitrate(CAN_500KBPS, MCP_CRYSTAL_MHZ) != MCP2515::ERROR_OK) {
            chip_detected_ = false;
            installed_     = false;
            Serial.printf("[CAN] %s MCP2515 NOT detected on SPI "
                          "(CS=%d SCK=%d MISO=%d MOSI=%d CLK=%s) — "
                          "check wiring, 5V power, and crystal\n",
                          label_, cs_pin_, sck_pin_, miso_pin_, mosi_pin_,
                          mcp_clock_name(MCP_CRYSTAL_MHZ));
            return false;
        }

        // Chip responded over SPI: we know the MCP2515 is physically there.
        chip_detected_ = true;

        // Switching to listen-only / normal mode is a chip-internal CANCTRL
        // change — does not require CAN bus traffic either, but if it fails
        // after a successful setBitrate it is still a chip / SPI issue.
        MCP2515::ERROR err = listen_only
            ? mcp_.setListenOnlyMode()
            : mcp_.setNormalMode();
        listen_only_ = listen_only;
        installed_ = (err == MCP2515::ERROR_OK);
        if (installed_) {
            Serial.printf("[CAN] %s MCP2515 detected on SPI — %s mode @ 500 kbps, clock=%s\n",
                          label_,
                          listen_only ? "Listen-Only" : "Normal",
                          mcp_clock_name(MCP_CRYSTAL_MHZ));
        } else {
            Serial.printf("[CAN] %s MCP2515 detected but mode change FAILED (err=%d)\n",
                          label_, (int)err);
        }
        return installed_;
    }

    bool hardwarePresent() override { return chip_detected_; }

    bool send(const CanFrame &frame) override {
        if (!installed_ || listen_only_) return false;
        if (!tesla_can_tx_valid(frame.id, frame.dlc)) return false;
        struct can_frame f;
        f.can_id  = frame.id;
        f.can_dlc = frame.dlc;
        memcpy(f.data, frame.data, frame.dlc);
        if (mcp_.sendMessage(&f) != MCP2515::ERROR_OK) {
            err_count_++;
            return false;
        }
        tx_count_++;
        return true;
    }

    bool receive(CanFrame &frame) override {
        if (!installed_) return false;
        struct can_frame f;
        MCP2515::ERROR err = mcp_.readMessage(&f);
        if (err == MCP2515::ERROR_FAIL) {
            // autowp returns FAIL for a DLC 9..15 frame WITHOUT clearing its
            // RXnIF flag, and readMessage() always re-reads RXB0 first, so that
            // one frame would wedge RX for good. Drop it (and whatever sits in
            // the other buffer) by clearing the RX interrupt flags.
            mcp_.clearInterrupts();
            rx_rejected_++;
            return false;
        }
        if (err != MCP2515::ERROR_OK) return false;
        // autowp flags extended / remote frames in can_id (a remote frame's
        // data bytes are whatever the previous frame left in the buffer).
        bool extended = (f.can_id & CAN_EFF_FLAG) != 0u;
        memset(&frame, 0, sizeof(frame));
        frame.id  = f.can_id & (extended ? CAN_EFF_MASK : CAN_SFF_MASK);
        frame.ext = extended ? 1u : 0u;
        frame.req = (f.can_id & CAN_RTR_FLAG) != 0u ? 1u : 0u;
        frame.dlc = f.can_dlc;   // readMessage() already refused DLC > 8
        memcpy(frame.data, f.data, frame.dlc);
        rx_count_++;
        return true;
    }

    uint32_t errorCount() override {
        return err_count_;
    }

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

    uint32_t rxRejectedCount() override { return rx_rejected_; }

    // rxMissedCount(): use the CanDriver default (0). The MCP2515 overflow flag
    // would need an extra SPI read on the hot path, so it is not surfaced here.
    // busErrorCount(): default 0 too; the chip only exposes live TEC/REC levels,
    // not a cumulative bus-error count.

    // Every errorCount() event is a sendMessage() failure.
    uint32_t txFailedCount() override { return err_count_; }

    void setListenOnly(bool enable) override {
        if (!installed_ || listen_only_ == enable) return;
        MCP2515::ERROR err = enable ? mcp_.setListenOnlyMode() : mcp_.setNormalMode();
        if (err == MCP2515::ERROR_OK) {
            listen_only_ = enable;
        } else {
            Serial.printf("[CAN] %s MCP2515 mode switch FAILED (err=%d)\n",
                          label_, (int)err);
        }
    }

    void setAcceptanceFilter(bool single, uint32_t id) override {
        setAcceptanceFilters(single ? &id : nullptr, single ? 1 : 0);
    }

    const char *acceptanceFilterMode() override {
        return installed_ ? filter_mode_ : "down";
    }

    // Pre-reboot quiesce: CONFIG mode takes the MCP2515 off the bus (no TX, no
    // ACK), and it stays there across the ESP32 reset until begin() runs again.
    // If the mode request doesn't take (e.g. a frame stuck pending TX), the SPI
    // RESET instruction forces the chip into CONFIG mode.
    void shutdown() override {
        if (!chip_detected_) return;  // nothing answering on SPI
        if (mcp_.setConfigMode() != MCP2515::ERROR_OK) mcp_.reset();
        installed_ = false;
    }

    void setAcceptanceFilters(const uint32_t *ids, uint8_t count) override {
        if (!installed_) return;
        if (!ids) count = 0;
        if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
        bool ok = can_capture_program_mcp_filters(mcp_, ids, count, listen_only_);
        filter_mode_ = ok ? (count ? "exact" : "accept-all") : "unknown";
        if (!ok) {
            Serial.printf("[CAN] %s MCP2515 filter switch FAILED\n", label_);
        } else if (count) {
            Serial.printf("[CAN] %s MCP2515 hardware filter -> %u exact ids\n",
                          label_, (unsigned)count);
        } else {
            Serial.printf("[CAN] %s MCP2515 hardware filter -> accept all\n", label_);
        }
    }
};
#endif

CanDriver *can_driver_create() {
#if defined(CAN_DRIVER_TWAI)
    return new TwaiDriver("can0", PIN_CAN_TX, PIN_CAN_RX);
#elif defined(CAN_DRIVER_MCP2515)
    return new Mcp2515Driver("can0", PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO,
                             PIN_MCP_MOSI, PIN_MCP_RST);
#elif defined(CAN_DRIVER_T2CAN_DUAL)
    return can_driver_create(CAN_BUS_PRIMARY);
#else
    return nullptr;
#endif
}

CanDriver *can_driver_create(CanBusId bus) {
#if defined(CAN_DRIVER_T2CAN_DUAL)
    if (bus == CAN_BUS_SECONDARY) {
        return new Mcp2515Driver("can1", PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO,
                                 PIN_MCP_MOSI, PIN_MCP_RST);
    }
    return new TwaiDriver("can0", PIN_CAN_TX, PIN_CAN_RX);
#else
    (void)bus;
    return can_driver_create();
#endif
}

CanErrorSplit can_error_split(CanDriver **buses, uint8_t count) {
    CanErrorSplit s = {};
    for (uint8_t i = 0; i < count; i++) {
        if (!buses[i]) continue;
        s.rx_missed_count += buses[i]->rxMissedCount();
        s.bus_error_count += buses[i]->busErrorCount();
        s.tx_failed_count += buses[i]->txFailedCount();
    }
    return s;
}

void can_shutdown_all(CanDriver **buses, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        if (buses[i]) buses[i]->shutdown();
    }
    Serial.println("[CAN] Controllers stopped — bus released");
}

#if !defined(CAN_DRIVER_TWAI) && !defined(CAN_DRIVER_MCP2515) && !defined(CAN_DRIVER_T2CAN_DUAL)
#error "Define CAN_DRIVER_TWAI, CAN_DRIVER_MCP2515, or CAN_DRIVER_T2CAN_DUAL in platformio.ini build_flags"
#endif
