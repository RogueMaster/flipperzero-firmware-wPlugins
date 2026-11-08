// Host regression tests for CAN Commander firmware logic.
// Build and run: test/host/run.sh

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "core/ParseUtils.h"
#include "decode/DbcSignalStore.h"
#include "decode/SignalDecode.h"
#include "fake_host.h"
#include "tools/CustomInjectTool.h"
#include "io/CliShell.h"
#include "io/FlipperProtocol.h"
#include "tools/DbcDecodeTool.h"
#include "tools/UniqueIdsTool.h"
#include "tools/ValueTrackerTool.h"
#include "tools/BitTrackerTool.h"
#include "tools/ToolManager.h"
#include "tools/ReadAllTool.h"
#include "tools/ObdPidTool.h"
#include "tools/ReplayTool.h"
#include "tools/ReverseTool.h"
#include "tools/WriteTool.h"

static int g_failures = 0;
static int g_checks = 0;
static const char *g_test = "";

#define CHECK(cond)                                                                \
  do {                                                                             \
    g_checks++;                                                                    \
    if (!(cond)) {                                                                 \
      g_failures++;                                                                \
      printf("  FAIL [%s] %s:%d: %s\n", g_test, __FILE__, __LINE__, #cond);       \
    }                                                                              \
  } while (0)

#define CHECK_EQ(a, b)                                                             \
  do {                                                                             \
    g_checks++;                                                                    \
    const auto _va = (a);                                                          \
    const auto _vb = (b);                                                          \
    if (!(_va == _vb)) {                                                           \
      g_failures++;                                                                \
      printf("  FAIL [%s] %s:%d: %s == %s (got %lld vs %lld)\n", g_test, __FILE__, \
             __LINE__, #a, #b, (long long)_va, (long long)_vb);                    \
    }                                                                              \
  } while (0)

#define TEST(name) static void name()
#define RUN(name)      \
  do {                 \
    g_test = #name;    \
    name();            \
  } while (0)

// ---------------------------------------------------------------- parsing (F18)

TEST(parse_numbers_are_strict) {
  using namespace core::parse;
  uint32_t v = 0;
  CHECK(parseU32("id=7FF", "id", v, 16) && v == 0x7FF);
  CHECK(parseU32("id=0x7FF", "id", v, 16) && v == 0x7FF);
  CHECK(!parseU32("id=7FFtypo", "id", v, 16));
  CHECK(!parseU32("count=1oops", "count", v, 10));
  CHECK(!parseU32("count=-1", "count", v, 10));
  CHECK(!parseU32("count=4294967296", "count", v, 10));
  CHECK(!parseU32("count=", "count", v, 10));
  CHECK(parseU32("count=4294967295", "count", v, 10) && v == 4294967295UL);

  float f = 0;
  CHECK(parseFloat("factor=0.25", "factor", f) && f == 0.25f);
  CHECK(!parseFloat("factor=0.25x", "factor", f));
  CHECK(!parseFloat("factor=nan", "factor", f));

  bool b = false;
  CHECK(parseBool01("ext=1", "ext", b) && b);
  CHECK(parseBool01("ext=0", "ext", b) && !b);
  CHECK(!parseBool01("ext=2", "ext", b));
  CHECK(!parseBool01("ext=yes", "ext", b));
}

TEST(parse_optional_distinguishes_absent_from_invalid) {
  using namespace core::parse;
  uint32_t v = 42;
  CHECK(parseOptU32("other=1", "count", v, 10) && v == 42);
  CHECK(parseOptU32("count=7", "count", v, 10) && v == 7);
  CHECK(!parseOptU32("count=7x", "count", v, 10));

  core::BusId bus = core::BusId::CAN0;
  CHECK(parseOptBus("id=1", "bus", bus) && bus == core::BusId::CAN0);
  CHECK(!parseOptBus("bus=can2", "bus", bus));
}

TEST(parse_strings_never_truncate) {
  using namespace core::parse;
  char ssid[33];
  CHECK(!parseString("ssid=ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", "ssid", ssid, sizeof(ssid)));
  CHECK(parseString("ssid=Lab", "ssid", ssid, sizeof(ssid)) && strcmp(ssid, "Lab") == 0);
  char name[8];
  CHECK(parseStringTruncate("name=throttle_override", "name", name, sizeof(name)) && strcmp(name, "throttl") == 0);
}

TEST(parse_hex_rejects_partial_digits) {
  using namespace core::parse;
  uint8_t out[8] = {0};
  uint8_t n = 0;
  CHECK(parseHexBytes("A1b2", out, 8, n) && n == 2 && out[0] == 0xA1 && out[1] == 0xB2);
  CHECK(!parseHexBytes("1G", out, 8, n));
  CHECK(!parseHexBytes("+1", out, 8, n));
  CHECK(!parseHexBytes("123", out, 8, n));
  CHECK(!parseHexBytes("112233445566778899", out, 8, n));
}

// ---------------------------------------------------------------- DBC decode (F01-F07)

static core::DbcSignalDef sig(uint16_t sid, uint8_t start, uint8_t len, core::ByteOrder order,
                              core::SignMode sign = core::SignMode::UNSIGNED) {
  core::DbcSignalDef d{};
  d.used = true;
  d.sid = sid;
  d.bus = core::BusId::CAN0;
  d.id = 0x100;
  d.start_bit = start;
  d.bit_len = len;
  d.order = order;
  d.sign = sign;
  d.factor = 1;
  d.offset = 0;
  d.min = 0;
  d.max = 1e9f;
  return d;
}

static int64_t raw(const core::CanFrame &f, const core::DbcSignalDef &d) {
  int64_t r = -12345;
  if (!decode::extractRawSignal(f, d, r)) {
    return -99999;
  }
  return r;
}

TEST(dbc_motorola_matches_dbc_sawtooth_layout) {
  const core::CanFrame f = makeFrame(0x100, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0});
  CHECK_EQ(raw(f, sig(1, 7, 16, core::ByteOrder::MOTOROLA)), 0x1234);
  CHECK_EQ(raw(f, sig(1, 7, 8, core::ByteOrder::MOTOROLA)), 0x12);
  CHECK_EQ(raw(f, sig(1, 23, 16, core::ByteOrder::MOTOROLA)), 0x5678);
  CHECK_EQ(raw(f, sig(1, 7, 4, core::ByteOrder::MOTOROLA)), 0x1);
  CHECK_EQ(raw(f, sig(1, 3, 8, core::ByteOrder::MOTOROLA)), 0x23);  // low nibble b0 + high nibble b1
  CHECK_EQ(raw(f, sig(1, 63, 8, core::ByteOrder::MOTOROLA)), 0xF0);
  CHECK_EQ(raw(f, sig(1, 0, 16, core::ByteOrder::INTEL)), 0x3412);
  // Signed Motorola: 0xF0 in byte 7 as an 8-bit signed value is -16.
  CHECK_EQ(raw(f, sig(1, 63, 8, core::ByteOrder::MOTOROLA, core::SignMode::SIGNED)), -16);
}

TEST(dbc_motorola_mux_uses_signal_byte_order) {
  FakeHost h;
  core::DbcSignalDef d = sig(5, 15, 8, core::ByteOrder::MOTOROLA);
  d.mux_enabled = true;
  d.mux_start_bit = 7;  // high nibble of byte 0, Motorola numbering
  d.mux_bit_len = 4;
  d.mux_value = 0x3;
  CHECK(h.store.add(d));
  tools::DbcDecodeTool t;
  CHECK(t.start("bus=can0", h));
  t.onFrame(makeFrame(0x100, {0x30, 0x42}), h);
  CHECK_EQ(h.decoded.size(), 1u);
  t.onFrame(makeFrame(0x100, {0x20, 0x42}), h);
  CHECK_EQ(h.decoded.size(), 1u);
}

TEST(dbc_bus_both_matches_either_bus) {
  decode::DbcSignalStore store;
  core::DbcSignalDef d = sig(1, 0, 8, core::ByteOrder::INTEL);
  d.bus = core::BusId::BOTH;
  CHECK(store.add(d));
  const core::DbcSignalDef *m[4];
  CHECK_EQ(store.findMatches(makeFrame(0x100, {1}, core::BusId::CAN0), m, 4), 1);
  CHECK_EQ(store.findMatches(makeFrame(0x100, {1}, core::BusId::CAN1), m, 4), 1);
}

TEST(dbc_decodes_beyond_sixteen_mux_variants) {
  FakeHost h;
  for (int i = 0; i < 17; i++) {
    core::DbcSignalDef d = sig((uint16_t)i, 8, 8, core::ByteOrder::INTEL);
    d.mux_enabled = true;
    d.mux_start_bit = 0;
    d.mux_bit_len = 8;
    d.mux_value = (uint64_t)i;
    CHECK(h.store.add(d));
  }
  tools::DbcDecodeTool t;
  CHECK(t.start("bus=can0", h));
  t.onFrame(makeFrame(0x100, {16, 0x55}), h);
  CHECK_EQ(h.decoded.size(), 1u);
  if (!h.decoded.empty()) {
    CHECK_EQ(h.decoded[0].sid, 16);
  }
}

TEST(dbc_unsigned_64_bit_scales_as_unsigned) {
  core::DbcSignalDef d = sig(1, 0, 64, core::ByteOrder::INTEL);
  d.max = 1e30f;
  core::DbcDecodedValue out{};
  CHECK(decode::decodeSignal(makeFrame(0x100, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}), d, out));
  CHECK(out.value > 1.8e19f);
}

