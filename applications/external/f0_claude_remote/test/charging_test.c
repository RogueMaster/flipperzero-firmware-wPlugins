#include "charging.h"
#include "test.h"

static void test_charger_below_full_charges(void) {
    assert(charging_now(true, 0, false));
    assert(charging_now(true, 99, false));
}

static void test_full_charge_ends_it(void) {
    assert(!charging_now(true, 100, false));
}

static void test_no_charger_or_done_ends_it(void) {
    assert(!charging_now(false, 50, false));
    assert(!charging_now(true, 50, true));
}

int main(void) {
    test_run(test_charger_below_full_charges);
    test_run(test_full_charge_ends_it);
    test_run(test_no_charger_or_done_ends_it);
    return 0;
}
