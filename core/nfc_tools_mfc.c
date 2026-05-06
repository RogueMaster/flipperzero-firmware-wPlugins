#include "../include/nfc_tools_mfc.h"
#include "../include/nfc_tools_ndef.h"

// ── NFC Forum keys ───────────────────────────────────────────────────────────

static const MfClassicKey mfc_mad_key_a  = {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}};
static const MfClassicKey mfc_ndef_key_a = {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}};
static const MfClassicKey mfc_ff_key     = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

// ── MAD CRC-8 ───────────────────────────────────────────────────────────────

static uint8_t mfc_mad_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x6C;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(int j = 0; j < 8; j++) {
            if(crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x1D);
            else            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

// ── Multi-key authentication ──────────────────────────────────────────────────

static bool mfc_ndef_try_auth(
    NfcToolsApp*      app,
    uint8_t           first_block,
    MfClassicKey*     out_key,
    MfClassicKeyType* out_type)
{
    static const MfClassicKey keys[] = {
        {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
        {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}},
        {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}},
        {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    };
    static const uint8_t key_count = sizeof(keys) / sizeof(keys[0]);
    MfClassicAuthContext ctx;

    for(uint8_t ki = 0; ki < key_count; ki++) {
        MfClassicKey k = keys[ki];
        if(mf_classic_poller_sync_auth(
               app->nfc, first_block, &k, MfClassicKeyTypeA, &ctx) ==
           MfClassicErrorNone) {
            *out_key  = k;
            *out_type = MfClassicKeyTypeA;
            return true;
        }
        if(mf_classic_poller_sync_auth(
               app->nfc, first_block, &k, MfClassicKeyTypeB, &ctx) ==
           MfClassicErrorNone) {
            *out_key  = k;
            *out_type = MfClassicKeyTypeB;
            return true;
        }
    }
    return false;
}

// ── NDEF Read ─────────────────────────────────────────────────────────────────

void nfc_tools_mfc_read_ndef(NfcToolsApp* app) {
    static const MfClassicKey mad_cand[] = {
        {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}},
        {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
    };
    static const MfClassicKey ndef_cand[] = {
        {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}},
        {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
        {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}},
    };

    MfClassicBlock blk1 = {{0}}, blk2 = {{0}};
    bool           mad_ok = false;

    for(uint8_t ki = 0; ki < sizeof(mad_cand) / sizeof(mad_cand[0]) && !mad_ok; ki++) {
        MfClassicKey     k = mad_cand[ki];
        MfClassicAuthContext ctx;
        if(mf_classic_poller_sync_auth(app->nfc, 1, &k, MfClassicKeyTypeA, &ctx) !=
           MfClassicErrorNone)
            continue;

        {
            MfClassicKey k1 = k, k2 = k;
            if(mf_classic_poller_sync_read_block(
                   app->nfc, 1, &k1, MfClassicKeyTypeA, &blk1) == MfClassicErrorNone &&
               mf_classic_poller_sync_read_block(
                   app->nfc, 2, &k2, MfClassicKeyTypeA, &blk2) == MfClassicErrorNone) {
                mad_ok = true;
            }
        }
    }

    if(!mad_ok) return;

    bool ndef_sec[40] = {false};

    for(uint8_t s = 1; s <= 7u; s++) {
        uint8_t i = 2u + (s - 1u) * 2u;
        if(blk1.data[i] == 0xE1 && blk1.data[i + 1u] == 0x03)
            ndef_sec[s] = true;
    }

    for(uint8_t s = 8; s <= 15u; s++) {
        uint8_t i = (s - 8u) * 2u;
        if(blk2.data[i] == 0xE1 && blk2.data[i + 1u] == 0x03)
            ndef_sec[s] = true;
    }

    if(app->mfc_type == MfClassicType4k) {
        uint8_t        first16 = mf_classic_get_first_block_num_of_sector(16);
        MfClassicBlock m21 = {{0}}, m22 = {{0}};

        for(uint8_t ki = 0; ki < sizeof(mad_cand) / sizeof(mad_cand[0]); ki++) {
            MfClassicKey     k = mad_cand[ki];
            MfClassicAuthContext ctx;
            if(mf_classic_poller_sync_auth(
                   app->nfc, first16 + 1u, &k, MfClassicKeyTypeA, &ctx) ==
               MfClassicErrorNone) {
                MfClassicKey k1 = k, k2 = k;
                mf_classic_poller_sync_read_block(
                    app->nfc, first16 + 1u, &k1, MfClassicKeyTypeA, &m21);
                mf_classic_poller_sync_read_block(
                    app->nfc, first16 + 2u, &k2, MfClassicKeyTypeA, &m22);
                break;
            }
        }

        for(uint8_t s = 17; s <= 23u; s++) {
            uint8_t i = 2u + (s - 17u) * 2u;
            if(m21.data[i] == 0xE1 && m21.data[i + 1u] == 0x03)
                ndef_sec[s] = true;
        }
        for(uint8_t s = 24; s <= 31u; s++) {
            uint8_t i = (s - 24u) * 2u;
            if(m22.data[i] == 0xE1 && m22.data[i + 1u] == 0x03)
                ndef_sec[s] = true;
        }
    }

    uint8_t sec_count = mf_classic_get_total_sectors_num(app->mfc_type);
    size_t  total     = 0;

    for(uint8_t s = 1; s < sec_count; s++) {
        if(!ndef_sec[s]) continue;
        total += (size_t)(mf_classic_get_blocks_num_in_sector(s) - 1u) * 16u;
    }

    if(total == 0) return;

    uint8_t* buf = malloc(total);
    if(!buf) return;
    memset(buf, 0, total);

    size_t offset = 0;
    bool   any_ok = false;

    for(uint8_t s = 1; s < sec_count; s++) {
        if(!ndef_sec[s]) continue;
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) break;

        uint8_t first_blk = mf_classic_get_first_block_num_of_sector(s);
        uint8_t data_blks = mf_classic_get_blocks_num_in_sector(s) - 1u;

        MfClassicKey sec_key = {{0}};
        bool         sec_ok  = false;

        for(uint8_t ki = 0;
            ki < sizeof(ndef_cand) / sizeof(ndef_cand[0]) && !sec_ok;
            ki++) {
            MfClassicKey     k = ndef_cand[ki];
            MfClassicAuthContext ctx;
            if(mf_classic_poller_sync_auth(
                   app->nfc, first_blk, &k, MfClassicKeyTypeA, &ctx) ==
               MfClassicErrorNone) {
                sec_key = k;
                sec_ok  = true;
            }
        }

        if(!sec_ok) {
            offset += (size_t)data_blks * 16u;
            continue;
        }

        for(uint8_t bi = 0; bi < data_blks; bi++) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
                free(buf);
                return;
            }
            MfClassicKey   k = sec_key;
            MfClassicBlock blk;
            if(mf_classic_poller_sync_read_block(
                   app->nfc, first_blk + bi, &k, MfClassicKeyTypeA, &blk) ==
               MfClassicErrorNone) {
                memcpy(buf + offset, blk.data, 16u);
                any_ok = true;
            }
            offset += 16u;
        }
    }

    if(any_ok) {
        nfc_tools_ndef_parse_type2_tag(buf, total, app->ndef_str);
        nfc_tools_ndef_parse_type2_tag_structured(app, buf, total);
    }

    free(buf);
}