TEST(dbc_zero_range_means_unspecified) {
  core::DbcSignalDef d = sig(1, 0, 8, core::ByteOrder::INTEL);
  d.min = 0;
  d.max = 0;
  core::DbcDecodedValue out{};
  CHECK(decode::decodeSignal(makeFrame(0x100, {200}), d, out));
  CHECK(out.in_range);
}

TEST(dbc_rejects_impossible_descriptors_and_rtr) {
  CHECK(decode::DbcSignalStore::isValid(sig(1, 63, 8, core::ByteOrder::MOTOROLA)));
  CHECK(!decode::DbcSignalStore::isValid(sig(1, 60, 8, core::ByteOrder::INTEL)));
  CHECK(!decode::DbcSignalStore::isValid(sig(1, 0, 0, core::ByteOrder::INTEL)));
  core::DbcSignalDef bad_id = sig(1, 0, 8, core::ByteOrder::INTEL);
  bad_id.id = 0x800;
  CHECK(!decode::DbcSignalStore::isValid(bad_id));
  core::DbcSignalDef bad_factor = sig(1, 0, 8, core::ByteOrder::INTEL);
  bad_factor.factor = NAN;
  CHECK(!decode::DbcSignalStore::isValid(bad_factor));

  core::CanFrame rtr = makeFrame(0x100, {1});
  rtr.rtr = true;
  core::DbcDecodedValue out{};
  CHECK(!decode::decodeSignal(rtr, sig(1, 0, 8, core::ByteOrder::INTEL), out));
}

