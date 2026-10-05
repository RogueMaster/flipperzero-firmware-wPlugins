/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * ISO-DEP, the block transmission protocol of ISO/IEC 14443-4.
 *
 * This layer exists because the firmware's own ISO 14443-4A poller gives a
 * card 120 microseconds to answer whenever its ATS carries no TB1, which no
 * smartcard can meet; see transport/emrtd_isodep.h. The timings are therefore
 * not decoration and the first group of checks pins them.
 *
 * The rest drives the layer against a simulated chip that implements the card
 * half of the protocol: it reassembles a chained command, chains its own
 * answer, asks for a waiting time extension, numbers its blocks by the rules
 * of section 7.5.3, and can lose a frame on the way in or its answer on the
 * way out, so that recovery has something to recover from.
 */

#include "emrtd_test.h"

#include <stdlib.h>

#include "../../transport/emrtd_isodep.h"

/* --- A chip that speaks the card half ----------------------------------- */

#define CHIP_APDU_MAX 1024

typedef struct {
    const uint8_t* ats;
    size_t ats_len;

    bool activated;
    uint8_t block_number; /**< The card's own, rules C to E of ISO/IEC 14443-4. */

    /* The block last sent, which rule 11 has the card send again. */
    uint8_t last_block[CHIP_APDU_MAX + 1];
    size_t last_block_len;

    uint8_t command[CHIP_APDU_MAX]; /**< The command, reassembled. */
    size_t command_len;

    const uint8_t* response;
    size_t response_len;
    size_t response_sent;
    size_t response_chunk; /**< 0 means the whole answer in one frame. */

    unsigned wtx_pending; /**< Extensions still to be asked for. */
    uint8_t wtxm;
    unsigned wtx_answered;

    unsigned drop_skip; /**< Frames to let through before the dropping starts. */
    unsigned drop_next; /**< Frames to swallow before the card sees them. */
    unsigned lose_skip; /**< Answers to deliver before the losing starts. */
    unsigned lose_answers; /**< Answers the card sends that never arrive. */
    bool lose_damaged; /**< A lost answer arrives with a bad CRC, not at all. */

    /* The protocol control byte of every frame the reader sent, in order. */
    uint8_t pcbs[32];
    unsigned pcb_count;

    uint32_t last_fwt;
    unsigned frames;
    unsigned commands; /**< Complete commands received, to catch a replay. */

    /*
     * Nobody sleeps in a test. The guard time the reader would have waited is
     * recorded instead, and whether it waited before the first block is what
     * the checks are really about.
     */
    uint32_t waited_ms;
    unsigned waits;
    unsigned frames_at_wait;
} Chip;

static void chip_delay(void* context, uint32_t ms) {
    Chip* chip = context;
    chip->waits++;
    chip->waited_ms += ms;
    chip->frames_at_wait = chip->frames;
}

/**
 * Put a block on the wire and remember it, because rule 11 may ask for it again.
 *
 * An answer the test has told the card to lose is still sent, as far as the
 * card knows: its state has moved on, and only the reader never hears it.
 */
static EmrtdError chip_send(
    Chip* chip,
    const uint8_t* block,
    size_t len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len) {
    if(len > rx_cap || len > sizeof(chip->last_block)) {
        return EmrtdErrorBufferTooSmall;
    }
    /* Rule 11 sends the stored block itself, which must not be copied onto itself. */
    if(block != chip->last_block) {
        memcpy(chip->last_block, block, len);
        chip->last_block_len = len;
    }

    if(chip->lose_skip > 0) {
        chip->lose_skip--;
    } else if(chip->lose_answers > 0) {
        chip->lose_answers--;
        /* What the port makes of a bad CRC, as against a frame that never came. */
        return chip->lose_damaged ? EmrtdErrorTransport : EmrtdErrorCardLost;
    }
    memcpy(rx, block, len);
    *rx_len = len;
    return EmrtdErrorNone;
}

