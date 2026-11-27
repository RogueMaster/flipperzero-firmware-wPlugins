#include <cassert>
#include <cstdio>
#include <vector>
#include "can_mcp2518fd.h"

enum Format {
    Standard,
    Extended
};

// Records what the driver loads, and answers acceptance the way the chip's
// filter/mask pairs do for what ACAN2517FDFilters builds from those calls.
struct FakeFilters {
    struct Entry {
        bool pass_all;
        Format format;
        uint32_t id;
    };
    std::vector<Entry> entries;
    void appendPassAllFilter(void*) {
        entries.push_back({true, Standard, 0});
    }
    void appendFrameFilter(Format f, uint32_t id, void*) {
        entries.push_back({false, f, id});
    }
    bool accepts(uint32_t id, bool ext) const {
        for(const auto& e : entries) {
            if(e.pass_all) return true;
            if((e.format == Extended) == ext && e.id == id) return true;
        }
        return false;
    }
};

// Same fields and enum as the library's CANFDMessage, including its default
// type (CAN FD with bit rate switching).
struct FakeMsg {
    enum Type {
        CAN_REMOTE,
        CAN_DATA,
        CANFD_NO_BIT_RATE_SWITCH,
        CANFD_WITH_BIT_RATE_SWITCH
    };
    uint32_t id = 0;
    bool ext = false;
    Type type = CANFD_WITH_BIT_RATE_SWITCH;
    uint8_t idx = 7;
    uint8_t len = 0;
    uint8_t data[64] = {};
};

static void test_osc() {
    // 20 ms at 40 and 20 MHz with the 1/1024 prescaler.
    assert(mcp2518fd_sysclk_hz(781, 20000) == 39987200u);
    assert(mcp2518fd_sysclk_hz(390, 20000) == 19968000u);
    assert(mcp2518fd_osc_matches(mcp2518fd_sysclk_hz(781, 20000), 40000000u));
    assert(mcp2518fd_osc_matches(mcp2518fd_sysclk_hz(390, 20000), 20000000u));
    // Either crystal is refused when the build expects the other.
    assert(!mcp2518fd_osc_matches(mcp2518fd_sysclk_hz(390, 20000), 40000000u));
    assert(!mcp2518fd_osc_matches(mcp2518fd_sysclk_hz(781, 20000), 20000000u));
    // A stopped counter, no time, or a wild value never passes.
    assert(mcp2518fd_sysclk_hz(0, 20000) == 0 && !mcp2518fd_osc_matches(0, 40000000u));
    assert(mcp2518fd_sysclk_hz(781, 0) == 0);
    assert(mcp2518fd_sysclk_hz(0xFFFFFFFFu, 1) == 0xFFFFFFFFu);
    assert(!mcp2518fd_osc_matches(0xFFFFFFFFu, 40000000u));
    assert(!mcp2518fd_osc_matches(40000000u, 0));
    // 2 % window.
    assert(
        mcp2518fd_osc_matches(40800000u, 40000000u) &&
        !mcp2518fd_osc_matches(40800001u, 40000000u));
    assert(
        mcp2518fd_osc_matches(39200000u, 40000000u) &&
        !mcp2518fd_osc_matches(39199999u, 40000000u));
    // Torn TBC reads are off by a carry; neighbours, including across a wrap, pass.
    assert(mcp2518fd_tbc_steady(0x1FF, 0x200) && mcp2518fd_tbc_steady(0xFFFFFFFFu, 2));
    assert(!mcp2518fd_tbc_steady(0x2FF, 0x200) && !mcp2518fd_tbc_steady(0x1FF, 0x2FF));
    assert(!mcp2518fd_tbc_steady(0x201, 0x200));
}

static void test_bus_errors() {
    assert(mcp2518fd_bdiag0_errors(0) == 0);
    assert(mcp2518fd_bdiag0_errors(0x00000305u) == 8);
    assert(mcp2518fd_bdiag0_errors(0x01020304u) == 10);
    assert(mcp2518fd_bdiag0_errors(0xFFFFFFFFu) == 4 * 255);
}