// ---------------------------------------------------------------- OBD (F08-F10, F51)

static tools::ObdPidTool *startObd(FakeHost &h, tools::ObdPidTool &t, const char *args) {
  CHECK(t.start(args, h));
  t.tick(h);
  return &t;
}

TEST(obd_dtc_count_byte_is_skipped) {
  FakeHost h;
  tools::ObdPidTool t;
  startObd(h, t, "bus=can0 pid=dtc_stored");
  t.onFrame(makeFrame(0x7E8, {0x06, 0x43, 0x02, 0x01, 0x43, 0x01, 0x96, 0x00}), h);
  CHECK(h.anyEventContains("stored DTCs: P0143 P0196 ecu=0x7E8"));
}

TEST(obd_dtc_none_and_truncated) {
  FakeHost h;
  tools::ObdPidTool t;
  startObd(h, t, "bus=can0 pid=dtc_stored");
  t.onFrame(makeFrame(0x7E8, {0x02, 0x43, 0x00, 0, 0, 0, 0, 0}), h);
  CHECK(h.anyEventContains("stored DTCs: none ecu=0x7E8"));
  t.onFrame(makeFrame(0x7E9, {0x04, 0x43, 0x03, 0x03, 0x01, 0, 0, 0}), h);
  CHECK(h.anyEventContains("truncated"));
  CHECK(h.anyEventContains("stored DTCs: P0301 ecu=0x7E9"));
}

TEST(obd_dtc_line_matches_flipper_dashboard) {
  FakeHost h;
  tools::ObdPidTool t;
  startObd(h, t, "bus=can0 pid=dtc_pending");
  t.onFrame(makeFrame(0x7E8, {0x04, 0x47, 0x01, 0x01, 0x33, 0, 0, 0}), h);
  bool seen = false;
  for (const auto &e : h.events) {
    if (strstr(e.c_str(), "pending DTCs:") != nullptr) {
      seen = true;
      CHECK(e.size() < 96);
    }
  }
  CHECK(seen);
}

TEST(obd_dtc_all_backs_off_when_send_fails) {
  FakeHost h;
  h.tx_ok = false;
  tools::ObdPidTool t;
  CHECK(t.start("bus=can0 pid=dtc_all", h));
  for (int i = 0; i < 50; i++) {
    h.now += 1;
    t.tick(h);
  }
  CHECK(h.tx.size() <= 2u);
}

TEST(obd_dtc_all_advances_on_terminal_nrc) {
  FakeHost h;
  tools::ObdPidTool t;
  CHECK(t.start("bus=can0 pid=dtc_all req_id=7E0", h));
  for (int i = 0; i < 6; i++) {
    t.tick(h);
    const uint8_t service = h.tx.empty() ? 0 : h.tx.back().data[1];
    t.onFrame(makeFrame(0x7E8, {0x03, 0x7F, service, 0x11, 0, 0, 0, 0}), h);
    h.now += 1;
  }
  // One request each for services 03, 07 and 0A, then done.
  CHECK_EQ(h.tx.size(), 3u);
  CHECK(h.anyEventContains("dtc_all complete"));
}

