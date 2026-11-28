/* Reader inspection, driven a frame at a time the way the listener does.
 *
 * The reader selects the operation selector and the passthrough applet, then
 * polls with GET DATA. Each poll is answered with a command for it to run, and
 * it hands the result back in a PUT DATA. The credential table is the one
 * result that does not fit a single PUT DATA: the first carries the length the
 * table declares and the start of it, the rest carry on with bare table bytes.
 */
#include "munit.h"
#include "test_helpers.h"

#include <string.h>

#include <seos_inspect.h>

#define I_BLOCK 0x02

/* Hivecodes the table is searched for, and one it is not. */
#define HIVE_ICLASS  0x63BC8AFF
#define HIVE_SIO     0x4C87F761
#define HIVE_UNKNOWN 0x11223344

#define CARD_TYPE_SEOS 7

/* Most a short PUT DATA can carry. */
#define FRAGMENT_MAX 255

typedef struct {
    SeosInspectData ins;
    BitBuffer* tx;
    SeosInspectEvent last_event;
} Session;

static void session_begin(Session* s) {
    seos_inspect_reset(&s->ins);
    s->tx = bit_buffer_alloc(128);
    s->last_event = SeosInspectEventNone;
}

static void session_end(Session* s) {
    bit_buffer_free(s->tx);
}

/* Sends one command in a single I-block and leaves the answer in s->tx. */
static void send_apdu(Session* s, const uint8_t* apdu, size_t len) {
    uint8_t frame[1 + 5 + FRAGMENT_MAX];
    munit_assert_size(len, <=, sizeof(frame) - 1);
    frame[0] = I_BLOCK;
    memcpy(frame + 1, apdu, len);
    seos_inspect_handle_frame(&s->ins, frame, len + 1, s->tx, &s->last_event);
    /* Every command gets an answer: a silent card is what a hang looks like
     * from the reader's side. */
    munit_assert_size(bit_buffer_get_size_bytes(s->tx), >=, 3);
}

static void send_select(Session* s, uint8_t applet) {
    uint8_t apdu[] = {0x00, 0xa4, 0x04, 0x00, 0x0a, 0xa0, 0x00, 0x00, 0x03,
                      0x82, 0x00, applet, 0x00, 0x01, 0x01, 0x00};
    send_apdu(s, apdu, sizeof(apdu));
}

static void send_get_data(Session* s) {
    const uint8_t apdu[] = {0x00, 0xca, 0x00, 0x00, 0x00};
    send_apdu(s, apdu, sizeof(apdu));
}

static void send_put_data(Session* s, const uint8_t* data, size_t len) {
    uint8_t apdu[5 + FRAGMENT_MAX] = {0x00, 0xda, 0x00, 0x00, (uint8_t)len};
    munit_assert_size(len, <=, FRAGMENT_MAX);
    if(len) memcpy(apdu + 5, data, len);
    send_apdu(s, apdu, 5 + len);
}

/* Takes the exchange from the first SELECT to the poll that asks for the
 * credential table. */
