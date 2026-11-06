#include "minitest.h"

#include "line_sender.h"

#define PACKET      4
#define MAX_PACKETS 32

/* The fake endpoint: takes packets until the one numbered fail_at, which
 * gets fail_with. A packet that may have gone out is on the wire. */
static char wire[256];
static size_t wire_len;
static size_t packet_len[MAX_PACKETS];
static int packets;
static int fail_at;
static LinePacketResult fail_with;

static LinePacketResult send_packet(const uint8_t* data, size_t len, void* ctx) {
    (void)ctx;
    LinePacketResult result = LinePacketSent;
    if(packets == fail_at) {
        fail_at = -1;
        result = fail_with;
        if(result == LinePacketNotSent) return result;
    }
    if(len > 0) memcpy(wire + wire_len, data, len);
    wire_len += len;
    wire[wire_len] = '\0';
    if(packets < MAX_PACKETS) packet_len[packets] = len;
    packets++;
    return result;
}

static void clear_wire(void) {
    wire_len = 0;
    wire[0] = '\0';
    packets = 0;
    fail_at = -1;
    fail_with = LinePacketNotSent;
}

static void setup(LineSender* ls) {
    clear_wire();
    line_sender_init(ls, PACKET, send_packet, NULL);
}

static TransportSendResult send_str(LineSender* ls, const char* line) {
    return line_sender_send(ls, line, strlen(line));
}