TEST(obd_interleaved_multiframe_from_two_ecus) {
  FakeHost h;
  tools::ObdPidTool t;
  startObd(h, t, "bus=can0 pid=dtc_stored");
  // 7E8: 4 DTCs (10 bytes), 7E9: 3 DTCs (8 bytes), first frames interleaved.
  t.onFrame(makeFrame(0x7E8, {0x10, 0x0A, 0x43, 0x04, 0x01, 0x01, 0x01, 0x02}), h);
  t.onFrame(makeFrame(0x7E9, {0x10, 0x08, 0x43, 0x03, 0x02, 0x01, 0x02, 0x02}), h);
  t.onFrame(makeFrame(0x7E8, {0x21, 0x01, 0x03, 0x01, 0x04, 0, 0, 0}), h);
  t.onFrame(makeFrame(0x7E9, {0x21, 0x02, 0x03, 0, 0, 0, 0, 0}), h);
  CHECK(h.anyEventContains("P0101 P0102 P0103 P0104 ecu=0x7E8"));
  CHECK(h.anyEventContains("P0201 P0202 P0203 ecu=0x7E9"));
}

TEST(obd_rejects_bad_bus_and_req_id) {
  FakeHost h;
  tools::ObdPidTool t;
  CHECK(!t.start("bus=both pid=rpm", h));
  CHECK(!t.start("bus=can2 pid=rpm", h));
  CHECK(!t.start("bus=can0 pid=rpm req_id=800", h));
  CHECK(!t.start("bus=can0 pid=rpm interval_ms=fast", h));
}

// ---------------------------------------------------------------- custom inject (F11-F13, F52)

TEST(inject_mux_change_drops_cached_frame) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=1 id=123 mux_start=0 mux_len=8 mux_value=1", h));
  t.onFrame(makeFrame(0x123, {0x01, 0xAA}), h);
  CHECK(t.configure("cmd=modify slot=1 mux_start=0 mux_len=8 mux_value=2", h));
  CHECK(!t.configure("cmd=inject slot=1", h));
  t.tick(h);
  CHECK(h.tx.empty());
  t.onFrame(makeFrame(0x123, {0x02, 0xBB}), h);
  CHECK(t.configure("cmd=inject slot=1", h));
  t.tick(h);
  CHECK_EQ(h.tx.size(), 1u);
  if (!h.tx.empty()) {
    CHECK_EQ(h.tx[0].data[0], 0x02);
  }
}

TEST(inject_rejected_modify_changes_nothing) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=1 id=123", h));
  t.onFrame(makeFrame(0x123, {0xAA}), h);
  CHECK(!t.configure("cmd=modify slot=1 mask=FF value=GG", h));
  CHECK(t.configure("cmd=inject slot=1", h));
  t.tick(h);
  CHECK_EQ(h.tx.size(), 1u);
  if (!h.tx.empty()) {
    CHECK_EQ(h.tx[0].data[0], 0xAA);
  }
}

TEST(inject_rejected_add_keeps_existing_slot) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=1 id=123", h));
  t.onFrame(makeFrame(0x123, {0xAA}), h);
  CHECK(!t.configure("cmd=add slot=1 id=456 mask=ZZ", h));
  CHECK(t.configure("cmd=inject slot=1", h));
  t.tick(h);
  CHECK(!h.tx.empty() && h.tx[0].id == 0x123);
}

TEST(inject_replacing_slot_cancels_pending) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=1 id=123", h));
  t.onFrame(makeFrame(0x123, {1}), h);
  CHECK(t.configure("cmd=inject slot=1 count=2 interval_ms=10", h));
  t.tick(h);
  CHECK(t.configure("cmd=add slot=1 id=456", h));
  t.onFrame(makeFrame(0x456, {2}), h);
  h.now += 20;
  t.tick(h);
  CHECK_EQ(h.tx.size(), 1u);
  CHECK(h.anyEventContains("pending injection canceled"));
}

TEST(inject_ignores_rtr_and_mux_beyond_dlc) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=1 id=123 mux_start=8 mux_len=8 mux_value=0", h));
  t.onFrame(makeFrame(0x123, {0x55}), h);  // 1 byte: byte 1 is not carried
  core::CanFrame rtr = makeFrame(0x123, {0, 0});
  rtr.rtr = true;
  t.onFrame(rtr, h);
  CHECK(!t.configure("cmd=inject slot=1", h));
}

TEST(inject_rejects_modifications_beyond_dlc) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=1 id=123", h));
  t.onFrame(makeFrame(0x123, {0x01, 0x02}), h);
  CHECK(t.configure("cmd=bit slot=1 bit=40 value=1", h));
  CHECK(!t.configure("cmd=inject slot=1", h));
  CHECK(h.anyEventContains("beyond frame DLC"));
}