static void run_to_credential_request(Session* s) {
    send_select(s, 0x2f);
    munit_assert_int(s->last_event, ==, SeosInspectEventAidSelected);
    send_select(s, 0x2d);
    /* The application is confirmed by naming it back. */
    const uint8_t named[] = {I_BLOCK, 0x6f, 0x0c, 0x84, 0x0a, 0xa0, 0x00, 0x00, 0x03, 0x82,
                             0x00,    0x2d, 0x00, 0x01, 0x01, 0x90, 0x00};
    munit_assert_size(bit_buffer_get_size_bytes(s->tx), ==, sizeof(named));
    munit_assert_memory_equal(sizeof(named), bit_buffer_get_data(s->tx), named);

    /* Each result is wrapped twice before the values. */
    const uint8_t sam[] = {0xbd, 0x06, 0x8a, 0x04, 0x80, 0x02, 0x01, 0xbd};
    const uint8_t core[] = {0xbd, 0x06, 0x8a, 0x04, 0x80, 0x02, 0x01, 0xb0};
    const uint8_t mem[] = {0xbd, 0x0e, 0x8a, 0x0c, 0x80, 0x01, 0x04, 0x81,
                           0x01, 0x20, 0x82, 0x04, 0x00, 0x02, 0x44, 0x00};

    send_get_data(s);
    send_put_data(s, sam, sizeof(sam));
    send_get_data(s);
    send_put_data(s, core, sizeof(core));
    send_get_data(s);
    send_put_data(s, mem, sizeof(mem));
    munit_assert_uint16(s->ins.sam_ver, ==, 0x01bd);
    munit_assert_uint16(s->ins.core_ver, ==, 0x01b0);
    munit_assert_uint8(s->ins.assembly_count, ==, 0x20);
    munit_assert_uint32(s->ins.persist_mem, ==, 0x00024400);

    send_get_data(s);
    munit_assert_int(s->ins.state, ==, SeosInspectCredConfig);
}

/* Appends one group to a table: a protocol, a count, and that many nine-byte
 * entries carrying the same hivecode and card type. */
static size_t table_add_group(
    uint8_t* table,
    size_t pos,
    uint8_t proto,
    uint8_t count,
    uint32_t hivecode,
    uint8_t card_type) {
    table[pos++] = proto;
    table[pos++] = count;
    for(uint8_t i = 0; i < count; i++) {
        table[pos + 0] = (uint8_t)(hivecode >> 24);
        table[pos + 1] = (uint8_t)(hivecode >> 16);
        table[pos + 2] = (uint8_t)(hivecode >> 8);
        table[pos + 3] = (uint8_t)hivecode;
        table[pos + 4] = 0;
        table[pos + 5] = 0;
        table[pos + 6] = 0;
        table[pos + 7] = card_type;
        table[pos + 8] = 0;
        pos += 9;
    }
    return pos;
}

/* Hands the table to the card the way the reader does, then finishes the
 * exchange. Returns once the reader has nothing more to say. */
static void deliver_table(Session* s, const uint8_t* table, size_t table_len) {
    /* First piece: the two wrappers, the object identifier the table was asked
     * for by, and an octet string declaring the whole table's length. The
     * wrappers declare more than the piece holds whenever the table runs on. */
    uint8_t first[FRAGMENT_MAX];
    size_t n = 0;
    const size_t value_header = 7 + 4;
    size_t inner_len = value_header + table_len;
    size_t outer_len = 4 + inner_len;
    first[n++] = 0xbd;
    first[n++] = 0x82;
    first[n++] = (uint8_t)(outer_len >> 8);
    first[n++] = (uint8_t)outer_len;
    first[n++] = 0x8a;
    first[n++] = 0x82;
    first[n++] = (uint8_t)(inner_len >> 8);
    first[n++] = (uint8_t)inner_len;
    const uint8_t oid[] = {0x06, 0x05, 0x03, 0x01, 0x07, 0x01, 0x5f};
    memcpy(first + n, oid, sizeof(oid));
    n += sizeof(oid);
    first[n++] = 0x04;
    first[n++] = 0x82;
    first[n++] = (uint8_t)(table_len >> 8);
    first[n++] = (uint8_t)table_len;

    size_t sent = table_len < sizeof(first) - n ? table_len : sizeof(first) - n;
    memcpy(first + n, table, sent);
    send_put_data(s, first, n + sent);

    /* The rest follows as bare table bytes. */
    while(sent < table_len) {
        size_t piece = table_len - sent;
        if(piece > 200) piece = 200;
        send_put_data(s, table + sent, piece);
        sent += piece;
    }

    /* The reader polls again, is told to suspend, and acknowledges. Bounded:
     * a card still waiting for table bytes would poll forever. */
    for(int i = 0; i < 4 && s->last_event != SeosInspectEventDone; i++) {
        send_get_data(s);
        send_put_data(s, NULL, 0);
    }
}

