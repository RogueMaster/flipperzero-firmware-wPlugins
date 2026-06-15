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
#include "config.h"
#include <Arduino.h>
#include <string.h>

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
    bool     filter_single_ = false;  // accept only filter_id_ when true
    uint32_t filter_id_     = 0;      // standard 11-bit id for single-id capture
    bool     recovering_    = false;  // true while a bus-off recovery is in flight

    bool install_and_start(bool listen_only) {
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
        if (filter_single_) {
            // Standard-frame single filter: match exactly filter_id_. The id
            // sits in bits [31:21] of the acceptance code; mask bits set to 1
            // are "don't care", so we clear only the 11 id bits.
            f.acceptance_code = (filter_id_ & 0x7FFu) << 21;
            f.acceptance_mask = ~(((uint32_t)0x7FFu) << 21);
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
        twai_message_t msg;
        // Non-blocking receive (timeout = 0)
        if (twai_receive(&msg, 0) != ESP_OK) return false;
        frame.id  = msg.identifier;
        frame.dlc = msg.data_length_code;
        memcpy(frame.data, msg.data, frame.dlc);
        rx_count_++;
        return true;
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

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

    void setListenOnly(bool enable) override {
        if (listen_only_ == enable) return;
        stop_and_uninstall();
        if (!install_and_start(enable)) {
            Serial.printf("[CAN] %s TWAI mode switch FAILED\n", label_);
        }
    }

    void setAcceptanceFilter(bool single, uint32_t id) override {
        if (filter_single_ == single && (!single || filter_id_ == id)) return;
        filter_single_ = single;
        filter_id_     = id;
        if (!installed_) return;  // begin() will pick up the new filter
        bool lo = listen_only_;
        stop_and_uninstall();
        if (!install_and_start(lo)) {
            Serial.printf("[CAN] %s TWAI filter switch FAILED\n", label_);
        } else if (single) {
            Serial.printf("[CAN] %s TWAI hardware filter -> 0x%03lX only (full-rate capture)\n",
                          label_, (unsigned long)(id & 0x7FFu));
        } else {
            Serial.printf("[CAN] %s TWAI hardware filter -> accept all\n", label_);
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
    uint32_t err_count_    = 0;
    uint32_t tx_count_     = 0;
    uint32_t rx_count_     = 0;

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

        mcp_.reset();

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
        if (mcp_.readMessage(&f) != MCP2515::ERROR_OK) return false;
        frame.id  = f.can_id & CAN_EFF_MASK;
        frame.dlc = f.can_dlc;
        memcpy(frame.data, f.data, f.can_dlc);
        rx_count_++;
        return true;
    }

    uint32_t errorCount() override {
        return err_count_;
    }

    uint32_t txCount() override { return tx_count_; }

    uint32_t rxCount() override { return rx_count_; }

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
        if (!installed_) return;
        // Both receive buffers: set the mask so all 11 id bits must match
        // (0x7FF) for single-id capture, or 0x000 (don't-care = accept all) to
        // restore. Point every filter at the wanted id. setFilter* enter CONFIG
        // mode internally, so the run mode is re-applied afterwards.
        uint32_t mask = single ? 0x7FFu : 0x000u;
        uint32_t fid  = single ? (id & 0x7FFu) : 0x000u;
        bool ok = true;
        ok &= (mcp_.setFilterMask(MCP2515::MASK0, false, mask) == MCP2515::ERROR_OK);
        ok &= (mcp_.setFilterMask(MCP2515::MASK1, false, mask) == MCP2515::ERROR_OK);
        const MCP2515::RXF rxf[6] = {MCP2515::RXF0, MCP2515::RXF1, MCP2515::RXF2,
                                     MCP2515::RXF3, MCP2515::RXF4, MCP2515::RXF5};
        for (uint8_t i = 0; i < 6; i++) {
            ok &= (mcp_.setFilter(rxf[i], false, fid) == MCP2515::ERROR_OK);
        }
        // setFilter* leave the chip in CONFIG mode; restore the prior run mode.
        MCP2515::ERROR merr = listen_only_ ? mcp_.setListenOnlyMode() : mcp_.setNormalMode();
        ok &= (merr == MCP2515::ERROR_OK);
        if (!ok) {
            Serial.printf("[CAN] %s MCP2515 filter switch FAILED\n", label_);
        } else if (single) {
            Serial.printf("[CAN] %s MCP2515 hardware filter -> 0x%03lX only (full-rate capture)\n",
                          label_, (unsigned long)fid);
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

#if !defined(CAN_DRIVER_TWAI) && !defined(CAN_DRIVER_MCP2515) && !defined(CAN_DRIVER_T2CAN_DUAL)
#error "Define CAN_DRIVER_TWAI, CAN_DRIVER_MCP2515, or CAN_DRIVER_T2CAN_DUAL in platformio.ini build_flags"
#endif
