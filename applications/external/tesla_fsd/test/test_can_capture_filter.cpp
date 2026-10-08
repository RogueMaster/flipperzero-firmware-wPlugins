#include <cassert>
#include <cstdio>
#include <vector>
#include "can_capture_filter.h"

struct FakeCan : CanDriver {
    unsigned calls = 0;
    std::vector<uint32_t> ids;
    bool begin(bool) override {
        return true;
    }
    bool send(const CanFrame&) override {
        return false;
    }
    bool receive(CanFrame&) override {
        return false;
    }
    uint32_t errorCount() override {
        return 0;
    }
    uint32_t txCount() override {
        return 0;
    }
    uint32_t rxCount() override {
        return 0;
    }
    void setListenOnly(bool) override {
    }
    void setAcceptanceFilters(const uint32_t* list, uint8_t count) override {
        calls++;
        ids.clear();
        for(uint8_t i = 0; i < count; i++)
            ids.push_back(list[i] & 0x7FFu);
    }
};

struct FakeMcp {
    enum RXF {
        RXF0,
        RXF1,
        RXF2,
        RXF3,
        RXF4,
        RXF5
    };
    enum MASK {
        MASK0,
        MASK1
    };
    enum ERROR {
        ERROR_OK,
        ERROR_FAIL
    };
    struct Register {
        bool ext = false;
        uint32_t id = 0;
    };
    Register filters[6], masks[2];
    unsigned calls = 0;
    unsigned fail_at = 0;
    bool listen_only = false;
    ERROR result() {
        return ++calls == fail_at ? ERROR_FAIL : ERROR_OK;
    }
    ERROR setFilterMask(MASK n, bool ext, uint32_t id) {
        masks[n] = {ext, id};
        return result();
    }
    ERROR setFilter(RXF n, bool ext, uint32_t id) {
        filters[n] = {ext, id};
        return result();
    }
    ERROR setListenOnlyMode() {
        listen_only = true;
        return result();
    }
    ERROR setNormalMode() {
        listen_only = false;
        return result();
    }
    bool accepts(uint32_t id, bool ext) const {
        for(unsigned i = 0; i < 6; i++) {
            if(filters[i].ext == ext && ((id ^ filters[i].id) & masks[i < 2 ? 0 : 1].id) == 0)
                return true;
        }
        return false;
    }
};

static void test_twai() {
    auto all = can_capture_twai_prefilter(nullptr, 0);
    assert(all.code == 0 && all.mask == 0xFFFFFFFFu);
    for(uint32_t id = 0; id < 2048; id++) {
        auto f = can_capture_twai_prefilter(&id, 1);
        assert(f.code == id << 21 && f.mask == 0x001FFFFFu);
    }
    const std::vector<std::vector<uint32_t>> lists = {
        {0x399, 0x3FD},
        {0, 0x7FF},
        {0x398, 0x399, 0x39B},
        {0x229, 0x3F5, 0x132, 0x292, 0x312, 0x318},
        {0x399, 0x399}};
    for(const auto& ids : lists) {
        auto f = can_capture_twai_prefilter(ids.data(), ids.size());
        for(auto id : ids)
            assert(((id << 21) & ~f.mask) == f.code);
        assert((f.mask & 0x001FFFFFu) == 0x001FFFFFu);
    }
    uint32_t pair[] = {0x399, 0x3FD};
    auto f = can_capture_twai_prefilter(pair, 2);
    unsigned accepted = 0;
    for(uint32_t id = 0; id < 2048; id++) {
        accepted += ((id << 21) & ~f.mask) == f.code;
    }
    assert(accepted == 8); // prefilter admits a superset, not an exact whitelist
}

static void test_mcp() {
    FakeMcp mcp;
    uint32_t ids[] = {0x399, 0x3FD, 0x318, 0x257, 0x229, 0x132};
    for(uint8_t count : {1, 6}) {
        mcp.calls = 0;
        assert(can_capture_program_mcp_filters(mcp, ids, count, true));
        assert(mcp.calls == 9 && mcp.listen_only);
        for(unsigned i = 0; i < 2; i++) {
            assert(!mcp.masks[i].ext && mcp.masks[i].id == 0x7FF);
        }
        for(unsigned i = 0; i < 6; i++) {
            assert(!mcp.filters[i].ext);
            assert(mcp.filters[i].id == ids[i < count ? i : 0]);
        }
        for(uint32_t id = 0; id < 2048; id++) {
            bool requested = false;
            for(uint8_t i = 0; i < count; i++)
                requested |= ids[i] == id;
            assert(mcp.accepts(id, false) == requested);
            assert(!mcp.accepts(id, true));
        }
        for(bool listen : {false, true}) {
            mcp.calls = 0;
            assert(can_capture_program_mcp_filters(mcp, nullptr, 0, listen));
            assert(mcp.calls == 9 && mcp.listen_only == listen);
            for(unsigned i = 0; i < 2; i++) {
                assert(mcp.masks[i].ext && mcp.masks[i].id == 0);
            }
            for(unsigned i = 0; i < 6; i++) {
                assert(mcp.filters[i].ext == (i == 1) && mcp.filters[i].id == 0);
            }
            for(uint32_t id : {0u, 0x399u, 0x7FFu})
                assert(mcp.accepts(id, false));
            for(uint32_t id : {0u, 0x399u, 0x18DAF110u, 0x1FFFFFFFu}) {
                assert(mcp.accepts(id, true));
            }
        }
    }
    // A failed mask/filter write still attempts all writes and restores run mode.
    for(unsigned failure = 1; failure <= 9; failure++) {
        mcp.calls = 0;
        mcp.fail_at = failure;
        assert(!can_capture_program_mcp_filters(mcp, ids, 6, true));
        assert(mcp.calls == 9);
    }
}

