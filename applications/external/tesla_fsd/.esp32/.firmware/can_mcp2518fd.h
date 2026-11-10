#pragma once

// The parts of the MCP2518FD driver (LilyGO T-2CAN FD, can1) that need no
// hardware: the crystal check, the bit-timing check, the not-detected
// diagnostic, the capture filters and the classic-CAN-only frame conversion.
// Host-tested by test/test_can_mcp2518fd.cpp; can_driver.cpp holds the SPI
// side.

#include "can_capture_filter.h"
#include "../../fsd_logic/fsd_can_ops.h"  // TESLA_CAN_MAX_DLC / tesla_can_tx_valid
#include <string.h>

// MCP2518FD registers the driver touches itself (DS20006027B, Table 3-2). The
// time base counter is not latched across its four bytes.
static constexpr uint16_t MCP2518FD_REG_TBC    = 0x010;
static constexpr uint16_t MCP2518FD_REG_TSCON  = 0x014;
static constexpr uint16_t MCP2518FD_REG_BDIAG0 = 0x038;
static constexpr uint32_t MCP2518FD_TSCON_TBCEN = 1u << 16;

// The crystal check runs the time base counter at SYSCLK / 1024: one count
// every 25.6 us at 40 MHz, so it moves by about one count during an SPI read.
static constexpr uint32_t MCP2518FD_TBC_PRESCALE = 1024;

// Two reads back to back must be this close: a read that tore across a carry
// between bytes is off by at least 255 counts.
inline bool mcp2518fd_tbc_steady(uint32_t first, uint32_t second) {
    return second - first <= 4u;
}

// SYSCLK in Hz from a TBC delta taken over elapsed_us microseconds.
inline uint32_t mcp2518fd_sysclk_hz(uint32_t ticks, uint32_t elapsed_us) {
    if (elapsed_us == 0) return 0;
    uint64_t hz = (uint64_t)ticks * MCP2518FD_TBC_PRESCALE * 1000000u / elapsed_us;
    return hz > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)hz;
}

// The crystal the build expects, within 2 %. The chip takes 20 or 40 MHz, a
// factor of two apart, and the measurement itself is good to well under 1 %.
inline bool mcp2518fd_osc_matches(uint32_t measured_hz, uint32_t expected_hz) {
    uint32_t diff = measured_hz > expected_hz ? measured_hz - expected_hz
                                              : expected_hz - measured_hz;
    return expected_hz != 0 && diff <= expected_hz / 50u;
}

// CiBDIAG0: four 8-bit error counters (nominal RX/TX, data RX/TX), each
// incremented once per error and cleared by writing zero.
inline uint32_t mcp2518fd_bdiag0_errors(uint32_t bdiag0) {
    return (bdiag0 & 0xFFu) + ((bdiag0 >> 8) & 0xFFu) + ((bdiag0 >> 16) & 0xFFu) + (bdiag0 >> 24);
}

// ── Reset, and what a failed detection read back ────────────────────────────
// Registers the driver reads before the library starts the chip, and when it
// does not answer. CiCON resets to 0x04980760 (Configuration mode); OSC to
// 0x00000060 with OSCRDY (bit 10) set once the oscillator is stable, and its
// bits 13-31 are unimplemented, read as zero. DEVID is printed as read.
static constexpr uint16_t MCP2518FD_REG_CON   = 0x000;
static constexpr uint16_t MCP2518FD_REG_OSC   = 0xE00;
static constexpr uint16_t MCP2518FD_REG_DEVID = 0xE14;
static constexpr uint32_t MCP2518FD_OSC_OSCRDY        = 1u << 10;
static constexpr uint32_t MCP2518FD_OSC_UNIMPLEMENTED = 0xFFFFE000u;
// CiCON byte 3: REQOP = Configuration, ABAT set (abort pending transmissions).
static constexpr uint8_t  MCP2518FD_CON_REQ_CONFIG = 0x04 | 0x08;
static constexpr uint32_t MCP2518FD_OPMOD_CONFIG   = 4;

inline uint32_t mcp2518fd_opmod(uint32_t con) { return (con >> 21) & 0x7u; }

// A word a disconnected MISO reads: held low or floating high.
inline bool mcp2518fd_word_stuck(uint32_t v) { return v == 0 || v == 0xFFFFFFFFu; }