static void test_line_goes_out_in_packets_with_its_newline(void) {
    LineSender ls;
    setup(&ls);
    CHECK(send_str(&ls, "{\"a\":12}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\":12}\n");
    /* a short last packet ends the transfer: no zero-length packet */
    CHECK_EQ_INT(packets, 3);
    CHECK_EQ_INT(packet_len[0], 4);
    CHECK_EQ_INT(packet_len[1], 4);
    CHECK_EQ_INT(packet_len[2], 1);
}

static void test_full_last_packet_is_followed_by_a_zero_length_packet(void) {
    LineSender ls;
    setup(&ls);
    CHECK(send_str(&ls, "{\"a\":1}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\":1}\n");
    CHECK_EQ_INT(packets, 3);
    CHECK_EQ_INT(packet_len[1], 4);
    CHECK_EQ_INT(packet_len[2], 0);
}

static void test_failed_first_packet_sends_nothing(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 0;
    CHECK(send_str(&ls, "{\"a\":1}") == TransportNotSent);
    CHECK_EQ_INT(wire_len, 0);
    /* nothing is left over on the host: the next line needs no resync */
    CHECK(send_str(&ls, "{\"b\":2}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"b\":2}\n");
}

static void test_line_cut_short_is_not_sent_and_the_next_line_resyncs(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 1;
    CHECK(send_str(&ls, "{\"a\":1}") == TransportNotSent);
    CHECK_STR_EQ(wire, "{\"a\"");
    /* the newline ends the fragment on the host, which drops it as invalid */
    CHECK(send_str(&ls, "{\"b\":2}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\"\n{\"b\":2}\n");
    clear_wire();
    CHECK(send_str(&ls, "{\"c\":3}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"c\":3}\n");
}

static void test_failed_resync_keeps_the_next_line_resyncing(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 1;
    send_str(&ls, "{\"a\":1}");
    clear_wire();
    fail_at = 0;
    CHECK(send_str(&ls, "{\"b\":2}") == TransportNotSent);
    CHECK_EQ_INT(wire_len, 0);
    CHECK(send_str(&ls, "{\"c\":3}") == TransportSent);
    CHECK_STR_EQ(wire, "\n{\"c\":3}\n");
}

static void test_line_missing_only_its_newline_may_have_gone_out(void) {
    LineSender ls;
    setup(&ls);
    /* "{\"a\":12}" + '\n' is 2 full packets and the newline in a third */
    fail_at = 2;
    CHECK(send_str(&ls, "{\"a\":12}") == TransportMaybeSent);
    CHECK_STR_EQ(wire, "{\"a\":12}");
    /* the resync newline completes it on the host */
    CHECK(send_str(&ls, "{\"b\":2}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\":12}\n{\"b\":2}\n");
}

static void test_failed_zero_length_packet_may_have_gone_out(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 2;
    CHECK(send_str(&ls, "{\"a\":1}") == TransportMaybeSent);
    CHECK_STR_EQ(wire, "{\"a\":1}\n");
    /* the line was whole: no resync */
    CHECK(send_str(&ls, "{\"b\":2}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\":1}\n{\"b\":2}\n");
}

static void test_unconfirmed_packet_short_of_the_line_is_not_sent(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 0;
    fail_with = LinePacketMaybeSent;
    CHECK(send_str(&ls, "{\"a\":1}") == TransportNotSent);
    CHECK_STR_EQ(wire, "{\"a\"");
    /* the start may be on the host: the next line resyncs */
    CHECK(send_str(&ls, "{\"b\":2}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\"\n{\"b\":2}\n");
}

static void test_unconfirmed_last_packet_may_have_gone_out(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 2;
    fail_with = LinePacketMaybeSent;
    CHECK(send_str(&ls, "{\"a\":12}") == TransportMaybeSent);
    CHECK_STR_EQ(wire, "{\"a\":12}\n");
    CHECK(send_str(&ls, "{\"b\":2}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"a\":12}\n\n{\"b\":2}\n");
}

static void test_unconfirmed_packet_before_the_newline_may_have_gone_out(void) {
    LineSender ls;
    setup(&ls);
    /* the second packet ends the text, the third holds only the newline */
    fail_at = 1;
    fail_with = LinePacketMaybeSent;
    CHECK(send_str(&ls, "{\"a\":12}") == TransportMaybeSent);
    CHECK_STR_EQ(wire, "{\"a\":12}");
    CHECK_EQ_INT(packets, 2);
}

static void test_unconfirmed_zero_length_packet_may_have_gone_out(void) {
    LineSender ls;
    setup(&ls);
    fail_at = 2;
    fail_with = LinePacketMaybeSent;
    CHECK(send_str(&ls, "{\"a\":1}") == TransportMaybeSent);
    CHECK_STR_EQ(wire, "{\"a\":1}\n");
}

static void test_packet_size_can_change_between_lines(void) {
    LineSender ls;
    setup(&ls);
    CHECK(send_str(&ls, "{\"a\":12}") == TransportSent);
    clear_wire();
    line_sender_set_packet_size(&ls, 6);
    CHECK(send_str(&ls, "{\"b\":34}") == TransportSent);
    CHECK_STR_EQ(wire, "{\"b\":34}\n");
    CHECK_EQ_INT(packets, 2);
    CHECK_EQ_INT(packet_len[0], 6);
    CHECK_EQ_INT(packet_len[1], 3);
}

static void test_packet_size_is_kept_within_the_buffer(void) {
    LineSender ls;
    setup(&ls);
    line_sender_set_packet_size(&ls, 0);
    CHECK_EQ_INT(ls.packet_size, LINE_SENDER_PACKET_MAX);
    line_sender_set_packet_size(&ls, LINE_SENDER_PACKET_MAX + 1);
    CHECK_EQ_INT(ls.packet_size, LINE_SENDER_PACKET_MAX);
    /* a BLE notification of up to 243 bytes fits */
    CHECK(LINE_SENDER_PACKET_MAX >= 243);
}

int main(void) {
    RUN_TEST(test_packet_size_can_change_between_lines);
    RUN_TEST(test_packet_size_is_kept_within_the_buffer);
    RUN_TEST(test_line_goes_out_in_packets_with_its_newline);
    RUN_TEST(test_full_last_packet_is_followed_by_a_zero_length_packet);
    RUN_TEST(test_failed_first_packet_sends_nothing);
    RUN_TEST(test_line_cut_short_is_not_sent_and_the_next_line_resyncs);
    RUN_TEST(test_failed_resync_keeps_the_next_line_resyncing);
    RUN_TEST(test_line_missing_only_its_newline_may_have_gone_out);
    RUN_TEST(test_failed_zero_length_packet_may_have_gone_out);
    RUN_TEST(test_unconfirmed_packet_short_of_the_line_is_not_sent);
    RUN_TEST(test_unconfirmed_last_packet_may_have_gone_out);
    RUN_TEST(test_unconfirmed_packet_before_the_newline_may_have_gone_out);
    RUN_TEST(test_unconfirmed_zero_length_packet_may_have_gone_out);
    return MINITEST_RESULT();
}
