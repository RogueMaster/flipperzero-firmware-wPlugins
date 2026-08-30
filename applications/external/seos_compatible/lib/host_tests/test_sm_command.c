/* The shared handler for commands inside a secure messaging session.
 *
 * Each transport used to carry its own copy of this. Testing the one copy is
 * what makes the four agree.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_protocol.h>
#include <seos_sm_command.h>
#include <seos_custom_event.h>

#define BUFFER_CAPACITY 512

typedef struct {
    unsigned sio_requested;
    unsigned sio_written;
} EventLog;

static void record_event(void* context, SeosSmEvent event) {
    EventLog* log = context;
    if(event == SeosSmEventSioRequested) log->sio_requested++;
    if(event == SeosSmEventSioWritten) log->sio_written++;
}

static AuthParameters command_params(void) {
    AuthParameters params;
    memset(&params, 0, sizeof(params));
    params.cipher = AES_128_CBC;
    params.hash = SHA256;
    for(size_t i = 0; i < sizeof(params.rndICC); i++)
        params.rndICC[i] = (uint8_t)(0x10 + i);
    for(size_t i = 0; i < sizeof(params.UID); i++)
        params.UID[i] = (uint8_t)(0x20 + i);
    return params;
}

static SeosCredential credential_with_sio(size_t sio_len) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));
    credential.sio_len = sio_len;
    for(size_t i = 0; i < sio_len; i++)
        credential.sio[i] = (uint8_t)(0xc0 + i);
    return credential;
}

/* Runs one command through the handler as a reader would send it, and hands
 * back the plaintext the reader recovers from the answer. */
static uint16_t last_status_word;
static uint16_t last_protected_status_word;
static unsigned last_frames;

static size_t exchange_framed(
    SeosCredential* credential,
    const uint8_t* plain_command,
    size_t plain_command_len,
    size_t max_frame_len,
    uint8_t* recovered,
    size_t recovered_cap,
    EventLog* log);

static size_t exchange(
    SeosCredential* credential,
    const uint8_t* plain_command,
    size_t plain_command_len,
    uint8_t* recovered,
    size_t recovered_cap,
    EventLog* log) {
    return exchange_framed(
        credential,
        plain_command,
        plain_command_len,
        SEOS_SM_MAX_FRAME,
        recovered,
        recovered_cap,
        log);
}

static const uint8_t* exchange_header = SEOS_SM_HEADER;

static size_t exchange_framed(
    SeosCredential* credential,
    const uint8_t* plain_command,
    size_t plain_command_len,
    size_t max_frame_len,
    uint8_t* recovered,
    size_t recovered_cap,
    EventLog* log) {
    AuthParameters params = command_params();
    SecureMessaging* reader = secure_messaging_alloc(&params);
    SecureMessaging* card = secure_messaging_alloc(&params);
    munit_assert_not_null(reader);
    munit_assert_not_null(card);

    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        reader,
        (uint8_t*)plain_command,
        plain_command_len,
        (uint8_t*)exchange_header,
        sizeof(SEOS_SM_HEADER),
        true,
        wire));

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    seos_sm_command_handle(
        card,
        credential,
        bit_buffer_get_data(wire),
        bit_buffer_get_size_bytes(wire),
        max_frame_len,
        answer,
        record_event,
        log);

    /* Collect the pieces of a chained answer, as a reader would. */
    BitBuffer* collected = bit_buffer_alloc(BUFFER_CAPACITY);
    last_frames = 0;
    while(true) {
        size_t len = bit_buffer_get_size_bytes(answer);
        munit_assert_size(len, >=, 2);
        munit_assert_size(len, <=, max_frame_len);
        last_frames++;

        uint8_t sw1 = bit_buffer_get_byte(answer, len - 2);
        uint8_t sw2 = bit_buffer_get_byte(answer, len - 1);
        bit_buffer_append_bytes(collected, bit_buffer_get_data(answer), len - 2);
        if(sw1 != 0x61) {
            bit_buffer_append_byte(collected, sw1);
            bit_buffer_append_byte(collected, sw2);
            break;
        }
        bit_buffer_reset(answer);
        seos_sm_command_get_response(card, max_frame_len, 0x00, answer);
    }
    bit_buffer_free(answer);
    answer = collected;

    size_t recovered_len = 0;
    last_status_word = 0;
    last_protected_status_word = 0;

    size_t answer_len = bit_buffer_get_size_bytes(answer);
    munit_assert_size(answer_len, >=, 2);

    /* Every answer ends with a status word in the clear. */
    last_status_word = (uint16_t)((bit_buffer_get_byte(answer, answer_len - 2) << 8) |
                                  bit_buffer_get_byte(answer, answer_len - 1));

    if(answer_len > 2) {
        BitBuffer* body = bit_buffer_alloc(BUFFER_CAPACITY);
        bit_buffer_copy_bytes(body, bit_buffer_get_data(answer), answer_len - 2);

        munit_assert_true(secure_messaging_unwrap_rapdu(reader, body));
        last_protected_status_word = reader->last_response_sw;
        recovered_len = bit_buffer_get_size_bytes(body);
        munit_assert_size(recovered_len, <=, recovered_cap);
        memcpy(recovered, bit_buffer_get_data(body), recovered_len);
        bit_buffer_free(body);
    }

    bit_buffer_free(wire);
    bit_buffer_free(answer);
    secure_messaging_free(reader);
    secure_messaging_free(card);
    return recovered_len;
}