// The oscillator is up: OSCRDY set in a word that can be OSC at all, so a
// MISO floating high does not pass.
inline bool mcp2518fd_osc_ready(uint32_t osc) {
    return (osc & MCP2518FD_OSC_UNIMPLEMENTED) == 0 && (osc & MCP2518FD_OSC_OSCRDY) != 0;
}

enum Mcp2518fdSpiState : uint8_t {
    MCP2518FD_SPI_SILENT,     // CiCON and OSC stuck at all zeros or all ones
    MCP2518FD_SPI_GARBLED,    // something drives MISO, not with these registers
    MCP2518FD_SPI_NO_CLOCK,   // the chip answers, OSCRDY clear
    MCP2518FD_SPI_CLOCK_OK,   // the chip answers, OSCRDY set
};

inline Mcp2518fdSpiState mcp2518fd_spi_state(uint32_t con, uint32_t osc) {
    if (mcp2518fd_word_stuck(con) && mcp2518fd_word_stuck(osc)) return MCP2518FD_SPI_SILENT;
    if (mcp2518fd_word_stuck(con) || (osc & MCP2518FD_OSC_UNIMPLEMENTED) != 0) {
        return MCP2518FD_SPI_GARBLED;
    }
    return (osc & MCP2518FD_OSC_OSCRDY) ? MCP2518FD_SPI_CLOCK_OK : MCP2518FD_SPI_NO_CLOCK;
}

// Only when no MCP2518FD answered is it worth asking for an MCP2515.
inline bool mcp2518fd_probe_mcp2515(Mcp2518fdSpiState state) {
    return state == MCP2518FD_SPI_SILENT || state == MCP2518FD_SPI_GARBLED;
}

// An MCP2515 right after its RESET, CANSTAT and CANCTRL from one sequential
// READ: OPMOD Configuration, and CANCTRL's ABAT clear, CLKEN set, CLKPRE 1/8.
// CANCTRL's REQOP and OSM are left out, as the Linux mcp251x probe does.
inline bool mcp2515_reset_signature(uint8_t canstat, uint8_t canctrl) {
    return (canstat & 0xE0u) == 0x80u && (canctrl & 0x17u) == 0x07u;
}

struct Mcp2518fdReadback {
    uint32_t con;
    uint32_t osc;
    uint32_t devid;
    bool     mcp2515;  // an MCP2515 answered on the same pins
};

inline bool mcp2518fd_readback_same(const Mcp2518fdReadback &a, const Mcp2518fdReadback &b) {
    return a.con == b.con && a.osc == b.osc && a.devid == b.devid && a.mcp2515 == b.mcp2515;
}

// A retry after a silent readback reads CiCON and OSC once instead of waiting
// out the oscillator start-up (kOscStartMs): when they read back as they did,
// nothing answers on MISO again and the wait cannot change that. @p prev is
// the last failed attempt's readback, null on the first attempt.
inline bool mcp2518fd_retry_skips_osc_wait(const Mcp2518fdReadback *prev, uint32_t con,
                                           uint32_t osc) {
    return prev && mcp2518fd_spi_state(prev->con, prev->osc) == MCP2518FD_SPI_SILENT &&
           con == prev->con && osc == prev->osc;
}

inline const char *mcp2518fd_readback_text(const Mcp2518fdReadback &rb) {
    if (rb.mcp2515) {
        return "an MCP2515 answers on these pins: this looks like a classic T-2CAN, "
               "flash the lilygo-t2can image";
    }
    switch (mcp2518fd_spi_state(rb.con, rb.osc)) {
    case MCP2518FD_SPI_SILENT:
        return "nothing answers on MISO: check wiring and power, and that this is a T-2CAN FD";
    case MCP2518FD_SPI_GARBLED:
        return "MISO answers, but not with MCP2518FD registers: check the SPI wiring";
    case MCP2518FD_SPI_NO_CLOCK:
        return "the chip answers, its oscillator is not running: check the crystal";
    case MCP2518FD_SPI_CLOCK_OK:
    default:
        return "the chip answers and its oscillator runs: the start sequence failed";
    }
}