/* A table that fits in the first piece. */
static MunitResult test_small_table(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    Session s;
    session_begin(&s);
    run_to_credential_request(&s);

    uint8_t table[64];
    size_t len = 0;
    len = table_add_group(table, len, 0x02, 1, HIVE_ICLASS, 0);
    len = table_add_group(table, len, 0x09, 1, HIVE_UNKNOWN, 0);
    deliver_table(&s, table, len);

    munit_assert_int(s.last_event, ==, SeosInspectEventDone);
    munit_assert_true(s.ins.cred_parsed);
    munit_assert_true(s.ins.cred_enabled[0]); /* iCLASS */
    munit_assert_true(s.ins.cred_enabled[7]); /* 125kHz Prox, from the group's protocol */
    munit_assert_false(s.ins.cred_enabled[1]);
    munit_assert_false(s.ins.seos_enabled);

    session_end(&s);
    return MUNIT_OK;
}

/* A table longer than any buffer the card might have kept it in. The entries
 * that matter sit at the far end, so they are only seen if all of it is read. */
static MunitResult test_large_table(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    Session s;
    session_begin(&s);
    run_to_credential_request(&s);

    uint8_t table[1024];
    size_t len = 0;
    len = table_add_group(table, len, 0x02, 60, HIVE_UNKNOWN, 0);
    len = table_add_group(table, len, 0x02, 1, HIVE_ICLASS, 0);
    len = table_add_group(table, len, 0x02, 1, HIVE_SIO, CARD_TYPE_SEOS);
    munit_assert_size(len, >, 512);
    deliver_table(&s, table, len);

    munit_assert_int(s.last_event, ==, SeosInspectEventDone);
    munit_assert_true(s.ins.cred_parsed);
    munit_assert_true(s.ins.cred_enabled[0]); /* iCLASS */
    munit_assert_true(s.ins.seos_enabled);
    munit_assert_false(s.ins.cred_enabled[7]);

    session_end(&s);
    return MUNIT_OK;
}

/* A reader with nothing enabled answers with an empty table, which is an
 * answer all the same. */
static MunitResult test_empty_table(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    Session s;
    session_begin(&s);
    run_to_credential_request(&s);

    deliver_table(&s, NULL, 0);

    munit_assert_int(s.last_event, ==, SeosInspectEventDone);
    munit_assert_true(s.ins.cred_parsed);
    munit_assert_false(s.ins.seos_enabled);
    for(size_t i = 0; i < SEOS_INSPECT_CRED_COUNT; i++) {
        munit_assert_false(s.ins.cred_enabled[i]);
    }

    session_end(&s);
    return MUNIT_OK;
}

/* A group split across two pieces is read as one. */
static MunitResult test_entry_across_pieces(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    Session s;
    session_begin(&s);
    run_to_credential_request(&s);

    /* The first piece has room for 236 table bytes. An empty group and a
     * group of 25 take 229 of them, so the next group's header fits and its
     * first entry is cut after five bytes. */
    uint8_t table[512];
    size_t len = 0;
    len = table_add_group(table, len, 0x02, 0, 0, 0);
    len = table_add_group(table, len, 0x02, 25, HIVE_UNKNOWN, 0);
    len = table_add_group(table, len, 0x02, 2, HIVE_ICLASS, 0);
    deliver_table(&s, table, len);

    munit_assert_int(s.last_event, ==, SeosInspectEventDone);
    munit_assert_true(s.ins.cred_enabled[0]);
    munit_assert_false(s.ins.seos_enabled);

    session_end(&s);
    return MUNIT_OK;
}

static MunitTest test_inspect_cases[] = {
    {(char*)"/table/small", test_small_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/table/large", test_large_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/table/empty", test_empty_table, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/table/split-entry",
     test_entry_across_pieces,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_inspect_suite = {
    (char*)"/inspect",
    test_inspect_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
