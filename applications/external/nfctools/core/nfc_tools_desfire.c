#include "../include/nfc_tools_desfire.h"
#include "../include/nfc_tools_ndef.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <toolbox/bit_buffer.h>

// ── NFC Forum Type 4 Tag constants ───────────────────────────────────────────

// ISO7816-4 DF Name : D2 76 00 00 85 01 01 (inline dans les APDUs)
#define NDEF_AID_LEN  7U

// Native DESFire AID (3 bytes little-endian) used during application creation
static const uint8_t DESFIRE_NDEF_AID_LE[] = { 0x01, 0x00, 0x00 };

// File IDs NFC Forum T4T
#define CC_FILE_ID_HI    0xE1U
#define CC_FILE_ID_LO    0x03U
#define NDEF_FILE_ID_HI  0xE1U
#define NDEF_FILE_ID_LO  0x04U

// Fixed size of the CC file (NFC Forum T4T v2.0 = 15 bytes)
#define CC_SIZE  15U

// NDEF file size bounds, computed dynamically from the chip's free memory
#define NDEF_FILE_MIN_SIZE   256U   // conservative minimum (EV1 2K loaded cards)
#define NDEF_FILE_MAX_SIZE  8190U   // maximum (DESFire EV3 8K)

// Max chunk for UPDATE BINARY (conservative: < 59 bytes C-APDU limit)
#define APDU_MAX_LC  54U

// Status words ISO7816-4
#define SW1_OK         0x90U
#define SW2_OK         0x00U
#define SW1_NOT_FOUND  0x6AU
#define SW2_NOT_FOUND  0x82U

// DESFire ISO-wrapped status words (CLA=0x90)
#define DSW1_OK  0x91U
#define DSW2_OK  0x00U

// ── Helper: APDU exchange ─────────────────────────────────────────────────────

typedef struct {
    uint8_t data[258];
    size_t  len;
    bool    ok;
} ApduResp;

