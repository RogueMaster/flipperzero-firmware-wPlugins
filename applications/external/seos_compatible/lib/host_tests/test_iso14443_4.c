/* Block framing per ISO 14443-4.
 *
 * The PCB gives the block type in its top bits, whether a card identifier and
 * a node address follow it, and the block number in its low bit.
 */
#include "munit.h"
#include "test_helpers.h"

#include <string.h>

#include <seos_iso14443_4.h>

/* I-block, block number 0. Bit 2 is set in every PCB. */
#define I_BLOCK       0x02
#define I_BLOCK_NUM1  0x03
#define I_BLOCK_CID   0x0a
#define I_BLOCK_NAD   0x06
#define I_BLOCK_BOTH  0x0e
#define I_BLOCK_CHAIN 0x12

/* R-block: top bits 101. NAK sets bit 4. */
#define R_BLOCK_ACK 0xa2
#define R_BLOCK_NAK 0xb2
#define R_BLOCK_CID 0xaa

/* S-block: top bits 11. Deselect clears bits 5 and 4, WTX sets both. */
#define S_BLOCK_DESELECT 0xc2
#define S_BLOCK_WTX      0xf2

/* ---- classifying ---- */

static MunitResult test_classify(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_int(seos_iso14443_4_classify(I_BLOCK), ==, SeosIso14443_4BlockI);
    munit_assert_int(seos_iso14443_4_classify(I_BLOCK_NUM1), ==, SeosIso14443_4BlockI);
    munit_assert_int(seos_iso14443_4_classify(I_BLOCK_CHAIN), ==, SeosIso14443_4BlockI);

    munit_assert_int(seos_iso14443_4_classify(R_BLOCK_ACK), ==, SeosIso14443_4BlockR);
    munit_assert_int(seos_iso14443_4_classify(R_BLOCK_NAK), ==, SeosIso14443_4BlockR);

    munit_assert_int(seos_iso14443_4_classify(S_BLOCK_DESELECT), ==, SeosIso14443_4BlockS);
    munit_assert_int(seos_iso14443_4_classify(S_BLOCK_WTX), ==, SeosIso14443_4BlockS);

    return MUNIT_OK;
}

/* Every byte is one of the four, and the three known types never overlap. */
static MunitResult test_classify_is_total(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    unsigned counts[4] = {0};

    for(unsigned pcb = 0; pcb <= 0xff; pcb++) {
        SeosIso14443_4BlockType type = seos_iso14443_4_classify((uint8_t)pcb);
        munit_assert_int(type, >=, SeosIso14443_4BlockI);
        munit_assert_int(type, <=, SeosIso14443_4BlockUnknown);
        counts[type]++;
    }

    munit_assert_uint(counts[SeosIso14443_4BlockI], >, 0);
    munit_assert_uint(counts[SeosIso14443_4BlockR], >, 0);
    munit_assert_uint(counts[SeosIso14443_4BlockS], >, 0);
    munit_assert_uint(
        counts[SeosIso14443_4BlockI] + counts[SeosIso14443_4BlockR] +
            counts[SeosIso14443_4BlockS] + counts[SeosIso14443_4BlockUnknown],
        ==,
        256);

    return MUNIT_OK;
}

static MunitResult test_block_flags(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_true(seos_iso14443_4_is_chaining(I_BLOCK_CHAIN));
    munit_assert_false(seos_iso14443_4_is_chaining(I_BLOCK));
    /* The same bit means something else on an R-block. */
    munit_assert_false(seos_iso14443_4_is_chaining(R_BLOCK_NAK));

    munit_assert_true(seos_iso14443_4_is_nak(R_BLOCK_NAK));
    munit_assert_false(seos_iso14443_4_is_nak(R_BLOCK_ACK));
    munit_assert_false(seos_iso14443_4_is_nak(I_BLOCK_CHAIN));

    munit_assert_true(seos_iso14443_4_is_deselect(S_BLOCK_DESELECT));
    munit_assert_false(seos_iso14443_4_is_deselect(S_BLOCK_WTX));
    munit_assert_false(seos_iso14443_4_is_deselect(I_BLOCK));

    return MUNIT_OK;
}

/* ---- header length ---- */

