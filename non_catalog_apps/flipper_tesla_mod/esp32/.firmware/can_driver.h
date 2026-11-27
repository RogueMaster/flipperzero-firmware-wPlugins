#pragma once

#include <stddef.h>
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

/** The MCP2515 receive task (Mcp2515Driver::setRxTask) as it stands. */
struct CanRxTaskInfo {
    bool     wanted;      // the setting
    bool     running;     // the task is up and receive() reads its queue
    bool     int_pin;     // woken by the chip's INT pin, else a 1 ms poll
    uint32_t queue_len;   // frames the queue holds
    uint32_t high_water;  // most frames waiting at once
    uint32_t dropped;     // frames lost on a full queue
    uint32_t overflows;   // RXnOVR events seen by the task, one or more frames each
    uint32_t stack_free;  // the task's lowest free stack, bytes (0 when not running)
};

// ── Abstract CAN driver ───────────────────────────────────────────────────────
// Implemented by TwaiDriver (CAN_DRIVER_TWAI), Mcp2515Driver (CAN_DRIVER_MCP2515)
// and Mcp2518FdDriver (T-2CAN FD can1). Compile-time selection via platformio.ini
// build_flags.

class CanDriver {
public:
    /** Initialise hardware and start the CAN bus.
     *  @param listen_only  If true, enter hardware listen-only mode (no ACK, no TX). */
    virtual bool begin(bool listen_only) = 0;

    /** Send one CAN frame.  Returns false when TX is not allowed (listen-only, bus-off, etc.). */
    virtual bool send(const CanFrame &frame) = 0;

    /** Non-blocking receive.  Fills frame and returns true if a frame was available. */
    virtual bool receive(CanFrame &frame) = 0;

    /** Cumulative combined error counter = rxMissedCount() + busErrorCount()
     *  + txFailedCount(). On a busy bus this is mostly RX-queue drops.
     *  TWAI: rx_missed + bus_errors + tx_failed.
     *  MCP2515: number of sendMessage() failures (typically ALLTXBUSY). */
    virtual uint32_t errorCount() = 0;

    /** Cumulative count of frames successfully transmitted on the bus. */
    virtual uint32_t txCount() = 0;

    /** Cumulative count of frames received from the bus. */
    virtual uint32_t rxCount() = 0;

    /** Frames the driver dropped because their DLC was above 8 (legal on the
     *  wire, but CanFrame holds 8 bytes), and on the MCP2518FD any CAN FD
     *  frame. Extended / remote frames are passed up with frame.ext /
     *  frame.req set; process_frame() filters those. */
    virtual uint32_t rxRejectedCount() { return 0; }

    /** Cumulative count of frames the CONTROLLER dropped because its RX queue
     *  overflowed (silent decimation of a busy bus). TWAI: twai_status_info_t
     *  .rx_missed_count, carried across the driver reinstalls of a mode switch
     *  or a capture filter change, as are bus errors and failed sends.
     *  MCP2518FD: RX FIFO overflow events, each one or more frames. MCP2515:
     *  only while its receive task runs, frames lost on the task's full queue
     *  plus RX overflow events. Drivers without the metric return 0. */
    virtual uint32_t rxMissedCount() { return 0; }

    /** Cumulative bus errors (bit/stuff/form/CRC/ACK) the controller saw on
     *  the wire. TWAI: .bus_error_count. MCP2518FD: the CiBDIAG0 counters.
     *  Drivers without the metric return 0. */
    virtual uint32_t busErrorCount() { return 0; }

    /** Cumulative frames that failed to transmit. TWAI: .tx_failed_count;
     *  MCP2515: sendMessage() failures. Drivers without the metric return 0. */
    virtual uint32_t txFailedCount() { return 0; }

    /** Mode, controller state and error counters in the driver's own terms,
     *  for the serial `canstat` command. TWAI and MCP2515 count different
     *  things, so errorCount() alone reads the same on both while meaning
     *  different things; this spells out what each one has. Reads registers
     *  (over SPI on the MCP2515): not for the receive path. */
    virtual void describeState(char *out, size_t n) { if (n) out[0] = '\0'; }

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

