/*
 * can_driver.cpp
 *
 * CAN driver abstraction: compile-time selection between
 *   CAN_DRIVER_TWAI   — ESP32 built-in TWAI peripheral (M5Stack ATOM Lite + ATOMIC CAN Base)
 *   CAN_DRIVER_MCP2515 — SPI-attached MCP2515 (generic ESP32 boards)
 *
 * Both drivers implement the CanDriver interface from can_driver.h.
 */

#include "can_driver.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

// ── TWAI driver ───────────────────────────────────────────────────────────────
#if defined(CAN_DRIVER_TWAI)

#include "driver/twai.h"

class TwaiDriver : public CanDriver {
    bool     listen_only_ = false;
    bool     installed_   = false;
    uint32_t tx_count_    = 0;

    bool install_and_start(bool listen_only) {
        twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
            (gpio_num_t)PIN_CAN_TX,
            (gpio_num_t)PIN_CAN_RX,
            listen_only ? TWAI_MODE_LISTEN_ONLY : TWAI_MODE_NORMAL);
        // Queue depths: 10 RX, 5 TX — sufficient for polling loop
        g.rx_queue_len = 10;
        g.tx_queue_len = 5;

        twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
        if (twai_start() != ESP_OK) {
            twai_driver_uninstall();
            return false;
        }
        installed_    = true;
        listen_only_  = listen_only;
        return true;
    }

    void stop_and_uninstall() {
        if (!installed_) return;
        twai_stop();
        twai_driver_uninstall();
        installed_ = false;
    }

public:
    bool begin(bool listen_only) override {
        return install_and_start(listen_only);
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
        return true;
    }

    uint32_t errorCount() override {
        twai_status_info_t info;
        if (twai_get_status_info(&info) != ESP_OK) return 0;
        return info.rx_missed_count + info.bus_error_count + info.tx_failed_count;
    }

    uint32_t txCount() override { return tx_count_; }

    void setListenOnly(bool enable) override {
        if (listen_only_ == enable) return;
        stop_and_uninstall();
        install_and_start(enable);
    }
};

CanDriver *can_driver_create() {
    return new TwaiDriver();
}

// ── MCP2515 driver ────────────────────────────────────────────────────────────
#elif defined(CAN_DRIVER_MCP2515)

#include <SPI.h>
#include <mcp2515.h>   // autowp/autowp-mcp2515

class Mcp2515Driver : public CanDriver {
#if defined(BOARD_TTGO_DISPLAY)
    SPIClass spi_;
    bool     spi_begun_    = false;
#endif
    MCP2515  mcp_;
    bool     listen_only_  = false;
    bool     installed_    = false;
    bool     chip_detected_ = false;
    uint32_t err_count_    = 0;
    uint32_t tx_count_     = 0;

public:
#if defined(BOARD_TTGO_DISPLAY)
    Mcp2515Driver() : spi_(HSPI), mcp_(PIN_MCP_CS, 10000000, &spi_) {}
#else
    Mcp2515Driver() : mcp_(PIN_MCP_CS) {}
#endif

    bool begin(bool listen_only) override {
#if defined(BOARD_TTGO_DISPLAY)
        // Keep MCP2515 on HSPI so TFT_eSPI can own the T-Display LCD SPI bus.
        if (!spi_begun_) {
            spi_.begin(PIN_MCP_SCK, PIN_MCP_MISO, PIN_MCP_MOSI, PIN_MCP_CS);
            spi_.setFrequency(8000000);
            spi_begun_ = true;
        }
#else
        static bool s_spi_begun = false;
        if (!s_spi_begun) {
            SPI.begin(PIN_MCP_SCK, PIN_MCP_MISO, PIN_MCP_MOSI, PIN_MCP_CS);
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
            Serial.printf("[CAN] MCP2515 NOT detected on SPI "
                          "(CS=%d SCK=%d MISO=%d MOSI=%d) — "
                          "check wiring, 5V power, and crystal\n",
                          PIN_MCP_CS, PIN_MCP_SCK, PIN_MCP_MISO, PIN_MCP_MOSI);
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
            Serial.printf("[CAN] MCP2515 detected on SPI — %s mode @ 500 kbps\n",
                          listen_only ? "Listen-Only" : "Normal");
        } else {
            Serial.printf("[CAN] MCP2515 detected but mode change FAILED (err=%d)\n",
                          (int)err);
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
        frame.id  = f.can_id;
        frame.dlc = f.can_dlc;
        memcpy(frame.data, f.data, f.can_dlc);
        return true;
    }

    uint32_t errorCount() override {
        return err_count_;
    }

    uint32_t txCount() override { return tx_count_; }

    void setListenOnly(bool enable) override {
        if (!installed_ || listen_only_ == enable) return;
        listen_only_ = enable;
        if (enable)
            mcp_.setListenOnlyMode();
        else
            mcp_.setNormalMode();
    }
};

CanDriver *can_driver_create() {
    return new Mcp2515Driver();
}

#else
#error "Define CAN_DRIVER_TWAI or CAN_DRIVER_MCP2515 in platformio.ini build_flags"
#endif