static MunitResult test_header_len(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_size(seos_iso14443_4_header_len(I_BLOCK), ==, 1);
    munit_assert_size(seos_iso14443_4_header_len(I_BLOCK_CID), ==, 2);
    munit_assert_size(seos_iso14443_4_header_len(I_BLOCK_NAD), ==, 2);
    munit_assert_size(seos_iso14443_4_header_len(I_BLOCK_BOTH), ==, 3);

    /* Only an I-block carries a node address. The same bit on an R-block or an
     * S-block means nothing and must not add a byte. */
    munit_assert_size(seos_iso14443_4_header_len(R_BLOCK_ACK), ==, 1);
    munit_assert_size(seos_iso14443_4_header_len(R_BLOCK_CID), ==, 2);
    munit_assert_size(seos_iso14443_4_header_len(R_BLOCK_ACK | 0x04), ==, 1);
    munit_assert_size(seos_iso14443_4_header_len(S_BLOCK_DESELECT), ==, 1);

    return MUNIT_OK;
}

/* ---- where the command starts ---- */

static MunitResult test_plain_i_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t frame[] = {I_BLOCK, 0x00, 0xa4, 0x04, 0x00};
    size_t offset = 0;
    size_t apdu_len = 0;

    munit_assert_true(seos_iso14443_4_apdu_bounds(frame, sizeof(frame), &offset, &apdu_len));
    munit_assert_size(offset, ==, 1);
    munit_assert_size(apdu_len, ==, 4);

    return MUNIT_OK;
}

/* A card identifier, a node address, or both put more header ahead of the
 * command. */
static MunitResult test_addressed_blocks(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    size_t offset = 0;
    size_t apdu_len = 0;

    const uint8_t with_cid[] = {I_BLOCK_CID, 0x00, 0x00, 0xa4, 0x04, 0x00};
    munit_assert_true(seos_iso14443_4_apdu_bounds(with_cid, sizeof(with_cid), &offset, &apdu_len));
    munit_assert_size(offset, ==, 2);
    munit_assert_size(apdu_len, ==, 4);

    const uint8_t with_nad[] = {I_BLOCK_NAD, 0x00, 0x00, 0xa4, 0x04, 0x00};
    munit_assert_true(seos_iso14443_4_apdu_bounds(with_nad, sizeof(with_nad), &offset, &apdu_len));
    munit_assert_size(offset, ==, 2);
    munit_assert_size(apdu_len, ==, 4);

    const uint8_t with_both[] = {I_BLOCK_BOTH, 0x00, 0x00, 0x00, 0xa4, 0x04, 0x00};
    munit_assert_true(
        seos_iso14443_4_apdu_bounds(with_both, sizeof(with_both), &offset, &apdu_len));
    munit_assert_size(offset, ==, 3);
    munit_assert_size(apdu_len, ==, 4);

    return MUNIT_OK;
}

/* A frame carrying only its header carries no command. */
static MunitResult test_header_only(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    size_t offset = 0;
    size_t apdu_len = 0;

    const uint8_t plain[] = {I_BLOCK};
    munit_assert_false(seos_iso14443_4_apdu_bounds(plain, sizeof(plain), &offset, &apdu_len));

    const uint8_t with_cid[] = {I_BLOCK_CID, 0x00};
    munit_assert_false(
        seos_iso14443_4_apdu_bounds(with_cid, sizeof(with_cid), &offset, &apdu_len));

    const uint8_t with_both[] = {I_BLOCK_BOTH, 0x00, 0x00};
    munit_assert_false(
        seos_iso14443_4_apdu_bounds(with_both, sizeof(with_both), &offset, &apdu_len));

    return MUNIT_OK;
}

/* A frame shorter than the header its own PCB claims. */
static MunitResult test_shorter_than_its_header(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    size_t offset = 0;
    size_t apdu_len = 0;

    const uint8_t claims_cid[] = {I_BLOCK_CID};
    munit_assert_false(
        seos_iso14443_4_apdu_bounds(claims_cid, sizeof(claims_cid), &offset, &apdu_len));

    const uint8_t claims_both[] = {I_BLOCK_BOTH, 0x00};
    munit_assert_false(
        seos_iso14443_4_apdu_bounds(claims_both, sizeof(claims_both), &offset, &apdu_len));

    return MUNIT_OK;
}

static MunitResult test_empty_frame(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const uint8_t frame[] = {0x00};
    size_t offset = 0;
    size_t apdu_len = 0;

    munit_assert_false(seos_iso14443_4_apdu_bounds(frame, 0, &offset, &apdu_len));

    return MUNIT_OK;
}

