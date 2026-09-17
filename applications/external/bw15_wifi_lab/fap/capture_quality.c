#include "capture_quality.h"

#include <string.h>

#define G5_CQ_FTYPE_DATA 2u
#define G5_CQ_FTYPE_MGMT 0u
#define G5_CQ_BEACON_SUBTYPE 8u

#define G5_CQ_KEY_PAIRWISE 0x0008u
#define G5_CQ_KEY_INSTALL 0x0040u
#define G5_CQ_KEY_ACK 0x0080u
#define G5_CQ_KEY_MIC 0x0100u
#define G5_CQ_KEY_SECURE 0x0200u
#define G5_CQ_KEY_ERROR 0x0400u
#define G5_CQ_KEY_REQUEST 0x0800u
#define G5_CQ_KEY_SMK 0x2000u

typedef struct {
    G5CqRecord record;
    uint8_t bssid[6];
} G5CqParsed;

static bool bytes_all(const uint8_t* data, size_t length, uint8_t value) {
    for(size_t i = 0; i < length; ++i) {
        if(data[i] != value) return false;
    }
    return true;
}

static bool bytes_invalid_secret(const uint8_t* data, size_t length) {
    return bytes_all(data, length, 0x00) || bytes_all(data, length, 0xFF);
}

static bool replay_equal(const uint8_t left[8], const uint8_t right[8]) {
    return memcmp(left, right, 8) == 0;
}

static bool replay_is_next(const uint8_t earlier[8], const uint8_t later[8]) {
    uint8_t expected[8];
    memcpy(expected, earlier, sizeof(expected));
    for(int i = 7; i >= 0; --i) {
        expected[i]++;
        if(expected[i] != 0) break;
    }
    return memcmp(expected, later, sizeof(expected)) == 0;
}

static bool record_equal(const G5CqRecord* left, const G5CqRecord* right) {
    return left->message == right->message && left->key_version == right->key_version &&
           memcmp(left->sta, right->sta, sizeof(left->sta)) == 0 &&
           memcmp(left->replay, right->replay, sizeof(left->replay)) == 0 &&
           memcmp(left->nonce, right->nonce, sizeof(left->nonce)) == 0 &&
           memcmp(left->mic, right->mic, sizeof(left->mic)) == 0;
}

static int classify_message(uint16_t key_info) {
    const bool ack = (key_info & G5_CQ_KEY_ACK) != 0;
    const bool mic = (key_info & G5_CQ_KEY_MIC) != 0;
    const bool secure = (key_info & G5_CQ_KEY_SECURE) != 0;
    const bool install = (key_info & G5_CQ_KEY_INSTALL) != 0;

    if(ack && !mic && !secure && !install) return 1;
    if(!ack && mic && !secure && !install) return 2;
    if(ack && mic && secure && install) return 3;
    if(!ack && mic && secure && !install) return 4;
    return 0;
}

