#include "minitest.h"

#include "link_select.h"

enum {
    USB = 0,
    BLE = 1
};

#define BIT(slot) (uint8_t)(1u << (slot))

static void check_out(LinkOutput out, uint8_t hello, bool connected, bool lost, bool accepted) {
    CHECK_EQ_INT(out.send_hello, hello);
    CHECK_EQ_INT(out.host_connected, connected);
    CHECK_EQ_INT(out.host_lost, lost);
    CHECK_EQ_INT(out.accepted, accepted);
}

static void test_starts_without_a_host(void) {
    LinkSelect ls;
    link_select_init(&ls);
    CHECK_EQ_INT(link_select_active(&ls), LINK_NONE);
    CHECK(!link_select_listening(&ls, USB));
    CHECK(!link_select_listening(&ls, BLE));
    check_out(link_select_tick(&ls, 100000), 0, false, false, false);
}

/* With one port in use it is host_link: hello on open, a host hello
 * within 2 s connects without a reply, a later one gets a reply. */
static void test_one_port_handshakes_as_host_link(void) {
    LinkSelect ls;
    link_select_init(&ls);
    check_out(link_select_port_opened(&ls, USB, 1000), BIT(USB), false, false, false);
    CHECK(link_select_listening(&ls, USB));
    check_out(link_select_host_hello(&ls, USB, 1500), 0, true, false, true);
    CHECK_EQ_INT(link_select_active(&ls), USB);
    /* a repeated hello is answered and still counts */
    check_out(link_select_host_hello(&ls, USB, 9000), BIT(USB), false, false, true);
    check_out(link_select_port_closed(&ls, USB, 0), 0, false, true, false);
    CHECK_EQ_INT(link_select_active(&ls), LINK_NONE);

    link_select_port_opened(&ls, BLE, 20000);
    link_select_tick(&ls, 20000 + HOST_LINK_HELLO_TIMEOUT_MS);
    check_out(link_select_host_hello(&ls, BLE, 30000), BIT(BLE), true, false, true);
    CHECK_EQ_INT(link_select_active(&ls), BLE);
}

static void test_a_hello_on_a_closed_port_is_stale(void) {
    LinkSelect ls;
    link_select_init(&ls);
    check_out(link_select_host_hello(&ls, BLE, 10), 0, false, false, false);
    link_select_port_opened(&ls, BLE, 20);
    link_select_port_closed(&ls, BLE, 0);
    check_out(link_select_host_hello(&ls, BLE, 30), 0, false, false, false);
    CHECK_EQ_INT(link_select_active(&ls), LINK_NONE);
}

/* Both ports greet; the first host to answer wins, the other is ignored
 * and no longer greeted. */
static void test_the_first_host_to_say_hello_wins(void) {
    LinkSelect ls;
    link_select_init(&ls);
    check_out(link_select_port_opened(&ls, USB, 0), BIT(USB), false, false, false);
    check_out(link_select_port_opened(&ls, BLE, 100), BIT(BLE), false, false, false);
    check_out(link_select_host_hello(&ls, BLE, 300), 0, true, false, true);
    CHECK_EQ_INT(link_select_active(&ls), BLE);
    CHECK(!link_select_listening(&ls, USB));
    check_out(link_select_host_hello(&ls, USB, 400), 0, false, false, false);
    check_out(link_select_tick(&ls, 60000), 0, false, false, false);
    CHECK_EQ_INT(link_select_active(&ls), BLE);
}

static void test_a_port_that_opens_while_a_host_is_up_is_not_greeted(void) {
    LinkSelect ls;
    link_select_init(&ls);
    link_select_port_opened(&ls, USB, 0);
    link_select_host_hello(&ls, USB, 10);
    check_out(link_select_port_opened(&ls, BLE, 500), 0, false, false, false);
    CHECK(!link_select_listening(&ls, BLE));
    check_out(link_select_host_hello(&ls, BLE, 600), 0, false, false, false);
    /* closing it changes nothing for the host */
    check_out(link_select_port_closed(&ls, BLE, 0), 0, false, false, false);
    CHECK_EQ_INT(link_select_active(&ls), USB);
}

/* The host goes: the other open port is greeted, and its host, which got
 * no answer before, answers and connects. */
static void test_losing_the_host_greets_the_other_open_port(void) {
    LinkSelect ls;
    link_select_init(&ls);
    link_select_port_opened(&ls, USB, 0);
    link_select_host_hello(&ls, USB, 10);
    link_select_port_opened(&ls, BLE, 500);
    link_select_host_hello(&ls, BLE, 600); /* ignored */
    check_out(link_select_port_closed(&ls, USB, 4000), BIT(BLE), false, true, false);
    CHECK_EQ_INT(link_select_active(&ls), LINK_NONE);
    CHECK(link_select_listening(&ls, BLE));
    /* the greeting goes out now, so the answer is timed from now */
    link_select_tick(&ls, 5000);
    CHECK(link_select_listening(&ls, BLE));
    check_out(link_select_host_hello(&ls, BLE, 5500), 0, true, false, true);
    CHECK_EQ_INT(link_select_active(&ls), BLE);
}

static void test_a_late_answer_after_the_loss_gets_a_reply(void) {
    LinkSelect ls;
    link_select_init(&ls);
    link_select_port_opened(&ls, BLE, 0);
    link_select_host_hello(&ls, BLE, 10);
    link_select_port_opened(&ls, USB, 100);
    check_out(link_select_port_closed(&ls, BLE, 1000), BIT(USB), false, true, false);
    link_select_tick(&ls, 1000 + HOST_LINK_HELLO_TIMEOUT_MS);
    check_out(link_select_host_hello(&ls, USB, 20000), BIT(USB), true, false, true);
}

static void test_losing_the_host_with_no_other_port_greets_nobody(void) {
    LinkSelect ls;
    link_select_init(&ls);
    link_select_port_opened(&ls, USB, 0);
    link_select_port_opened(&ls, BLE, 0);
    link_select_host_hello(&ls, USB, 10);
    link_select_port_closed(&ls, BLE, 0);
    check_out(link_select_port_closed(&ls, USB, 0), 0, false, true, false);
    check_out(link_select_port_closed(&ls, USB, 0), 0, false, false, false);
    check_out(link_select_port_opened(&ls, USB, 9000), BIT(USB), false, false, false);
    check_out(link_select_host_hello(&ls, USB, 9100), 0, true, false, true);
}

static void test_a_repeated_port_open_is_ignored(void) {
    LinkSelect ls;
    link_select_init(&ls);
    link_select_port_opened(&ls, USB, 0);
    check_out(link_select_port_opened(&ls, USB, 10), 0, false, false, false);
    link_select_host_hello(&ls, USB, 20);
    check_out(link_select_port_opened(&ls, USB, 30), 0, false, false, false);
    CHECK_EQ_INT(link_select_active(&ls), USB);
}

int main(void) {
    RUN_TEST(test_starts_without_a_host);
    RUN_TEST(test_one_port_handshakes_as_host_link);
    RUN_TEST(test_a_hello_on_a_closed_port_is_stale);
    RUN_TEST(test_the_first_host_to_say_hello_wins);
    RUN_TEST(test_a_port_that_opens_while_a_host_is_up_is_not_greeted);
    RUN_TEST(test_losing_the_host_greets_the_other_open_port);
    RUN_TEST(test_a_late_answer_after_the_loss_gets_a_reply);
    RUN_TEST(test_losing_the_host_with_no_other_port_greets_nobody);
    RUN_TEST(test_a_repeated_port_open_is_ignored);
    return MINITEST_RESULT();
}