/* Across every PCB and every length, what is reported stays inside what was
 * given. */
static MunitResult test_never_reports_past_the_end(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    for(unsigned pcb = 0; pcb <= 0xff; pcb++) {
        uint8_t frame[8];
        frame[0] = (uint8_t)pcb;
        for(size_t b = 1; b < sizeof(frame); b++)
            frame[b] = (uint8_t)b;

        for(size_t len = 0; len <= sizeof(frame); len++) {
            size_t offset = 0;
            size_t apdu_len = 0;
            if(seos_iso14443_4_apdu_bounds(frame, len, &offset, &apdu_len)) {
                munit_assert_size(offset, <=, len);
                munit_assert_size(apdu_len, ==, len - offset);
                munit_assert_size(apdu_len, >, 0);
            }
        }
    }

    return MUNIT_OK;
}

/* ---- answering ---- */

/* A card answers with the block number it was sent. Echoing the whole PCB
 * happens to do that for a plain I-block and nothing else. */
static MunitResult test_response_keeps_the_block_number(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_uint8(seos_iso14443_4_response_pcb(I_BLOCK, false) & 0x01, ==, 0);
    munit_assert_uint8(seos_iso14443_4_response_pcb(I_BLOCK_NUM1, false) & 0x01, ==, 1);
    munit_assert_uint8(seos_iso14443_4_response_pcb(R_BLOCK_NAK, false) & 0x01, ==, 0);
    munit_assert_uint8(seos_iso14443_4_response_pcb(R_BLOCK_NAK | 0x01, false) & 0x01, ==, 1);

    return MUNIT_OK;
}

/* An R-block is answered with an I-block, not with the R-block echoed back. */
static MunitResult test_response_to_r_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t pcb = seos_iso14443_4_response_pcb(R_BLOCK_NAK, false);
    munit_assert_int(seos_iso14443_4_classify(pcb), ==, SeosIso14443_4BlockI);
    munit_assert_false(seos_iso14443_4_is_nak(pcb));

    return MUNIT_OK;
}

/* An S-block is answered with the same S-block. */
static MunitResult test_response_to_s_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_uint8(
        seos_iso14443_4_response_pcb(S_BLOCK_DESELECT, false), ==, S_BLOCK_DESELECT);

    return MUNIT_OK;
}

/* A card identifier is carried back; a node address is not. */
static MunitResult test_response_addressing(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    uint8_t with_cid = seos_iso14443_4_response_pcb(I_BLOCK_CID, false);
    munit_assert_uint8(with_cid & 0x08, ==, 0x08);
    munit_assert_size(seos_iso14443_4_header_len(with_cid), ==, 2);

    uint8_t with_nad = seos_iso14443_4_response_pcb(I_BLOCK_NAD, false);
    munit_assert_uint8(with_nad & 0x04, ==, 0);
    munit_assert_size(seos_iso14443_4_header_len(with_nad), ==, 1);

    return MUNIT_OK;
}

static MunitResult test_response_chaining(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    munit_assert_true(seos_iso14443_4_is_chaining(seos_iso14443_4_response_pcb(I_BLOCK, true)));
    munit_assert_false(seos_iso14443_4_is_chaining(seos_iso14443_4_response_pcb(I_BLOCK, false)));

    return MUNIT_OK;
}

/* Every reply is a well-formed block of the type it should be. */
static MunitResult test_response_is_well_formed(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    for(unsigned pcb = 0; pcb <= 0xff; pcb++) {
        uint8_t reply = seos_iso14443_4_response_pcb((uint8_t)pcb, false);
        SeosIso14443_4BlockType sent = seos_iso14443_4_classify((uint8_t)pcb);
        SeosIso14443_4BlockType got = seos_iso14443_4_classify(reply);

        if(sent == SeosIso14443_4BlockS) {
            munit_assert_int(got, ==, SeosIso14443_4BlockS);
        } else {
            munit_assert_int(got, ==, SeosIso14443_4BlockI);
            /* Bit 2 is set in every PCB. */
            munit_assert_uint8(reply & 0x02, ==, 0x02);
            munit_assert_uint8(reply & 0x01, ==, (uint8_t)pcb & 0x01);
        }
    }

    return MUNIT_OK;
}

