/* Host-only dependencies; application control flow is inserted by the runner. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "uhf_tag.h"
#include "uhf_protocol.h"

#define UHF_CMD_INV_ALT 0x8AU
#define UHF_CMD_READ_TAG 0x81U
#define UHF_CMD_GET_SESSION 0x5AU
#define UHF_CMD_SET_SESSION 0x5BU
#define UHF_CMD_SET_EPC_MATCH 0x85U
#define UHF_STOP_SETTLE_MS 50U
#define FURI_LOG_I(...) ((void)0)
#define uhf_hex_to_bytes uhf_protocol_hex_to_bytes
#define uhf_bytes_to_hex uhf_protocol_bytes_to_hex

typedef struct {
    bool custom_tid_read;
    uint8_t custom_tid_words;
    UhfTidReadResult tid_read_result;
    uint8_t tag_reply[256];
    size_t tag_reply_len;
    uint8_t tag_reply_cmd;
    bool tag_reply_ready, tag_reply_checksum_ok;
    bool epc_filter_active, inventory_running, selected_tid_valid;
    bool tag_access_unfiltered, ascii_filter_active, list_selection_manual;
    char selected_epc[UHF_EPC_HEX_MAX + 1U], access_epc[UHF_EPC_HEX_MAX + 1U];
    char selected_tid[UHF_TID_HEX_MAX + 1U];
    uint16_t selected_pc;
    int selected_rssi;
    size_t list_selected_index, list_top_index, frame_buffer_len;
    void *view_port, *rx_stream;
    uint8_t access_keys[1][4], active_key_slot;
    size_t tag_count;
    char added_epc[UHF_EPC_HEX_MAX + 1U];
    uint16_t added_pc;
    char status[128];
} UhfApp;

typedef struct { uint8_t bytes[256]; size_t len; bool ok; uint8_t words; } Reply;
static struct {
    Reply custom, reads[2];
    size_t read_count, reads_used;
    bool pending, completion_batch, bad_checksum;
    unsigned stops, restores, resets, starts;
    uint32_t tick;
} reader;
static const int uhf_led_read_start = 1, uhf_led_read_ok = 2, uhf_led_read_fail = 3;

static void uhf_set_status(UhfApp* app, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(app->status, sizeof(app->status), fmt, args);
    va_end(args);
}
static void view_port_update(void* view) { (void)view; }
static void uhf_led_send(UhfApp* app, const int* led) { (void)app; (void)led; }
static void furi_delay_ms(uint32_t ms) { reader.tick += ms; }
static uint32_t furi_get_tick(void) { return reader.tick; }
static void furi_stream_buffer_reset(void* stream) { (void)stream; reader.resets++; }
static const char* uhf_tag_error_text(uint8_t code) { (void)code; return "reader error"; }
static bool uhf_set_selected_epc_filter(UhfApp* app) { (void)app; assert(false); return false; }
static size_t uhf_count_tags(UhfApp* app) { return app->tag_count; }
static void overwrite_reply(UhfApp* app, uint8_t cmd, const uint8_t* bytes, size_t len) {
    memset(app->tag_reply, 0xCC, sizeof(app->tag_reply));
    memcpy(app->tag_reply, bytes, len);
    app->tag_reply_len = len;
    app->tag_reply_cmd = cmd;
    app->tag_reply_ready = true;
    app->tag_reply_checksum_ok = true;
}
static void uhf_stop_inventory(UhfApp* app) {
    const uint8_t ack = 0x10U;
    reader.stops++;
    app->inventory_running = false;
    overwrite_reply(app, 0x93U, &ack, 1U);
}
static void uhf_start_inventory(UhfApp* app) { app->inventory_running = true; reader.starts++; }
static bool uhf_add_inventory_tag(
    UhfApp* app, const uint8_t* epc, size_t len, unsigned antenna, int rssi,
    unsigned frequency, unsigned count, uint16_t pc) {
    (void)antenna; (void)rssi; (void)frequency; (void)count;
    assert(!app->ascii_filter_active);
    assert(uhf_bytes_to_hex(epc, len, app->added_epc, sizeof(app->added_epc)));
    app->added_pc = pc;
    app->tag_count++;
    return true;
}

/* Execute precisely the custom-read branch of the real frame dispatcher. */
static void dispatch_custom(UhfApp* app, uint8_t cmd, const uint8_t* data,
                            size_t data_len, uint8_t checksum_acc) {
/* APPLICATION_CUSTOM_HANDLER */
    assert(false && "unexpected command in custom dispatcher");
}
static void uhf_process_rx(UhfApp* app) {
    if(!reader.pending) return;
    reader.pending = false;
    dispatch_custom(app, 0x53U, reader.custom.bytes, reader.custom.len,
                    reader.bad_checksum ? 1U : 0U);
    if(reader.completion_batch) {
        const uint8_t completion = 0x12U;
        dispatch_custom(app, UHF_CMD_INV_ALT, &completion, 1U, 0U);
        assert(app->tag_reply_len == 1U && app->tag_reply[0] == 0x12U);
    }
}
static bool uhf_send_command(UhfApp* app, uint8_t cmd, const uint8_t* bytes, size_t len) {
    (void)app;
    assert(cmd == UHF_CMD_INV_ALT && len == 4U);
    assert(bytes[0] == 1U && bytes[2] == 1U && bytes[3] == 4U);
    reader.pending = true;
    return true;
}
static bool uhf_wait_tag_reply(UhfApp* app, uint8_t cmd, const uint8_t* payload,
                              size_t len, uint32_t timeout) {
    (void)timeout;
    if(cmd == UHF_CMD_READ_TAG) {
        assert(len == 11U && payload[0] == 2U && payload[4] == 0U);
        assert(reader.reads_used < reader.read_count);
        const Reply* reply = &reader.reads[reader.reads_used++];
        assert(payload[6] == reply->words);
        overwrite_reply(app, cmd, reply->bytes, reply->len);
        return reply->ok;
    }
    if(cmd == UHF_CMD_GET_SESSION) {
        assert(!payload && len == 0U);
        const uint8_t saved[] = {2U, 1U};
        overwrite_reply(app, cmd, saved, sizeof(saved));
        return true;
    }
    if(cmd == UHF_CMD_SET_SESSION) {
        assert(len == 2U);
        if(payload[0] == 2U && payload[1] == 1U) reader.restores++;
        else assert(payload[0] == 0U && payload[1] == 0U);
    } else {
        assert(cmd == UHF_CMD_SET_EPC_MATCH && len == 1U && payload[0] == 1U);
    }
    const uint8_t ack = 0x10U;
    overwrite_reply(app, cmd, &ack, 1U);
    return true;
}