TEST(inject_validates_id_and_bus) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(!t.configure("cmd=add slot=1 id=800", h));
  CHECK(t.configure("cmd=add slot=1 id=800 ext=1", h));
  CHECK(!t.configure("cmd=add slot=2 id=123 bus=can9", h));
  CHECK(!t.start("bus=can9", h));
}

TEST(inject_events_match_flipper_dashboard) {
  FakeHost h;
  tools::CustomInjectTool t;
  CHECK(t.start("bus=can0", h));
  CHECK(t.configure("cmd=add slot=2 id=1D5 name=a_thirty_one_character_slot_nm", h));
  t.onFrame(makeFrame(0x1D5, {1}), h);
  CHECK(t.configure("cmd=list", h));
  unsigned slot = 0, latest = 0;
  char bus[16] = {0}, ext[8] = {0};
  unsigned long id = 0;
  bool tracking = false, listed = false;
  for (const auto &e : h.events) {
    if (sscanf(e.c_str(), "slot=%u tracking bus=%15s id=0x%lx %7s", &slot, bus, &id, ext) == 4) {
      tracking = (slot == 2 && id == 0x1D5);
    } else if (sscanf(e.c_str(), "slot=%u bus=%15s id=0x%lx %7s latest=%u", &slot, bus, &id, ext, &latest) == 5) {
      listed = (slot == 2 && latest == 1);
    }
  }
  CHECK(tracking);
  CHECK(listed);
  CHECK(h.anyEventContains("name=a_thirty_one_character_slot_nm"));
}

// ---------------------------------------------------------------- write / replay / reverse (F14-F16)

TEST(write_rejects_invalid_bus_id_and_counts) {
  FakeHost h;
  tools::WriteTool t;
  CHECK(!t.start("bus=can2 id=123 dlc=1 data=01", h));
  CHECK(!t.start("bus=both id=123 dlc=1 data=01", h));
  CHECK(!t.start("bus=can0 id=800 dlc=1 data=01", h));
  CHECK(t.start("bus=can0 id=800 ext=1 dlc=1 data=01", h));
  CHECK(!t.start("bus=can0 id=123 count=0 period_ms=1", h));
  CHECK(!t.start("bus=can0 id=123 count=5", h));
  CHECK(!t.start("bus=can0 id=123 count=4294967296 period_ms=1", h));
  CHECK(!t.start("bus=can0 id=123 data=1G", h));
}

TEST(write_counts_and_continuous_mode) {
  FakeHost h;
  tools::WriteTool t;
  CHECK(t.start("bus=can0 id=123 dlc=1 data=01 period_ms=1 count=3", h));
  for (int i = 0; i < 10; i++) {
    t.tick(h);
    h.now += 1;
  }
  CHECK_EQ(h.tx.size(), 3u);

  FakeHost h2;
  tools::WriteTool t2;
  CHECK(t2.start("bus=can0 id=123 dlc=1 data=01 period_ms=1", h2));
  for (int i = 0; i < 10; i++) {
    t2.tick(h2);
    h2.now += 1;
  }
  CHECK_EQ(h2.tx.size(), 10u);

  FakeHost h3;
  tools::WriteTool t3;
  CHECK(t3.start("bus=can0 id=123 dlc=1 data=01", h3));
  for (int i = 0; i < 5; i++) {
    t3.tick(h3);
    h3.now += 1;
  }
  CHECK_EQ(h3.tx.size(), 1u);
}

TEST(replay_validates_args) {
  FakeHost h;
  tools::ReplayTool t;
  CHECK(!t.start("bus=can2 id=123", h));
  CHECK(!t.start("bus=can0 id=800", h));
  CHECK(t.start("bus=can0 id=2C1", h));
  CHECK(t.configure("cmd=record", h));
  t.onFrame(makeFrame(0x2C1, {1}), h);
  CHECK(t.configure("cmd=stop", h));
  CHECK(!t.configure("cmd=replay speed=5000", h));
  CHECK(!t.configure("cmd=replay count=0", h));
  CHECK(t.configure("cmd=replay speed=200 count=2", h));
}

TEST(reverse_read_mode_validates_bytes) {
  FakeHost h;
  tools::ReverseTool t;
  CHECK(t.start("bus=can0 mode=read id=273 bytes=0,1,7", h));
  CHECK(!t.start("bus=can0 mode=read id=273 bytes=0,9", h));
  CHECK(!t.start("bus=can0 mode=read id=273 bytes=a", h));
  CHECK(!t.start("bus=can0 mode=reed", h));
}

// ---------------------------------------------------------------- bus availability (F27)