/** Put one frame of the answer on the wire, chaining if the chip was told to. */
static EmrtdError chip_answer(Chip* chip, uint8_t* rx, size_t rx_cap, size_t* rx_len) {
    const size_t remaining = chip->response_len - chip->response_sent;
    size_t chunk = chip->response_chunk;
    if(chunk == 0 || chunk > remaining) {
        chunk = remaining;
    }
    const bool last = (chip->response_sent + chunk) == chip->response_len;

    uint8_t block[CHIP_APDU_MAX + 1];
    if(1 + chunk > sizeof(block)) {
        return EmrtdErrorBufferTooSmall;
    }
    block[0] = (uint8_t)(0x02 | (chip->block_number & 0x01) | (last ? 0x00 : 0x10));
    memcpy(block + 1, chip->response + chip->response_sent, chunk);
    chip->response_sent += chunk;
    return chip_send(chip, block, 1 + chunk, rx, rx_cap, rx_len);
}

/** Ask the reader for more time, as rule 9 allows instead of an answer. */
static EmrtdError chip_ask_wtx(Chip* chip, uint8_t* rx, size_t rx_cap, size_t* rx_len) {
    const uint8_t block[2] = {0xF2, chip->wtxm};
    return chip_send(chip, block, sizeof(block), rx, rx_cap, rx_len);
}

static EmrtdError chip_frame(
    void* context,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t fwt_fc) {
    Chip* chip = context;

    chip->frames++;
    chip->last_fwt = fwt_fc;
    *rx_len = 0;
    if(tx_len > 0 && chip->pcb_count < sizeof(chip->pcbs)) {
        chip->pcbs[chip->pcb_count++] = tx[0];
    }

    if(chip->drop_skip > 0) {
        chip->drop_skip--;
    } else if(chip->drop_next > 0) {
        chip->drop_next--;
        return EmrtdErrorCardLost;
    }
    if(tx_len == 0) {
        return EmrtdErrorInvalidInput;
    }

    const uint8_t pcb = tx[0];

    /* RATS, which is the only thing an unactivated card answers. */
    if(pcb == 0xE0) {
        if(chip->ats_len > rx_cap) {
            return EmrtdErrorBufferTooSmall;
        }
        memcpy(rx, chip->ats, chip->ats_len);
        *rx_len = chip->ats_len;
        chip->activated = true;
        /* Rule C: the card's block number starts at 1. */
        chip->block_number = 1;
        return EmrtdErrorNone;
    }
    if(!chip->activated) {
        return EmrtdErrorCardLost;
    }

    /* S(DESELECT). */
    if((pcb & 0xF7) == 0xC2) {
        rx[0] = 0xC2;
        *rx_len = 1;
        chip->activated = false;
        return EmrtdErrorNone;
    }

    /* S(WTX): the reader granting the time that was asked for. */
    if((pcb & 0xF7) == 0xF2) {
        chip->wtx_answered++;
        if(tx_len < 2 || (tx[1] & 0x3F) != chip->wtxm) {
            return EmrtdErrorProtocol;
        }
        if(chip->wtx_pending > 0) {
            chip->wtx_pending--;
        }
        if(chip->wtx_pending > 0) {
            return chip_ask_wtx(chip, rx, rx_cap, rx_len);
        }
        return chip_answer(chip, rx, rx_cap, rx_len);
    }

    /* R-blocks. */
    if((pcb & 0xE6) == 0xA2) {
        const bool nak = (pcb & 0x10) != 0;
        const bool current = (pcb & 0x01) == chip->block_number;

        /* Rule 11: the reader lost what the card last sent, so it goes again. */
        if(current) {
            return chip_send(chip, chip->last_block, chip->last_block_len, rx, rx_cap, rx_len);
        }
        /* Rule 12: the reader lost a block the card never had, and is told so. */
        if(nak) {
            const uint8_t ack[1] = {(uint8_t)(0xA2 | chip->block_number)};
            return chip_send(chip, ack, sizeof(ack), rx, rx_cap, rx_len);
        }
        /*
         * Rules E and 13: the next block of a chained answer. Outside a chain
         * this is a protocol error, and section 7.5.5 has the card attempt no
         * recovery: it goes back to listening, and the reader hears nothing.
         */
        if(chip->response_sent == 0 || chip->response_sent >= chip->response_len) {
            return EmrtdErrorCardLost;
        }
        chip->block_number ^= 1;
        return chip_answer(chip, rx, rx_cap, rx_len);
    }

    /* I-block. */
    if((pcb & 0xE2) == 0x02) {
        /*
         * Rule D, and the reason recovery is never a second copy of this block:
         * the card toggles its number and executes whatever I-block arrives,
         * whichever number it carries.
         */
        chip->block_number ^= 1;

        const size_t inf = tx_len - 1;
        if(chip->command_len + inf > CHIP_APDU_MAX) {
            return EmrtdErrorBufferTooSmall;
        }
        memcpy(chip->command + chip->command_len, tx + 1, inf);
        chip->command_len += inf;

        if(pcb & 0x10) {
            /* Chained: rule 2, an acknowledgement carrying the card's number. */
            const uint8_t ack[1] = {(uint8_t)(0xA2 | chip->block_number)};
            return chip_send(chip, ack, sizeof(ack), rx, rx_cap, rx_len);
        }

        chip->commands++;
        chip->response_sent = 0;
        if(chip->wtx_pending > 0) {
            return chip_ask_wtx(chip, rx, rx_cap, rx_len);
        }
        return chip_answer(chip, rx, rx_cap, rx_len);
    }

    return EmrtdErrorProtocol;
}

