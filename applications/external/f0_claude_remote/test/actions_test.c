#include "actions.h"
#include "test.h"
#include "transport_fake.h"

static void
    assert_event(const Transport* transport, size_t index, TransportKey key, bool pressed) {
    TransportFakeEvent event = transport_fake_event(transport, index);
    assert(event.key == key);
    assert(event.pressed == pressed);
}

static void assert_tap_sends_one_key(Action action, TransportKey key) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, action);

    assert(transport_fake_count(transport) == 2);
    assert_event(transport, 0, key, true);
    assert_event(transport, 1, key, false);
    transport_fake_free(transport);
}

static void test_enter_sends_enter(void) {
    assert_tap_sends_one_key(ActionEnter, TransportKeyEnter);
}

static void test_arrow_up_sends_arrow_up(void) {
    assert_tap_sends_one_key(ActionArrowUp, TransportKeyArrowUp);
}

static void test_arrow_down_sends_arrow_down(void) {
    assert_tap_sends_one_key(ActionArrowDown, TransportKeyArrowDown);
}

static void test_arrow_left_sends_arrow_left(void) {
    assert_tap_sends_one_key(ActionArrowLeft, TransportKeyArrowLeft);
}

static void test_arrow_right_sends_arrow_right(void) {
    assert_tap_sends_one_key(ActionArrowRight, TransportKeyArrowRight);
}

static void test_esc_sends_esc(void) {
    assert_tap_sends_one_key(ActionEsc, TransportKeyEsc);
}

static void test_clear_sends_ctrl_x_then_ctrl_l(void) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, ActionClear);

    assert(transport_fake_count(transport) == 4);
    assert_event(transport, 0, TransportKeyControlX, true);
    assert_event(transport, 1, TransportKeyControlX, false);
    assert_event(transport, 2, TransportKeyControlL, true);
    assert_event(transport, 3, TransportKeyControlL, false);
    transport_fake_free(transport);
}

static void test_voice_start_leaves_ctrl_q_down(void) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, ActionVoiceStart);

    assert(transport_fake_count(transport) == 1);
    assert_event(transport, 0, TransportKeyControlQ, true);
    transport_fake_free(transport);
}

static void test_voice_stop_releases_ctrl_q(void) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, ActionVoiceStop);

    assert(transport_fake_count(transport) == 1);
    assert_event(transport, 0, TransportKeyControlQ, false);
    transport_fake_free(transport);
}

static void test_effort_high_sends_ctrl_x_then_ctrl_f(void) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, ActionEffortHigh);

    assert(transport_fake_count(transport) == 4);
    assert_event(transport, 0, TransportKeyControlX, true);
    assert_event(transport, 1, TransportKeyControlX, false);
    assert_event(transport, 2, TransportKeyControlF, true);
    assert_event(transport, 3, TransportKeyControlF, false);
    transport_fake_free(transport);
}

static void test_effort_max_sends_ctrl_x_twice(void) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, ActionEffortMax);

    assert(transport_fake_count(transport) == 4);
    assert_event(transport, 0, TransportKeyControlX, true);
    assert_event(transport, 1, TransportKeyControlX, false);
    assert_event(transport, 2, TransportKeyControlX, true);
    assert_event(transport, 3, TransportKeyControlX, false);
    transport_fake_free(transport);
}

static void test_none_and_exit_send_nothing(void) {
    Transport* transport = transport_fake_alloc();

    actions_perform(transport, ActionNone);
    actions_perform(transport, ActionExit);

    assert(transport_fake_count(transport) == 0);
    transport_fake_free(transport);
}

static void test_give_up_withdraws_sending_nothing(void) {
    Transport* transport = transport_fake_alloc();
    assert(transport_fake_offered(transport));

    actions_perform(transport, ActionGiveUp);

    assert(!transport_fake_offered(transport));
    assert(transport_fake_count(transport) == 0);
    transport_fake_free(transport);
}

static void test_start_over_offers_sending_nothing(void) {
    Transport* transport = transport_fake_alloc();
    actions_perform(transport, ActionGiveUp);

    actions_perform(transport, ActionStartOver);

    assert(transport_fake_offered(transport));
    assert(transport_fake_count(transport) == 0);
    transport_fake_free(transport);
}

int main(void) {
    test_run(test_enter_sends_enter);
    test_run(test_arrow_up_sends_arrow_up);
    test_run(test_arrow_down_sends_arrow_down);
    test_run(test_arrow_left_sends_arrow_left);
    test_run(test_arrow_right_sends_arrow_right);
    test_run(test_esc_sends_esc);
    test_run(test_clear_sends_ctrl_x_then_ctrl_l);
    test_run(test_voice_start_leaves_ctrl_q_down);
    test_run(test_voice_stop_releases_ctrl_q);
    test_run(test_effort_high_sends_ctrl_x_then_ctrl_f);
    test_run(test_effort_max_sends_ctrl_x_twice);
    test_run(test_none_and_exit_send_nothing);
    test_run(test_give_up_withdraws_sending_nothing);
    test_run(test_start_over_offers_sending_nothing);
    return 0;
}