static ApduResp apdu_exchange(
    Iso14443_4aPoller* poller,
    const uint8_t*     cmd,
    size_t             cmd_len)
{
    ApduResp resp = {0};
    BitBuffer* tx = bit_buffer_alloc(cmd_len);
    BitBuffer* rx = bit_buffer_alloc(258);

    bit_buffer_append_bytes(tx, cmd, cmd_len);
    Iso14443_4aError err = iso14443_4a_poller_send_block(poller, tx, rx);

    if(err == Iso14443_4aErrorNone) {
        resp.len = bit_buffer_get_size_bytes(rx);
        if(resp.len > sizeof(resp.data)) resp.len = sizeof(resp.data);
        memcpy(resp.data, bit_buffer_get_data(rx), resp.len);
        resp.ok = true;
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return resp;
}

static bool sw_ok(const ApduResp* r) {
    return r->ok && r->len >= 2 &&
           r->data[r->len - 2] == SW1_OK &&
           r->data[r->len - 1] == SW2_OK;
}

static bool sw_not_found(const ApduResp* r) {
    return r->ok && r->len >= 2 &&
           r->data[r->len - 2] == SW1_NOT_FOUND &&
           r->data[r->len - 1] == SW2_NOT_FOUND;
}

// DESFire ISO-wrapped response: 91 00
static bool dsw_ok(const ApduResp* r) {
    return r->ok && r->len >= 2 &&
           r->data[r->len - 2] == DSW1_OK &&
           r->data[r->len - 1] == DSW2_OK;
}

// Full validation of an NFC Forum T4T v2.0 Capability Container.
// Checks critical fields according to the specification:
//   cc[2]     = 0x20 : Mapping version 2.0
//   cc[7]     = 0x04 : NDEF File Control TLV tag
//   cc[8]     = 0x06 : TLV length = 6 (FileID 2 + MaxNDEF 2 + Access 2)
//   cc[9..10] = E1 04 : NDEF File ID
//   cc[11..12] > 0   : coherent MaxNDEF (positive user capacity)
static bool cc_is_valid(const uint8_t* cc, size_t cc_len) {
    if(cc_len < CC_SIZE) return false;
    return (cc[2]  == 0x20)
        && (cc[7]  == 0x04)
        && (cc[8]  == 0x06)
        && (cc[9]  == NDEF_FILE_ID_HI)
        && (cc[10] == NDEF_FILE_ID_LO)
        && ((uint16_t)(((uint16_t)cc[11] << 8) | (uint16_t)cc[12]) > 0u);
}

// ── Helpers ISO7816-4 ─────────────────────────────────────────────────────────

// SELECT FILE by EF ID (P1=00, P2=0C: no response data)
static bool t4t_select_file(Iso14443_4aPoller* poller, uint8_t fid_hi, uint8_t fid_lo) {
    const uint8_t cmd[] = { 0x00, 0xA4, 0x00, 0x0C, 0x02, fid_hi, fid_lo };
    ApduResp r = apdu_exchange(poller, cmd, sizeof(cmd));
    return sw_ok(&r);
}

// READ BINARY from offset (up to 127 bytes per call)
static bool t4t_read_binary(
    Iso14443_4aPoller* poller,
    uint16_t           offset,
    uint8_t            length,
    uint8_t*           out,
    size_t*            out_len)
{
    const uint8_t cmd[] = {
        0x00, 0xB0,
        (uint8_t)(offset >> 8),
        (uint8_t)(offset & 0xFF),
        length
    };
    ApduResp r = apdu_exchange(poller, cmd, sizeof(cmd));
    if(!sw_ok(&r) || r.len < 2) return false;
    size_t dlen = r.len - 2;
    if(out_len) *out_len = dlen;
    if(out && dlen) memcpy(out, r.data, dlen);
    return true;
}

// UPDATE BINARY: length <= APDU_MAX_LC
static bool t4t_update_binary(
    Iso14443_4aPoller* poller,
    uint16_t           offset,
    const uint8_t*     data,
    uint8_t            length)
{
    uint8_t cmd[5 + APDU_MAX_LC];
    cmd[0] = 0x00;              // CLA
    cmd[1] = 0xD6;              // INS UPDATE BINARY
    cmd[2] = (uint8_t)(offset >> 8);
    cmd[3] = (uint8_t)(offset & 0xFF);
    cmd[4] = length;            // Lc
    memcpy(&cmd[5], data, length);
    ApduResp r = apdu_exchange(poller, cmd, 5u + length);
    return sw_ok(&r);
}

// ── DESFire memory helpers ───────────────────────────────────────────────────

// GET FREE MEMORY (INS=0x6E) — native DeSFire command.
// Must be called from the PICC master application context (90 5A 00 00 00 selected).
// Returns 0 on failure (insufficient rights or unsupported command).
static uint32_t desfire_get_free_memory(Iso14443_4aPoller* poller) {
    // Case-2 ISO7816 format: CLA INS P1 P2 Le (5 bytes, no Lc)
    // Expected response: 3 LE bytes (24-bit free memory) + 91 00
    const uint8_t cmd[] = { 0x90, 0x6E, 0x00, 0x00, 0x00 };
    ApduResp r = apdu_exchange(poller, cmd, sizeof(cmd));
    if(!dsw_ok(&r) || r.len < 5) return 0u;
    return (uint32_t)r.data[0]
         | ((uint32_t)r.data[1] << 8)
         | ((uint32_t)r.data[2] << 16);
}

// Computes the optimal NDEF file size based on available free memory.
// The size is clamped between NDEF_FILE_MIN_SIZE and NDEF_FILE_MAX_SIZE and
// rounded down to an even value to avoid odd sizes (rejected by some chips).
// free_memory = 0 → conservative fallback (MIN_SIZE).
static uint16_t desfire_compute_ndef_file_size(uint32_t free_memory) {
    if(free_memory == 0u) return (uint16_t)NDEF_FILE_MIN_SIZE;
    // Overhead for DeSFire internal structures (app entry, CC file, headers)
    const uint32_t overhead = 400u;
    if(free_memory <= overhead + NDEF_FILE_MIN_SIZE) return (uint16_t)NDEF_FILE_MIN_SIZE;
    uint32_t available = free_memory - overhead;
    if(available < NDEF_FILE_MIN_SIZE) available = NDEF_FILE_MIN_SIZE;
    if(available > NDEF_FILE_MAX_SIZE) available = NDEF_FILE_MAX_SIZE;
    // Round down to nearest even
    available &= ~1u;
    return (uint16_t)available;
}

// ── Forward declarations ──────────────────────────────────────────────────────
static bool desfire_ensure_ndef_files(Iso14443_4aPoller* poller, NfcToolsApp* app);

// ── NFC Forum application creation (DESFire EV1+) ────────────────────────────
// Called only when SELECT AID returns 6A 82 (application absent).
// Uses DESFire ISO-wrapped commands (CLA=0x90) to create the app and
// both files (CC=E103, NDEF=E104), then writes the initial CC.
// Returns true if everything succeeded without error.
// On failure, app->info_str contains the error message.

static bool desfire_create_ndef_app(Iso14443_4aPoller* poller, NfcToolsApp* app) {
    // ── 1. SELECT MASTER APPLICATION (AID = 00 00 00, Lc length = 0) ──────
    {
        // SELECT PICC (AID 00 00 00) in native DeSFire mode.
        // Required before CREATE APPLICATION: needs PICC master context.
        const uint8_t cmd[] = { 0x90, 0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00 };
        ApduResp picc_sel = apdu_exchange(poller, cmd, sizeof(cmd));
        if(!dsw_ok(&picc_sel)) {
            if(picc_sel.ok && picc_sel.len >= 2) {
                furi_string_printf(
                    app->info_str,
                    "SELECT PICC failed\n%02X %02X",
                    (unsigned)picc_sel.data[picc_sel.len - 2],
                    (unsigned)picc_sel.data[picc_sel.len - 1]);
            } else {
                furi_string_set(app->info_str, "SELECT PICC failed\n(no response)");
            }
            return false;
        }
    }

    // ── 2. CREATE APPLICATION (DESFire ISO-wrapped: CLA=0x90, INS=0xCA) ────
    // AID DESFire (LE): 01 00 00
    // KeySettings1 = 0x0F: all operations free, key 0 can change everything
    // NumOfKeys    = 0x81: bit7=1 enables ISO file numbering + 1 DES key
    // KeySettings2 = 0x03 (EV2/EV3 ONLY):
    //   bit0 = 1: ISO SELECT FILE by FileID active for DF files
    //   bit1 = 1: ISO SELECT APPLICATION by DF Name active
    //   Without this byte, 00 A4 00 0C commands on files are refused
    //   on EV2/EV3 even with NumOfKeys bit7=1.
    // NumOfISOFiles = 0x02 (EV3 ONLY): number of ISO files (CC + NDEF)
    //   EV3 requires this extra field after KeySettings2 (Lc=0x10 instead of 0x0F).
    //   Without this field, EV3 responds 91 7E (Length Error).
    // ISO FileID: E1 10 (DF file identifier)
    // ISO DFName: D2 76 00 00 85 01 01 (NFC Forum AID)
    //
    // Strategy: fallback chain EV3 -> EV2 -> EV1.
    // Each format returns 91 7E if the Lc length does not match the version.
    {
        // Format EV3 : Lc=0x10 (16 bytes) avec KeySettings2 + NumOfISOFiles
        const uint8_t cmd_ev3[] = {
            0x90, 0xCA, 0x00, 0x00, 0x10,
            DESFIRE_NDEF_AID_LE[0], DESFIRE_NDEF_AID_LE[1], DESFIRE_NDEF_AID_LE[2],
            0x0F,                           // KeySettings1
            0x81,                           // NumOfKeys: ISO file IDs (bit7) + 1 DES key
            0x03,                           // KeySettings2 (EV2/EV3): ISO FileIDs + DFName
            0x02,                           // NumOfISOFiles (EV3): CC + NDEF
            0x10, 0xE1,                     // ISO FileID of DF = E110h (LE, NFC Forum)
            0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, // ISO DFName
            0x00                            // Le
        };
        // Format EV2 : Lc=0x0F (15 bytes) avec KeySettings2
        const uint8_t cmd_ev2[] = {
            0x90, 0xCA, 0x00, 0x00, 0x0F,
            DESFIRE_NDEF_AID_LE[0], DESFIRE_NDEF_AID_LE[1], DESFIRE_NDEF_AID_LE[2],
            0x0F,                           // KeySettings1
            0x81,                           // NumOfKeys: ISO file IDs (bit7) + 1 DES key
            0x03,                           // KeySettings2 (EV2): ISO FileIDs + DFName
            0x10, 0xE1,                     // ISO FileID of DF = E110h (LE, NFC Forum)
            0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, // ISO DFName
            0x00                            // Le
        };
        // Format EV1 : Lc=0x0E (14 bytes), sans KeySettings2
        const uint8_t cmd_ev1[] = {
            0x90, 0xCA, 0x00, 0x00, 0x0E,
            DESFIRE_NDEF_AID_LE[0], DESFIRE_NDEF_AID_LE[1], DESFIRE_NDEF_AID_LE[2],
            0x0F,                           // KeySettings1
            0x81,                           // NumOfKeys: ISO file IDs (bit7) + 1 DES key
            0x10, 0xE1,                     // ISO FileID of DF = E110h (LE, NFC Forum)
            0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, // ISO DFName
            0x00                            // Le
        };

        // Local macro: true if the DESFire response is sw1=0x91, sw2=code
        #define DSW_IS(resp, code) \
            ((resp).ok && (resp).len >= 2 && \
             (resp).data[(resp).len - 2] == 0x91U && \
             (resp).data[(resp).len - 1] == (code))

        // Inline subroutine: tries EV3 -> EV2 -> EV1 and returns the last response
        #define TRY_CREATE(out_r) \
            do { \
                (out_r) = apdu_exchange(poller, cmd_ev3, sizeof(cmd_ev3)); \
                if(DSW_IS((out_r), 0x7EU)) \
                    (out_r) = apdu_exchange(poller, cmd_ev2, sizeof(cmd_ev2)); \
                if(DSW_IS((out_r), 0x7EU)) \
                    (out_r) = apdu_exchange(poller, cmd_ev1, sizeof(cmd_ev1)); \
            } while(0)

        ApduResp r;
        TRY_CREATE(r);

        // 91 DE = Duplicate: application already exists (partial creation, or card
        // formatted by another tool without KeySettings2).
        // Delete it (DELETE APPLICATION, INS=0xDA) from the PICC master context,
        // then retry the CREATE chain. Works without authentication on factory
        // cards (default PICC key = all zeros).
        if(DSW_IS(r, 0xDEU)) {
            const uint8_t del_cmd[] = {
                0x90, 0xDA, 0x00, 0x00, 0x03,
                DESFIRE_NDEF_AID_LE[0], DESFIRE_NDEF_AID_LE[1], DESFIRE_NDEF_AID_LE[2],
                0x00  // Le
            };
            ApduResp del = apdu_exchange(poller, del_cmd, sizeof(del_cmd));
            if(!dsw_ok(&del)) {
                // Deletion refused (non-default PICC key, or protected card)
                furi_string_set(
                    app->info_str,
                    "NFC Forum app exists\nbut can't be reset.\nUse a factory card.");
                return false;
            }
            // Deletion OK -> full retry
            TRY_CREATE(r);
        }

        #undef TRY_CREATE
        #undef DSW_IS

        if(!dsw_ok(&r)) {
            if(r.ok && r.len >= 2) {
                furi_string_printf(
                    app->info_str,
                    "CREATE APP failed\n%02X %02X",
                    (unsigned)r.data[r.len - 2],
                    (unsigned)r.data[r.len - 1]);
            } else {
                furi_string_set(app->info_str, "CREATE APP failed\n(no response)");
            }
            return false;
        }
    }

    return desfire_ensure_ndef_files(poller, app);
}

// ── CC and NDEF file creation / recovery within the NFC Forum app ─────────────
// Called:
//   (a) immediately after CREATE APPLICATION (in desfire_create_ndef_app)
//   (b) as fallback in the write callback if SELECT CC fails while
//       the application is already present and ISO-selectable.
//
// Sequence:
//   1. Native SELECT APP (90 5A) — required before CREATE FILE (90 CD)
//   2. CREATE CC FILE   — tolerates 91 DE (file already present)
//   3. CREATE NDEF FILE — tolerates 91 DE (file already present)
//   4. ISO SELECT APP   — switch to ISO mode for 00 xx commands
//   5. SELECT CC FILE
//   6. Write initial CC (always written to guarantee correct content)
//
// Returns true if everything succeeded without error.
static bool desfire_ensure_ndef_files(Iso14443_4aPoller* poller, NfcToolsApp* app) {
    // 91 DE = Duplicate: file already exists, we can continue
    #define DSW_IS_DE(r) \
        ((r).ok && (r).len >= 2 && \
         (r).data[(r).len - 2] == 0x91U && (r).data[(r).len - 1] == 0xDEU)

    // -- 0. SELECT PICC MASTER + GET FREE MEMORY --------------------------------
    // Requires PICC master context (before selecting the NDEF app).
    // Used to size the NDEF file according to the chip's actual capacity.
    // We re-select the master regardless (may already be selected
    // from desfire_create_ndef_app, or we arrive from ISO mode after SELECT AID).
    uint16_t ndef_file_size = (uint16_t)NDEF_FILE_MIN_SIZE; // fallback
    {
        const uint8_t master_cmd[] = {
            0x90, 0x5A, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00
        };
        ApduResp picc_sel = apdu_exchange(poller, master_cmd, sizeof(master_cmd));
        if(dsw_ok(&picc_sel)) {
            // PICC master selected: we can query free memory.
            uint32_t free_mem = desfire_get_free_memory(poller);
            ndef_file_size = desfire_compute_ndef_file_size(free_mem);
        }
        // If SELECT PICC fails (non-default PICC key, active ISO context resistant),
        // keep the minimum size (NDEF_FILE_MIN_SIZE).
        // Step 1 (native SELECT NDEF APP) will validate the context for CREATE FILE.
    }

    // -- 1. Native SELECT APPLICATION (90 5A) ----------------------------------
    // Required before native commands (90 CD, etc.).
    {
        const uint8_t cmd[] = {
            0x90, 0x5A, 0x00, 0x00, 0x03,
            DESFIRE_NDEF_AID_LE[0], DESFIRE_NDEF_AID_LE[1], DESFIRE_NDEF_AID_LE[2],
            0x00
        };
        ApduResp r = apdu_exchange(poller, cmd, sizeof(cmd));
        if(!dsw_ok(&r)) {
            furi_string_set(app->info_str, "SELECT APP (native)\nfailed");
            return false;
        }
    }

    // Local macro: true if the DeSFire response is 91 7E (Length Error)
    #define DSW_IS_7E(r) \
        ((r).ok && (r).len >= 2 && \
         (r).data[(r).len - 2] == 0x91U && (r).data[(r).len - 1] == 0x7EU)

    // -- 2. CREATE CC FILE (tolerates 91 DE if file already exists) -------------
    // EV3 → EV2 strategy: EV3 requires 2 AdditionalAccessRights bytes (Lc=0x0B),
    // EV1/EV2 use the classic format (Lc=0x09).
    // 0xFF 0xFF for AdditionalAccessRights = free access on all operations.
    {
        // Format EV3: Lc=0x0B (with AdditionalAccessRights)
        const uint8_t cmd_ev3[] = {
            0x90, 0xCD, 0x00, 0x00, 0x0B,
            0x01,                          // FileNo DeSFire
            CC_FILE_ID_HI, CC_FILE_ID_LO, // ISO FileID = E1 03
            0x00,                          // CommSettings: plain
            0xEE, 0xEE,                    // AccessRights: all free
            (uint8_t)CC_SIZE, 0x00, 0x00,  // FileSize LE 24-bit = 15 bytes
            0xFF, 0xFF,                    // AdditionalAccessRights (EV3): free
            0x00
        };
        // Format EV1/EV2: Lc=0x09 (without AdditionalAccessRights)
        const uint8_t cmd_ev2[] = {
            0x90, 0xCD, 0x00, 0x00, 0x09,
            0x01,                          // FileNo DeSFire
            CC_FILE_ID_HI, CC_FILE_ID_LO, // ISO FileID = E1 03
            0x00,                          // CommSettings: plain
            0xEE, 0xEE,                    // AccessRights: all free
            (uint8_t)CC_SIZE, 0x00, 0x00,  // FileSize LE 24-bit = 15 bytes
            0x00
        };
        ApduResp r = apdu_exchange(poller, cmd_ev3, sizeof(cmd_ev3));
        if(DSW_IS_7E(r)) {
            r = apdu_exchange(poller, cmd_ev2, sizeof(cmd_ev2));
        }
        if(!dsw_ok(&r) && !DSW_IS_DE(r)) {
            furi_string_printf(
                app->info_str, "CREATE CC FILE\nfailed %02X %02X",
                r.ok && r.len >= 2 ? (unsigned)r.data[r.len - 2] : 0u,
                r.ok && r.len >= 2 ? (unsigned)r.data[r.len - 1] : 0u);
            return false;
        }
    }

    // -- 3. CREATE NDEF FILE (tolerates 91 DE if file already exists) -----------
    // Same EV3 → EV2 strategy as for the CC file.
    // Size computed from free memory (NDEF_FILE_MIN_SIZE … NDEF_FILE_MAX_SIZE).
    // If the file already exists (91 DE), keep the minimum size for the CC.
    bool ndef_file_created = false;
    {
        // Format EV3: Lc=0x0B (with AdditionalAccessRights)
        const uint8_t cmd_ev3[] = {
            0x90, 0xCD, 0x00, 0x00, 0x0B,
            0x02,                                              // FileNo DeSFire
            NDEF_FILE_ID_HI, NDEF_FILE_ID_LO,                 // ISO FileID = E1 04
            0x00,                                              // CommSettings: plain
            0xEE, 0xEE,                                        // AccessRights: all free
            (uint8_t)(ndef_file_size & 0xFFU),                 // FileSize LE 24-bit (byte 0)
            (uint8_t)((ndef_file_size >> 8) & 0xFFU),          // FileSize LE 24-bit (byte 1)
            0x00,                                              // FileSize LE 24-bit (byte 2)
            0xFF, 0xFF,                                        // AdditionalAccessRights (EV3)
            0x00
        };
        // Format EV1/EV2: Lc=0x09 (without AdditionalAccessRights)
        const uint8_t cmd_ev2[] = {
            0x90, 0xCD, 0x00, 0x00, 0x09,
            0x02,                                              // FileNo DeSFire
            NDEF_FILE_ID_HI, NDEF_FILE_ID_LO,                 // ISO FileID = E1 04
            0x00,                                              // CommSettings: plain
            0xEE, 0xEE,                                        // AccessRights: all free
            (uint8_t)(ndef_file_size & 0xFFU),                 // FileSize LE 24-bit (byte 0)
            (uint8_t)((ndef_file_size >> 8) & 0xFFU),          // FileSize LE 24-bit (byte 1)
            0x00,                                              // FileSize LE 24-bit (byte 2)
            0x00
        };
        ApduResp r = apdu_exchange(poller, cmd_ev3, sizeof(cmd_ev3));
        if(DSW_IS_7E(r)) {
            r = apdu_exchange(poller, cmd_ev2, sizeof(cmd_ev2));
        }
        if(dsw_ok(&r)) {
            ndef_file_created = true;
        } else if(!DSW_IS_DE(r)) {
            furi_string_printf(
                app->info_str, "CREATE NDEF FILE\nfailed %02X %02X",
                r.ok && r.len >= 2 ? (unsigned)r.data[r.len - 2] : 0u,
                r.ok && r.len >= 2 ? (unsigned)r.data[r.len - 1] : 0u);
            return false;
        }
        // If 91 DE (file exists), its actual size is unknown.
        // Use the minimum size in the CC to remain conservative.
        if(!ndef_file_created) {
            ndef_file_size = (uint16_t)NDEF_FILE_MIN_SIZE;
        }
    }

    // -- 3b. Initialise NLEN = 0000 in the NDEF file --------------------------
    // A newly created NDEF file contains uninitialised data.
    // An NFC phone reading the tag before the first Flipper write
    // would see a random NLEN and might interpret the content as corrupt.
    // Write NLEN = 0000 via native DeSFire WRITE DATA (still in native mode,
    // no need to switch to ISO for this init operation).
    // Non-fatal if it fails (tag already initialised, or stubborn EV1).
    // Only done if the file was just created (ndef_file_created=true) to
    // avoid overwriting valid NDEF content during a fallback pass.
    if(ndef_file_created) {
        const uint8_t nlen_init[] = {
            0x90, 0x3D, 0x00, 0x00, 0x09,  // WRITE DATA (INS=0x3D)
            0x02,                            // FileNo NDEF
            0x00, 0x00, 0x00,               // Offset LE = 0
            0x02, 0x00, 0x00,               // Length LE = 2 bytes
            0x00, 0x00,                      // NLEN = 0x0000 (empty but valid tag)
            0x00                             // Le
        };
        apdu_exchange(poller, nlen_init, sizeof(nlen_init)); // non fatal
    }

    #undef DSW_IS_7E

    #undef DSW_IS_DE

    // -- 4. Switch to ISO mode: SELECT NDEF APPLICATION (00 A4) ---------------
    // Required before SELECT FILE (00 A4 00 0C) and ISO commands.
    {
        const uint8_t cmd[] = {
            0x00, 0xA4, 0x04, 0x00, (uint8_t)NDEF_AID_LEN,
            0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01,
            0x00
        };
        ApduResp r = apdu_exchange(poller, cmd, sizeof(cmd));
        if(!sw_ok(&r)) {
            furi_string_set(app->info_str, "ISO SELECT (files)\nfailed");
            return false;
        }
    }

    // -- 5. SELECT CC FILE -----------------------------------------------------
    if(!t4t_select_file(poller, CC_FILE_ID_HI, CC_FILE_ID_LO)) {
        furi_string_set(app->info_str, "SELECT CC (init)\nfailed");
        return false;
    }

    // -- 6. Write (or re-write) the CC -----------------------------------------
    // The CC is always rewritten to guarantee NFC Forum T4T v2.0-compliant content,
    // even if the file already existed with invalid content.
    //
    // MaxNDEF (bytes 11-12) = NDEF file size - 2 (NFC Forum T4T spec).
    // The first 2 bytes of the NDEF file are reserved for NLEN and are not
    // part of the NDEF message itself.
    const uint16_t cc_max_ndef = ndef_file_size - 2u;
    const uint8_t cc_init[CC_SIZE] = {
        0x00, (uint8_t)CC_SIZE,             // CC Length = 15
        0x20,                               // Mapping version 2.0
        0x00, 0x7F,                         // MLe : 127 bytes (max READ BINARY)
        0x00, (uint8_t)APDU_MAX_LC,         // MLc : 54 bytes (max UPDATE BINARY)
        0x04,                               // NDEF File Control TLV tag
        0x06,                               // TLV length = 6
        NDEF_FILE_ID_HI, NDEF_FILE_ID_LO,   // NDEF File ID = E1 04
        (uint8_t)(cc_max_ndef >> 8),         // MaxNDEF HI = (file_size-2) >> 8
        (uint8_t)(cc_max_ndef & 0xFFU),      // MaxNDEF LO = (file_size-2) & 0xFF
        0x00,                               // Read access : libre
        0x00,                               // Write access : libre
    };
    if(!t4t_update_binary(poller, 0, cc_init, (uint8_t)CC_SIZE)) {
        furi_string_set(app->info_str, "CC init write\nfailed");
        return false;
    }

    return true;
}

// ── Main callback (ISO14443-4A) ───────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    const uint8_t* ndef_buf;
    size_t         ndef_size;
    bool           success;
} DesfireNdefCtx;