/* APPLICATION_TID_BLOCK */

static const uint8_t normal_epc[] = {0x30U, 0x08U, 0x33U, 0xB2U};
static const uint8_t short_tid[] = {0xE2U, 0x80U, 0x11U, 0x05U, 0x00U, 0x00U, 0x12U, 0x34U};
static const uint8_t ucode_tid[] = {
    0xE2U, 0x80U, 0x69U, 0x94U, 0x20U, 0x00U, 0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU};

static Reply custom_reply(uint16_t pc, const uint8_t* epc, size_t epc_len,
                          const uint8_t* tid, size_t tid_len, int length_mode) {
    Reply reply = {.ok = true};
    reply.bytes[reply.len++] = 0x01U; /* Antenna */
    reply.bytes[reply.len++] = (uint8_t)(pc >> 8U);
    reply.bytes[reply.len++] = (uint8_t)pc;
    if(epc_len) memcpy(&reply.bytes[reply.len], epc, epc_len);
    reply.len += epc_len;
    reply.bytes[reply.len++] = 0xABU; /* CRC */
    reply.bytes[reply.len++] = 0xCDU;
    if(length_mode) reply.bytes[reply.len++] = length_mode == 2 ? 0U : (uint8_t)tid_len;
    memcpy(&reply.bytes[reply.len], tid, tid_len);
    reply.len += tid_len;
    if(length_mode == 2) { /* Zero ReadLen is followed by reader metadata. */
        memset(&reply.bytes[reply.len], 0xAA, 7U);
        reply.len += 7U;
    }
    return reply;
}
static Reply bank_reply(uint16_t pc, const uint8_t* epc, size_t epc_len,
                        const uint8_t* tid, size_t tid_len, bool extended, uint8_t words) {
    Reply reply = {.len = 3U, .ok = true, .words = words};
    reply.bytes[reply.len++] = (uint8_t)(pc >> 8U);
    reply.bytes[reply.len++] = (uint8_t)pc;
    if(epc_len) memcpy(&reply.bytes[reply.len], epc, epc_len);
    reply.len += epc_len;
    reply.bytes[reply.len++] = 0xABU;
    reply.bytes[reply.len++] = 0xCDU;
    memcpy(&reply.bytes[reply.len], tid, tid_len);
    reply.len += tid_len;
    reply.bytes[2] = (uint8_t)(reply.len - 3U);
    if(extended) reply.bytes[reply.len++] = 0U;
    reply.bytes[reply.len++] = (uint8_t)tid_len;
    reply.bytes[reply.len++] = 0xD0U;
    reply.bytes[reply.len++] = 1U;
    return reply;
}
static UhfApp fresh(void) {
    memset(&reader, 0, sizeof(reader));
    UhfApp app = {0};
    app.ascii_filter_active = true;
    app.rx_stream = &reader;
    strcpy(app.selected_epc, "STALE");
    strcpy(app.access_epc, "STALE");
    app.selected_pc = 0xFFFFU;
    return app;
}
static void expect_identity(const UhfApp* app, const char* epc, uint16_t pc, const char* tid) {
    assert(app->tag_count == 1U && app->selected_tid_valid && app->tid_read_result.valid);
    assert(strcmp(app->added_epc, epc) == 0 && app->added_pc == pc);
    assert(strcmp(app->selected_epc, epc) == 0 && strcmp(app->access_epc, epc) == 0);
    assert(app->selected_pc == pc && app->tid_read_result.pc == pc);
    assert(strcmp(app->selected_tid, tid) == 0 && strcmp(app->tid_read_result.tid, tid) == 0);
    assert(app->ascii_filter_active && app->list_selection_manual);
    assert(reader.restores == 1U && reader.stops >= 2U);
    assert(app->tag_reply_cmd == UHF_CMD_SET_SESSION);
    assert(app->tag_reply_len == 1U && app->tag_reply[0] == 0x10U);
}
static void no_custom_tag(void) {
    reader.custom = (Reply){.bytes = {0x12U}, .len = 1U, .ok = true};
}
static void test_probe_custom(bool blank, bool completion_batch) {
    UhfApp app = fresh();
    app.inventory_running = true;
    const uint16_t pc = blank ? 0x0305U : 0x1305U;
    reader.custom = custom_reply(pc, normal_epc, blank ? 0U : sizeof(normal_epc),
                                 short_tid, sizeof(short_tid), 1);
    reader.completion_batch = completion_batch;
    assert(uhf_probe_tid_only_tag(&app));
    expect_identity(&app, blank ? "" : "300833B2", pc, "E280110500001234");
    assert(reader.reads_used == 0U);
}
static void test_probe_fallback(bool legacy) {
    UhfApp app = fresh();
    no_custom_tag();
    reader.read_count = 1U;
    reader.reads[0] = bank_reply(0x1305U, normal_epc, sizeof(normal_epc),
                                 short_tid, sizeof(short_tid), !legacy, 4U);
    assert(uhf_probe_tid_only_tag(&app));
    expect_identity(&app, "300833B2", 0x1305U, "E280110500001234");
    assert(reader.reads_used == 1U);
}
static void test_extension(unsigned variant) {
    UhfApp app = fresh();
    reader.custom = custom_reply(0x1305U, normal_epc, sizeof(normal_epc), ucode_tid, 8U, 0);
    reader.read_count = 1U;
    uint8_t epc[sizeof(normal_epc)], tid[sizeof(ucode_tid)];
    memcpy(epc, normal_epc, sizeof(epc));
    memcpy(tid, ucode_tid, sizeof(tid));
    uint16_t pc = 0x1305U;
    if(variant == 1U) epc[0] ^= 1U;
    if(variant == 2U) pc ^= 1U;
    if(variant == 3U) tid[0] ^= 1U;
    reader.reads[0] = bank_reply(pc, epc, sizeof(epc), tid,
                                variant == 4U ? 8U : sizeof(tid), true, 6U);
    if(variant == 5U) reader.reads[0] = (Reply){.bytes = {0x43U}, .len = 1U, .ok = true, .words = 6U};
    if(variant == 0U) {
        assert(uhf_probe_tid_only_tag(&app));
        expect_identity(&app, "300833B2", pc, "E28069942000123456789ABC");
    } else {
        assert(!uhf_probe_tid_only_tag(&app));
        assert(app.tag_count == 0U && !app.tid_read_result.valid && !app.selected_tid_valid);
        assert(app.selected_tid[0] == '\0' && reader.restores == 1U);
        assert(strcmp(app.status, "Incomplete TID read") == 0);
    }
}
static void test_malformed_probe(bool malformed_custom, bool bad_checksum) {
    UhfApp app = fresh();
    /* Header claims 12 EPC bytes, but the frame has only a blank-EPC record. */
    reader.custom = custom_reply(0x3305U, NULL, 0U, short_tid, sizeof(short_tid), 0);
    if(!malformed_custom) no_custom_tag();
    reader.bad_checksum = bad_checksum;
    if(bad_checksum) reader.custom = custom_reply(0x1305U, normal_epc, sizeof(normal_epc), short_tid, 8U, 1);
    reader.completion_batch = true;
    reader.read_count = 1U;
    /* PC claims an EPC, while the record contains none: never add a phantom. */
    reader.reads[0] = bank_reply(0x1305U, NULL, 0U, short_tid, sizeof(short_tid), true, 4U);
    assert(!uhf_probe_tid_only_tag(&app));
    assert(app.tag_count == 0U && !app.tid_read_result.valid && !app.selected_tid_valid);
    assert(app.selected_tid[0] == '\0' && reader.restores == 1U);
}
static void test_parser_layouts(void) {
    UhfTidReadResult result;
    uint8_t max_epc[48];
    for(size_t i = 0; i < sizeof(max_epc); i++) max_epc[i] = (uint8_t)(i + 1U);
    for(unsigned mode = 0U; mode < 3U; mode++) {
        Reply reply = custom_reply(0xC305U, max_epc, sizeof(max_epc), short_tid, 8U, (int)mode);
        memset(&result, 0xAA, sizeof(result));
        assert(uhf_tag_parse_custom_tid(reply.bytes, reply.len, 4U, &result));
        assert(result.valid && result.pc == 0xC305U && strlen(result.epc) == 96U);
        assert(strcmp(result.tid, "E280110500001234") == 0);
        for(size_t size = 0U; size < 3U + sizeof(max_epc) + 2U; size++) {
            assert(!uhf_tag_parse_custom_tid(reply.bytes, size, 4U, &result));
            assert(!result.valid);
        }
    }
    for(unsigned extended = 0U; extended < 2U; extended++) {
        Reply reply = bank_reply(0xC305U, max_epc, sizeof(max_epc), short_tid, 8U, extended, 4U);
        assert(uhf_tag_parse_tid_reply(reply.bytes, reply.len, 4U, &result));
        assert(result.valid && result.pc == 0xC305U && strlen(result.epc) == 96U);
        assert(strcmp(result.tid, "E280110500001234") == 0);
        reply.bytes[3] = 0xB8U; /* PC declares 46 rather than 48 bytes. */
        assert(!uhf_tag_parse_tid_reply(reply.bytes, reply.len, 4U, &result));
        assert(!result.valid);
    }
    Reply reply = custom_reply(0x0305U, NULL, 0U, short_tid, 4U, 1);
    assert(uhf_tag_parse_custom_tid(reply.bytes, reply.len, 4U, &result));
    assert(result.pc == 0x0305U && !result.epc[0] && strcmp(result.tid, "E2801105") == 0);
    reply = custom_reply(0x0B05U, normal_epc, 2U, short_tid, 8U, 0);
    assert(!uhf_tag_parse_custom_tid(reply.bytes, reply.len, 4U, &result));
    assert(!result.valid); /* A 2-byte EPC is not a supported identity. */
    assert(!uhf_tag_parse_custom_tid(NULL, 0U, 4U, &result));
    assert(!uhf_tag_parse_tid_reply(NULL, 0U, 4U, &result));
}
int main(void) {
    test_parser_layouts();
    test_probe_custom(false, false);
    test_probe_custom(true, false);
    test_probe_custom(false, true);
    test_probe_custom(true, true);
    test_probe_fallback(false);
    test_probe_fallback(true);
    for(unsigned variant = 0U; variant <= 5U; variant++) test_extension(variant);
    test_malformed_probe(true, false);
    test_malformed_probe(false, false);
    test_malformed_probe(false, true);
    puts("TID probe regression tests passed (ASan/UBSan; app functions + custom RX branch + pure parsers)");
    return 0;
}