// ── NDEF Write ────────────────────────────────────────────────────────────────

bool nfc_tools_mfc_write_ndef(
    NfcToolsApp*   app,
    const uint8_t* ndef_buf,
    size_t         ndef_size)
{
    MfClassicType mfc_type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(app->nfc, &mfc_type) != MfClassicErrorNone) {
        furi_string_set(app->info_str, "Detection failed");
        return false;
    }

    if(mfc_type == MfClassicTypeMini) {
        furi_string_set(app->info_str, "Mifare Mini:\nNDEF not supported");
        return false;
    }

    bool    is_4k       = (mfc_type == MfClassicType4k);
    uint8_t sec_count   = mf_classic_get_total_sectors_num(mfc_type);
    uint8_t max_sector  = is_4k ? 31u : (uint8_t)(sec_count - 1u);

    size_t  avail            = 0;
    uint8_t last_ndef_sector = 0;
    size_t  remaining        = ndef_size;

    for(uint8_t s = 1; s <= max_sector; s++) {
        if(is_4k && s == 16) continue;

        uint8_t data_blks    = mf_classic_get_blocks_num_in_sector(s) - 1u;
        size_t  sector_bytes = (size_t)data_blks * 16u;
        avail += sector_bytes;

        if(remaining > 0) {
            if(remaining <= sector_bytes) remaining = 0;
            else                          remaining -= sector_bytes;
            last_ndef_sector = s;
        }
    }

    if(last_ndef_sector == 0) last_ndef_sector = 1;

    if(remaining > 0) {
        furi_string_printf(
            app->info_str,
            "NDEF too large!\n%u bytes needed\n%u available",
            (unsigned)ndef_size,
            (unsigned)avail);
        return false;
    }

    // Sector 0: write MAD1
    {
        uint8_t          first_blk = mf_classic_get_first_block_num_of_sector(0);
        MfClassicKey     found_key;
        MfClassicKeyType found_type;

        if(!mfc_ndef_try_auth(app, first_blk, &found_key, &found_type)) {
            furi_string_set(app->info_str, "Sector 0 auth\nfailed");
            return false;
        }

        uint8_t mad1[16] = {0};
        mad1[1] = 0x00;
        for(uint8_t s = 1; s <= 7u && s <= last_ndef_sector; s++) {
            mad1[2 + (s - 1u) * 2u] = 0xE1;
            mad1[3 + (s - 1u) * 2u] = 0x03;
        }

        uint8_t mad2[16] = {0};
        for(uint8_t s = 8; s <= 15u && s <= last_ndef_sector; s++) {
            uint8_t i  = (s - 8u) * 2u;
            mad2[i]    = 0xE1;
            mad2[i + 1] = 0x03;
        }

        {
            uint8_t crc_input[31];
            memcpy(crc_input,       mad1 + 1, 15);
            memcpy(crc_input + 15u, mad2,     16);
            mad1[0] = mfc_mad_crc8(crc_input, 31);
        }

        MfClassicBlock blk;
        memcpy(blk.data, mad1, 16);
        if(mf_classic_poller_sync_write_block(
               app->nfc, 1, &found_key, found_type, &blk) != MfClassicErrorNone) {
            furi_string_set(app->info_str, "MAD1 write\nfailed");
            return false;
        }

        memcpy(blk.data, mad2, 16);
        if(mf_classic_poller_sync_write_block(
               app->nfc, 2, &found_key, found_type, &blk) != MfClassicErrorNone) {
            furi_string_set(app->info_str, "MAD2 write\nfailed");
            return false;
        }

        {
            uint8_t        tnum = mf_classic_get_sector_trailer_num_by_sector(0);
            MfClassicBlock tr   = {{0}};
            memcpy(tr.data,       mfc_mad_key_a.data, 6);
            tr.data[6] = 0xFF; tr.data[7] = 0x07; tr.data[8] = 0x80;
            tr.data[9] = 0xC1;
            memcpy(tr.data + 10, mfc_ff_key.data, 6);
            mf_classic_poller_sync_write_block(app->nfc, tnum, &found_key, found_type, &tr);
        }
    }

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;

    // Sector 16 (4K): MAD2
    if(is_4k && last_ndef_sector >= 17u) {
        uint8_t          first_blk = mf_classic_get_first_block_num_of_sector(16);
        MfClassicKey     found_key;
        MfClassicKeyType found_type;

        if(!mfc_ndef_try_auth(app, first_blk, &found_key, &found_type)) {
            furi_string_set(app->info_str, "Sector 16 auth\nfailed");
            return false;
        }

        uint8_t m21[16] = {0};
        m21[1] = 0x00;
        for(uint8_t s = 17; s <= 23u && s <= last_ndef_sector; s++) {
            uint8_t i  = (s - 17u) * 2u;
            m21[2 + i] = 0xE1;
            m21[3 + i] = 0x03;
        }

        uint8_t m22[16] = {0};
        for(uint8_t s = 24; s <= 31u && s <= last_ndef_sector; s++) {
            uint8_t i  = (s - 24u) * 2u;
            m22[i]     = 0xE1;
            m22[i + 1] = 0x03;
        }

        {
            uint8_t crc_input[31];
            memcpy(crc_input,       m21 + 1, 15);
            memcpy(crc_input + 15u, m22,     16);
            m21[0] = mfc_mad_crc8(crc_input, 31);
        }

        MfClassicBlock blk;
        memcpy(blk.data, m21, 16);
        if(mf_classic_poller_sync_write_block(
               app->nfc, (uint8_t)(first_blk + 1u), &found_key, found_type, &blk) !=
           MfClassicErrorNone) {
            furi_string_set(app->info_str, "MAD2 blk1\nwrite failed");
            return false;
        }

        memcpy(blk.data, m22, 16);
        if(mf_classic_poller_sync_write_block(
               app->nfc, (uint8_t)(first_blk + 2u), &found_key, found_type, &blk) !=
           MfClassicErrorNone) {
            furi_string_set(app->info_str, "MAD2 blk2\nwrite failed");
            return false;
        }

        {
            uint8_t        tnum = mf_classic_get_sector_trailer_num_by_sector(16);
            MfClassicBlock tr   = {{0}};
            memcpy(tr.data,       mfc_mad_key_a.data, 6);
            tr.data[6] = 0xFF; tr.data[7] = 0x07; tr.data[8] = 0x80;
            tr.data[9] = 0xC2;
            memcpy(tr.data + 10, mfc_ff_key.data, 6);
            mf_classic_poller_sync_write_block(app->nfc, tnum, &found_key, found_type, &tr);
        }

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;
    }

    // Write NDEF TLV
    size_t ndef_offset = 0;

    for(uint8_t s = 1; s <= last_ndef_sector; s++) {
        if(is_4k && s == 16u) continue;

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;

        uint8_t first_blk  = mf_classic_get_first_block_num_of_sector(s);
        uint8_t total_blks = mf_classic_get_blocks_num_in_sector(s);
        uint8_t data_blks  = total_blks - 1u;
        uint8_t tnum       = mf_classic_get_sector_trailer_num_by_sector(s);

        MfClassicKey     found_key;
        MfClassicKeyType found_type;

        if(!mfc_ndef_try_auth(app, first_blk, &found_key, &found_type)) {
            ndef_offset += (size_t)data_blks * 16u;
            continue;
        }

        for(uint8_t bi = 0; bi < data_blks; bi++) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP)
                return false;

            uint8_t        blk_num = first_blk + bi;
            MfClassicBlock blk     = {{0}};

            if(ndef_offset < ndef_size) {
                size_t chunk = ndef_size - ndef_offset;
                if(chunk > 16u) chunk = 16u;
                memcpy(blk.data, ndef_buf + ndef_offset, chunk);
            }
            ndef_offset += 16u;

            if(mf_classic_poller_sync_write_block(
                   app->nfc, blk_num, &found_key, found_type, &blk) != MfClassicErrorNone) {
                furi_string_printf(
                    app->info_str, "Block %u write\nfailed", (unsigned)blk_num);
                return false;
            }
        }

        {
            MfClassicBlock tr = {{0}};
            memcpy(tr.data,       mfc_ndef_key_a.data, 6);
            tr.data[6] = 0xFF; tr.data[7] = 0x07; tr.data[8] = 0x80;
            tr.data[9] = 0x40;
            memcpy(tr.data + 10, mfc_ndef_key_a.data, 6);
            mf_classic_poller_sync_write_block(app->nfc, tnum, &found_key, found_type, &tr);
        }
    }

    furi_string_printf(
        app->info_str, "%u bytes written\nBack to exit", (unsigned)ndef_size);
    return true;
}