/* The tag list naming the SIO file gets the file back. */
static MunitResult test_returns_sio(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(20);
    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    /* fileId(2), length(1), then the file. */
    munit_assert_size(len, ==, 3 + credential.sio_len);
    munit_assert_uint8(recovered[0], ==, 0xff);
    munit_assert_uint8(recovered[1], ==, 0x00);
    munit_assert_uint8(recovered[2], ==, (uint8_t)credential.sio_len);
    munit_assert_memory_equal(credential.sio_len, recovered + 3, credential.sio);
    munit_assert_uint(log.sio_requested, ==, 1);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    munit_assert_uint16(last_protected_status_word, ==, last_status_word);
    return MUNIT_OK;
}

/* A command that is not the SIO tag list must not report one was requested. */
static MunitResult test_ignores_other_tags(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    uint8_t request[] = {0x5c, 0x02, 0xff, 0x42};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    /* An object the card does not hold is answered with success and no data. */
    munit_assert_size(len, ==, 0);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    munit_assert_uint16(last_protected_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    munit_assert_uint(log.sio_requested, ==, 0);
    return MUNIT_OK;
}

/* A plaintext shorter than the tag list must not be compared against it. */
static MunitResult test_short_plaintext(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    uint8_t request[] = {0x5c};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    /* A data field that is not a well formed tag list is a wrong-data error. */
    munit_assert_uint16(last_status_word, ==, SEOS_SW_WRONG_DATA);
    munit_assert_uint(log.sio_requested, ==, 0);
    return MUNIT_OK;
}

/* A reader asking with an extended header list gets the same answer as one
 * asking with a tag list. */
static MunitResult test_extended_header_list(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    uint8_t request[] = {0x4d, 0x03, 0xff, 0x00, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    munit_assert_size(len, ==, 3 + credential.sio_len);
    munit_assert_memory_equal(credential.sio_len, recovered + 3, credential.sio);
    munit_assert_uint(log.sio_requested, ==, 1);
    return MUNIT_OK;
}

/* Several tags at once, concatenated with nothing between them. Only one of
 * them names something this card holds. */
static MunitResult test_extended_header_list_many(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(12);
    uint8_t request[] = {0x4d, 0x06, 0xff, 0x41, 0x00, 0xff, 0x00, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    munit_assert_size(len, ==, 3 + credential.sio_len);
    munit_assert_uint(log.sio_requested, ==, 1);
    return MUNIT_OK;
}

/* A list naming only objects we do not hold is answered with no data. */
static MunitResult test_extended_header_list_unknown(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    uint8_t request[] = {0x4d, 0x06, 0xff, 0x41, 0x00, 0xff, 0x42, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    munit_assert_size(len, ==, 0);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    munit_assert_uint(log.sio_requested, ==, 0);
    return MUNIT_OK;
}

/* A tag list still names exactly one object. */
static MunitResult test_tag_list_names_one(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    uint8_t request[] = {0x5c, 0x04, 0xff, 0x00, 0xff, 0x41};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_WRONG_DATA);
    return MUNIT_OK;
}

/* A command that does not unwrap ends the session, answered in the clear. */
static MunitResult test_unwrappable_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    AuthParameters params = command_params();
    SecureMessaging* card = secure_messaging_alloc(&params);
    EventLog log = {0};

    uint8_t garbage[] = {0x0c, 0xcb, 0x3f, 0xff, 0x04, 0x85, 0x02, 0x00, 0x00};
    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_false(seos_sm_command_handle(
        card, &credential, garbage, sizeof(garbage), SEOS_SM_MAX_FRAME, answer, record_event, &log));

    /* The error is answered unprotected, and nothing else is sent. */
    munit_assert_size(bit_buffer_get_size_bytes(answer), ==, 2);
    munit_assert_uint8(bit_buffer_get_byte(answer, 0), ==, 0x69);
    munit_assert_uint(log.sio_requested, ==, 0);

    bit_buffer_free(answer);
    secure_messaging_free(card);
    return MUNIT_OK;
}

/* An empty command is not a command. */
static MunitResult test_empty_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    AuthParameters params = command_params();
    SecureMessaging* card = secure_messaging_alloc(&params);
    EventLog log = {0};

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(seos_sm_command_handle(
        card, &credential, NULL, 0, SEOS_SM_MAX_FRAME, answer, record_event, &log));
    munit_assert_size(bit_buffer_get_size_bytes(answer), ==, 0);

    bit_buffer_free(answer);
    secure_messaging_free(card);
    return MUNIT_OK;
}

/* A SIO too large for one frame comes back in pieces and reassembles. */
static MunitResult test_chained_sio(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(120);
    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    munit_assert_uint(last_frames, >, 1);
    munit_assert_size(len, ==, 3 + credential.sio_len);
    munit_assert_memory_equal(credential.sio_len, recovered + 3, credential.sio);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    return MUNIT_OK;
}

/* A small frame budget splits even a short answer, and it still reassembles. */
static MunitResult test_chaining_across_many_frames(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(40);
    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len = exchange_framed(
        &credential, request, sizeof(request), 24, recovered, sizeof(recovered), &log);

    munit_assert_uint(last_frames, >, 2);
    munit_assert_size(len, ==, 3 + credential.sio_len);
    munit_assert_memory_equal(credential.sio_len, recovered + 3, credential.sio);
    return MUNIT_OK;
}

/* Asking to continue when nothing is pending is a bad request, not a crash. */
static MunitResult test_get_response_without_pending(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = command_params();
    SecureMessaging* card = secure_messaging_alloc(&params);

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    seos_sm_command_get_response(card, SEOS_SM_MAX_FRAME, 0x00, answer);

    munit_assert_size(bit_buffer_get_size_bytes(answer), ==, 2);
    munit_assert_uint8(bit_buffer_get_byte(answer, 0), ==, 0x6a);
    munit_assert_uint8(bit_buffer_get_byte(answer, 1), ==, 0x86);

    bit_buffer_free(answer);
    secure_messaging_free(card);
    return MUNIT_OK;
}

/* A new command drops a response the reader never collected. */
static MunitResult test_new_command_drops_pending(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(120);
    AuthParameters params = command_params();
    SecureMessaging* reader = secure_messaging_alloc(&params);
    SecureMessaging* card = secure_messaging_alloc(&params);
    EventLog log = {0};

    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);

    munit_assert_true(secure_messaging_wrap_apdu(
        reader,
        request,
        sizeof(request),
        (uint8_t*)SEOS_SM_HEADER,
        sizeof(SEOS_SM_HEADER),
        true,
        wire));
    seos_sm_command_handle(
        card,
        &credential,
        bit_buffer_get_data(wire),
        bit_buffer_get_size_bytes(wire),
        SEOS_SM_MAX_FRAME,
        answer,
        record_event,
        &log);
    munit_assert_size(card->pending_len, >, 0);

    /* Walk away without collecting it. The counter still advanced for the
     * response the reader would have unwrapped, so account for that before
     * sending the next command. */
    secure_messaging_increment_context(reader);
    bit_buffer_reset(answer);
    bit_buffer_reset(wire);
    munit_assert_true(secure_messaging_wrap_apdu(
        reader,
        request,
        sizeof(request),
        (uint8_t*)SEOS_SM_HEADER,
        sizeof(SEOS_SM_HEADER),
        true,
        wire));
    seos_sm_command_handle(
        card,
        &credential,
        bit_buffer_get_data(wire),
        bit_buffer_get_size_bytes(wire),
        SEOS_SM_MAX_FRAME,
        answer,
        record_event,
        &log);

    /* The card served the new command, and what is pending belongs to it. */
    munit_assert_uint(log.sio_requested, ==, 2);
    munit_assert_size(card->pending_offset, ==, 0);

    bit_buffer_free(wire);
    bit_buffer_free(answer);
    secure_messaging_free(reader);
    secure_messaging_free(card);
    return MUNIT_OK;
}