static void test_readback() {
    const uint32_t con_reset = 0x04980760u; // CiCON after RESET: Configuration mode
    const uint32_t osc_reset = 0x00000060u; // OSC after RESET, before OSCRDY
    const uint32_t osc_up = 0x00000460u; // ... and once the oscillator is stable
    assert(mcp2518fd_opmod(con_reset) == MCP2518FD_OPMOD_CONFIG);
    assert(mcp2518fd_opmod(0x0) == 0 && mcp2518fd_opmod(0xFFFFFFFFu) == 7);
    // CiCON byte 3 as the library requests Configuration mode.
    assert(MCP2518FD_CON_REQ_CONFIG == 0x0C);

    // The start-up wait: OSCRDY in a word that can be OSC. A MISO floating
    // high has OSCRDY set too and must not end the wait.
    assert(mcp2518fd_osc_ready(osc_up) && mcp2518fd_osc_ready(0x00001460u));
    assert(!mcp2518fd_osc_ready(osc_reset) && !mcp2518fd_osc_ready(0));
    assert(!mcp2518fd_osc_ready(0xFFFFFFFFu) && !mcp2518fd_osc_ready(0x00010460u));

    // Nothing on MISO: all ones or all zeros, in any mix.
    for(uint32_t con : {0u, 0xFFFFFFFFu}) {
        for(uint32_t osc : {0u, 0xFFFFFFFFu}) {
            assert(mcp2518fd_spi_state(con, osc) == MCP2518FD_SPI_SILENT);
        }
    }
    // Something answers, but OSC has unimplemented bits set or CiCON is stuck.
    assert(mcp2518fd_spi_state(0x12345678u, 0xABCD0460u) == MCP2518FD_SPI_GARBLED);
    assert(mcp2518fd_spi_state(con_reset, 0xFFFFFFFFu) == MCP2518FD_SPI_GARBLED);
    assert(mcp2518fd_spi_state(0xFFFFFFFFu, osc_up) == MCP2518FD_SPI_GARBLED);
    assert(mcp2518fd_spi_state(0, osc_reset) == MCP2518FD_SPI_GARBLED);
    // The chip answers: OSCRDY decides. CiCON in any mode counts.
    assert(mcp2518fd_spi_state(con_reset, osc_reset) == MCP2518FD_SPI_NO_CLOCK);
    assert(mcp2518fd_spi_state(con_reset, 0) == MCP2518FD_SPI_NO_CLOCK);
    assert(mcp2518fd_spi_state(con_reset, 0x00000064u) == MCP2518FD_SPI_NO_CLOCK); // OSCDIS: sleep
    assert(mcp2518fd_spi_state(con_reset, osc_up) == MCP2518FD_SPI_CLOCK_OK);
    assert(
        mcp2518fd_spi_state(0x03780760u, 0x00001470u) ==
        MCP2518FD_SPI_CLOCK_OK); // Listen-Only, SYSCLK/2

    // An MCP2515 is asked for only when no MCP2518FD answered.
    assert(
        mcp2518fd_probe_mcp2515(MCP2518FD_SPI_SILENT) &&
        mcp2518fd_probe_mcp2515(MCP2518FD_SPI_GARBLED));
    assert(
        !mcp2518fd_probe_mcp2515(MCP2518FD_SPI_NO_CLOCK) &&
        !mcp2518fd_probe_mcp2515(MCP2518FD_SPI_CLOCK_OK));

    // MCP2515 after RESET: CANSTAT 0x80, CANCTRL 0x87 (REQOP reads 100 or 111).
    assert(mcp2515_reset_signature(0x80, 0x87) && mcp2515_reset_signature(0x80, 0xE7));
    assert(mcp2515_reset_signature(0x8E, 0x8F)); // ICOD and OSM ignored
    assert(!mcp2515_reset_signature(0xFF, 0xFF) && !mcp2515_reset_signature(0, 0));
    assert(!mcp2515_reset_signature(0x80, 0xFF) && !mcp2515_reset_signature(0x00, 0x87));
    assert(!mcp2515_reset_signature(0x60, 0x67)); // listen-only: not just reset
    assert(!mcp2515_reset_signature(0x80, 0x83) && !mcp2515_reset_signature(0x80, 0x97));

    // One text per case, the MCP2515 one ahead of the rest.
    const Mcp2518fdReadback cases[] = {
        {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, false},
        {0x12345678u, 0xABCD0460u, 0, false},
        {con_reset, osc_reset, 0, false},
        {con_reset, osc_up, 0x14, false},
        {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, true},
    };
    const size_t n = sizeof(cases) / sizeof(cases[0]);
    for(size_t i = 0; i < n; i++) {
        const char* text = mcp2518fd_readback_text(cases[i]);
        assert(text && text[0]);
        for(size_t j = 0; j < i; j++)
            assert(strcmp(text, mcp2518fd_readback_text(cases[j])) != 0);
    }
    assert(strstr(mcp2518fd_readback_text(cases[4]), "lilygo-t2can"));
    assert(strstr(mcp2518fd_readback_text(cases[0]), "nothing answers"));
    assert(strstr(mcp2518fd_readback_text(cases[2]), "oscillator is not running"));
    assert(strstr(mcp2518fd_readback_text(cases[3]), "start sequence failed"));

    // A retry prints the long line again only when something changed.
    Mcp2518fdReadback a = cases[0], b = cases[0];
    assert(mcp2518fd_readback_same(a, b));
    b.mcp2515 = true;
    assert(!mcp2518fd_readback_same(a, b));
    b = a;
    b.devid = 0;
    assert(!mcp2518fd_readback_same(a, b));
    b = a;
    b.osc = osc_up;
    assert(!mcp2518fd_readback_same(a, b));
    b = a;
    b.con = con_reset;
    assert(!mcp2518fd_readback_same(a, b));

    // The retry skips the oscillator wait only after a silent readback, and
    // only while CiCON and OSC read back the same.
    const Mcp2518fdReadback silent0 = {0, 0, 0, false};
    const Mcp2518fdReadback silent1 = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, true};
    assert(!mcp2518fd_retry_skips_osc_wait(nullptr, 0, 0)); // first attempt: full
    assert(mcp2518fd_retry_skips_osc_wait(&silent0, 0, 0));
    assert(mcp2518fd_retry_skips_osc_wait(&silent1, 0xFFFFFFFFu, 0xFFFFFFFFu));
    assert(!mcp2518fd_retry_skips_osc_wait(&silent0, 0xFFFFFFFFu, 0)); // MISO changed
    assert(!mcp2518fd_retry_skips_osc_wait(&silent0, con_reset, osc_reset)); // chip now answers
    assert(!mcp2518fd_retry_skips_osc_wait(&silent0, con_reset, 0));
    const Mcp2518fdReadback no_clock = {con_reset, osc_reset, 0, false};
    assert(!mcp2518fd_retry_skips_osc_wait(&no_clock, con_reset, osc_reset)); // not silent
    const Mcp2518fdReadback garbled = {0x12345678u, 0xABCD0460u, 0, false};
    assert(!mcp2518fd_retry_skips_osc_wait(&garbled, 0x12345678u, 0xABCD0460u));
}

