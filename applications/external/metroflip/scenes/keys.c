#include "../metroflip_i.h"
#include "keys.h"
#include <bit_lib.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <nfc/nfc.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <string.h>

#define TAG "keys_check"

// Generic key-based verification: auth a sector key (live) or compare trailer key (loaded)
static bool verify_mfc_key(
    Nfc* nfc,
    MfClassicData* mfc_data,
    bool data_loaded,
    uint64_t expected_key,
    uint8_t sector,
    uint8_t block_offset) {
    if(!data_loaded) {
        uint8_t block = mf_classic_get_first_block_num_of_sector(sector) + block_offset;
        MfClassicKey key = {0};
        bit_lib_num_to_bytes_be(expected_key, COUNT_OF(key.data), key.data);
        MfClassicAuthContext auth_ctx;
        return mf_classic_poller_sync_auth(nfc, block, &key, MfClassicKeyTypeA, &auth_ctx) ==
               MfClassicErrorNone;
    } else {
        MfClassicSectorTrailer* sec_tr = mf_classic_get_sector_trailer_by_sector(mfc_data, sector);
        return sec_tr && bit_lib_bytes_to_num_be(sec_tr->key_a.data, 6) == expected_key;
    }
}

// Table of simple key-check cards (key + sector + block offset)
typedef struct {
    CardType type;
    uint64_t key;
    uint8_t sector;
    uint8_t block_offset;
} SimpleCardCheck;

static const SimpleCardCheck simple_checks[] = {
    {CARD_TYPE_BIP, 0x3a42f33af429, 0, 0},
    {CARD_TYPE_METROMONEY, 0x9C616585E26D, 1, 1},
    {CARD_TYPE_SMARTRIDER, 0x2031D1E57A3B, 0, 1},
    {CARD_TYPE_CHARLIECARD, 0x5EC39B022F2B, 1, 1},
    {CARD_TYPE_TWO_CITIES, 0x2aa05ed1856f, 4, 1},
};

// Troika: same key, different sector for 1k vs 4k
static bool troika_verify(Nfc* nfc, MfClassicData* mfc_data, bool data_loaded) {
    const uint64_t troika_key = 0x08b386463229;
    return verify_mfc_key(nfc, mfc_data, data_loaded, troika_key, 11, 0) ||
           verify_mfc_key(nfc, mfc_data, data_loaded, troika_key, 8, 0);
}

// GoCard: data pattern match (loaded files only)
const uint8_t gocard_verify_data[1][14] = {
    {0x16, 0x18, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x5A, 0x5B, 0x20, 0x21, 0x22, 0x23}};
const uint8_t gocard_verify_data2[1][14] = {
    {0x16, 0x18, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x01, 0x01}};

static bool gocard_verify(MfClassicData* mfc_data, bool data_loaded) {
    if(!data_loaded) return false;
    const uint8_t* buf = &mfc_data->block[1].data[1];
    return memcmp(buf, gocard_verify_data[0], 14) == 0 ||
           memcmp(buf, gocard_verify_data2[0], 14) == 0;
}

// RENFE Suma 10
static const uint64_t renfe_suma10_keys[] = {0xA8844B0BCA06, 0xCB5ED0E57B08};

static bool renfe_suma10_verify(Nfc* nfc, MfClassicData* mfc_data, bool data_loaded) {
    if(!data_loaded) {
        // Live: try authenticating with either RENFE key
        return verify_mfc_key(nfc, mfc_data, false, renfe_suma10_keys[0], 0, 0) ||
               verify_mfc_key(nfc, mfc_data, false, renfe_suma10_keys[1], 1, 0);
    }

    if(!mfc_data) return false;

    // Check Mobilis pattern in block 12: 81 F8 01 02
    if(mf_classic_is_block_read(mfc_data, 12)) {
        const uint8_t* b12 = mfc_data->block[12].data;
        if(b12[0] == 0x81 && b12[1] == 0xF8 && b12[2] == 0x01 && b12[3] == 0x02) return true;
    }

    // Check for RENFE keys in sector trailers
    bool has_key0 = false, has_key1 = false;
    MfClassicSectorTrailer* st0 = mf_classic_get_sector_trailer_by_sector(mfc_data, 0);
    if(st0) has_key0 = bit_lib_bytes_to_num_be(st0->key_a.data, 6) == renfe_suma10_keys[0];
    MfClassicSectorTrailer* st1 = mf_classic_get_sector_trailer_by_sector(mfc_data, 1);
    if(st1) has_key1 = bit_lib_bytes_to_num_be(st1->key_a.data, 6) == renfe_suma10_keys[1];

    if(has_key0 && has_key1) return true;

    // Check block 5 RENFE value pattern: 01 00 00 00
    bool has_b5 = mfc_data->block[5].data[0] == 0x01 && mfc_data->block[5].data[1] == 0x00 &&
                  mfc_data->block[5].data[2] == 0x00 && mfc_data->block[5].data[3] == 0x00;

    if((has_key0 || has_key1) && has_b5) return true;

    // Check any RENFE key in first 4 sectors
    for(uint8_t s = 0; s < 4; s++) {
        MfClassicSectorTrailer* st = mf_classic_get_sector_trailer_by_sector(mfc_data, s);
        if(!st) continue;
        uint64_t k = bit_lib_bytes_to_num_be(st->key_a.data, 6);
        for(size_t i = 0; i < COUNT_OF(renfe_suma10_keys); i++) {
            if(k == renfe_suma10_keys[i]) return true;
        }
    }

    return false;
}