// ── Format ────────────────────────────────────────────────────────────────────

bool nfc_tools_mfc_format(NfcToolsApp* app) {
    MfClassicType mfc_type = MfClassicType1k;
    if(mf_classic_poller_sync_detect_type(app->nfc, &mfc_type) != MfClassicErrorNone) {
        furi_string_set(app->info_str, "Detection failed\nNo Mifare Classic\nfound");
        return false;
    }
    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;

    uint8_t sector_count = mf_classic_get_total_sectors_num(mfc_type);

    static const MfClassicKey mfc_default_keys[] = {
        {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
        {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}},
        {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}},
        {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {{0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}},
    };
    static const uint8_t mfc_key_count =
        sizeof(mfc_default_keys) / sizeof(mfc_default_keys[0]);

    static const MfClassicBlock mfc_factory_trailer = {{
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0x07, 0x80, 0x69,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    }};

    static const MfClassicBlock mfc_zero_block = {{
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }};

    uint8_t formatted_sectors = 0;
    uint8_t failed_sectors    = 0;
    bool    mfc_stop          = false;

    for(uint8_t sector = 0; sector < sector_count && !mfc_stop; sector++) {
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
            mfc_stop = true;
            break;
        }

        uint8_t first_block    = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sec  = mf_classic_get_blocks_num_in_sector(sector);
        uint8_t trailer_block  = mf_classic_get_sector_trailer_num_by_sector(sector);

        MfClassicKey     found_key  = {{0}};
        MfClassicKeyType found_type = MfClassicKeyTypeA;
        bool auth_ok = false;

        for(uint8_t ki = 0; ki < mfc_key_count && !auth_ok; ki++) {
            MfClassicKey     k = mfc_default_keys[ki];
            MfClassicAuthContext auth_ctx;

            if(mf_classic_poller_sync_auth(
                   app->nfc, first_block, &k, MfClassicKeyTypeA, &auth_ctx) ==
               MfClassicErrorNone) {
                found_key  = k;
                found_type = MfClassicKeyTypeA;
                auth_ok    = true;
            } else {
                if(mf_classic_poller_sync_auth(
                       app->nfc, first_block, &k, MfClassicKeyTypeB, &auth_ctx) ==
                   MfClassicErrorNone) {
                    found_key  = k;
                    found_type = MfClassicKeyTypeB;
                    auth_ok    = true;
                }
            }
        }

        if(!auth_ok) {
            failed_sectors++;
            continue;
        }

        for(uint8_t bi = 0; bi < blocks_in_sec - 1; bi++) {
            uint8_t block_num = first_block + bi;

            if(block_num == 0) continue;

            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
                mfc_stop = true;
                break;
            }

            MfClassicBlock blk = mfc_zero_block;
            mf_classic_poller_sync_write_block(
                app->nfc, block_num, &found_key, found_type, &blk);
        }

        if(!mfc_stop) {
            MfClassicBlock trailer = mfc_factory_trailer;

            MfClassicError terr = mf_classic_poller_sync_write_block(
                app->nfc, trailer_block, &found_key, found_type, &trailer);

            if(terr != MfClassicErrorNone && found_type != MfClassicKeyTypeA) {
                MfClassicKey key_ff = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
                mf_classic_poller_sync_write_block(
                    app->nfc, trailer_block, &key_ff, MfClassicKeyTypeA, &trailer);
            }

            formatted_sectors++;
        }
    }

    const char* mfc_type_str =
        (mfc_type == MfClassicType4k)   ? "4K" :
        (mfc_type == MfClassicTypeMini) ? "Mini" : "1K";

    if(formatted_sectors > 0) {
        if(failed_sectors == 0) {
            furi_string_printf(
                app->info_str,
                "MFC %s formatted!\n%u/%u sectors\nBack to exit",
                mfc_type_str,
                (unsigned)formatted_sectors,
                (unsigned)sector_count);
        } else {
            furi_string_printf(
                app->info_str,
                "Partial format!\n%u/%u sectors OK\n%u key unknown",
                (unsigned)formatted_sectors,
                (unsigned)sector_count,
                (unsigned)failed_sectors);
        }
        return true;
    } else {
        furi_string_printf(
            app->info_str,
            "Auth failed!\nNo default key\nworks on this tag");
        return false;
    }
}

