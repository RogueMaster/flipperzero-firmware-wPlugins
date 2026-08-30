/* Which BLE stack serves a role.
 *
 * Small enough to read, but it decides whether a feature is reachable at all,
 * which is what went wrong before: the dongle stack could never be chosen.
 */
#include "munit.h"

#include <seos_ble_policy.h>

/* A central role is the dongle's alone. */
static MunitResult test_central_needs_external(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_int(seos_ble_choose_stack(true, SeosBleRoleCentral), ==, SeosBleChoiceExternal);
    munit_assert_int(seos_ble_choose_stack(false, SeosBleRoleCentral), ==, SeosBleChoiceNone);
    return MUNIT_OK;
}

/* A peripheral role falls back to the Flipper's own radio. */
static MunitResult test_peripheral_falls_back(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_int(
        seos_ble_choose_stack(true, SeosBleRolePeripheral), ==, SeosBleChoiceExternal);
    munit_assert_int(seos_ble_choose_stack(false, SeosBleRolePeripheral), ==, SeosBleChoiceNative);
    return MUNIT_OK;
}

/* With the setting off, nothing reaches the dongle. That is the whole point
 * of the setting: its stack costs nothing until it is asked for. */
static MunitResult test_disabled_never_picks_external(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const SeosBleRole roles[] = {SeosBleRolePeripheral, SeosBleRoleCentral};
    for(size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        munit_assert_int(seos_ble_choose_stack(false, roles[i]), !=, SeosBleChoiceExternal);
    }
    return MUNIT_OK;
}

static MunitTest test_ble_policy_cases[] = {
    {(char*)"/central/needs-external",
     test_central_needs_external,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/peripheral/falls-back",
     test_peripheral_falls_back,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/disabled/never-external",
     test_disabled_never_picks_external,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_ble_policy_suite = {
    (char*)"/ble-policy",
    test_ble_policy_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
