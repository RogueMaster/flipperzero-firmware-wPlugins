#include "minitest.h"

#include "host_link.h"

static void check_out(HostLinkOutput out, bool send_hello, bool connected, bool lost) {
    CHECK_EQ_INT(out.send_hello, send_hello);
    CHECK_EQ_INT(out.host_connected, connected);
    CHECK_EQ_INT(out.host_lost, lost);
}

static void test_starts_closed_without_host(void) {
    HostLink link;
    host_link_init(&link);
    CHECK_EQ_INT(link.state, HostLinkStateClosed);
    CHECK(!host_link_is_up(&link));
    check_out(host_link_tick(&link, 100000), false, false, false);
}

static void test_port_open_sends_hello_and_waits(void) {
    HostLink link;
    host_link_init(&link);
    check_out(host_link_port_opened(&link, 1000), true, false, false);
    CHECK_EQ_INT(link.state, HostLinkStateWaiting);
    CHECK(!host_link_is_up(&link));
}

static void test_host_hello_within_timeout_connects(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 1000);
    check_out(host_link_tick(&link, 2999), false, false, false);
    check_out(host_link_host_hello(&link, 2999), false, true, false);
    CHECK_EQ_INT(link.state, HostLinkStateUp);
    CHECK(host_link_is_up(&link));
}

static void test_no_host_hello_within_timeout_means_no_host(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 1000);
    check_out(host_link_tick(&link, 1000 + HOST_LINK_HELLO_TIMEOUT_MS), false, false, false);
    CHECK_EQ_INT(link.state, HostLinkStateNoHost);
    CHECK(!host_link_is_up(&link));
    /* no hello spam while a terminal or another tool holds the port */
    check_out(host_link_tick(&link, 60000), false, false, false);
}

static void test_late_host_hello_connects_and_gets_a_reply(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 1000);
    host_link_tick(&link, 4000);
    check_out(host_link_host_hello(&link, 30000), true, true, false);
    CHECK(host_link_is_up(&link));
}

static void test_host_hello_after_timeout_before_tick_gets_a_reply(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 1000);
    check_out(host_link_host_hello(&link, 1000 + HOST_LINK_HELLO_TIMEOUT_MS), true, true, false);
}

static void test_repeated_host_hello_is_answered_without_reconnect(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 0);
    host_link_host_hello(&link, 10);
    check_out(host_link_host_hello(&link, 50000), true, false, false);
    CHECK(host_link_is_up(&link));
}

static void test_host_hello_on_closed_port_is_ignored(void) {
    HostLink link;
    host_link_init(&link);
    /* a hello still in flight when the port closed must not bring the host
     * back; a live host's hello is fed after the port-open check */
    check_out(host_link_host_hello(&link, 5), false, false, false);
    CHECK_EQ_INT(link.state, HostLinkStateClosed);
    CHECK(!host_link_is_up(&link));

    host_link_port_opened(&link, 10);
    host_link_port_closed(&link);
    check_out(host_link_host_hello(&link, 20), false, false, false);
    CHECK_EQ_INT(link.state, HostLinkStateClosed);
}

static void test_port_close_loses_host(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 0);
    host_link_host_hello(&link, 10);
    check_out(host_link_port_closed(&link), false, false, true);
    CHECK_EQ_INT(link.state, HostLinkStateClosed);
    CHECK(!host_link_is_up(&link));
    check_out(host_link_port_closed(&link), false, false, false);
}

static void test_port_close_while_waiting_loses_nothing(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 0);
    check_out(host_link_port_closed(&link), false, false, false);
    CHECK_EQ_INT(link.state, HostLinkStateClosed);
}

static void test_repeated_port_open_is_ignored(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 0);
    check_out(host_link_port_opened(&link, 500), false, false, false);
    host_link_host_hello(&link, 600);
    check_out(host_link_port_opened(&link, 700), false, false, false);
    CHECK(host_link_is_up(&link));
}

static void test_reopen_after_close_handshakes_again(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, 0);
    host_link_host_hello(&link, 10);
    host_link_port_closed(&link);
    check_out(host_link_port_opened(&link, 9000), true, false, false);
    check_out(host_link_host_hello(&link, 9100), false, true, false);
}

static void test_timeout_survives_tick_counter_wrap(void) {
    HostLink link;
    host_link_init(&link);
    host_link_port_opened(&link, UINT32_MAX - 500);
    check_out(host_link_tick(&link, 1000), false, false, false); /* 1501 ms later */
    CHECK_EQ_INT(link.state, HostLinkStateWaiting);
    check_out(host_link_host_hello(&link, 1200), false, true, false);
}

int main(void) {
    RUN_TEST(test_starts_closed_without_host);
    RUN_TEST(test_port_open_sends_hello_and_waits);
    RUN_TEST(test_host_hello_within_timeout_connects);
    RUN_TEST(test_no_host_hello_within_timeout_means_no_host);
    RUN_TEST(test_late_host_hello_connects_and_gets_a_reply);
    RUN_TEST(test_host_hello_after_timeout_before_tick_gets_a_reply);
    RUN_TEST(test_repeated_host_hello_is_answered_without_reconnect);
    RUN_TEST(test_host_hello_on_closed_port_is_ignored);
    RUN_TEST(test_port_close_loses_host);
    RUN_TEST(test_port_close_while_waiting_loses_nothing);
    RUN_TEST(test_repeated_port_open_is_ignored);
    RUN_TEST(test_reopen_after_close_handshakes_again);
    RUN_TEST(test_timeout_survives_tick_counter_wrap);
    return MINITEST_RESULT();
}