static void test_timing() {
    // What the library picks for 500 kbit/s from a 20 MHz SYSCLK (40 MHz
    // crystal divided by two, or a 20 MHz crystal) and from 40 MHz.
    assert(mcp2518fd_timing_ok(20000000u, 1, 31, 8, 8));
    assert(mcp2518fd_timing_ok(40000000u, 2, 31, 8, 8));
    // The same register values on the wrong clock are the wrong bit rate.
    assert(!mcp2518fd_timing_ok(40000000u, 1, 31, 8, 8));
    assert(!mcp2518fd_timing_ok(10000000u, 1, 31, 8, 8));
    assert(!mcp2518fd_timing_ok(20000000u, 1, 32, 8, 8));
    // Sample point limits: 75 % and 87.5 % pass, outside does not.
    assert(mcp2518fd_timing_ok(20000000u, 1, 29, 10, 4));
    assert(mcp2518fd_timing_ok(20000000u, 1, 34, 5, 4));
    assert(!mcp2518fd_timing_ok(20000000u, 1, 28, 11, 4));
    assert(!mcp2518fd_timing_ok(20000000u, 1, 35, 4, 4));
    // Field ranges and SJW.
    assert(!mcp2518fd_timing_ok(20000000u, 0, 31, 8, 8));
    assert(!mcp2518fd_timing_ok(20000000u, 1, 31, 8, 9));
    assert(!mcp2518fd_timing_ok(20000000u, 1, 31, 8, 0));
    assert(!mcp2518fd_timing_ok(0, 1, 31, 8, 8));
}

static void test_filters() {
    uint32_t ids[] = {0x399, 0x3FD, 0x318, 0x257, 0x229, 0x132, 0x312};
    for(uint8_t count : {1, 6}) {
        FakeFilters f;
        mcp2518fd_capture_filters(f, Standard, ids, count);
        assert(f.entries.size() == count);
        for(uint32_t id = 0; id < 2048; id++) {
            bool requested = false;
            for(uint8_t i = 0; i < count; i++)
                requested |= ids[i] == id;
            assert(f.accepts(id, false) == requested);
            assert(!f.accepts(id, true));
        }
    }
    // Over six ids is capped like the other drivers; bits above 11 are dropped.
    FakeFilters capped;
    mcp2518fd_capture_filters(capped, Standard, ids, 7);
    assert(capped.entries.size() == CAN_CAPTURE_MAX_IDS && !capped.accepts(0x312, false));
    uint32_t wide[] = {0x8399};
    FakeFilters masked;
    mcp2518fd_capture_filters(masked, Standard, wide, 1);
    assert(masked.accepts(0x399, false) && !masked.accepts(0x8399 & 0x1FFFFFFFu, true));
    // Accept-all: one pass-all filter, standard and extended frames.
    for(const uint32_t* list : {(const uint32_t*)nullptr, (const uint32_t*)ids}) {
        FakeFilters all;
        mcp2518fd_capture_filters(all, Standard, list, list ? 0 : 3);
        assert(all.entries.size() == 1 && all.entries[0].pass_all);
        for(uint32_t id : {0u, 0x399u, 0x7FFu})
            assert(all.accepts(id, false));
        for(uint32_t id : {0u, 0x18DAF110u, 0x1FFFFFFFu})
            assert(all.accepts(id, true));
    }
}