static G5CqIngestResult parse_frame(const uint8_t* frame, size_t length, G5CqParsed* parsed) {
    if(!frame || !parsed || length < 24) return G5CqIngestTruncated;
    if((frame[0] & 0x03u) != 0) return G5CqIngestNotEapol;
    const uint8_t type = (frame[0] >> 2) & 0x03u;
    if(type != G5_CQ_FTYPE_DATA) return G5CqIngestNotEapol;
    if(frame[1] & 0x40u) return G5CqIngestBadKeyInfo; // protected data is not clear EAPOL-Key

    const bool to_ds = (frame[1] & 0x01u) != 0;
    const bool from_ds = (frame[1] & 0x02u) != 0;
    if(to_ds == from_ds) return G5CqIngestUnsupportedDs;

    size_t header = 24;
    const uint8_t subtype = (frame[0] >> 4) & 0x0Fu;
    if(subtype & 0x08u) {
        header += 2; // QoS Control
        if(frame[1] & 0x80u) header += 4; // HT Control when Order is set on QoS data
    }
    if(length < header + 8) return G5CqIngestTruncated;

    const uint8_t* snap = frame + header;
    if(!(snap[0] == 0xAA && snap[1] == 0xAA && snap[2] == 0x03 && snap[3] == 0x00 &&
         snap[4] == 0x00 && snap[5] == 0x00 && snap[6] == 0x88 && snap[7] == 0x8E))
        return G5CqIngestNotEapol;

    const size_t eapol_offset = header + 8;
    if(length < eapol_offset + 4) return G5CqIngestTruncated;
    const uint8_t* eapol = frame + eapol_offset;
    if(eapol[1] != 3) return G5CqIngestNotEapol;
    const size_t body_length = ((size_t)eapol[2] << 8) | eapol[3];
    if(body_length < 95) return G5CqIngestBadEapolLength;
    if(body_length > SIZE_MAX - 4 || length - eapol_offset < body_length + 4)
        return G5CqIngestBadEapolLength;

    const uint8_t descriptor = eapol[4];
    if(descriptor != 2 && descriptor != 254) return G5CqIngestBadKeyDescriptor;
    const uint16_t key_info = ((uint16_t)eapol[5] << 8) | eapol[6];
    const uint8_t key_version = key_info & 0x07u;
    if(!(key_info & G5_CQ_KEY_PAIRWISE) || key_version == 0 || key_version > 3 ||
       (key_info & (G5_CQ_KEY_ERROR | G5_CQ_KEY_REQUEST | G5_CQ_KEY_SMK)))
        return G5CqIngestBadKeyInfo;

    const int message = classify_message(key_info);
    if(message == 0) return G5CqIngestBadKeyInfo;

    memset(parsed, 0, sizeof(*parsed));
    parsed->record.message = (uint8_t)message;
    parsed->record.key_version = key_version;
    memcpy(parsed->record.replay, eapol + 9, sizeof(parsed->record.replay));
    memcpy(parsed->record.nonce, eapol + 17, sizeof(parsed->record.nonce));
    memcpy(parsed->record.mic, eapol + 81, sizeof(parsed->record.mic));

    if(to_ds) {
        memcpy(parsed->bssid, frame + 4, 6);
        memcpy(parsed->record.sta, frame + 10, 6);
    } else {
        memcpy(parsed->bssid, frame + 10, 6);
        memcpy(parsed->record.sta, frame + 4, 6);
    }

    if(message <= 3 && bytes_invalid_secret(parsed->record.nonce, 32))
        return G5CqIngestBadNonce;
    if(message == 1) {
        if(!bytes_all(parsed->record.mic, 16, 0x00)) return G5CqIngestBadMic;
    } else if(bytes_invalid_secret(parsed->record.mic, 16)) {
        return G5CqIngestBadMic;
    }
    return G5CqIngestAccepted;
}

static void find_pair(G5CaptureQuality* quality) {
    quality->pair = G5CqPairNone;
    quality->pair_first_index = UINT8_MAX;
    quality->pair_second_index = UINT8_MAX;
    memset(quality->pair_sta, 0, sizeof(quality->pair_sta));

    // Prefer the challenge pair. The nearest preceding M1 prevents an older
    // replay value from a prior reconnect being paired across exchanges.
    for(size_t m2i = 0; m2i < quality->record_count; ++m2i) {
        const G5CqRecord* m2 = &quality->records[m2i];
        if(m2->message != 2) continue;
        const G5CqRecord* nearest_m1 = NULL;
        for(size_t back = m2i; back > 0; --back) {
            const G5CqRecord* candidate = &quality->records[back - 1];
            if(candidate->message == 1 && memcmp(candidate->sta, m2->sta, 6) == 0) {
                nearest_m1 = candidate;
                break;
            }
        }
        if(nearest_m1 && replay_equal(nearest_m1->replay, m2->replay)) {
            quality->pair = G5CqPairM1M2;
            quality->pair_first_index = (uint8_t)(nearest_m1 - quality->records);
            quality->pair_second_index = (uint8_t)m2i;
            memcpy(quality->pair_sta, m2->sta, sizeof(quality->pair_sta));
            return;
        }
    }

    // Authorized M2+M3 is the conservative fallback when M1 was missed.
    for(size_t m2i = 0; m2i < quality->record_count; ++m2i) {
        const G5CqRecord* m2 = &quality->records[m2i];
        if(m2->message != 2) continue;
        for(size_t m3i = m2i + 1; m3i < quality->record_count; ++m3i) {
            const G5CqRecord* m3 = &quality->records[m3i];
            if(m3->message != 3 || memcmp(m3->sta, m2->sta, 6) != 0) continue;
            if(!replay_is_next(m2->replay, m3->replay)) break;

            // When M1 for this exchange is present, M3 must repeat its ANonce.
            const G5CqRecord* nearest_m1 = NULL;
            for(size_t back = m2i; back > 0; --back) {
                const G5CqRecord* candidate = &quality->records[back - 1];
                if(candidate->message == 1 && memcmp(candidate->sta, m2->sta, 6) == 0) {
                    nearest_m1 = candidate;
                    break;
                }
            }
            if(nearest_m1 && replay_equal(nearest_m1->replay, m2->replay) &&
               memcmp(nearest_m1->nonce, m3->nonce, 32) != 0)
                break;

            quality->pair = G5CqPairM2M3;
            quality->pair_first_index = (uint8_t)m2i;
            quality->pair_second_index = (uint8_t)m3i;
            memcpy(quality->pair_sta, m2->sta, sizeof(quality->pair_sta));
            return;
        }
    }
}