/* A write command stores the object it carries. */
static MunitResult test_write_stores_sio(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(0);
    EventLog log = {0};

    uint8_t written[24];
    for(size_t i = 0; i < sizeof(written); i++)
        written[i] = (uint8_t)(0x40 + i);

    uint8_t command[3 + sizeof(written)];
    command[0] = 0xff;
    command[1] = 0x00;
    command[2] = (uint8_t)sizeof(written);
    memcpy(command + 3, written, sizeof(written));

    uint8_t recovered[BUFFER_CAPACITY];
    exchange_header = SEOS_SM_PUT_HEADER;
    size_t len =
        exchange(&credential, command, sizeof(command), recovered, sizeof(recovered), &log);
    exchange_header = SEOS_SM_HEADER;

    munit_assert_size(len, ==, 0);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    munit_assert_uint(log.sio_written, ==, 1);
    munit_assert_size(credential.sio_len, ==, sizeof(written));
    munit_assert_memory_equal(sizeof(written), credential.sio, written);
    return MUNIT_OK;
}

/* A write claiming more than there is room for is refused, and leaves what
 * was already stored alone. */
static MunitResult test_write_bounds(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(4);
    uint8_t original[4];
    memcpy(original, credential.sio, sizeof(original));
    EventLog log = {0};

    /* A length beyond the room there is, with a body to match. */
    uint8_t command[3 + 200];
    memset(command, 0xee, sizeof(command));
    command[0] = 0xff;
    command[1] = 0x00;
    command[2] = 200;

    uint8_t recovered[BUFFER_CAPACITY];
    exchange_header = SEOS_SM_PUT_HEADER;
    exchange(&credential, command, 3 + 160, recovered, sizeof(recovered), &log);
    exchange_header = SEOS_SM_HEADER;

    munit_assert_uint16(last_status_word, ==, SEOS_SW_NOT_ENOUGH_ROOM);
    munit_assert_uint(log.sio_written, ==, 0);
    munit_assert_size(credential.sio_len, ==, sizeof(original));
    munit_assert_memory_equal(sizeof(original), credential.sio, original);
    return MUNIT_OK;
}

