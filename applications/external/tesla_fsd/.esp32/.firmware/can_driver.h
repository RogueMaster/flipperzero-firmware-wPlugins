#pragma once

#include <stdint.h>
#include "fsd_handler.h"  // for CanFrame

enum CanBusId : uint8_t {
    CAN_BUS_PRIMARY = 0,    // can0
    CAN_BUS_SECONDARY = 1,  // can1
    CAN_BUS_COUNT = 2,
};

static inline const char *can_bus_name(CanBusId bus) {
    return bus == CAN_BUS_SECONDARY ? "can1" : "can0";
}

// ── Abstract CAN driver ───────────────────────────────────────────────────────
// Implemented by TwaiDriver (CAN_DRIVER_TWAI) and Mcp2515Driver (CAN_DRIVER_MCP2515).
// Compile-time selection via platformio.ini build_flags.

class CanDriver {
public:
    /** Initialise hardware and start the CAN bus.
     *  @param listen_only  If true, enter hardware listen-only mode (no ACK, no TX). */
    virtual bool begin(bool listen_only) = 0;

    /** Send one CAN frame.  Returns false when TX is not allowed (listen-only, bus-off, etc.). */
    virtual bool send(const CanFrame &frame) = 0;

    /** Non-blocking receive.  Fills frame and returns true if a frame was available. */
    virtual bool receive(CanFrame &frame) = 0;

    /** Cumulative bus/TX-error counter.
     *  TWAI: rx_missed + bus_errors + tx_failed.
     *  MCP2515: number of sendMessage() failures (typically ALLTXBUSY). */
    virtual uint32_t errorCount() = 0;

    /** Cumulative count of frames successfully transmitted on the bus. */
    virtual uint32_t txCount() = 0;

    /** Cumulative count of frames received from the bus. */
    virtual uint32_t rxCount() = 0;

    /** Switch between listen-only and normal TX mode at runtime.
     *  Implementations must reinitialise the hardware as needed. */
    virtual void setListenOnly(bool enable) = 0;

    /** Restrict hardware reception to a single CAN id for full-rate single-ID
     *  capture on a busy bus, or restore accept-all.
     *  @param single  true = accept only @p id; false = accept all ids.
     *  @param id      the standard 11-bit id to accept when @p single is true.
     *  Default no-op: drivers that don't implement it keep accept-all and rely
     *  on software filtering, which decimates a single id on a busy bus. */
    virtual void setAcceptanceFilter(bool single, uint32_t id) { (void)single; (void)id; }

    /** Whether the underlying CAN hardware was detected on the bus/SPI.
     *  TWAI lives inside the SoC and is therefore always present.
     *  MCP2515 returns true once the chip has answered an SPI probe. */
    virtual bool hardwarePresent() { return true; }

    virtual ~CanDriver() = default;
};

/** Factory function — returns the driver selected at compile time.
 *  Caller owns the returned pointer. */
CanDriver *can_driver_create();

/** Factory function for boards with two active CAN controllers.
 *  Caller owns the returned pointer. */
CanDriver *can_driver_create(CanBusId bus);