    /** Restrict reception to up to six standard IDs, or restore accept-all
     *  when count is zero. MCP2515 matches exactly; TWAI matches common bits
     *  and needs software filtering to enforce a multi-ID list. */
    virtual void setAcceptanceFilters(const uint32_t *ids, uint8_t count) {
        if (count == 1 && ids) setAcceptanceFilter(true, ids[0]);
        else setAcceptanceFilter(false, 0);
    }

    /** Actual reception filter state for capture metadata. Unknown is used
     *  when a driver cannot report successful hardware programming. */
    virtual const char *acceptanceFilterMode() { return "unknown"; }

    /** RX ID whitelist (performance) — opt-in, default OFF (see
     *  fsd_logic/fsd_rx_whitelist.h and recompute_rx_filter() in main.cpp).
     *  Program the controller's hardware acceptance filter to admit AT LEAST
     *  every id in @p ids — an EXACT SUPERSET, never a subset. Returns true only
     *  when such a filter was installed; false when this controller cannot
     *  express a superset of the set (more exact ids than it has filter slots),
     *  in which case the caller MUST call setAcceptAll() so no needed id is ever
     *  dropped (fits-or-accept-all, never a partial filter). This is separate
     *  from the capture acceptance filter (setAcceptanceFilters): the caller
     *  drives at most one of the two at a time, and never touches the filters at
     *  all while the whitelist mode is off. Default: cannot express it -> false. */
    virtual bool setAcceptFilter(const uint32_t *ids, size_t n) {
        (void)ids; (void)n; return false;
    }

    /** Restore accept-all reception (today's default behaviour), clearing any
     *  whitelist filter this driver installed. Default: delegate to the capture
     *  path's accept-all. */
    virtual void setAcceptAll() { setAcceptanceFilters(nullptr, 0); }

    /** Whether the underlying CAN hardware was detected on the bus/SPI.
     *  TWAI lives inside the SoC and is therefore always present.
     *  MCP2515 returns true once the chip has answered an SPI probe. */
    virtual bool hardwarePresent() { return true; }

    /** Consume-on-read edge: true exactly once after serviceHealth() has just
     *  initiated a bus-off recovery. Lets the main loop forward an EVT_BUSOFF to
     *  the event-core (the black-box trigger) without fsd_logic reaching into the
     *  TWAI controller state. Default false for drivers with no bus-off concept. */
    virtual bool busOffEvent() { return false; }

    /** Periodic health service — call once per main-loop iteration.
     *  TWAI: detects a bus-off controller (TX errors exceeded the limit) and
     *  drives the ESP-IDF recovery sequence so RX resumes without a manual
     *  Deactivate/Activate toggle. No-op while the bus is healthy. */
    virtual void serviceHealth() {}

    /** Stop TX/ACK and release the bus before a reboot. Default no-op. */
    virtual void shutdown() {}

    /** MCP2515 only: read the chip from a FreeRTOS task into a queue instead
     *  of from the loop (opt-in). Takes effect at once when the controller is
     *  up, else at the next successful begin(). Default no-op. */
    virtual void setRxTask(bool on) { (void)on; }

    /** Fills @p out and returns true on a driver that has the receive task. */
    virtual bool rxTaskInfo(CanRxTaskInfo &out) { (void)out; return false; }

    virtual ~CanDriver() = default;
};

/** Factory function — returns the driver selected at compile time.
 *  Caller owns the returned pointer. */
CanDriver *can_driver_create();

/** Factory function for boards with two active CAN controllers.
 *  Caller owns the returned pointer. */
CanDriver *can_driver_create(CanBusId bus);

/** Per-cause split of errorCount(), summed over every non-null driver. */
struct CanErrorSplit {
    uint32_t rx_missed_count;  // frames dropped on a full controller RX queue
    uint32_t bus_error_count;  // bus errors seen on the wire
    uint32_t tx_failed_count;  // frames that failed to transmit
};
CanErrorSplit can_error_split(CanDriver **buses, uint8_t count);

/** Quiesce every non-null driver (shutdown()) right before ESP.restart() or
 *  deep sleep, so no controller is still transmitting or ACKing when the chip
 *  resets or powers down. */
void can_shutdown_all(CanDriver **buses, uint8_t count);