TEST(tools_refuse_unavailable_bus) {
  FakeHost h;
  h.can1_available = false;
  tools::ReadAllTool r;
  CHECK(!r.start("bus=can1", h));
  CHECK(r.start("bus=both", h));
  CHECK(r.start("bus=can0", h));
  tools::WriteTool w;
  CHECK(!w.start("bus=can1 id=123", h));
  tools::CustomInjectTool c;
  CHECK(c.start("bus=can0", h));
  CHECK(!c.configure("cmd=add slot=1 id=123 bus=can1", h));
  tools::ObdPidTool o;
  CHECK(!o.start("bus=can1 pid=rpm", h));
  CHECK(h.anyEventContains("bus can1 unavailable"));
}

// ---------------------------------------------------------------- CLI / protocol / pacing (wave 3)

struct StrStream : Stream {
  std::string in;
  size_t pos = 0;
  std::vector<uint8_t> out;
  int available() override { return (int)(in.size() - pos); }
  int read() override { return pos < in.size() ? (unsigned char)in[pos++] : -1; }
  size_t write(uint8_t b) override {
    out.push_back(b);
    return 1;
  }
};

TEST(cli_discards_whole_overlong_line) {
  StrStream st;
  st.in = "wifi set pass=" + std::string(250, 'x') + " tool stop\nstatus\n";
  io::CliShell sh;
  sh.begin(st);
  std::vector<std::string> lines;
  for (int i = 0; i < 4; i++) {
    sh.poll();
    core::CliLine l;
    while (sh.popLine(l)) {
      lines.push_back(l.line);
    }
  }
  CHECK_EQ(lines.size(), 1u);
  CHECK(!lines.empty() && lines[0] == "status");
  CHECK(sh.takeOverflow());
  CHECK(!sh.takeOverflow());
}

TEST(cli_keeps_every_line_of_a_large_paste) {
  StrStream st;
  for (int i = 1; i <= 20; i++) {
    st.in += "dbc remove sid=" + std::to_string(i) + "\n";
  }
  io::CliShell sh;
  sh.begin(st);
  std::vector<std::string> lines;
  for (int i = 0; i < 10; i++) {  // one poll + drain per main-loop iteration
    sh.poll();
    core::CliLine l;
    while (sh.popLine(l)) {
      lines.push_back(l.line);
    }
  }
  CHECK_EQ(lines.size(), 20u);
  CHECK(!lines.empty() && lines.front() == "dbc remove sid=1");
}

static std::vector<uint8_t> encodePacket(uint8_t ver, uint8_t id, const uint8_t *payload, uint16_t len) {
  StrStream sink;
  io::FlipperProtocol proto;
  proto.begin(sink);
  core::FlipperPacket pkt{};
  pkt.ver = ver;
  pkt.kind = core::FlipperPacketKind::CMD;
  pkt.id = id;
  pkt.seq = 7;
  pkt.len = len;
  if (len > 0) {
    memcpy(pkt.payload, payload, len);
  }
  proto.sendPacket(pkt);
  return sink.out;
}

TEST(flipper_protocol_round_trip_and_version_check) {
  for (uint16_t len = 0; len <= BoardConfig::MAX_FLIPPER_PAYLOAD; len++) {
    uint8_t payload[BoardConfig::MAX_FLIPPER_PAYLOAD];
    for (uint16_t i = 0; i < len; i++) {
      payload[i] = (uint8_t)(i * 7);  // includes zeros
    }
    const std::vector<uint8_t> wire = encodePacket(1, 0x23, payload, len);
    CHECK(wire.size() <= io::FlipperProtocol::maxEncodedSize(len));
    StrStream rx;
    rx.in.assign(wire.begin(), wire.end());
    io::FlipperProtocol proto;
    proto.begin(rx);
    core::FlipperPacket got{};
    const bool ok = proto.pollPacket(got);
    CHECK(ok && got.len == len && got.seq == 7 && memcmp(got.payload, payload, len) == 0);
  }

  const uint8_t one = 1;
  const std::vector<uint8_t> v2 = encodePacket(2, 0x21, &one, 1);
  StrStream rx;
  rx.in.assign(v2.begin(), v2.end());
  io::FlipperProtocol proto;
  proto.begin(rx);
  core::FlipperPacket got{};
  CHECK(!proto.pollPacket(got));
  CHECK_EQ(proto.rxErrors(), 1u);
}

