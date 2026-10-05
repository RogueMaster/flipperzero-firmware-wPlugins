// Pod's on-air format: framing, CRC, encode/decode and duplicate rejection.
//
// Deliberately free of any Flipper dependency so the exact code that runs on
// the device can also be exercised on a host. Pod's whole point is two Flippers
// agreeing on these bytes, and that agreement is the one thing a single device
// cannot test.
#pragma once

#include "profile.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POD_FRAME_MAX 48
#define POD_PARSE_BUF 160
#define POD_DEDUP_N   24

typedef enum {
    PodMsgBeacon = 0x01, // presence broadcast
    PodMsgChallenge = 0x02, // src wants to battle dst
    PodMsgAccept = 0x03, // dst accepts the challenge
    PodMsgDecline = 0x04, // dst declines
    PodMsgTaps = 0x05, // final tap count exchange
} PodMsgType;

// A decoded message. Which fields are meaningful depends on `type`.
typedef struct {
    uint8_t type;
    uint32_t src_id;
    uint32_t dst_id; // control messages
    uint16_t battle_id; // control messages
    uint16_t taps; // TAPS
    uint8_t level; // beacon/challenge/accept
    uint16_t nonce; // beacon
    uint8_t icon; // beacon
    char name[POD_NAME_MAX + 1]; // beacon/challenge/accept
} PodMsg;

typedef void (*PodMsgCallback)(void* context, const PodMsg* msg);

// --- encoding: each returns the number of bytes written, or 0 if buf is too
// small. Frames are self-delimiting and CRC-tagged.

size_t pod_wire_beacon(
    uint8_t* buf,
    size_t cap,
    uint32_t src,
    uint8_t icon,
    uint8_t level,
    uint16_t nonce,
    const char* name);

size_t pod_wire_named(
    uint8_t* buf,
    size_t cap,
    uint8_t type, // PodMsgChallenge or PodMsgAccept
    uint32_t src,
    const char* name,
    uint8_t level,
    uint32_t dst,
    uint16_t battle_id);

size_t pod_wire_decline(uint8_t* buf, size_t cap, uint32_t src, uint32_t dst, uint16_t battle_id);

size_t pod_wire_taps(
    uint8_t* buf,
    size_t cap,
    uint32_t src,
    uint32_t dst,
    uint16_t battle_id,
    uint16_t taps);

// --- receiving

typedef struct {
    uint32_t src;
    uint8_t type;
    uint16_t key2; // nonce for beacon, battle_id for control
} PodSeen;

// Reassembles a byte stream into messages. Holds the duplicate history, because
// every frame is transmitted more than once for redundancy.
typedef struct {
    uint8_t buf[POD_PARSE_BUF];
    size_t len;
    PodSeen recent[POD_DEDUP_N];
    int recent_pos;
} PodWire;

void pod_wire_reset(PodWire* w);

// Feed received bytes. Each complete, valid, not-recently-seen message is
// handed to cb. Garbage is skipped by resynchronising on the frame preamble.
void pod_wire_feed(PodWire* w, const uint8_t* data, size_t len, PodMsgCallback cb, void* context);

// Exposed for tests: CRC over the first len bytes.
uint8_t pod_wire_crc8(const uint8_t* data, size_t len);