static void chip_init(Chip* chip, const uint8_t* ats, size_t ats_len) {
    memset(chip, 0, sizeof(*chip));
    chip->ats = ats;
    chip->ats_len = ats_len;
    chip->wtxm = 1;
}

/* --- The ATS, and the timings that come out of it ----------------------- */

/** Parse an ATS given as hex and return the layer's view of it. */
static void parse_ats(EmrtdIsoDep* isodep, const char* hex) {
    uint8_t ats[64];
    const size_t len = emrtd_test_hex(hex, ats, sizeof(ats));
    emrtd_isodep_init(isodep, NULL, NULL, NULL);
    emrtd_isodep_parse_ats(isodep, ats, len);
}

static void test_ats(void) {
    EmrtdIsoDep isodep;

    emrtd_test_begin("a passport ATS: frame size and waiting time index");
    /* TL T0 TA1 TB1 TC1: T0 = 78 is FSCI 8 with all three interface bytes. */
    parse_ats(&isodep, "0578807002");
    TEST_EQ_INT(emrtd_isodep_fsc(&isodep), 256);
    TEST_EQ_INT(isodep.fwi, 7);
    TEST_CHECK(isodep.fwi_announced);

    emrtd_test_begin("TB1 carries the start-up guard time as well as the waiting time");
    /* TB1 = 0x96 is FWI 9 and SFGI 6, which is what a real passport announced. */
    parse_ats(&isodep, "0578809602");
    TEST_EQ_INT(isodep.fwi, 9);
    TEST_EQ_INT(isodep.sfgi, 6);
    /* 4096 << 6 cycles is 19.33 ms; rounded up and with the margin, 22. */
    TEST_EQ_INT(emrtd_isodep_sfgt_ms(&isodep), 20 + EMRTD_ISODEP_SFGT_MARGIN_MS);

    emrtd_test_begin("SFGI 0 asks for no guard time, so none is taken");
    parse_ats(&isodep, "0578809002");
    TEST_EQ_INT(isodep.sfgi, 0);
    TEST_EQ_INT(emrtd_isodep_sfgt_ms(&isodep), 0);

    emrtd_test_begin("a card that announces no TB1 asks for no guard time either");
    parse_ats(&isodep, "031188");
    TEST_EQ_INT(emrtd_isodep_sfgt_ms(&isodep), 0);

    emrtd_test_begin("FWI 9 is below the floor, so the floor is what is waited");
    parse_ats(&isodep, "0578809002");
    TEST_EQ_INT(isodep.fwi, 9);
    /* 4096 << 9 is 2097152 cycles, about 155 ms, which the reference found marginal. */
    TEST_EQ_INT(isodep.fwt_fc, EMRTD_ISODEP_FWT_MIN_FC);

    emrtd_test_begin("a large FWI is honoured above the floor");
    parse_ats(&isodep, "057880D002");
    TEST_EQ_INT(isodep.fwi, 13);
    TEST_EQ_INT(isodep.fwt_fc, 4096u << 13);

    emrtd_test_begin("FWI 15 is reserved, and is read as the largest that is not");
    parse_ats(&isodep, "057880F002");
    TEST_EQ_INT(isodep.fwt_fc, EMRTD_ISODEP_FWT_MAX_FC);

    /*
     * This is the case the firmware's own poller gets wrong: with no TB1 it
     * falls back on ISO14443_3A_FDT_POLL_FC, 1620 cycles, and hands that to
     * every I-block. 1620 cycles is 120 microseconds.
     */
    emrtd_test_begin("an ATS without TB1 still gets a waiting time a chip can meet");
    parse_ats(&isodep, "031188");
    TEST_CHECK(!isodep.fwi_announced);
    TEST_EQ_INT(isodep.fwt_fc, EMRTD_ISODEP_FWT_MIN_FC);
    TEST_CHECK(isodep.fwt_fc > 1620u * 100u);

    emrtd_test_begin("TL alone means the ISO defaults");
    parse_ats(&isodep, "01");
    TEST_EQ_INT(emrtd_isodep_fsc(&isodep), 32);
    TEST_EQ_INT(isodep.fwt_fc, EMRTD_ISODEP_FWT_MIN_FC);

    emrtd_test_begin("no ATS at all is survivable");
    emrtd_isodep_init(&isodep, NULL, NULL, NULL);
    emrtd_isodep_parse_ats(&isodep, NULL, 0);
    TEST_EQ_INT(emrtd_isodep_fsc(&isodep), 32);
    TEST_EQ_INT(isodep.fwt_fc, EMRTD_ISODEP_FWT_MIN_FC);

    emrtd_test_begin("a frame size beyond what the reader announced is capped");
    /* FSCI 12 is 4096 bytes; the reader offered 256 in RATS. */
    parse_ats(&isodep, "057C809002");
    TEST_EQ_INT(emrtd_isodep_fsc(&isodep), 256);

    emrtd_test_begin("TB1 is found even when TA1 is absent");
    /* T0 = 68: TC1 and TB1 present, TA1 absent, FSCI 8. */
    parse_ats(&isodep, "0468A002");
    TEST_EQ_INT(isodep.fwi, 10);
    TEST_CHECK(isodep.fwi_announced);

    emrtd_test_begin("a TL longer than the bytes that arrived does not read past them");
    parse_ats(&isodep, "0A78");
    TEST_EQ_INT(emrtd_isodep_fsc(&isodep), 256);
    TEST_CHECK(!isodep.fwi_announced);
}