/* A write naming an object the card does not hold is refused. */
static MunitResult test_write_unknown_tag(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(4);
    EventLog log = {0};

    uint8_t command[] = {0xff, 0x42, 0x02, 0xaa, 0xbb};
    uint8_t recovered[BUFFER_CAPACITY];
    exchange_header = SEOS_SM_PUT_HEADER;
    exchange(&credential, command, sizeof(command), recovered, sizeof(recovered), &log);
    exchange_header = SEOS_SM_HEADER;

    munit_assert_uint16(last_status_word, ==, SEOS_SW_NOT_ENOUGH_ROOM);
    munit_assert_uint(log.sio_written, ==, 0);
    munit_assert_size(credential.sio_len, ==, 4);
    return MUNIT_OK;
}

/* Only these two instructions, on this file, are ours to answer. */
static MunitResult test_matches_only_our_commands(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t get_data[] = {0x0c, 0xcb, 0x3f, 0xff};
    uint8_t put_data[] = {0x0c, 0xdb, 0x3f, 0xff};
    uint8_t other_ins[] = {0x0c, 0xa4, 0x3f, 0xff};
    uint8_t other_file[] = {0x0c, 0xcb, 0x00, 0x00};
    uint8_t plain_cla[] = {0x00, 0xcb, 0x3f, 0xff};

    munit_assert_true(seos_sm_command_matches(get_data, sizeof(get_data)));
    munit_assert_true(seos_sm_command_matches(put_data, sizeof(put_data)));
    munit_assert_false(seos_sm_command_matches(other_ins, sizeof(other_ins)));
    munit_assert_false(seos_sm_command_matches(other_file, sizeof(other_file)));
    munit_assert_false(seos_sm_command_matches(plain_cla, sizeof(plain_cla)));
    munit_assert_false(seos_sm_command_matches(get_data, 3));
    return MUNIT_OK;
}

