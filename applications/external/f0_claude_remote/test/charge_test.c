#include "charge.h"
#include "test.h"

static void test_no_charge_fills_no_cell(void) {
    assert(charge_cells(0, 20) == 0);
}

static void test_part_of_a_cell_fills_it(void) {
    assert(charge_cells(1, 20) == 1);
    assert(charge_cells(5, 20) == 1);
    assert(charge_cells(6, 20) == 2);
    assert(charge_cells(94, 20) == 19);
    assert(charge_cells(95, 20) == 19);
}

static void test_full_charge_or_more_fills_every_cell(void) {
    assert(charge_cells(96, 20) == 20);
    assert(charge_cells(100, 20) == 20);
    assert(charge_cells(101, 20) == 20);
    assert(charge_cells(255, 20) == 20);
}

int main(void) {
    test_run(test_no_charge_fills_no_cell);
    test_run(test_part_of_a_cell_fills_it);
    test_run(test_full_charge_or_more_fills_every_cell);
    return 0;
}