/* --- Activation --------------------------------------------------------- */

static void test_activation(void) {
    static const uint8_t ats[] = {0x05, 0x78, 0x80, 0x70, 0x02};
    Chip chip;
    EmrtdIsoDep isodep;

    emrtd_test_begin("RATS asks for 256 byte frames and no card identifier");
    chip_init(&chip, ats, sizeof(ats));
    emrtd_isodep_init(&isodep, chip_frame, chip_delay, &chip);
    TEST_EQ_INT(emrtd_isodep_activate(&isodep), EmrtdErrorNone);
    TEST_CHECK(chip.activated);
    TEST_EQ_INT(chip.frames, 1);

    emrtd_test_begin("the ATS is kept for the log, exactly as it arrived");
    size_t len = 0;
    const uint8_t* stored = emrtd_isodep_ats(&isodep, &len);
    TEST_CHECK(stored != NULL);
    TEST_EQ_HEX(stored, len, "0578807002");

    emrtd_test_begin("RATS is given a window far above what the standard requires");
    /* ISO/IEC 14443-4 allows 65536 cycles for the answer to RATS. */
    TEST_CHECK(chip.last_fwt >= 65536u);
    TEST_EQ_INT(chip.last_fwt, EMRTD_ISODEP_RATS_FWT_FC);

    emrtd_test_begin("the guard time is waited out after the ATS, before any block");
    static const uint8_t slow_ats[] = {0x05, 0x78, 0x80, 0x96, 0x02};
    static const uint8_t answer[] = {0x90, 0x00};
    chip_init(&chip, slow_ats, sizeof(slow_ats));
    emrtd_isodep_init(&isodep, chip_frame, chip_delay, &chip);
    TEST_EQ_INT(emrtd_isodep_activate(&isodep), EmrtdErrorNone);
    TEST_EQ_INT(chip.waits, 1);
    TEST_EQ_INT(chip.waited_ms, 20 + EMRTD_ISODEP_SFGT_MARGIN_MS);
    /* One frame had gone at that point: RATS, and nothing after it. */
    TEST_EQ_INT(chip.frames_at_wait, 1);

    emrtd_test_begin("and it is waited once, not before every command");
    chip.response = answer;
    chip.response_len = sizeof(answer);
    uint8_t guard_rx[64];
    size_t guard_rx_len = 0;
    const uint8_t probe[] = {0x00, 0xA4, 0x04, 0x0C};
    TEST_EQ_INT(
        emrtd_isodep_transceive(
            &isodep, probe, sizeof(probe), guard_rx, sizeof(guard_rx), &guard_rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(chip.waits, 1);

    emrtd_test_begin("a card asking for no guard time is not made to wait");
    chip_init(&chip, ats, sizeof(ats));
    emrtd_isodep_init(&isodep, chip_frame, chip_delay, &chip);
    TEST_EQ_INT(emrtd_isodep_activate(&isodep), EmrtdErrorNone);
    TEST_EQ_INT(chip.waits, 0);

    emrtd_test_begin("a card that never answers RATS is not tried again from here");
    chip_init(&chip, ats, sizeof(ats));
    chip.drop_next = 1;
    emrtd_isodep_init(&isodep, chip_frame, chip_delay, &chip);
    TEST_EQ_INT(emrtd_isodep_activate(&isodep), EmrtdErrorCardLost);
    TEST_EQ_INT(chip.frames, 1);

    emrtd_test_begin("an exchange before activation is refused");
    uint8_t rx[64];
    size_t rx_len = 0;
    const uint8_t apdu[] = {0x00, 0xA4, 0x04, 0x0C};
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, apdu, sizeof(apdu), rx, sizeof(rx), &rx_len),
        EmrtdErrorNoCard);
}

