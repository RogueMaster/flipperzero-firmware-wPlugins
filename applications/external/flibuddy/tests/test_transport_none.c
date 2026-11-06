#include "minitest.h"

#include "transport_none.h"

static int rx_calls;

static void on_rx(const char* line, size_t len, void* ctx) {
    (void)line;
    (void)len;
    (void)ctx;
    rx_calls++;
}

static void test_never_opens_or_connects(void) {
    Transport* t = transport_none();
    transport_set_rx_callback(t, on_rx, NULL);
    CHECK(!transport_open(t));
    CHECK(!transport_is_connected(t));
    CHECK_EQ_INT(transport_send_line(t, "hello", 5), TransportNotSent);
    transport_close(t);
    CHECK(!transport_is_connected(t));
    CHECK_EQ_INT(rx_calls, 0);
}

static void test_is_one_shared_instance(void) {
    CHECK(transport_none() == transport_none());
}

int main(void) {
    RUN_TEST(test_never_opens_or_connects);
    RUN_TEST(test_is_one_shared_instance);
    return MINITEST_RESULT();
}
