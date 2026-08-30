/* What a long session costs in allocations.
 *
 * Wrapping and unwrapping size their scratch from the message rather than from
 * a fixed ceiling, so each one allocates. This pins how much churn that makes,
 * and that none of it is retained.
 */
#include "munit.h"
#include "test_helpers.h"

#include <secure_messaging.h>
#include <seos_protocol.h>
#include <seos_sm_command.h>

#include <stdlib.h>
#include <string.h>

#define BUFFER_CAPACITY 1024
#define EXCHANGES       2000

unsigned seos_test_allocation_count(void);
size_t seos_test_allocation_live(void);
void seos_test_allocation_reset(void);

static AuthParameters params_for(void) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = AES_128_CBC;
    params.hash = SHA256;
    for(size_t i = 0; i < sizeof(params.rndICC); i++)
        params.rndICC[i] = (uint8_t)(0x21 + i);
    for(size_t i = 0; i < sizeof(params.UID); i++)
        params.UID[i] = (uint8_t)(0x53 + i);
    return params;
}

/* Runs a long exchange and reports what the heap looked like afterwards. The
 * numbers are printed rather than asserted on: the point is to decide from
 * them, and a threshold pinned here would be a guess. */
static MunitResult test_long_session_allocation(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = params_for();
    SecureMessaging* reader = secure_messaging_alloc(&params);
    SecureMessaging* card = secure_messaging_alloc(&params);
    munit_assert_not_null(reader);
    munit_assert_not_null(card);

    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));
    credential.sio_len = 64;
    for(size_t i = 0; i < credential.sio_len; i++)
        credential.sio[i] = (uint8_t)(i * 3 + 1);

    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);

    /* Everything above is set up; only the exchange itself is counted. */
    seos_test_allocation_reset();
    size_t held_before = seos_test_allocation_live();

    for(unsigned i = 0; i < EXCHANGES; i++) {
        bit_buffer_reset(wire);
        bit_buffer_reset(answer);

        munit_assert_true(secure_messaging_wrap_apdu(
            reader,
            request,
            sizeof(request),
            (uint8_t*)SEOS_SM_HEADER,
            sizeof(SEOS_SM_HEADER),
            true,
            wire));

        munit_assert_true(seos_sm_command_handle(
            card,
            &credential,
            bit_buffer_get_data(wire),
            bit_buffer_get_size_bytes(wire),
            SEOS_SM_MAX_FRAME,
            answer,
            NULL,
            NULL));

        munit_assert_true(secure_messaging_unwrap_rapdu(reader, answer));
    }

    unsigned per_exchange = seos_test_allocation_count() / EXCHANGES;

    /* Nothing the exchange allocated is still held, so the churn is the whole
     * of the cost: no allocation outlives the message it was made for. */
    munit_assert_size(seos_test_allocation_live(), ==, held_before);

    munit_logf(MUNIT_LOG_INFO, "%u allocations per exchange", per_exchange);

    /* A wrap, a handle and an unwrap. Measured at eighteen, several of which
     * are the buffer mock taking two allocations where the device takes one,
     * so this is an upper bound on churn rather than the device's figure.
     *
     * Whether this much churn fragments anything is a question about the
     * device's allocator and cannot be answered here. */
    munit_assert_uint(per_exchange, <=, 20);

    bit_buffer_free(wire);
    bit_buffer_free(answer);
    secure_messaging_free(reader);
    secure_messaging_free(card);
    return MUNIT_OK;
}

static MunitTest test_allocation_cases[] = {
    {(char*)"/long-session", test_long_session_allocation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_allocation_suite = {
    (char*)"/allocation",
    test_allocation_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