/* --- Exchanging APDUs --------------------------------------------------- */

/** Activate against a chip and leave both ready for an exchange. */
static void open_session(Chip* chip, EmrtdIsoDep* isodep, const uint8_t* ats, size_t ats_len) {
    chip_init(chip, ats, ats_len);
    emrtd_isodep_init(isodep, chip_frame, chip_delay, chip);
    TEST_EQ_INT(emrtd_isodep_activate(isodep), EmrtdErrorNone);
    chip->command_len = 0;
}

static void test_exchange(void) {
    static const uint8_t ats[] = {0x05, 0x78, 0x80, 0x70, 0x02};
    static const uint8_t response[] = {0x6F, 0x02, 0x84, 0x00, 0x90, 0x00};
    static const uint8_t select[] = {
        0x00, 0xA4, 0x04, 0x0C, 0x07, 0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};

    Chip chip;
    EmrtdIsoDep isodep;
    uint8_t rx[512];
    size_t rx_len = 0;

    emrtd_test_begin("one APDU there and back");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, select, sizeof(select), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(chip.command, chip.command_len, "00A4040C07A0000002471001");
    TEST_EQ_HEX(rx, rx_len, "6F02840090 00");

    emrtd_test_begin("the block number toggles between commands");
    /* The reader started at 0, so the second command carries 1. */
    chip.command_len = 0;
    chip.response_sent = 0;
    TEST_EQ_INT(isodep.block_number, 1);
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, select, sizeof(select), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(isodep.block_number, 0);
    TEST_EQ_INT(chip.commands, 2);

    emrtd_test_begin("a command longer than one frame is chained");
    /* FSCI 2 is 32 bytes, which leaves 28 for the information field. */
    static const uint8_t small_ats[] = {0x05, 0x72, 0x80, 0x70, 0x02};
    open_session(&chip, &isodep, small_ats, sizeof(small_ats));
    TEST_EQ_INT(emrtd_isodep_fsc(&isodep), 32);

    uint8_t long_command[70];
    for(size_t i = 0; i < sizeof(long_command); i++) {
        long_command[i] = (uint8_t)i;
    }
    chip.response = response;
    chip.response_len = sizeof(response);
    unsigned before = chip.frames;
    TEST_EQ_INT(
        emrtd_isodep_transceive(
            &isodep, long_command, sizeof(long_command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(chip.command_len, sizeof(long_command));
    TEST_CHECK(memcmp(chip.command, long_command, sizeof(long_command)) == 0);
    /* 70 bytes in 28 byte pieces is three blocks; the last one brings the answer. */
    TEST_EQ_INT(chip.frames - before, 3);
    TEST_EQ_HEX(rx, rx_len, "6F0284009000");

    emrtd_test_begin("a chained answer is reassembled");
    open_session(&chip, &isodep, ats, sizeof(ats));
    uint8_t long_response[200];
    for(size_t i = 0; i < sizeof(long_response); i++) {
        long_response[i] = (uint8_t)(0xFF - i);
    }
    chip.response = long_response;
    chip.response_len = sizeof(long_response);
    chip.response_chunk = 64;
    before = chip.frames;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, select, sizeof(select), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(rx_len, sizeof(long_response));
    TEST_CHECK(memcmp(rx, long_response, sizeof(long_response)) == 0);
    /* The command, then an acknowledgement for each of the three further blocks. */
    TEST_EQ_INT(chip.frames - before, 4);

    emrtd_test_begin("an answer larger than the caller's buffer is reported, not truncated");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = long_response;
    chip.response_len = sizeof(long_response);
    chip.response_chunk = 64;
    uint8_t small_rx[100];
    TEST_EQ_INT(
        emrtd_isodep_transceive(
            &isodep, select, sizeof(select), small_rx, sizeof(small_rx), &rx_len),
        EmrtdErrorBufferTooSmall);
}

/* --- Waiting time extension --------------------------------------------- */

static void test_wtx(void) {
    static const uint8_t ats[] = {0x05, 0x78, 0x80, 0x70, 0x02};
    static const uint8_t response[] = {0x90, 0x00};
    static const uint8_t command[] = {0x00, 0x86, 0x00, 0x00};

    Chip chip;
    EmrtdIsoDep isodep;
    uint8_t rx[64];
    size_t rx_len = 0;

    emrtd_test_begin("a chip that asks for more time is granted it and then answers");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.wtx_pending = 3;
    chip.wtxm = 5;

    const uint32_t ordinary = isodep.fwt_fc;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.wtx_answered, 3);

    emrtd_test_begin("the granted window is the multiplier the chip asked for");
    uint64_t expected = (uint64_t)ordinary * 5u;
    if(expected > EMRTD_ISODEP_FWT_MAX_FC) {
        expected = EMRTD_ISODEP_FWT_MAX_FC;
    }
    TEST_EQ_INT(chip.last_fwt, (uint32_t)expected);

    emrtd_test_begin("the command is executed once, however long the chip takes");
    TEST_EQ_INT(chip.commands, 1);
}

/* --- Recovery ------------------------------------------------------------ */

/** The frames the reader sent since @p from, as hex, for the sequence checks. */
static void chip_pcbs_since(const Chip* chip, unsigned from, char* out, size_t out_size) {
    size_t pos = 0;
    out[0] = '\0';
    for(unsigned i = from; i < chip->pcb_count && pos + 3 <= out_size; i++) {
        pos += (size_t)snprintf(out + pos, out_size - pos, "%02X", chip->pcbs[i]);
    }
}

static void test_retransmission(void) {
    static const uint8_t ats[] = {0x05, 0x78, 0x80, 0x70, 0x02};
    static const uint8_t response[] = {0x90, 0x00};
    static const uint8_t command[] = {0x00, 0xB0, 0x00, 0x00, 0x20};

    Chip chip;
    EmrtdIsoDep isodep;
    uint8_t rx[512];
    size_t rx_len = 0;
    char sequence[80];
    unsigned mark = 0;

    /*
     * ISO/IEC 14443-4 annex B, scenario 8. The card executed the command and
     * its answer was lost; the reader asks for it with R(NAK) and gets it,
     * and the command has run once. Sending the I-block a second time would
     * have run it twice - which under Secure Messaging ends the session.
     */
    emrtd_test_begin("a lost answer is asked for with R(NAK), and the command runs once");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.lose_answers = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B2");

    /*
     * Neither side checks the other's number on an I-block, so a slip only
     * shows at the next recovery. The sequence is what says it did not slip.
     */
    emrtd_test_begin("and the block numbers are still in step afterwards");
    chip.command_len = 0;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 2);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "03");
    TEST_EQ_INT(isodep.block_number, chip.block_number ^ 1);

    emrtd_test_begin("an answer that arrived damaged is recovered the same way");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.lose_answers = 1;
    chip.lose_damaged = true;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B2");

    /* Scenario 9: the R(NAK)'s own answer is lost too, and a second one is sent. */
    emrtd_test_begin("an answer lost twice over is still recovered, still without a replay");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.lose_answers = 2;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B2B2");

    /*
     * Scenario 6. The command never reached the card; R(NAK) finds that out,
     * because the card answers with an R(ACK) that is not the reader's number,
     * and only then is the block sent again.
     */
    emrtd_test_begin("a command that never arrived is sent again once the card says so");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B202");

    /* Scenario 7: the same, one command later, with the other block number. */
    emrtd_test_begin("the recovery carries whichever block number is current");
    chip.command_len = 0;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(chip.commands, 2);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "03B303");

    /* The block the card asked for again is itself answered, and that answer lost. */
    emrtd_test_begin("a resent command whose answer is lost is not sent a third time");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.drop_next = 1;
    chip.lose_skip = 1;
    chip.lose_answers = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B202B2");

    /* Scenario 10: the card's request for more time is what went missing. */
    emrtd_test_begin("a lost request for more time is asked for again and granted");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.wtx_pending = 1;
    chip.wtxm = 4;
    chip.lose_answers = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    TEST_EQ_INT(chip.wtx_answered, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B2F2");

    /* Scenario 11: the request is lost, and so is the first R(NAK) asking for it. */
    emrtd_test_begin("a lost request for more time survives a lost R(NAK) as well");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.wtx_pending = 1;
    chip.wtxm = 4;
    chip.lose_answers = 1;
    chip.drop_skip = 1;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B2B2F2");

    /*
     * Scenario 12, the path PACE takes: the reader's grant of more time never
     * reached the card, so the card is still waiting for it, and R(NAK) has it
     * ask again.
     */
    emrtd_test_begin("a grant of more time that never arrived is asked for and given again");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.wtx_pending = 1;
    chip.wtxm = 4;
    chip.drop_skip = 1;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    TEST_EQ_INT(chip.wtx_answered, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02F2B2F2");

    /* Scenario 13: the time was granted, and the answer after it was lost. */
    emrtd_test_begin("an answer lost after an extension is recovered without a replay");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.wtx_pending = 1;
    chip.wtxm = 4;
    chip.lose_skip = 1;
    chip.lose_answers = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02F2B2");

    /* Scenario 14: as 13, with the first R(NAK) lost on the way as well. */
    emrtd_test_begin("an answer lost after an extension survives a lost R(NAK) as well");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.wtx_pending = 1;
    chip.wtxm = 4;
    chip.lose_skip = 1;
    chip.lose_answers = 1;
    chip.drop_skip = 2;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_HEX(rx, rx_len, "9000");
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02F2B2B2");

    /*
     * Rule 5. While the card is chaining its answer the recovery is the same
     * R(ACK) again, and the card repeats the block it sent rather than moving
     * on - so the answer arrives whole, with no piece missing or doubled.
     */
    emrtd_test_begin("a lost block of a chained answer is asked for with the same R(ACK)");
    open_session(&chip, &isodep, ats, sizeof(ats));
    uint8_t long_response[200];
    for(size_t i = 0; i < sizeof(long_response); i++) {
        long_response[i] = (uint8_t)(0x30 + i);
    }
    chip.response = long_response;
    chip.response_len = sizeof(long_response);
    chip.response_chunk = 64;
    chip.lose_skip = 1;
    chip.lose_answers = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(rx_len, sizeof(long_response));
    TEST_CHECK(memcmp(rx, long_response, sizeof(long_response)) == 0);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02A3A3A2A3");

    /*
     * Scenario 19: this time the reader's R(ACK) is what was lost. The same
     * R(ACK) again is new to the card, which moves on to the next block.
     */
    emrtd_test_begin("a lost R(ACK) during a chained answer neither skips nor repeats a block");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = long_response;
    chip.response_len = sizeof(long_response);
    chip.response_chunk = 64;
    chip.drop_skip = 1;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(rx_len, sizeof(long_response));
    TEST_CHECK(memcmp(rx, long_response, sizeof(long_response)) == 0);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02A3A3A2A3");

    /*
     * The other direction: the reader is chaining the command, and the card's
     * acknowledgement of a piece is lost. R(NAK) brings the acknowledgement
     * back and the piece is not delivered twice.
     */
    emrtd_test_begin("a lost acknowledgement of a chained command does not double a piece");
    static const uint8_t small_ats[] = {0x05, 0x72, 0x80, 0x70, 0x02};
    open_session(&chip, &isodep, small_ats, sizeof(small_ats));
    uint8_t long_command[70];
    for(size_t i = 0; i < sizeof(long_command); i++) {
        long_command[i] = (uint8_t)i;
    }
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.lose_answers = 1;
    TEST_EQ_INT(
        emrtd_isodep_transceive(
            &isodep, long_command, sizeof(long_command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(chip.command_len, sizeof(long_command));
    TEST_CHECK(memcmp(chip.command, long_command, sizeof(long_command)) == 0);
    TEST_EQ_INT(chip.commands, 1);
    TEST_EQ_HEX(rx, rx_len, "9000");

    /* Scenario 17: a piece of the command is what never arrived. */
    emrtd_test_begin("a lost piece of a chained command is sent again, and only once");
    open_session(&chip, &isodep, small_ats, sizeof(small_ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.drop_skip = 1;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(
            &isodep, long_command, sizeof(long_command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(chip.command_len, sizeof(long_command));
    TEST_CHECK(memcmp(chip.command, long_command, sizeof(long_command)) == 0);
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "1213B31302");

    /* Scenario 18: the acknowledgement is lost, and then the first R(NAK). */
    emrtd_test_begin("a lost acknowledgement survives a lost R(NAK) as well");
    open_session(&chip, &isodep, small_ats, sizeof(small_ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.lose_answers = 1;
    chip.drop_skip = 1;
    chip.drop_next = 1;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(
            &isodep, long_command, sizeof(long_command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNone);
    TEST_EQ_INT(chip.command_len, sizeof(long_command));
    TEST_CHECK(memcmp(chip.command, long_command, sizeof(long_command)) == 0);
    TEST_EQ_INT(chip.commands, 1);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "12B2B21302");

    emrtd_test_begin("a chip that has really gone is reported after the rounds run out");
    open_session(&chip, &isodep, ats, sizeof(ats));
    chip.response = response;
    chip.response_len = sizeof(response);
    chip.drop_next = 99;
    const unsigned before = chip.frames;
    mark = chip.pcb_count;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorCardLost);
    /* The block, then one R(NAK) per round, and not one frame more. */
    TEST_EQ_INT(chip.frames - before, EMRTD_ISODEP_RETRIES + 1);
    TEST_EQ_INT(chip.commands, 0);
    chip_pcbs_since(&chip, mark, sequence, sizeof(sequence));
    TEST_EQ_STR(sequence, "02B2B2");

    emrtd_test_begin("deselect releases the card and closes the session");
    open_session(&chip, &isodep, ats, sizeof(ats));
    emrtd_isodep_deselect(&isodep);
    TEST_CHECK(!chip.activated);
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorNoCard);
}

/* --- Refusing nonsense --------------------------------------------------- */

/** A chip that answers every block with something the reader must not accept. */
static EmrtdError rude_frame(
    void* context,
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t fwt_fc) {
    (void)tx_len;
    (void)fwt_fc;
    unsigned* calls = context;
    (*calls)++;

    if(rx_cap < 2) {
        return EmrtdErrorBufferTooSmall;
    }
    if(tx[0] == 0xE0) {
        rx[0] = 0x05;
        rx[1] = 0x78;
        *rx_len = 2;
        return EmrtdErrorNone;
    }
    /* An I-block that never stops chaining. */
    rx[0] = 0x12;
    *rx_len = 1;
    return EmrtdErrorNone;
}

static void test_malformed(void) {
    unsigned calls = 0;
    EmrtdIsoDep isodep;
    uint8_t rx[64];
    size_t rx_len = 0;
    const uint8_t command[] = {0x00, 0xB0, 0x00, 0x00, 0x20};

    emrtd_test_begin("an endless chain is cut rather than followed for ever");
    emrtd_isodep_init(&isodep, rude_frame, NULL, &calls);
    TEST_EQ_INT(emrtd_isodep_activate(&isodep), EmrtdErrorNone);
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, sizeof(command), rx, sizeof(rx), &rx_len),
        EmrtdErrorProtocol);
    TEST_CHECK(calls < 200);

    emrtd_test_begin("a zero length command is refused before it reaches the radio");
    const unsigned before = calls;
    TEST_EQ_INT(
        emrtd_isodep_transceive(&isodep, command, 0, rx, sizeof(rx), &rx_len),
        EmrtdErrorInvalidInput);
    TEST_EQ_INT(calls, before);
}

void test_suite_isodep(void) {
    test_ats();
    test_activation();
    test_exchange();
    test_wtx();
    test_retransmission();
    test_malformed();
}