// RENFE Regular
static bool renfe_regular_verify(Nfc* nfc, MfClassicData* mfc_data, bool data_loaded) {
    if(!data_loaded) {
        // Live: try RENFE Regular specific keys
        if(verify_mfc_key(nfc, mfc_data, false, 0x37E0DB717D08, 0, 0)) return true;
        if(verify_mfc_key(nfc, mfc_data, false, 0x421BFA445657, 1, 0)) return true;
        return false;
    }

    if(!mfc_data) return false;

    int score = 0;

    // Block 8: second byte E2 is RENFE signature
    if(mf_classic_is_block_read(mfc_data, 8)) {
        if(mfc_data->block[8].data[1] == 0xE2) score += 2;
        if(mfc_data->block[8].data[2] == 0xA5 || mfc_data->block[8].data[2] == 0xB5) score += 1;
    }

    // Block 12: E4 or E8 first byte
    if(mf_classic_is_block_read(mfc_data, 12)) {
        uint8_t b = mfc_data->block[12].data[0];
        if(b == 0xE4 || b == 0xE8) score += 2;
    }

    // Block 2: 5C 9F network signature
    if(mf_classic_is_block_read(mfc_data, 2)) {
        if(mfc_data->block[2].data[2] == 0x5C && mfc_data->block[2].data[3] == 0x9F) score += 3;
    }

    // Trip data patterns in blocks 13-21
    for(int blk = 13; blk <= 21; blk++) {
        if(mf_classic_is_block_read(mfc_data, blk)) {
            const uint8_t* d = mfc_data->block[blk].data;
            if((d[6] == 0x4D && d[7] == 0xDF) || (d[0] > 0x50 && d[1] < 0x10)) score += 1;
        }
    }

    // Protected sectors (non-default keys)
    int prot = 0;
    for(int s = 0; s < 16; s++) {
        MfClassicSectorTrailer* st = mf_classic_get_sector_trailer_by_sector(mfc_data, s);
        if(!st) continue;
        uint64_t k = bit_lib_bytes_to_num_be(st->key_a.data, 6);
        if(k != 0xFFFFFFFFFFFF && k != 0xA0A1A2A3A4A5 && k != 0x000000000000 &&
           k != 0xB0B1B2B3B4B5)
            prot++;
    }
    if(prot >= 3) score += 1;

    return score >= 1;
}

CardType determine_card_type(Nfc* nfc, MfClassicData* mfc_data, bool data_loaded) {
    // Check simple key-based cards first
    for(size_t i = 0; i < COUNT_OF(simple_checks); i++) {
        if(verify_mfc_key(
               nfc,
               mfc_data,
               data_loaded,
               simple_checks[i].key,
               simple_checks[i].sector,
               simple_checks[i].block_offset)) {
            return simple_checks[i].type;
        }
    }

    if(troika_verify(nfc, mfc_data, data_loaded)) return CARD_TYPE_TROIKA;
    if(gocard_verify(mfc_data, data_loaded)) return CARD_TYPE_GOCARD;
    if(renfe_suma10_verify(nfc, mfc_data, data_loaded)) return CARD_TYPE_RENFE_SUM10;
    if(renfe_regular_verify(nfc, mfc_data, data_loaded)) return CARD_TYPE_RENFE_REGULAR;

    return CARD_TYPE_UNKNOWN;
}