void g5_cq_reset(G5CaptureQuality* quality, const uint8_t target_bssid[6]) {
    if(!quality) return;
    memset(quality, 0, sizeof(*quality));
    quality->reported_count = -1;
    quality->pair_first_index = UINT8_MAX;
    quality->pair_second_index = UINT8_MAX;
    quality->result = G5CqResultLive;
    if(target_bssid) {
        memcpy(quality->target_bssid, target_bssid, sizeof(quality->target_bssid));
        quality->target_set = true;
    }
}

bool g5_cq_hex_decode_exact(
    const char* hex,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_length) {
    if(output_length) *output_length = 0;
    if(!hex || !output || !output_length) return false;
    const size_t chars = strlen(hex);
    if(chars == 0 || (chars & 1u) != 0 || chars / 2 > output_capacity) return false;

    for(size_t i = 0; i < chars; i += 2) {
        const char hi = hex[i], lo = hex[i + 1];
        const int high = (hi >= '0' && hi <= '9') ? hi - '0' :
                         (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 :
                         (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 :
                                                    -1;
        const int low = (lo >= '0' && lo <= '9') ? lo - '0' :
                        (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 :
                        (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 :
                                                   -1;
        if(high < 0 || low < 0) return false;
        output[i / 2] = (uint8_t)((high << 4) | low);
    }
    *output_length = chars / 2;
    return true;
}

G5CqIngestResult
    g5_cq_ingest_frame(G5CaptureQuality* quality, const uint8_t* frame, size_t length) {
    if(!quality) return G5CqIngestTruncated;
    if(quality->wire_count != UINT8_MAX) quality->wire_count++;

    G5CqParsed parsed;
    G5CqIngestResult result = parse_frame(frame, length, &parsed);
    if(result != G5CqIngestAccepted) {
        if(result == G5CqIngestBadNonce || result == G5CqIngestBadMic)
            quality->invalid_crypto_count++;
        else if(result == G5CqIngestWrongBssid)
            quality->wrong_target_count++;
        else if(result != G5CqIngestNotEapol)
            quality->malformed_count++;
        return result;
    }
    if(!quality->target_set || memcmp(parsed.bssid, quality->target_bssid, 6) != 0) {
        quality->wrong_target_count++;
        return G5CqIngestWrongBssid;
    }
    for(size_t i = 0; i < quality->record_count; ++i) {
        if(record_equal(&quality->records[i], &parsed.record)) {
            quality->duplicate_count++;
            return G5CqIngestDuplicate;
        }
    }
    if(quality->record_count >= G5_CQ_MAX_RECORDS) {
        quality->capacity_drop_count++;
        return G5CqIngestCapacity;
    }
    quality->records[quality->record_count++] = parsed.record;
    find_pair(quality);
    return G5CqIngestAccepted;
}

bool g5_cq_ingest_beacon(G5CaptureQuality* quality, const uint8_t* frame, size_t length) {
    if(!quality || !frame || length < 24 || !quality->target_set) return false;
    const uint8_t type = (frame[0] >> 2) & 0x03u;
    const uint8_t subtype = (frame[0] >> 4) & 0x0Fu;
    if((frame[0] & 0x03u) != 0 || type != G5_CQ_FTYPE_MGMT || subtype != G5_CQ_BEACON_SUBTYPE)
        return false;
    if(memcmp(frame + 16, quality->target_bssid, 6) != 0) {
        quality->wrong_target_count++;
        return false;
    }
    quality->beacon_valid = true;
    return true;
}

bool g5_cq_has_pair(const G5CaptureQuality* quality) {
    return quality && quality->pair != G5CqPairNone;
}

uint8_t g5_cq_message_mask(const G5CaptureQuality* quality) {
    uint8_t mask = 0;
    if(!quality) return mask;
    for(size_t i = 0; i < quality->record_count; ++i) {
        const uint8_t message = quality->records[i].message;
        if(message >= 1 && message <= 4) mask |= (uint8_t)(1u << (message - 1));
    }
    return mask;
}

static G5CqResult incomplete_reason(const G5CaptureQuality* quality) {
    bool have_m2 = false, have_ap_nonce = false, same_sta = false, replay_mismatch = false;
    for(size_t i = 0; i < quality->record_count; ++i) {
        if(quality->records[i].message == 2) have_m2 = true;
        if(quality->records[i].message == 1 || quality->records[i].message == 3)
            have_ap_nonce = true;
    }
    for(size_t i = 0; i < quality->record_count; ++i) {
        const G5CqRecord* m2 = &quality->records[i];
        if(m2->message != 2) continue;
        for(size_t j = 0; j < quality->record_count; ++j) {
            const G5CqRecord* ap = &quality->records[j];
            if(ap->message != 1 && ap->message != 3) continue;
            if(memcmp(m2->sta, ap->sta, 6) != 0) continue;
            same_sta = true;
            if((ap->message == 1 && !replay_equal(ap->replay, m2->replay)) ||
               (ap->message == 3 && !replay_is_next(m2->replay, ap->replay)))
                replay_mismatch = true;
        }
    }
    if(!have_m2) return G5CqResultNoM2;
    if(!have_ap_nonce) return G5CqResultNoApNonce;
    if(!same_sta) return G5CqResultStaMismatch;
    if(replay_mismatch) return G5CqResultReplayMismatch;
    return G5CqResultMalformed;
}

G5CqResult g5_cq_finish(G5CaptureQuality* quality, int reported_count) {
    if(!quality) return G5CqResultMalformed;
    quality->reported_count = reported_count;
    if(reported_count < 0 || reported_count != quality->wire_count)
        quality->result = G5CqResultCountMismatch;
    else if(quality->capacity_drop_count)
        quality->result = G5CqResultCapacity;
    else if(!quality->beacon_valid)
        quality->result = G5CqResultNoBeacon;
    else if(quality->pair == G5CqPairM1M2)
        quality->result = G5CqResultValidM1M2;
    else if(quality->pair == G5CqPairM2M3)
        quality->result = G5CqResultValidM2M3;
    else if(quality->record_count == 0) {
        if(quality->invalid_crypto_count)
            quality->result = G5CqResultInvalidCrypto;
        else if(quality->wrong_target_count)
            quality->result = G5CqResultWrongTarget;
        else if(quality->malformed_count)
            quality->result = G5CqResultMalformed;
        else
            quality->result = G5CqResultNoEapol;
    } else {
        quality->result = incomplete_reason(quality);
    }
    return quality->result;
}

const char* g5_cq_result_name(G5CqResult result) {
    switch(result) {
    case G5CqResultLive: return "live";
    case G5CqResultValidM1M2: return "valid_m1_m2";
    case G5CqResultValidM2M3: return "valid_m2_m3";
    case G5CqResultNoEapol: return "no_eapol";
    case G5CqResultNoBeacon: return "no_target_beacon";
    case G5CqResultNoM2: return "no_m2";
    case G5CqResultNoApNonce: return "no_ap_nonce";
    case G5CqResultStaMismatch: return "sta_mismatch";
    case G5CqResultReplayMismatch: return "replay_mismatch";
    case G5CqResultInvalidCrypto: return "invalid_nonce_or_mic";
    case G5CqResultWrongTarget: return "wrong_target";
    case G5CqResultMalformed: return "malformed";
    case G5CqResultCountMismatch: return "frame_count_mismatch";
    case G5CqResultCapacity: return "capacity_exceeded";
    }
    return "unknown";
}

const char* g5_cq_pair_name(G5CqPair pair) {
    switch(pair) {
    case G5CqPairM1M2: return "M1+M2";
    case G5CqPairM2M3: return "M2+M3";
    case G5CqPairNone: return "none";
    }
    return "none";
}