// Nominal bit timing the driver accepts from the library before it starts the
// controller: exactly 500 kbit/s and a sample point between 75 and 87.5 %.
// tseg1/tseg2/sjw in time quanta (CiNBTCFG field + 1); one quantum of sync.
inline bool mcp2518fd_timing_ok(uint32_t sysclk_hz, uint32_t brp, uint32_t tseg1,
                                uint32_t tseg2, uint32_t sjw) {
    if (brp < 1 || brp > 256 || tseg1 < 2 || tseg1 > 256 || tseg2 < 1 || tseg2 > 128) return false;
    if (sjw < 1 || sjw > tseg2) return false;
    uint32_t tq = 1 + tseg1 + tseg2;
    if ((uint64_t)brp * tq * 500000u != sysclk_hz) return false;
    uint32_t sample_permille = (1 + tseg1) * 1000u / tq;
    return sample_permille >= 750 && sample_permille <= 875;
}

// Capture filters: one exact filter per standard id, or a single pass-all
// filter (standard and extended) when count is zero, matching the MCP2515
// restore. Filters only load in Configuration mode, so the caller restarts
// the controller with them.
template <typename Filters, typename Format>
void mcp2518fd_capture_filters(Filters &filters, Format standard,
                               const uint32_t *ids, uint8_t count) {
    if (!ids) count = 0;
    if (count > CAN_CAPTURE_MAX_IDS) count = CAN_CAPTURE_MAX_IDS;
    if (count == 0) {
        filters.appendPassAllFilter(nullptr);
        return;
    }
    for (uint8_t i = 0; i < count; i++) {
        filters.appendFrameFilter(standard, ids[i] & 0x7FFu, nullptr);
    }
}

// RX ID whitelist (fsd_rx_whitelist.h): one exact filter per standard id, up to
// the MCP2518FD's 32 filter objects. Each filter uses an exact 0x7FF mask, so
// the installed set is an EXACT SUPERSET (equal to) the requested ids — never a
// subset. The caller rejects a set larger than MCP2518FD_RXW_MAX_IDS before
// calling, so the library's own "> 32 filters" error is never reached.
static constexpr size_t MCP2518FD_RXW_MAX_IDS = 32;

template <typename Filters, typename Format>
void mcp2518fd_whitelist_filters(Filters &filters, Format standard,
                                 const uint32_t *ids, size_t count) {
    if (!ids || count == 0) {
        filters.appendPassAllFilter(nullptr);
        return;
    }
    if (count > MCP2518FD_RXW_MAX_IDS) count = MCP2518FD_RXW_MAX_IDS;
    for (size_t i = 0; i < count; i++) {
        filters.appendFrameFilter(standard, ids[i] & 0x7FFu, nullptr);
    }
}

// A received message as CanFrame, classic CAN only. CAN FD frames (the chip
// takes them in Listen-Only) and classic frames with DLC 9..15 (the library
// reports those as 12..64 bytes) return false and are counted as rejected.
// Remote frames carry no payload, so their data stays zero.
template <typename Msg>
bool mcp2518fd_rx_frame(const Msg &msg, CanFrame &frame) {
    bool remote = msg.type == Msg::CAN_REMOTE;
    if (!remote && msg.type != Msg::CAN_DATA) return false;
    if (msg.len > TESLA_CAN_MAX_DLC) return false;
    memset(&frame, 0, sizeof(frame));
    frame.id  = msg.id & (msg.ext ? 0x1FFFFFFFu : 0x7FFu);
    frame.ext = msg.ext ? 1u : 0u;
    frame.req = remote ? 1u : 0u;
    frame.dlc = msg.len;
    if (!remote) memcpy(frame.data, msg.data, frame.dlc);
    return true;
}

// The only message the driver hands to the controller: a classic data frame
// with a standard id and at most 8 bytes, never CAN FD (the library's default
// message type is CAN FD with bit rate switching).
template <typename Msg>
bool mcp2518fd_tx_message(const CanFrame &frame, Msg &msg) {
    if (!tesla_can_tx_valid(frame.id, frame.dlc)) return false;
    msg.id   = frame.id;
    msg.ext  = false;
    msg.type = Msg::CAN_DATA;
    msg.idx  = 0;  // transmit FIFO, not the TXQ
    msg.len  = frame.dlc;
    memcpy(msg.data, frame.data, frame.dlc);
    return true;
}