/* A block that cannot be handled is refused with an R-block carrying the same
 * block number, so the other end knows which one is meant and can recover. */
static MunitResult test_nak_pcb(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    for(unsigned pcb = 0; pcb <= 0xff; pcb++) {
        uint8_t nak = seos_iso14443_4_nak_pcb((uint8_t)pcb);

        munit_assert_int(seos_iso14443_4_classify(nak), ==, SeosIso14443_4BlockR);
        munit_assert_true(seos_iso14443_4_is_nak(nak));
        munit_assert_uint8(nak & 0x01, ==, (uint8_t)pcb & 0x01);
        munit_assert_uint8(nak & 0x02, ==, 0x02);
        /* A card identifier is carried back; nothing else is. Setting the bit
         * obliges the caller to send the byte, so the header length says how
         * long the block is. */
        munit_assert_uint8(nak & 0x08, ==, (uint8_t)pcb & 0x08);
        if(nak & 0x08) {
            munit_assert_size(seos_iso14443_4_header_len(nak), ==, 2);
        } else {
            munit_assert_size(seos_iso14443_4_header_len(nak), ==, 1);
        }
    }

    return MUNIT_OK;
}

/* ---- frame budget ---- */

static MunitResult test_payload_budget(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    /* Room for the header at its longest and the checksum. */
    munit_assert_size(seos_iso14443_4_payload_budget(256), ==, 251);
    munit_assert_size(seos_iso14443_4_payload_budget(128), ==, 123);
    munit_assert_size(seos_iso14443_4_payload_budget(64), ==, 59);

    munit_assert_size(seos_iso14443_4_payload_budget(5), ==, 0);
    munit_assert_size(seos_iso14443_4_payload_budget(0), ==, 0);

    return MUNIT_OK;
}

/* ---- the card's side ---- */

static const uint8_t CARD_ATS[] = {0x05, 0x78, 0x80, 0x70, 0x02};

/* A card that has been through the request that starts the protocol, with the
 * reader having given it card identifier 1. */
static void card_begin(SeosIso14443_4Card* card) {
    const uint8_t rats[] = {0xe0, 0x81};
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    seos_iso14443_4_card_init(card, CARD_ATS, sizeof(CARD_ATS));
    seos_iso14443_4_card_receive(card, rats, sizeof(rats), &reply, &reply_len);
}

static MunitResult test_card_start(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    seos_iso14443_4_card_init(&card, CARD_ATS, sizeof(CARD_ATS));

    /* The request to start is answered with what the card was given to say,
     * and the identifier in it is kept. */
    const uint8_t rats[] = {0xe0, 0x83};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, rats, sizeof(rats), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_size(reply_len, ==, sizeof(CARD_ATS));
    munit_assert_memory_equal(reply_len, reply, CARD_ATS);
    munit_assert_uint8(card.cid, ==, 3);

    /* A change of speed is accepted by sending its first byte back. */
    const uint8_t pps[] = {0xd3, 0x11, 0x00};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, pps, sizeof(pps), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_size(reply_len, ==, 1);
    munit_assert_uint8(reply[0], ==, 0xd3);

    /* Nothing at all gets nothing back. */
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, pps, 0, &reply, &reply_len),
        ==,
        SeosIso14443_4CardSilent);
    munit_assert_size(reply_len, ==, 0);

    return MUNIT_OK;
}

/* A command in one block is handed over whole, with the start of the block
 * that will answer it. */
static MunitResult test_card_single_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    const uint8_t plain[] = {I_BLOCK_NUM1, 0x00, 0xca, 0x00, 0x00, 0x00};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, plain, sizeof(plain), &reply, &reply_len),
        ==,
        SeosIso14443_4CardCommand);
    munit_assert_size(card.command_len, ==, 5);
    munit_assert_memory_equal(5, card.command, plain + 1);
    munit_assert_size(reply_len, ==, 1);
    munit_assert_uint8(reply[0], ==, I_BLOCK_NUM1);

    /* Reporting the answer makes room for the next command. */
    const uint8_t answer[] = {I_BLOCK_NUM1, 0x90, 0x00};
    seos_iso14443_4_card_answered(&card, answer, sizeof(answer));
    munit_assert_size(card.command_len, ==, 0);

    /* With a card identifier, the answer carries the one the reader assigned
     * and the command starts after it. */
    const uint8_t with_cid[] = {I_BLOCK_CID, 0x01, 0x00, 0xca};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, with_cid, sizeof(with_cid), &reply, &reply_len),
        ==,
        SeosIso14443_4CardCommand);
    munit_assert_size(card.command_len, ==, 2);
    munit_assert_memory_equal(2, card.command, with_cid + 2);
    munit_assert_size(reply_len, ==, 2);
    munit_assert_uint8(reply[0], ==, I_BLOCK_CID);
    munit_assert_uint8(reply[1], ==, 1);

    return MUNIT_OK;
}