TEST(unique_ids_snapshot_is_paced_and_complete) {
  FakeHost h;
  h.queue_capacity = 64;
  tools::UniqueIdsTool t;
  CHECK(t.start("bus=can0", h));
  for (uint32_t i = 0; i < 300; i++) {
    t.onFrame(makeFrame(0x100 + i, {1}), h);
  }
  h.events.clear();
  h.drainOutput();
  CHECK(t.configure("snapshot=1", h));
  for (int i = 0; i < 100 && !h.anyEventContains("snapshot end"); i++) {
    CHECK(h.queued <= h.queue_capacity);
    h.drainOutput();
    t.tick(h);
  }
  int ids = 0;
  for (const auto &e : h.events) {
    if (e.rfind("id=0x", 0) == 0) {
      ids++;
    }
  }
  CHECK_EQ(ids, 300);
  CHECK(h.anyEventContains("unique_id snapshot end"));
}

// ---------------------------------------------------------------- tool behavior (wave 4)

TEST(valtrack_default_args_lock_to_one_id) {
  FakeHost h;
  tools::ValueTrackerTool t;
  CHECK(t.start("bus=can0 strict=1", h));  // Flipper default
  const uint32_t ids[] = {0x100, 0x200, 0x300};
  for (int i = 0; i < 30; i++) {
    t.onFrame(makeFrame(ids[i % 3], {(uint8_t)(i / 3), 0}), h);
  }
  int warnings = 0, changes = 0;
  for (const auto &e : h.events) {
    if (e.find("tracker reset") != std::string::npos) {
      warnings++;
    }
    unsigned b = 0, o = 0, oo = 0, n = 0, nn = 0;
    if (sscanf(e.c_str(), "byte %u changed 0x%x(%u)->0x%x(%u)", &b, &o, &oo, &n, &nn) == 5) {
      changes++;
    }
  }
  CHECK_EQ(warnings, 0);
  CHECK_EQ(changes, 9);  // byte 0 of ID 0x100 changes 9 times
  CHECK(h.anyEventContains("valtrack locked id=0x100"));
}

TEST(valtrack_id_filter_and_reset_clears_min_max) {
  FakeHost h;
  tools::ValueTrackerTool t;
  CHECK(t.start("bus=can0 id=123", h));
  t.onFrame(makeFrame(0x123, {5}), h);
  t.onFrame(makeFrame(0x456, {1}), h);
  t.onFrame(makeFrame(0x123, {100, 0}), h);  // DLC change: reset
  h.events.clear();
  CHECK(t.configure("show=min", h));
  CHECK(!h.events.empty() && h.events[0].find("b0=100 ") != std::string::npos);
}

TEST(unique_ids_tracks_buses_separately_and_reports_full_once) {
  FakeHost h;
  tools::UniqueIdsTool t;
  CHECK(t.start("bus=both", h));
  t.onFrame(makeFrame(0x123, {1}, core::BusId::CAN0), h);
  t.onFrame(makeFrame(0x123, {1}, core::BusId::CAN1), h);
  int news = 0;
  for (const auto &e : h.events) {
    if (e.rfind("new id bus=", 0) == 0) {
      news++;
    }
  }
  CHECK_EQ(news, 2);
  for (uint32_t i = 0; i < 600; i++) {
    t.onFrame(makeFrame(0x200 + i, {1}), h);
  }
  int full = 0;
  for (const auto &e : h.events) {
    if (e.find("table full") != std::string::npos) {
      full++;
    }
  }
  CHECK_EQ(full, 1);
}

TEST(reverse_reports_new_ids_once_and_tags_bus) {
  FakeHost h;
  tools::ReverseTool t;
  CHECK(t.start("bus=both calibration_ms=10 monitor_ms=100000 summary=0", h));
  t.onFrame(makeFrame(0x100, {1, 2}, core::BusId::CAN0), h);
  h.now += 20;
  t.tick(h);  // -> monitoring
  t.onFrame(makeFrame(0x555, {9, 9, 9, 9, 9, 9, 9, 9}, core::BusId::CAN0), h);
  t.onFrame(makeFrame(0x100, {1, 3}, core::BusId::CAN0), h);
  int changes = 0;
  bool tagged = false;
  for (const auto &e : h.events) {
    unsigned long id = 0, b = 0;
    if (sscanf(e.c_str(), "change id=0x%lx b%lu", &id, &b) == 2) {
      changes++;
      tagged = tagged || e.find("bus=can0") != std::string::npos;
    }
  }
  CHECK_EQ(changes, 1);
  CHECK(tagged);
  CHECK(h.anyEventContains("new id=0x555 std during monitoring"));
}

TEST(bittrack_keeps_state_per_bus) {
  FakeHost h;
  tools::BitTrackerTool t;
  CHECK(t.start("bus=both id=123", h));
  for (int i = 0; i < 10; i++) {
    t.onFrame(makeFrame(0x123, {0xAA}, core::BusId::CAN0), h);
    t.onFrame(makeFrame(0x123, {0x55}, core::BusId::CAN1), h);
  }
  CHECK_EQ(h.events.size(), 2u);  // first frame of each stream only
}