static void test_sync() {
    FakeCan primary, secondary;
    CanDriver* drivers[] = {&primary, &secondary};
    bool ready[] = {true, true};
    CanCaptureFilterSync sync;
    uint32_t ids[] = {0x399, 0x3FD, 0x318, 0x257, 0x229, 0x132, 0x312};
    auto tick = [&](OpMode mode,
                    bool active,
                    uint8_t count,
                    bool scoped = false,
                    CanBusId bus = CAN_BUS_SECONDARY) {
        sync.update(drivers, ready, 2, mode, active, ids, count, scoped, bus);
    };
    tick(OpMode_ListenOnly, false, 0);
    assert(primary.calls == 1 && secondary.calls == 1);
    tick(OpMode_ListenOnly, false, 0);
    assert(primary.calls == 1 && secondary.calls == 1);
    tick(OpMode_ListenOnly, true, 6);
    assert(primary.ids.size() == 6 && secondary.ids == primary.ids);
    unsigned pc = primary.calls, sc = secondary.calls;
    for(unsigned i = 0; i < 100; i++)
        tick(OpMode_ListenOnly, true, 6);
    assert(primary.calls == pc && secondary.calls == sc);
    tick(OpMode_ListenOnly, true, 6, true);
    assert(primary.ids.empty() && secondary.ids.size() == 6);
    ids[0] = 0x370;
    tick(OpMode_ListenOnly, true, 6, true);
    assert(secondary.ids[0] == 0x370);
    tick(OpMode_ListenOnly, true, 6, true, CAN_BUS_PRIMARY);
    assert(primary.ids.size() == 6 && secondary.ids.empty());
    tick(OpMode_ListenOnly, true, 7);
    assert(primary.ids.empty() && secondary.ids.empty());
    tick(OpMode_ListenOnly, true, 1);
    assert(primary.ids.size() == 1 && secondary.ids.size() == 1);
    tick(OpMode_Active, true, 1);
    assert(primary.ids.empty() && secondary.ids.empty());
    tick(OpMode_ListenOnly, true, 1);
    tick(OpMode_Service, true, 1);
    assert(primary.ids.empty() && secondary.ids.empty());
    tick(OpMode_ListenOnly, true, 1);
    ready[1] = false;
    tick(OpMode_ListenOnly, true, 1);
    sc = secondary.calls;
    secondary.ids.clear(); // reconnect resets hardware filters
    ready[1] = true;
    tick(OpMode_ListenOnly, true, 1);
    assert(secondary.calls == sc + 1 && secondary.ids.size() == 1);
    tick(OpMode_ListenOnly, false, 1);
    assert(primary.ids.empty() && secondary.ids.empty());
    tick(OpMode_ListenOnly, true, 0);
    assert(primary.ids.empty() && secondary.ids.empty());
    // Single-controller builds never touch a nonexistent secondary controller.
    CanCaptureFilterSync single;
    single.update(drivers, ready, 1, OpMode_ListenOnly, true, ids, 1, false, CAN_BUS_PRIMARY);
    assert(primary.ids.size() == 1);
    pc = primary.calls;
    single.update(drivers, ready, 1, OpMode_ListenOnly, true, ids, 1, false, CAN_BUS_PRIMARY);
    assert(primary.calls == pc);
}

static void test_labels() {
    const char* modes[] = {"prefilter", "exact"};
    auto label = [&](uint8_t count,
                     uint8_t buses = 2,
                     bool scoped = false,
                     CanBusId bus = CAN_BUS_SECONDARY) {
        return can_capture_mode_label(count, buses, scoped, bus, modes);
    };
    assert(strcmp(label(2), "multi-id-hwprefilter") == 0);
    assert(strcmp(label(2, 2, true), "multi-id-hwfilter") == 0);
    modes[0] = "exact";
    assert(strcmp(label(1), "single-id-hwfilter") == 0);
    modes[0] = modes[1] = "accept-all";
    assert(strcmp(label(1), "software-filter") == 0); // Active single-ID capture
    assert(strcmp(label(7), "software-filter") == 0);
    assert(strcmp(label(0), "all-id-decimated") == 0);
    modes[0] = "down";
    assert(strcmp(label(1), "unavailable") == 0);
    assert(strcmp(label(1, 2, true), "software-filter") == 0);
    modes[0] = "unknown";
    assert(strcmp(label(1), "unknown") == 0);
    modes[0] = "exact";
    modes[1] = "down";
    assert(strcmp(label(1, 1), "single-id-hwfilter") == 0);
    assert(strcmp(label(1, 1, true), "unavailable") == 0);
}

int main() {
    test_twai();
    test_mcp();
    test_sync();
    test_labels();
    puts("CAN capture tests passed: TWAI math, MCP2515 restore, synchronization, labels");
}