/* A command sent in chained blocks is acknowledged a block at a time and
 * handed over once, joined. */
static MunitResult test_card_chained_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    const uint8_t first[] = {I_BLOCK_CHAIN, 0x00, 0xda, 0x00};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, first, sizeof(first), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_size(reply_len, ==, 1);
    munit_assert_uint8(reply[0], ==, R_BLOCK_ACK);

    /* The block number alternates, and the acknowledgement follows it. */
    const uint8_t second[] = {I_BLOCK_CHAIN | 0x01, 0x00, 0x02};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, second, sizeof(second), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_uint8(reply[0], ==, R_BLOCK_ACK | 0x01);

    const uint8_t last[] = {I_BLOCK, 0xaa, 0xbb};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, last, sizeof(last), &reply, &reply_len),
        ==,
        SeosIso14443_4CardCommand);
    const uint8_t joined[] = {0x00, 0xda, 0x00, 0x00, 0x02, 0xaa, 0xbb};
    munit_assert_size(card.command_len, ==, sizeof(joined));
    munit_assert_memory_equal(sizeof(joined), card.command, joined);
    munit_assert_uint8(reply[0], ==, I_BLOCK);

    return MUNIT_OK;
}

/* A command longer than there is room for loses the pieces that do not fit;
 * nothing is written past the end, and the reader still gets its replies. */
static MunitResult test_card_command_too_long(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    uint8_t block[1 + 200];
    memset(block, 0x5a, sizeof(block));
    block[0] = I_BLOCK_CHAIN;
    for(int i = 0; i < 4; i++) {
        munit_assert_int(
            seos_iso14443_4_card_receive(&card, block, sizeof(block), &reply, &reply_len),
            ==,
            SeosIso14443_4CardReply);
        munit_assert_size(card.command_len, <=, SEOS_ISO14443_4_CARD_COMMAND_MAX);
    }
    block[0] = I_BLOCK;
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, block, sizeof(block), &reply, &reply_len),
        ==,
        SeosIso14443_4CardCommand);
    munit_assert_size(card.command_len, <=, SEOS_ISO14443_4_CARD_COMMAND_MAX);

    return MUNIT_OK;
}

/* A block that claims a card identifier and stops there carries no command. */
static MunitResult test_card_short_block(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    const uint8_t stub[] = {I_BLOCK_CID};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, stub, sizeof(stub), &reply, &reply_len),
        ==,
        SeosIso14443_4CardCommand);
    munit_assert_size(card.command_len, ==, 0);

    return MUNIT_OK;
}

