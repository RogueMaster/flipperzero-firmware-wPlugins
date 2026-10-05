/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * A passport chip, simulated at the APDU level.
 *
 * The reader has exactly one way to reach a card, which is EmrtdTransceiver,
 * and the point of this object is to sit behind that port so that a whole read
 * can run with no radio underneath it.
 *
 * One caller is allowed and no others: the host test suite, which drives it so
 * that a read can be checked on a workstation with sanitizers attached. The
 * released package does not compile this file at all; see application.fam.
 * Nothing in the reader may reference it either: a layer that knows whether
 * the chip is real would stop being the thing the tests exercise.
 *
 * The chip side of every protocol is written out here rather than borrowed
 * from the reader. That is deliberate: if both ends shared an implementation,
 * a mistake in the padding, the counter or the framing would cancel itself out
 * and the suite would pass against a chip that does not exist. The parts that
 * are already pinned to published vectors elsewhere - the key derivation and
 * the curve arithmetic - are reused, because a vector is a better witness than
 * a second copy.
 *
 * The documents it serves are the ICAO specimen: Anna Maria Eriksson of
 * Utopia, who is not a person.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../access/emrtd_access.h"
#include "../crypto/emrtd_crypto.h"
#include "../transport/emrtd_transceiver.h"

/** Which access protocols the simulated chip will answer. */
typedef enum {
    EmrtdSimAccessBacOnly, /**< No EF.CardAccess; General Authenticate is refused. */
    EmrtdSimAccessPaceOnly, /**< EXTERNAL AUTHENTICATE is refused. */
    EmrtdSimAccessBoth,
} EmrtdSimAccess;

typedef struct {
    EmrtdSimAccess access;
    /** The credentials that open this document. */
    EmrtdCredentials credentials;
    /** PACE domain parameters; 0 means 13, brainpoolP256r1. */
    uint8_t pace_parameter_id;
    /**
     * The cipher the announced PACEInfo names.
     *
     * The zero value means AES-128 rather than 3DES: PACE over 3DES is extinct
     * in the field and this chip does not offer it, so a config left blank
     * gets the protocol a European passport actually implements.
     */
    EmrtdCipher pace_cipher;
    /** Card frame size; 0 means 256. */
    uint16_t fsc;
    /** Reader frame size the chip must keep its answers inside; 0 means 256. */
    uint16_t fsd;
} EmrtdSimConfig;

typedef struct EmrtdSim EmrtdSim;

/** A chip holding the specimen document. @p config may be NULL for the defaults. */
EmrtdSim* emrtd_sim_alloc(const EmrtdSimConfig* config);
void emrtd_sim_free(EmrtdSim* sim);

/** The port the reader talks through. Owned by @p sim. */
EmrtdTransceiver* emrtd_sim_transceiver(EmrtdSim* sim);

/* --- What the test can ask about afterwards ----------------------------- */

/** An elementary file as the chip holds it, or NULL when it has none. */
const uint8_t* emrtd_sim_file(const EmrtdSim* sim, uint16_t fid, size_t* out_len);

/** True once an access protocol has opened a Secure Messaging session. */
bool emrtd_sim_authenticated(const EmrtdSim* sim);

/** "PACE", "BAC", or "" when nothing has succeeded. */
const char* emrtd_sim_protocol(const EmrtdSim* sim);

/** How many APDUs the chip has been given, for the tests that count rounds. */
size_t emrtd_sim_exchanges(const EmrtdSim* sim);

/** File identifiers of the synthetic Logical Data Structure. */
#define EMRTD_SIM_FID_CARD_ACCESS 0x011C
#define EMRTD_SIM_FID_COM         0x011E
#define EMRTD_SIM_FID_DG1         0x0101
#define EMRTD_SIM_FID_SOD         0x011D