/* A hundred-and-twenty-eight byte object needs the long form length. Written
 * as a bare byte, 0x80 reads back as a length header. */
static MunitResult test_long_object_length(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(128);
    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    size_t len =
        exchange(&credential, request, sizeof(request), recovered, sizeof(recovered), &log);

    /* Tag, then 81 80, then the object. */
    munit_assert_size(len, ==, 4 + credential.sio_len);
    munit_assert_uint8(recovered[0], ==, 0xff);
    munit_assert_uint8(recovered[1], ==, 0x00);
    munit_assert_uint8(recovered[2], ==, 0x81);
    munit_assert_uint8(recovered[3], ==, 0x80);
    munit_assert_memory_equal(credential.sio_len, recovered + 4, credential.sio);
    return MUNIT_OK;
}

/* And a write carrying the long form is stored, not refused. */
static MunitResult test_write_long_object_length(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(0);
    EventLog log = {0};

    uint8_t written[128];
    for(size_t i = 0; i < sizeof(written); i++)
        written[i] = (uint8_t)(i ^ 0x5a);

    uint8_t command[4 + sizeof(written)];
    command[0] = 0xff;
    command[1] = 0x00;
    command[2] = 0x81;
    command[3] = (uint8_t)sizeof(written);
    memcpy(command + 4, written, sizeof(written));

    uint8_t recovered[BUFFER_CAPACITY];
    exchange_header = SEOS_SM_PUT_HEADER;
    exchange(&credential, command, sizeof(command), recovered, sizeof(recovered), &log);
    exchange_header = SEOS_SM_HEADER;

    munit_assert_uint16(last_status_word, ==, SEOS_SW_SUCCESS_VALUE);
    munit_assert_size(credential.sio_len, ==, sizeof(written));
    munit_assert_memory_equal(sizeof(written), credential.sio, written);
    return MUNIT_OK;
}

/* The chained continuation command carries an Le byte. */
static MunitResult test_get_response_has_le(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_size(sizeof(SEOS_GET_RESPONSE), ==, 5);
    munit_assert_uint8(SEOS_GET_RESPONSE[0], ==, 0x00);
    munit_assert_uint8(SEOS_GET_RESPONSE[1], ==, 0xc0);
    return MUNIT_OK;
}

/* What the reader does with each status word a card can answer with. */
static MunitResult test_exchange_steps(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t length = 0xaa;

    munit_assert_int(seos_sm_next_step(0x90, 0x00, false, &length), ==, SeosExchangeDone);

    /* More waiting: ask for as much as the card named. */
    munit_assert_int(seos_sm_next_step(0x61, 0x2f, false, &length), ==, SeosExchangeContinue);
    munit_assert_uint8(length, ==, 0x2f);

    /* A card that wants a different expected length gets one more try. */
    munit_assert_int(seos_sm_next_step(0x6c, 0x40, false, &length), ==, SeosExchangeResend);
    munit_assert_uint8(length, ==, 0x40);

    /* But only one, or a card asking forever would hold the reader. */
    munit_assert_int(seos_sm_next_step(0x6c, 0x40, true, &length), ==, SeosExchangeFailed);

    /* Anything else ends it. */
    munit_assert_int(seos_sm_next_step(0x6a, 0x82, false, &length), ==, SeosExchangeFailed);
    munit_assert_int(seos_sm_next_step(0x69, 0x88, false, &length), ==, SeosExchangeFailed);
    munit_assert_int(seos_sm_next_step(0x90, 0x01, false, &length), ==, SeosExchangeFailed);
    return MUNIT_OK;
}

/* A write and a read are opposite directions and must not share a view event.
 * They did, and the emulation scenes answer the read event by offering to save
 * over the file the credential was loaded from. */
static MunitResult test_write_and_read_events_differ(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint32_t requested = seos_sm_event_to_custom_event(SeosSmEventSioRequested);
    uint32_t written = seos_sm_event_to_custom_event(SeosSmEventSioWritten);

    munit_assert_uint32(requested, ==, SeosCustomEventSIORequested);
    munit_assert_uint32(written, ==, SeosCustomEventSIOWritten);
    munit_assert_uint32(written, !=, requested);
    /* Nor the event a card read raises. */
    munit_assert_uint32(written, !=, SeosCustomEventPollerSuccess);

    return MUNIT_OK;
}

/* The reader states how much it wants, and the data field must not exceed it.
 * A reader asking for sixteen more bytes used to be handed a full frame. */
