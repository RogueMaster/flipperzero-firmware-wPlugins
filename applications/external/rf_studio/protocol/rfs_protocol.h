#pragma once

#include "../rfs_types.h"
#include <lib/subghz/environment.h>
#include <lib/subghz/registry.h>
#include <lib/subghz/types.h>

/*
 * Protocol Registry (Specification Section 10).
 * RF Studio does not implement protocols itself; it reads the firmware's registry and classifies
 * each protocol by the type and flags the firmware declares.
 */

typedef struct {
    size_t total;
    size_t fixed;
    size_t rolling;
    size_t raw;
    size_t sendable;
} RfsProtocolStats;

/** Replay eligibility of a stored signal (Section 8.3, Addendum DYN-019). */
typedef enum {
    RfsReplayYes, /* decoded fixed code with a firmware encoder */
    RfsReplayRawUnverified, /* raw recording not yet confirmed by the user */
    RfsReplayRawVerified, /* raw recording the user confirmed works */
    RfsReplayRolling, /* rolling or encrypted code: never replayed as a static command */
    RfsReplayUnsupported, /* no encoder in this firmware */
} RfsReplay;

/** Allocate a protocol environment bound to the firmware registry. Free with
 *  subghz_environment_free(). Each engine owns its own environment. */
SubGhzEnvironment* rfs_protocol_env_alloc(void);

/** Look up a protocol by its .sub "Protocol" name. NULL if the firmware lacks it. */
const SubGhzProtocol* rfs_protocol_find(const char* name);

/** Classify a protocol (NULL means unknown). */
RfsSignalKind rfs_protocol_kind(const SubGhzProtocol* protocol);

/** True if the firmware can encode and transmit this protocol. */
bool rfs_protocol_can_send(const SubGhzProtocol* protocol);

/** Replay eligibility for a signal of this kind and protocol. */
RfsReplay rfs_protocol_replay(RfsSignalKind kind, const char* protocol_name, bool verified);

/** True when the replay status permits a transmission attempt. */
bool rfs_replay_allowed(RfsReplay replay);

const char* rfs_kind_name(RfsSignalKind kind);
const char* rfs_kind_tag(RfsSignalKind kind); /* short list marker such as "F" */
const char* rfs_replay_name(RfsReplay replay);

void rfs_protocol_stats(RfsProtocolStats* stats);