TEST(failed_tool_start_keeps_running_tool) {
  FakeHost h;
  tools::ToolManager m;
  CHECK(m.startById(core::ToolId::WRITE, "bus=can0 id=123 dlc=1 data=01 period_ms=10", h));
  CHECK(!m.startById(core::ToolId::READ_ALL, "bus=can9", h));
  CHECK(m.activeId() == core::ToolId::WRITE);
  CHECK(!m.startById(core::ToolId::WRITE, "bus=can0 id=zzz", h));
  CHECK(m.activeId() == core::ToolId::WRITE);
  m.tick(h);
  CHECK_EQ(h.tx.size(), 1u);
  CHECK(m.startById(core::ToolId::READ_ALL, "bus=can0", h));
  CHECK(m.activeId() == core::ToolId::READ_ALL);
}

TEST(replay_follows_recorded_gaps) {
  FakeHost h;
  tools::ReplayTool t;
  CHECK(t.start("bus=can0 id=2C1", h));
  CHECK(t.configure("cmd=record", h));
  // Three frames captured 10 ms apart, but processed back to back.
  for (uint32_t i = 0; i < 3; i++) {
    core::CanFrame f = makeFrame(0x2C1, {(uint8_t)i});
    f.ts_us = 5000000 + i * 10000;
    t.onFrame(f, h);
  }
  CHECK(t.configure("cmd=stop", h));
  CHECK(t.configure("cmd=replay", h));
  std::vector<uint32_t> sent_at;
  for (uint32_t step = 0; step < 400; step++) {  // 40 ms in 100 us steps
    const size_t before = h.tx.size();
    t.tick(h);
    if (h.tx.size() != before) {
      sent_at.push_back(host_clock::us());
    }
    host_clock::us() += 100;
  }
  CHECK_EQ(sent_at.size(), 3u);
  if (sent_at.size() == 3) {
    CHECK(sent_at[1] - sent_at[0] >= 9900 && sent_at[1] - sent_at[0] <= 10100);
    CHECK(sent_at[2] - sent_at[1] >= 9900 && sent_at[2] - sent_at[1] <= 10100);
  }
}

int main() {
  RUN(parse_numbers_are_strict);
  RUN(parse_optional_distinguishes_absent_from_invalid);
  RUN(parse_strings_never_truncate);
  RUN(parse_hex_rejects_partial_digits);

  RUN(dbc_motorola_matches_dbc_sawtooth_layout);
  RUN(dbc_motorola_mux_uses_signal_byte_order);
  RUN(dbc_bus_both_matches_either_bus);
  RUN(dbc_decodes_beyond_sixteen_mux_variants);
  RUN(dbc_unsigned_64_bit_scales_as_unsigned);
  RUN(dbc_zero_range_means_unspecified);
  RUN(dbc_rejects_impossible_descriptors_and_rtr);

  RUN(obd_dtc_count_byte_is_skipped);
  RUN(obd_dtc_none_and_truncated);
  RUN(obd_dtc_line_matches_flipper_dashboard);
  RUN(obd_dtc_all_backs_off_when_send_fails);
  RUN(obd_dtc_all_advances_on_terminal_nrc);
  RUN(obd_interleaved_multiframe_from_two_ecus);
  RUN(obd_rejects_bad_bus_and_req_id);

  RUN(inject_mux_change_drops_cached_frame);
  RUN(inject_rejected_modify_changes_nothing);
  RUN(inject_rejected_add_keeps_existing_slot);
  RUN(inject_replacing_slot_cancels_pending);
  RUN(inject_ignores_rtr_and_mux_beyond_dlc);
  RUN(inject_rejects_modifications_beyond_dlc);
  RUN(inject_validates_id_and_bus);
  RUN(inject_events_match_flipper_dashboard);

  RUN(write_rejects_invalid_bus_id_and_counts);
  RUN(write_counts_and_continuous_mode);
  RUN(replay_validates_args);
  RUN(reverse_read_mode_validates_bytes);
  RUN(tools_refuse_unavailable_bus);

  RUN(cli_discards_whole_overlong_line);
  RUN(cli_keeps_every_line_of_a_large_paste);
  RUN(flipper_protocol_round_trip_and_version_check);
  RUN(unique_ids_snapshot_is_paced_and_complete);

  RUN(valtrack_default_args_lock_to_one_id);
  RUN(valtrack_id_filter_and_reset_clears_min_max);
  RUN(unique_ids_tracks_buses_separately_and_reports_full_once);
  RUN(reverse_reports_new_ids_once_and_tags_bus);
  RUN(bittrack_keeps_state_per_bus);
  RUN(failed_tool_start_keeps_running_tool);
  RUN(replay_follows_recorded_gaps);

  printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