static MunitResult test_get_response_honours_le(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    AuthParameters params = command_params();
    SecureMessaging* card = secure_messaging_alloc(&params);
    SeosCredential credential = credential_with_sio(120);

    /* A read that will not fit one frame, so something is left pending. */
    uint8_t request[] = {0x5c, 0x02, 0xff, 0x00};
    SecureMessaging* reader = secure_messaging_alloc(&params);
    BitBuffer* wire = bit_buffer_alloc(BUFFER_CAPACITY);
    munit_assert_true(secure_messaging_wrap_apdu(
        reader,
        request,
        sizeof(request),
        (uint8_t*)SEOS_SM_HEADER,
        sizeof(SEOS_SM_HEADER),
        true,
        wire));

    BitBuffer* answer = bit_buffer_alloc(BUFFER_CAPACITY);
    seos_sm_command_handle(
        card,
        &credential,
        bit_buffer_get_data(wire),
        bit_buffer_get_size_bytes(wire),
        SEOS_SM_MAX_FRAME,
        answer,
        NULL,
        NULL);

    /* The first frame said more was coming. Ask for only sixteen of it. */
    bit_buffer_reset(answer);
    seos_sm_command_get_response(card, SEOS_SM_MAX_FRAME, 0x10, answer);

    /* The data field, status word aside, is what was asked for and no more. */
    munit_assert_size(bit_buffer_get_size_bytes(answer), ==, 0x10 + sizeof(uint16_t));

    bit_buffer_free(wire);
    bit_buffer_free(answer);
    secure_messaging_free(card);
    secure_messaging_free(reader);
    return MUNIT_OK;
}

/* A header list without the length beside each tag is not a header list. */
static MunitResult test_refuses_bare_tags_in_a_header_list(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosCredential credential = credential_with_sio(8);
    uint8_t recovered[BUFFER_CAPACITY];
    EventLog log = {0};

    /* Tags with no lengths between them, which is a tag list's shape. */
    uint8_t bare[] = {0x4d, 0x04, 0xff, 0x00, 0xff, 0x41};
    exchange(&credential, bare, sizeof(bare), recovered, sizeof(recovered), &log);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_WRONG_DATA);

    /* A header asking for part of an object, which is not served. */
    uint8_t partial[] = {0x4d, 0x03, 0xff, 0x00, 0x04};
    exchange(&credential, partial, sizeof(partial), recovered, sizeof(recovered), &log);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_WRONG_DATA);

    /* A tag with its length missing off the end. */
    uint8_t truncated[] = {0x4d, 0x02, 0xff, 0x00};
    exchange(&credential, truncated, sizeof(truncated), recovered, sizeof(recovered), &log);
    munit_assert_uint16(last_status_word, ==, SEOS_SW_WRONG_DATA);

    munit_assert_uint(log.sio_requested, ==, 0);
    return MUNIT_OK;
}

static MunitTest test_sm_command_cases[] = {
    {(char*)"/header-list/needs-lengths",
     test_refuses_bare_tags_in_a_header_list,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/get-response/honours-le",
     test_get_response_honours_le,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/events/write-differs-from-read",
     test_write_and_read_events_differ,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/returned", test_returns_sio, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sio/other-tag", test_ignores_other_tags, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sio/short-plaintext", test_short_plaintext, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/sio/extended-header-list",
     test_extended_header_list,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/extended-header-list-many",
     test_extended_header_list_many,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/extended-header-list-unknown",
     test_extended_header_list_unknown,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/sio/tag-list-names-one",
     test_tag_list_names_one,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/length/long-form-read",
     test_long_object_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/length/long-form-write",
     test_write_long_object_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/chaining/get-response-le",
     test_get_response_has_le,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/write/stores", test_write_stores_sio, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/write/bounds", test_write_bounds, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/write/unknown-tag", test_write_unknown_tag, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/exchange/steps", test_exchange_steps, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/matches", test_matches_only_our_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/chaining/large-sio", test_chained_sio, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/chaining/many-frames",
     test_chaining_across_many_frames,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/chaining/nothing-pending",
     test_get_response_without_pending,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/chaining/superseded",
     test_new_command_drops_pending,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reject/unwrappable",
     test_unwrappable_command,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/reject/empty", test_empty_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_sm_command_suite = {
    (char*)"/sm-command",
    test_sm_command_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