// ── Memory Dump ───────────────────────────────────────────────────────────────

void nfc_tools_mfc_dump(NfcToolsApp* app) {
    uint8_t total_sectors = mf_classic_get_total_sectors_num(app->mfc_type);

    static const MfClassicKey mfc_try_keys[] = {
        {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
        {{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}},
        {{0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7}},
        {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {{0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5}},
    };
    static const uint8_t mfc_key_count =
        sizeof(mfc_try_keys) / sizeof(mfc_try_keys[0]);

    bool mfc_stop = false;
    for(uint8_t sector = 0; sector < total_sectors && !mfc_stop; sector++) {
        uint8_t first_blk_num    = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sector = mf_classic_get_blocks_num_in_sector(sector);

        MfClassicKey     found_key  = {{0}};
        MfClassicKeyType found_type = MfClassicKeyTypeA;
        bool             auth_ok    = false;
        MfClassicBlock   probe_blk;
        memset(&probe_blk, 0, sizeof(probe_blk));

        for(uint8_t ki = 0; ki < mfc_key_count && !auth_ok; ki++) {
            MfClassicKey k = mfc_try_keys[ki];
            if(mf_classic_poller_sync_read_block(
                   app->nfc, first_blk_num, &k, MfClassicKeyTypeA, &probe_blk) ==
               MfClassicErrorNone) {
                found_key = k; found_type = MfClassicKeyTypeA; auth_ok = true;
            } else {
                k = mfc_try_keys[ki];
                if(mf_classic_poller_sync_read_block(
                       app->nfc, first_blk_num, &k, MfClassicKeyTypeB, &probe_blk) ==
                   MfClassicErrorNone) {
                    found_key = k; found_type = MfClassicKeyTypeB; auth_ok = true;
                }
            }
        }

        for(uint8_t bi = 0; bi < blocks_in_sector; bi++) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
                mfc_stop = true; break;
            }

            if(!auth_ok) {
                furi_string_cat_str(app->info_str,
                    "?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ??\n");
                continue;
            }

            MfClassicBlock blk;
            if(bi == 0) {
                blk = probe_blk;
            } else {
                uint8_t bnum = first_blk_num + bi;
                if(mf_classic_poller_sync_read_block(
                       app->nfc, bnum, &found_key, found_type, &blk) !=
                   MfClassicErrorNone) {
                    furi_string_cat_str(app->info_str,
                        "-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n");
                    continue;
                }
            }

            furi_string_cat_printf(app->info_str,
                "%02X %02X %02X %02X %02X %02X %02X %02X "
                "%02X %02X %02X %02X %02X %02X %02X %02X\n",
                blk.data[0],  blk.data[1],  blk.data[2],  blk.data[3],
                blk.data[4],  blk.data[5],  blk.data[6],  blk.data[7],
                blk.data[8],  blk.data[9],  blk.data[10], blk.data[11],
                blk.data[12], blk.data[13], blk.data[14], blk.data[15]);
        }
    }
}