static void test_rx() {
    FakeMsg m;
    CanFrame f;
    m.type = FakeMsg::CAN_DATA;
    m.id = 0x3FD;
    m.len = 8;
    for(uint8_t i = 0; i < 64; i++)
        m.data[i] = (uint8_t)(0xA0 + i);
    assert(mcp2518fd_rx_frame(m, f));
    assert(f.id == 0x3FD && !f.ext && !f.req && f.dlc == 8);
    for(uint8_t i = 0; i < 8; i++)
        assert(f.data[i] == 0xA0 + i);
    // Short frame: the bytes past the DLC are zero.
    m.len = 3;
    assert(mcp2518fd_rx_frame(m, f) && f.dlc == 3 && f.data[2] == 0xA2 && f.data[3] == 0);
    // Standard ids are masked to 11 bits, extended ones to 29.
    m.id = 0xFFFFF399u;
    assert(mcp2518fd_rx_frame(m, f) && f.id == 0x399);
    m.ext = true;
    m.id = 0xF8DAF110u;
    assert(mcp2518fd_rx_frame(m, f) && f.ext && f.id == 0x18DAF110u);
    // Remote frame: passed up flagged, no payload.
    m.ext = false;
    m.id = 0x229;
    m.type = FakeMsg::CAN_REMOTE;
    m.len = 4;
    assert(mcp2518fd_rx_frame(m, f) && f.req && f.dlc == 4);
    for(uint8_t i = 0; i < 8; i++)
        assert(f.data[i] == 0);
    // CAN FD of any length, and classic DLC 9..15, are refused.
    for(FakeMsg::Type t :
        {FakeMsg::CANFD_NO_BIT_RATE_SWITCH, FakeMsg::CANFD_WITH_BIT_RATE_SWITCH}) {
        for(uint8_t len : {0, 8, 12, 64}) {
            m.type = t;
            m.len = len;
            assert(!mcp2518fd_rx_frame(m, f));
        }
    }
    for(uint8_t len : {12, 16, 20, 24, 32, 48, 64}) {
        m.type = FakeMsg::CAN_DATA;
        m.len = len;
        assert(!mcp2518fd_rx_frame(m, f));
        m.type = FakeMsg::CAN_REMOTE;
        assert(!mcp2518fd_rx_frame(m, f));
    }
}

static void test_tx() {
    CanFrame f = {};
    f.id = 0x370;
    f.dlc = 8;
    for(uint8_t i = 0; i < 8; i++)
        f.data[i] = (uint8_t)(0x10 + i);
    FakeMsg m;
    assert(mcp2518fd_tx_message(f, m));
    assert(m.type == FakeMsg::CAN_DATA && !m.ext && m.idx == 0 && m.id == 0x370 && m.len == 8);
    for(uint8_t i = 0; i < 8; i++)
        assert(m.data[i] == 0x10 + i);
    // ext/req on the source frame never turn into an extended or remote message.
    f.ext = 1;
    f.req = 1;
    FakeMsg m2;
    assert(mcp2518fd_tx_message(f, m2) && m2.type == FakeMsg::CAN_DATA && !m2.ext);
    // Nothing that is not a standard id with at most 8 bytes is handed over.
    f.ext = f.req = 0;
    f.dlc = 9;
    FakeMsg m3;
    assert(!mcp2518fd_tx_message(f, m3) && m3.type == FakeMsg::CANFD_WITH_BIT_RATE_SWITCH);
    f.dlc = 8;
    f.id = 0x800;
    assert(!mcp2518fd_tx_message(f, m3));
    f.id = 0x7FF;
    f.dlc = 0;
    assert(mcp2518fd_tx_message(f, m3) && m3.len == 0 && m3.type == FakeMsg::CAN_DATA);
}

int main() {
    test_osc();
    test_bus_errors();
    test_readback();
    test_timing();
    test_filters();
    test_rx();
    test_tx();
    puts(
        "MCP2518FD tests passed: crystal check, error counters, not-detected readback, bit timing, "
        "filters, classic-only RX/TX");
}