/* An R-block gets the last answer again, exactly as it went out. */
static MunitResult test_card_resend(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    /* Before anything has been answered there is nothing to send. */
    const uint8_t nak[] = {R_BLOCK_NAK};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, nak, sizeof(nak), &reply, &reply_len),
        ==,
        SeosIso14443_4CardSilent);

    const uint8_t command[] = {I_BLOCK, 0x00, 0xca, 0x00, 0x00, 0x00};
    seos_iso14443_4_card_receive(&card, command, sizeof(command), &reply, &reply_len);
    const uint8_t answer[] = {I_BLOCK, 0x44, 0x0a, 0x90, 0x00};
    seos_iso14443_4_card_answered(&card, answer, sizeof(answer));

    munit_assert_int(
        seos_iso14443_4_card_receive(&card, nak, sizeof(nak), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_size(reply_len, ==, sizeof(answer));
    munit_assert_memory_equal(sizeof(answer), reply, answer);

    /* An answer too long to keep leaves the one before it in place. */
    uint8_t huge[SEOS_ISO14443_4_CARD_ANSWER_MAX + 1] = {0};
    seos_iso14443_4_card_answered(&card, huge, sizeof(huge));
    seos_iso14443_4_card_receive(&card, nak, sizeof(nak), &reply, &reply_len);
    munit_assert_size(reply_len, ==, sizeof(answer));

    return MUNIT_OK;
}

/* S-blocks are sent back. Ending the exchange also ends a command that was
 * partway in; asking for more time carries its multiplier back. */
static MunitResult test_card_supervisory(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    const uint8_t first[] = {I_BLOCK_CHAIN, 0x00, 0xda};
    seos_iso14443_4_card_receive(&card, first, sizeof(first), &reply, &reply_len);
    munit_assert_size(card.command_len, ==, 2);

    const uint8_t deselect[] = {S_BLOCK_DESELECT};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, deselect, sizeof(deselect), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_size(reply_len, ==, 1);
    munit_assert_uint8(reply[0], ==, S_BLOCK_DESELECT);
    munit_assert_size(card.command_len, ==, 0);

    /* The top two bits of the multiplier byte are not part of it. */
    const uint8_t wtx[] = {S_BLOCK_WTX, 0xc5};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, wtx, sizeof(wtx), &reply, &reply_len),
        ==,
        SeosIso14443_4CardReply);
    munit_assert_size(reply_len, ==, 2);
    munit_assert_uint8(reply[0], ==, S_BLOCK_WTX);
    munit_assert_uint8(reply[1], ==, 0x05);

    /* With a card identifier, the one the reader assigned goes back. */
    const uint8_t deselect_cid[] = {S_BLOCK_DESELECT | 0x08, 0x01};
    seos_iso14443_4_card_receive(&card, deselect_cid, sizeof(deselect_cid), &reply, &reply_len);
    munit_assert_size(reply_len, ==, 2);
    munit_assert_uint8(reply[1], ==, 1);

    return MUNIT_OK;
}

/* The field going away mid-command leaves nothing of it for the next one. */
static MunitResult test_card_drop_command(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    SeosIso14443_4Card card;
    const uint8_t* reply = NULL;
    size_t reply_len = 0;
    card_begin(&card);

    const uint8_t first[] = {I_BLOCK_CHAIN, 0x11, 0x22};
    seos_iso14443_4_card_receive(&card, first, sizeof(first), &reply, &reply_len);
    seos_iso14443_4_card_drop_command(&card);

    const uint8_t whole[] = {I_BLOCK, 0x33};
    munit_assert_int(
        seos_iso14443_4_card_receive(&card, whole, sizeof(whole), &reply, &reply_len),
        ==,
        SeosIso14443_4CardCommand);
    munit_assert_size(card.command_len, ==, 1);
    munit_assert_uint8(card.command[0], ==, 0x33);

    /* Starting the protocol again does the same. */
    seos_iso14443_4_card_answered(&card, whole, sizeof(whole));
    seos_iso14443_4_card_receive(&card, first, sizeof(first), &reply, &reply_len);
    const uint8_t rats[] = {0xe0, 0x80};
    seos_iso14443_4_card_receive(&card, rats, sizeof(rats), &reply, &reply_len);
    munit_assert_size(card.command_len, ==, 0);

    return MUNIT_OK;
}

static MunitTest test_iso14443_4_cases[] = {
    {(char*)"/classify", test_classify, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/classify/total", test_classify_is_total, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/flags", test_block_flags, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/header-len", test_header_len, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/bounds/i-block", test_plain_i_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/bounds/addressed", test_addressed_blocks, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/bounds/header-only", test_header_only, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/bounds/short-header",
     test_shorter_than_its_header,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/bounds/empty", test_empty_frame, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/bounds/never-past-end",
     test_never_reports_past_the_end,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/response/block-number",
     test_response_keeps_the_block_number,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/response/r-block", test_response_to_r_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/response/s-block", test_response_to_s_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/response/addressing",
     test_response_addressing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/response/chaining", test_response_chaining, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/response/well-formed",
     test_response_is_well_formed,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/response/nak", test_nak_pcb, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/payload-budget", test_payload_budget, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/card/start", test_card_start, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/card/single-block", test_card_single_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/card/chained-command",
     test_card_chained_command,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/card/command-too-long",
     test_card_command_too_long,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/card/short-block", test_card_short_block, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/card/resend", test_card_resend, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/card/supervisory", test_card_supervisory, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/card/drop-command", test_card_drop_command, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_iso14443_4_suite = {
    (char*)"/iso14443-4",
    test_iso14443_4_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