static NfcCommand nfc_tools_desfire_ndef_write_cb(NfcGenericEvent event, void* context) {
    DesfireNdefCtx*         ctx = context;
    Iso14443_4aPollerEvent* ev  = event.event_data;
    NfcToolsApp*            app = ctx->app;

    if(ev->type != Iso14443_4aPollerEventTypeReady) {
        furi_string_set(app->info_str, "Tag contact error");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    Iso14443_4aPoller* poller = event.instance;

    // ── Step 1 : SELECT NDEF APPLICATION ─────────────────────────────────────
    const uint8_t sel_cmd[5 + NDEF_AID_LEN + 1] = {
        0x00, 0xA4, 0x04, 0x00, (uint8_t)NDEF_AID_LEN,
        0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, // NDEF_AID inline
        0x00
    };
    ApduResp sel = apdu_exchange(poller, sel_cmd, sizeof(sel_cmd));

    if(sw_not_found(&sel)) {
        // Application absent: attempt to create it
        if(!desfire_create_ndef_app(poller, app)) {
            // info_str already set in desfire_create_ndef_app
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
        // After creation, the app is selected and the CC is written.
        // Re-select to restore a clean state.
        sel = apdu_exchange(poller, sel_cmd, sizeof(sel_cmd));
        if(!sw_ok(&sel)) {
            furi_string_set(app->info_str, "Post-create SELECT\nfailed");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
    } else if(!sw_ok(&sel)) {
        furi_string_set(app->info_str, "SELECT AID failed");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 2: SELECT CC FILE and read ──────────────────────────────────────
    // If SELECT CC fails (file absent in an existing app — partial creation
    // from a previous session, or app formatted by another tool without
    // CC/NDEF files), attempt to create the missing files, then re-select
    // the app and retry SELECT CC.
    if(!t4t_select_file(poller, CC_FILE_ID_HI, CC_FILE_ID_LO)) {
        if(!desfire_ensure_ndef_files(poller, app)) {
            // info_str already set in desfire_ensure_ndef_files
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
        // ISO re-select the app to restore a clean context
        ApduResp resel = apdu_exchange(poller, sel_cmd, sizeof(sel_cmd));
        if(!sw_ok(&resel) || !t4t_select_file(poller, CC_FILE_ID_HI, CC_FILE_ID_LO)) {
            furi_string_set(app->info_str, "SELECT CC failed");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
    }

    uint8_t cc[CC_SIZE];
    size_t  cc_read = 0;
    if(!t4t_read_binary(poller, 0, (uint8_t)CC_SIZE, cc, &cc_read) ||
       cc_read < CC_SIZE) {
        furi_string_set(app->info_str, "Read CC failed");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Full CC validation per the NFC Forum T4T v2.0 spec.
    // cc_is_valid() checks: Mapping version (0x20), TLV tag (0x04),
    // TLV length (0x06), NDEF File ID (E1 04), MaxNDEF > 0.
    // If invalid (third-party formatting, corruption, or missing field),
    // attempt repair via desfire_ensure_ndef_files() then re-read.
    if(!cc_is_valid(cc, cc_read)) {
        if(!desfire_ensure_ndef_files(poller, app)) {
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
        // Clean re-select of the app + CC
        ApduResp resel2 = apdu_exchange(poller, sel_cmd, sizeof(sel_cmd));
        if(!sw_ok(&resel2) ||
           !t4t_select_file(poller, CC_FILE_ID_HI, CC_FILE_ID_LO) ||
           !t4t_read_binary(poller, 0, (uint8_t)CC_SIZE, cc, &cc_read) ||
           cc_read < CC_SIZE || !cc_is_valid(cc, cc_read)) {
            furi_string_set(app->info_str, "CC invalid\n(repair failed)");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
    }
    // cc[14] = Write Access: 0x00=free, 0xFF=locked
    if(cc[14] == 0xFF) {
        furi_string_set(app->info_str, "NDEF write locked\n(access 0xFF)");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Maximum NDEF message capacity (cc[11..12] big-endian).
    // Per the NFC Forum T4T spec, the CC stores MaxNDEF = file_size - 2,
    // i.e. directly the user capacity (NLEN not included).
    uint16_t ndef_file_max = ((uint16_t)cc[11] << 8) | cc[12];
    // Compatibility with old CCs (written before the spec fix) that
    // stored the raw file size: if ndef_file_max looks like a multiple of 256
    // and is greater than 254, subtract 2 as a precaution.
    uint16_t ndef_user_max;
    if(ndef_file_max > 254u && (ndef_file_max & 0x00FFu) == 0u) {
        // Likely an old CC (size = multiple of 256) — conservative
        ndef_user_max = (ndef_file_max >= 2u) ? (ndef_file_max - 2u) : 0u;
    } else {
        // NFC Forum-compliant CC: MaxNDEF = file_size - 2 → direct value
        ndef_user_max = ndef_file_max;
    }

    if(ctx->ndef_size > (size_t)ndef_user_max) {
        furi_string_printf(
            app->info_str,
            "Tag too small!\n%u bytes needed\n%u available",
            (unsigned)ctx->ndef_size,
            (unsigned)ndef_user_max);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 3 : SELECT NDEF FILE ─────────────────────────────────────────────
    if(!t4t_select_file(poller, NDEF_FILE_ID_HI, NDEF_FILE_ID_LO)) {
        furi_string_set(app->info_str, "SELECT NDEF failed");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 4: Invalidate (length = 00 00) ──────────────────────────────────
    // Read the old NDEF length to erase any residual data
    uint8_t  old_len_raw[2] = {0, 0};
    size_t   old_len_got    = 0;
    uint16_t old_ndef_len   = 0;
    if(t4t_read_binary(poller, 0, 2, old_len_raw, &old_len_got) && old_len_got >= 2) {
        old_ndef_len = ((uint16_t)old_len_raw[0] << 8) | old_len_raw[1];
    }

    const uint8_t zeros[2] = { 0x00, 0x00 };
    if(!t4t_update_binary(poller, 0, zeros, 2)) {
        furi_string_set(app->info_str, "Invalidate failed");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 5: Write NDEF payload in chunks (offset 2) ──────────────────────
    size_t written = 0;
    while(written < ctx->ndef_size) {
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
        size_t   chunk  = ctx->ndef_size - written;
        if(chunk > APDU_MAX_LC) chunk = APDU_MAX_LC;
        uint16_t offset = (uint16_t)(2u + written);

        if(!t4t_update_binary(poller, offset, ctx->ndef_buf + written, (uint8_t)chunk)) {
            furi_string_printf(
                app->info_str, "Write failed\n@ offset %u", (unsigned)offset);
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
        written += chunk;
    }

    // ── Step 6: Validate (write the actual length) ────────────────────────────
    // Erase residuals if the new message is shorter than the old one
    if(old_ndef_len > (uint16_t)ctx->ndef_size) {
        static const uint8_t zero_pad[APDU_MAX_LC] = {0};
        size_t gap_off = 2u + ctx->ndef_size;
        size_t gap_end = 2u + (size_t)old_ndef_len;
        while(gap_off < gap_end) {
            size_t  remaining = gap_end - gap_off;
            uint8_t chunk_sz  = (remaining > APDU_MAX_LC) ?
                                (uint8_t)APDU_MAX_LC : (uint8_t)remaining;
            if(!t4t_update_binary(poller, (uint16_t)gap_off, zero_pad, chunk_sz)) break;
            gap_off += chunk_sz;
        }
    }

    const uint8_t len_bytes[2] = {
        (uint8_t)(ctx->ndef_size >> 8),
        (uint8_t)(ctx->ndef_size & 0xFF)
    };
    if(!t4t_update_binary(poller, 0, len_bytes, 2)) {
        furi_string_set(app->info_str, "Validate failed");
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    furi_string_printf(
        app->info_str, "%u bytes written\nBack to exit", (unsigned)ctx->ndef_size);
    ctx->success = true;
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool nfc_tools_desfire_write_ndef(
    NfcToolsApp*   app,
    const uint8_t* ndef_data,
    size_t         ndef_size)
{
    // The T4T NDEF file contains the raw NDEF message, without TLV encapsulation.
    // nfc_tools_ndef_build() produces TLV (Type 2/5 format): strip the
    // 03 [len] ... FE envelope before writing to the DESFire NDEF file.
    const uint8_t* raw  = ndef_data;
    size_t         rlen = ndef_size;

    if(ndef_size >= 2 && ndef_data[0] == 0x03) {
        if(ndef_data[1] == 0xFF && ndef_size >= 5) {
            // 3-byte length: FF len_hi len_lo
            uint16_t len = ((uint16_t)ndef_data[2] << 8) | ndef_data[3];
            raw  = ndef_data + 4;
            rlen = (size_t)len;
            if(rlen > ndef_size - 4u) rlen = ndef_size - 4u; // safety bound
        } else {
            // 1-byte length
            raw  = ndef_data + 2;
            rlen = ndef_data[1];
            if(rlen > ndef_size - 2u) rlen = ndef_size - 2u; // safety bound
        }
    }

    DesfireNdefCtx ctx = {
        .app       = app,
        .done      = furi_event_flag_alloc(),
        .ndef_buf  = raw,
        .ndef_size = rlen,
        .success   = false,
    };
    ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(ctx.poller, nfc_tools_desfire_ndef_write_cb, &ctx);

    furi_event_flag_wait(ctx.done, 1u, FuriFlagWaitAny, 15000);

    nfc_poller_stop(ctx.poller);
    nfc_poller_free(ctx.poller);
    furi_event_flag_free(ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;
    return ctx.success;
}

// ── DESFire NDEF read (NFC Forum Type 4 Tag) ─────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} DesfireNdefReadCtx;

#define DESFIRE_READ_CHUNK  100U

static NfcCommand nfc_tools_desfire_ndef_read_cb(NfcGenericEvent event, void* context) {
    DesfireNdefReadCtx*     ctx = context;
    Iso14443_4aPollerEvent* ev  = event.event_data;
    NfcToolsApp*            app = ctx->app;

    if(ev->type != Iso14443_4aPollerEventTypeReady) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    Iso14443_4aPoller* poller = event.instance;

    // Step 1 : SELECT NDEF APPLICATION (D2 76 00 00 85 01 01)
    const uint8_t sel_cmd[5 + NDEF_AID_LEN + 1] = {
        0x00, 0xA4, 0x04, 0x00, (uint8_t)NDEF_AID_LEN,
        0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01,
        0x00
    };
    ApduResp sel = apdu_exchange(poller, sel_cmd, sizeof(sel_cmd));
    if(!sw_ok(&sel)) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Step 2 : SELECT CC FILE (E1 03)
    if(!t4t_select_file(poller, CC_FILE_ID_HI, CC_FILE_ID_LO)) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Step 3 : READ CC (15 bytes) et validation du TLV tag
    uint8_t cc[CC_SIZE];
    size_t  cc_read = 0;
    if(!t4t_read_binary(poller, 0, (uint8_t)CC_SIZE, cc, &cc_read) ||
       !cc_is_valid(cc, cc_read)) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Maximum NDEF file capacity (cc[11..12] big-endian)
    uint16_t ndef_file_max = ((uint16_t)cc[11] << 8) | cc[12];

    // Step 4 : SELECT NDEF FILE (E1 04)
    if(!t4t_select_file(poller, NDEF_FILE_ID_HI, NDEF_FILE_ID_LO)) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Step 5: Read NDEF length (first 2 bytes of the file)
    uint8_t len_bytes[2];
    size_t  len_read = 0;
    if(!t4t_read_binary(poller, 0, 2, len_bytes, &len_read) || len_read < 2) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    uint16_t ndef_len = ((uint16_t)len_bytes[0] << 8) | len_bytes[1];

    // Zero or invalid length: no NDEF record (not an error)
    if(ndef_len == 0 || ndef_len > ndef_file_max) {
        ctx->success = true;
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Step 6: Read NDEF payload in chunks (starting at offset 2)
    uint8_t* ndef_buf = malloc(ndef_len);
    if(!ndef_buf) {
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    uint16_t read_total = 0;
    bool     read_ok    = true;
    while(read_total < ndef_len) {
        uint16_t remaining = ndef_len - read_total;
        uint8_t  chunk     = (remaining > DESFIRE_READ_CHUNK) ?
                             (uint8_t)DESFIRE_READ_CHUNK : (uint8_t)remaining;
        uint16_t offset    = (uint16_t)(2u + read_total);
        size_t   got       = 0;

        if(!t4t_read_binary(poller, offset, chunk, ndef_buf + read_total, &got) || got == 0) {
            read_ok = false;
            break;
        }
        read_total += (uint16_t)got;
    }

    if(!read_ok) {
        free(ndef_buf);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // Step 7: TLV encapsulation expected by nfc_tools_ndef_parse_type2_tag
    // Format: 0x03 [len] [ndef_bytes] 0xFE
    //   - len <= 254: 1-byte length  -> overhead = 3 bytes
    //   - len >  254: 0xFF + 2 BE bytes -> overhead = 5 bytes
    size_t tlv_len = (ndef_len <= 254u) ?
                     ((size_t)ndef_len + 3u) :
                     ((size_t)ndef_len + 5u);
    uint8_t* tlv = malloc(tlv_len);
    if(!tlv) {
        free(ndef_buf);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    size_t pos = 0;
    tlv[pos++] = 0x03;
    if(ndef_len <= 254u) {
        tlv[pos++] = (uint8_t)ndef_len;
    } else {
        tlv[pos++] = 0xFF;
        tlv[pos++] = (uint8_t)(ndef_len >> 8);
        tlv[pos++] = (uint8_t)(ndef_len & 0xFF);
    }
    memcpy(&tlv[pos], ndef_buf, ndef_len);
    pos += ndef_len;
    tlv[pos] = 0xFE;

    free(ndef_buf);

    nfc_tools_ndef_parse_type2_tag(tlv, tlv_len, app->ndef_str);
    nfc_tools_ndef_parse_type2_tag_structured(app, tlv, tlv_len);

    free(tlv);
    ctx->success = true;
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

bool nfc_tools_desfire_read_ndef(NfcToolsApp* app) {
    DesfireNdefReadCtx ctx = {
        .app     = app,
        .done    = furi_event_flag_alloc(),
        .success = false,
    };
    ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(ctx.poller, nfc_tools_desfire_ndef_read_cb, &ctx);

    furi_event_flag_wait(ctx.done, 1u, FuriFlagWaitAny, 8000);

    nfc_poller_stop(ctx.poller);
    nfc_poller_free(ctx.poller);
    furi_event_flag_free(ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;
    return ctx.success;
}
