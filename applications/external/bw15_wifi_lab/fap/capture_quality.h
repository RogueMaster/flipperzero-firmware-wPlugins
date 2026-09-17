#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define G5_CQ_MAX_RECORDS 16

typedef enum {
    G5CqIngestAccepted = 0,
    G5CqIngestDuplicate,
    G5CqIngestNotEapol,
    G5CqIngestWrongBssid,
    G5CqIngestUnsupportedDs,
    G5CqIngestTruncated,
    G5CqIngestBadEapolLength,
    G5CqIngestBadKeyDescriptor,
    G5CqIngestBadKeyInfo,
    G5CqIngestBadNonce,
    G5CqIngestBadMic,
    G5CqIngestCapacity,
} G5CqIngestResult;

typedef enum {
    G5CqPairNone = 0,
    G5CqPairM1M2,
    G5CqPairM2M3,
} G5CqPair;

typedef enum {
    G5CqResultLive = 0,
    G5CqResultValidM1M2,
    G5CqResultValidM2M3,
    G5CqResultNoEapol,
    G5CqResultNoBeacon,
    G5CqResultNoM2,
    G5CqResultNoApNonce,
    G5CqResultStaMismatch,
    G5CqResultReplayMismatch,
    G5CqResultInvalidCrypto,
    G5CqResultWrongTarget,
    G5CqResultMalformed,
    G5CqResultCountMismatch,
    G5CqResultCapacity,
} G5CqResult;

typedef struct {
    uint8_t message;
    uint8_t sta[6];
    uint8_t replay[8];
    uint8_t nonce[32];
    uint8_t mic[16];
    uint8_t key_version;
} G5CqRecord;

typedef struct {
    uint8_t target_bssid[6];
    bool target_set;
    bool beacon_valid;
    G5CqRecord records[G5_CQ_MAX_RECORDS];
    uint8_t record_count;
    uint8_t wire_count;
    uint8_t duplicate_count;
    uint8_t wrong_target_count;
    uint8_t malformed_count;
    uint8_t invalid_crypto_count;
    uint8_t capacity_drop_count;
    int reported_count;
    G5CqPair pair;
    uint8_t pair_first_index;
    uint8_t pair_second_index;
    uint8_t pair_sta[6];
    G5CqResult result;
} G5CaptureQuality;

void g5_cq_reset(G5CaptureQuality* quality, const uint8_t target_bssid[6]);

bool g5_cq_hex_decode_exact(
    const char* hex,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_length);

G5CqIngestResult
    g5_cq_ingest_frame(G5CaptureQuality* quality, const uint8_t* frame, size_t length);

bool g5_cq_ingest_beacon(G5CaptureQuality* quality, const uint8_t* frame, size_t length);

bool g5_cq_has_pair(const G5CaptureQuality* quality);
uint8_t g5_cq_message_mask(const G5CaptureQuality* quality);
G5CqResult g5_cq_finish(G5CaptureQuality* quality, int reported_count);
const char* g5_cq_result_name(G5CqResult result);
const char* g5_cq_pair_name(G5CqPair pair);
