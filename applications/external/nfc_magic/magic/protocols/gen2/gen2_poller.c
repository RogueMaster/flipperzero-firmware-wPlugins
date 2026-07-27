#include "gen2_poller_i.h"
#include <nfc/helpers/nfc_data_generator.h>

#include <furi/furi.h>

#define GEN2_POLLER_THREAD_FLAG_DETECTED (1U << 0)

#define TAG "GEN2"

typedef NfcCommand (*Gen2PollerStateHandler)(Gen2Poller* instance);

typedef struct {
    NfcPoller* poller;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    FuriThreadId thread_id;
    bool detected;
    Gen2PollerError error;
} Gen2PollerDetectContext;

// Array of known Gen2 ATS responses
// 0978009102DABC1910F005 - flavour 2
// 0978009102DABC1910F005 - flavour 4
// 0D780071028849A13020150608563D - flavour 6
// Other flavours can't be detected other than by just trying to write to block 0
const uint8_t GEN2_ATS[3][16] = {
    {0x09, 0x78, 0x00, 0x91, 0x02, 0xDA, 0xBC, 0x19, 0x10, 0xF0, 0x05},
    {0x09, 0x78, 0x00, 0x91, 0x02, 0xDA, 0xBC, 0x19, 0x10, 0xF0, 0x05},
    {0x0D, 0x78, 0x00, 0x71, 0x02, 0x88, 0x49, 0xA1, 0x30, 0x20, 0x15, 0x06, 0x08, 0x56, 0x3D}};

// Wiped block 0 (1K base): all-zero UID matching the Gen1a + USCUID-UL wipes; BCC stays 0x00
// (0^0^0^0). SAK/ATQA below are the 1K values -- the wipe handler overrides them for 4K/Mini so a
// wiped card keeps announcing its real type.
static const MfClassicBlock gen2_poller_default_block_0 = {
    .data =
        {0x00,
         0x00,
         0x00,
         0x00,
         0x00, // BCC = UID0^UID1^UID2^UID3 = 0
         0x08, // SAK
         0x04, // ATQA0
         0x00, // ATQA1
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00},
};

static const MfClassicBlock gen2_poller_default_empty_block = {
    .data =
        {0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00},
};

static MfClassicBlock gen2_poller_default_sector_trailer_block = {
    .data =
        {0xFF,
         0xFF,
         0xFF,
         0xFF,
         0xFF,
         0xFF,
         0xFF,
         0x07,
         0x80,
         0x69,
         0xFF,
         0xFF,
         0xFF,
         0xFF,
         0xFF,
         0xFF},
};

const char* const gen2_problem_strings[] = {
    "UID may be non-\nrewritable. Check data after writing",
    "No data in selected file",
    "Some sectors are locked",
    "Can't find keys to some sectors",
    "The selected file is incomplete",
};

static Gen2Poller* gen2_poller_alloc_internal(Nfc* nfc, bool with_write_ctx) {
    Gen2Poller* instance = malloc(sizeof(Gen2Poller));
    instance->poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    instance->data = mf_classic_alloc();
    instance->crypto = crypto1_alloc();
    instance->tx_plain_buffer = bit_buffer_alloc(GEN2_POLLER_MAX_BUFFER_SIZE);
    instance->tx_encrypted_buffer = bit_buffer_alloc(GEN2_POLLER_MAX_BUFFER_SIZE);
    instance->rx_plain_buffer = bit_buffer_alloc(GEN2_POLLER_MAX_BUFFER_SIZE);
    instance->rx_encrypted_buffer = bit_buffer_alloc(GEN2_POLLER_MAX_BUFFER_SIZE);
    instance->card_state = Gen2CardStateLost;
    // malloc'd above, so the dispatch state isn't zeroed for us: start the machine at Idle.
    instance->state = Gen2PollerStateIdle;

    instance->gen2_event.data = &instance->gen2_event_data;

    // The two ~4 KB write/wipe data buffers are only used by the write path. Detection
    // sessions skip them and leave the pointers NULL (gen2_poller_free's free(NULL) is
    // a no-op), since gen2_poller_alloc runs up to 5 times per detection.
    if(with_write_ctx) {
        instance->mode_ctx.write_ctx.mfc_data_source = malloc(sizeof(MfClassicData));
        instance->mode_ctx.write_ctx.mfc_data_target = malloc(sizeof(MfClassicData));
    } else {
        instance->mode_ctx.write_ctx.mfc_data_source = NULL;
        instance->mode_ctx.write_ctx.mfc_data_target = NULL;
    }

    instance->mode_ctx.write_ctx.need_halt_before_write = true;

    return instance;
}

Gen2Poller* gen2_poller_alloc(Nfc* nfc) {
    return gen2_poller_alloc_internal(nfc, true);
}

void gen2_poller_free(Gen2Poller* instance) {
    furi_assert(instance);
    furi_assert(instance->data);
    furi_assert(instance->crypto);
    furi_assert(instance->tx_plain_buffer);
    furi_assert(instance->rx_plain_buffer);
    furi_assert(instance->tx_encrypted_buffer);
    furi_assert(instance->rx_encrypted_buffer);

    nfc_poller_free(instance->poller);
    mf_classic_free(instance->data);
    crypto1_free(instance->crypto);
    bit_buffer_free(instance->tx_plain_buffer);
    bit_buffer_free(instance->rx_plain_buffer);
    bit_buffer_free(instance->tx_encrypted_buffer);
    bit_buffer_free(instance->rx_encrypted_buffer);

    free(instance->mode_ctx.write_ctx.mfc_data_source);
    free(instance->mode_ctx.write_ctx.mfc_data_target);

    free(instance);
}

NfcCommand gen2_poller_detect_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    NfcCommand command = NfcCommandStop;
    Gen2PollerDetectContext* detect_ctx = context;
    Iso14443_3aPoller* iso3_poller = event.instance;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;
    detect_ctx->error = Gen2PollerErrorTimeout;

    bit_buffer_reset(detect_ctx->tx_buffer);
    bit_buffer_append_byte(detect_ctx->tx_buffer, GEN2_CMD_READ_ATS);
    bit_buffer_append_byte(detect_ctx->tx_buffer, GEN2_FSDI_256 << 4);

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        do {
            const Iso14443_3aError iso14443_3a_error = iso14443_3a_poller_send_standard_frame(
                iso3_poller, detect_ctx->tx_buffer, detect_ctx->rx_buffer, GEN2_POLLER_MAX_FWT);

            if(iso14443_3a_error != Iso14443_3aErrorNone &&
               iso14443_3a_error != Iso14443_3aErrorWrongCrc) {
                FURI_LOG_E(TAG, "ATS request failed");
                detect_ctx->error = Gen2PollerErrorProtocol;
                break;

            } else {
                FURI_LOG_D(TAG, "ATS request succeeded:");
                // Check against known ATS responses
                for(size_t i = 0; i < COUNT_OF(GEN2_ATS); i++) {
                    if(memcmp(
                           bit_buffer_get_data(detect_ctx->rx_buffer),
                           GEN2_ATS[i],
                           sizeof(GEN2_ATS[i])) == 0) {
                        detect_ctx->error = Gen2PollerErrorNone;
                        break;
                    }
                }
            }
        } while(false);
    } else if(iso3_event->type == Iso14443_3aPollerEventTypeError) {
        detect_ctx->error = Gen2PollerErrorTimeout;
    }
    furi_thread_flags_set(detect_ctx->thread_id, GEN2_POLLER_THREAD_FLAG_DETECTED);

    return command;
}

Gen2PollerError gen2_poller_detect(Nfc* nfc) {
    furi_assert(nfc);

    Gen2PollerDetectContext detect_ctx = {
        .poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a),
        .tx_buffer = bit_buffer_alloc(GEN2_POLLER_MAX_BUFFER_SIZE),
        .rx_buffer = bit_buffer_alloc(GEN2_POLLER_MAX_BUFFER_SIZE),
        .thread_id = furi_thread_get_current_id(),
        .detected = false,
        .error = Gen2PollerErrorNone,
    };

    nfc_poller_start(detect_ctx.poller, gen2_poller_detect_callback, &detect_ctx);
    uint32_t flags =
        furi_thread_flags_wait(GEN2_POLLER_THREAD_FLAG_DETECTED, FuriFlagWaitAny, FuriWaitForever);
    if(flags & GEN2_POLLER_THREAD_FLAG_DETECTED) {
        furi_thread_flags_clear(GEN2_POLLER_THREAD_FLAG_DETECTED);
    }
    nfc_poller_stop(detect_ctx.poller);

    bit_buffer_free(detect_ctx.tx_buffer);
    bit_buffer_free(detect_ctx.rx_buffer);
    nfc_poller_free(detect_ctx.poller);

    return detect_ctx.error;
}

// --- Gen2 sub-type classification (CUID write probe + static-nonce detection) ---

#define GEN2_STATIC_NONCE_SAMPLES (3)

// Default sector-0 keys tried for the CUID write probe. Blank/fresh magic cards
// (and blank normal cards) use FF..FF; matching Proxmark3 we try B then A.
typedef struct {
    MfClassicKeyType key_type;
    MfClassicKey key;
} Gen2ProbeKey;

static const Gen2ProbeKey gen2_cuid_probe_keys[] = {
    {MfClassicKeyTypeB, {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}},
    {MfClassicKeyTypeA, {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}},
};

typedef struct {
    Gen2Poller* poller;
    FuriThreadId thread_id;
    MfClassicKey key;
    MfClassicKeyType key_type;
    bool cuid_writable;
    MfClassicNt nt;
    bool nt_valid;
} Gen2DetectContext;

static NfcCommand gen2_poller_cuid_probe_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    Gen2DetectContext* ctx = context;
    Gen2Poller* instance = ctx->poller;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;
    instance->iso3_poller = event.instance;
    instance->auth_state = Gen2AuthStateIdle;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        Gen2PollerError error = gen2_poller_auth(instance, 0, &ctx->key, ctx->key_type, NULL);
        if(error == Gen2PollerErrorNone) {
            // The probe sends only the first write phase and reads the ACK/NAK.
            // We never send the data phase: returning Stop below drops the field,
            // so block 0 cannot be written whether the card ACKed or NAKed.
            Gen2PollerError probe_error =
                gen2_poller_probe_block0_writable(instance, &ctx->cuid_writable);
            if(probe_error != Gen2PollerErrorNone) {
                FURI_LOG_D(TAG, "Block 0 write probe did not complete: %d", probe_error);
            }
        }
    }

    furi_thread_flags_set(ctx->thread_id, GEN2_POLLER_THREAD_FLAG_DETECTED);
    return NfcCommandStop;
}

static NfcCommand gen2_poller_nt_probe_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    Gen2DetectContext* ctx = context;
    Gen2Poller* instance = ctx->poller;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;
    instance->iso3_poller = event.instance;
    ctx->nt_valid = false;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        // Plain auth step 1 only: read the tag nonce, never complete the handshake.
        // No key needed.
        Gen2PollerError error = gen2_poller_get_nt(instance, 0, MfClassicKeyTypeA, &ctx->nt);
        ctx->nt_valid = (error == Gen2PollerErrorNone);
    }

    furi_thread_flags_set(ctx->thread_id, GEN2_POLLER_THREAD_FLAG_DETECTED);
    return NfcCommandStop;
}

// Runs one detection session in its own freshly-allocated Gen2Poller, so each call
// starts from a fresh RF field (the reset static-nonce detection relies on). A fresh
// poller per session is mandatory, not just convenient: an NfcPoller can be started
// only once. Stopping a session resets the shared Nfc config_state to Idle, and only
// nfc_poller_alloc (via iso14443_3a_poller_alloc -> nfc_config) sets it back to Done,
// so restarting the same poller would trip nfc_start's config_state check.
static void
    gen2_poller_run_detect_session(Nfc* nfc, NfcGenericCallback callback, Gen2DetectContext* ctx) {
    ctx->poller = gen2_poller_alloc_internal(nfc, false);
    ctx->thread_id = furi_thread_get_current_id();
    nfc_poller_start(ctx->poller->poller, callback, ctx);
    furi_thread_flags_wait(GEN2_POLLER_THREAD_FLAG_DETECTED, FuriFlagWaitAny, FuriWaitForever);
    furi_thread_flags_clear(GEN2_POLLER_THREAD_FLAG_DETECTED);
    nfc_poller_stop(ctx->poller->poller);
    gen2_poller_free(ctx->poller);
    ctx->poller = NULL;
}

static bool gen2_poller_probe_cuid(Nfc* nfc) {
    for(size_t i = 0; i < COUNT_OF(gen2_cuid_probe_keys); i++) {
        Gen2DetectContext ctx = {
            .key = gen2_cuid_probe_keys[i].key,
            .key_type = gen2_cuid_probe_keys[i].key_type,
            .cuid_writable = false,
        };
        gen2_poller_run_detect_session(nfc, gen2_poller_cuid_probe_callback, &ctx);
        if(ctx.cuid_writable) {
            return true;
        }
    }
    return false;
}

static bool gen2_poller_probe_static_nonce(Nfc* nfc) {
    MfClassicNt nonces[GEN2_STATIC_NONCE_SAMPLES];
    size_t valid = 0;

    for(size_t i = 0; i < GEN2_STATIC_NONCE_SAMPLES; i++) {
        Gen2DetectContext ctx = {
            .nt_valid = false,
        };
        gen2_poller_run_detect_session(nfc, gen2_poller_nt_probe_callback, &ctx);
        if(ctx.nt_valid) {
            nonces[valid++] = ctx.nt;
        }
    }

    // Need at least two readings to decide. A 32-bit random/PRNG nonce repeating
    // across fresh activations is effectively impossible, so any repeat means the
    // card emits a static nonce.
    if(valid < 2) {
        // Inconclusive (card removed / unstable RF): report not-static, but log it
        // so a missed static-nonce classification can be diagnosed.
        FURI_LOG_D(
            TAG,
            "Static-nonce check inconclusive: %u/%u valid samples",
            (unsigned)valid,
            GEN2_STATIC_NONCE_SAMPLES);
        return false;
    }
    for(size_t i = 0; i < valid; i++) {
        for(size_t j = i + 1; j < valid; j++) {
            if(memcmp(nonces[i].data, nonces[j].data, sizeof(MfClassicNt)) == 0) {
                return true;
            }
        }
    }
    return false;
}

Gen2PollerError gen2_poller_detect_type(Nfc* nfc, Gen2Type* type) {
    furi_assert(nfc);
    furi_assert(type);

    *type = Gen2TypeUnknown;

    // 1. Known-ATS fingerprint: cheap, no auth, identifies a recognisable subset.
    if(gen2_poller_detect(nfc) == Gen2PollerErrorNone) {
        *type = Gen2TypeAts;
        return Gen2PollerErrorNone;
    }

    // 2. Behavioural CUID confirmation, then static-nonce classification. Each probe
    // runs in its own poller session (allocated per session in run_detect_session).
    if(gen2_poller_probe_cuid(nfc)) {
        *type = gen2_poller_probe_static_nonce(nfc) ? Gen2TypeCuidStaticNonce : Gen2TypeCuid;
    }

    return (*type != Gen2TypeUnknown) ? Gen2PollerErrorNone : Gen2PollerErrorNotPresent;
}

const char* gen2_type_get_detail(Gen2Type type) {
    switch(type) {
    case Gen2TypeCuid:
        return "CUID";
    case Gen2TypeCuidStaticNonce:
        return "CUID. Static nonce";
    case Gen2TypeAts:
        return "CUID. ATS";
    case Gen2TypeUnknown:
    default:
        return NULL;
    }
}

NfcCommand gen2_poller_idle_handler(Gen2Poller* instance) {
    furi_assert(instance);

    NfcCommand command = NfcCommandContinue;

    instance->mode_ctx.write_ctx.current_block = 0;
    instance->mode_ctx.write_ctx.failed_block_count = 0;
    memset(
        instance->mode_ctx.write_ctx.failed_block_bitmap,
        0,
        sizeof(instance->mode_ctx.write_ctx.failed_block_bitmap));
    instance->gen2_event.type = Gen2PollerEventTypeDetected;
    command = instance->callback(instance->gen2_event, instance->context);
    instance->state = Gen2PollerStateRequestMode;

    return command;
}

NfcCommand gen2_poller_request_mode_handler(Gen2Poller* instance) {
    furi_assert(instance);

    NfcCommand command = NfcCommandContinue;

    instance->gen2_event.type = Gen2PollerEventTypeRequestMode;
    command = instance->callback(instance->gen2_event, instance->context);
    instance->mode = instance->gen2_event_data.poller_mode.mode;
    if(instance->gen2_event_data.poller_mode.mode == Gen2PollerModeWipe) {
        instance->state = Gen2PollerStateWriteTargetDataRequest;
    } else {
        instance->state = Gen2PollerStateWriteSourceDataRequest;
    }

    return command;
}

NfcCommand gen2_poller_write_source_data_request_handler(Gen2Poller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->gen2_event.type = Gen2PollerEventTypeRequestDataToWrite;
    command = instance->callback(instance->gen2_event, instance->context);
    memcpy(
        instance->mode_ctx.write_ctx.mfc_data_source,
        instance->gen2_event_data.data_to_write.mfc_data,
        sizeof(MfClassicData));
    instance->state = Gen2PollerStateWriteTargetDataRequest;

    return command;
}

NfcCommand gen2_poller_write_target_data_request_handler(Gen2Poller* instance) {
    NfcCommand command = NfcCommandContinue;

    instance->gen2_event.type = Gen2PollerEventTypeRequestTargetData;
    command = instance->callback(instance->gen2_event, instance->context);
    memcpy(
        instance->mode_ctx.write_ctx.mfc_data_target,
        instance->gen2_event_data.target_data.mfc_data,
        sizeof(MfClassicData));
    if(instance->mode == Gen2PollerModeWipe) {
        instance->state = Gen2PollerStateWipe;
    } else {
        instance->state = Gen2PollerStateWrite;
    }

    return command;
}

Gen2PollerError gen2_poller_write_block_handler(
    Gen2Poller* instance,
    uint16_t block_num,
    const MfClassicBlock* block) {
    furi_assert(instance);

    Gen2PollerError error = Gen2PollerErrorNone;
    Gen2PollerWriteContext* write_ctx = &instance->mode_ctx.write_ctx;
    MfClassicKey auth_key = write_ctx->auth_key;

    do {
        // Skip only a block we actually read that already matches. An unread block's buffer is
        // zero, which can falsely equal the default/source -- write it rather than trust it.
        if(mf_classic_is_block_read(write_ctx->mfc_data_target, block_num) &&
           memcmp(block->data, write_ctx->mfc_data_target->block[block_num].data, 16) == 0) {
            FURI_LOG_D(TAG, "Block %d is the same, skipping", block_num);
            break;
        }

        // Reauth if necessary
        if(write_ctx->need_halt_before_write) {
            FURI_LOG_D(TAG, "Auth before writing block %d", write_ctx->current_block);
            error = gen2_poller_auth(
                instance, write_ctx->current_block, &auth_key, write_ctx->write_key, NULL);
            if(error != Gen2PollerErrorNone) {
                FURI_LOG_D(
                    TAG, "Failed to auth to block %d for writing", write_ctx->current_block);
                break;
            }
        }

        // Write the block
        error = gen2_poller_write_block(instance, write_ctx->current_block, block);
        if(error != Gen2PollerErrorNone) {
            FURI_LOG_D(TAG, "Failed to write block %d", write_ctx->current_block);
            break;
        }
    } while(false);
    FURI_LOG_D(TAG, "Block %d finished, halting", write_ctx->current_block);
    gen2_poller_halt(instance);
    return error;
}

NfcCommand gen2_poller_wipe_handler(Gen2Poller* instance) {
    NfcCommand command = NfcCommandContinue;
    Gen2PollerError error = Gen2PollerErrorNone;
    Gen2PollerWriteContext* write_ctx = &instance->mode_ctx.write_ctx;
    uint16_t block_num = write_ctx->current_block;
    bool block_failed = false;

    do {
        // Check whether the ACs for that block are known in target data
        if(!mf_classic_is_block_read(
               write_ctx->mfc_data_target,
               mf_classic_get_sector_trailer_num_by_block(block_num))) {
            FURI_LOG_E(TAG, "Sector trailer for block %d not present in target data", block_num);
            block_failed = true;
            break;
        }

        // Check whether ACs need to be reset and whether they can be reset
        if(!gen2_poller_can_write_block(write_ctx->mfc_data_target, block_num)) {
            if(!gen2_can_reset_access_conditions(write_ctx->mfc_data_target, block_num)) {
                FURI_LOG_E(TAG, "Block %d cannot be written", block_num);
                block_failed = true;
                break;
            } else {
                FURI_LOG_D(TAG, "Resetting ACs for block %d", block_num);
                // Generate a block with old keys and default ACs (0xFF, 0x07, 0x80)
                MfClassicBlock block;
                memset(&block, 0, sizeof(block));
                memcpy(block.data, write_ctx->mfc_data_target->block[block_num].data, 16);
                memcpy(block.data + 6, "\xFF\x07\x80", 3);

                error = gen2_poller_write_block_handler(instance, block_num, &block);
                if(error != Gen2PollerErrorNone) {
                    FURI_LOG_E(TAG, "Failed to reset ACs for block %d", block_num);
                    block_failed = true;
                    break;
                } else {
                    FURI_LOG_D(TAG, "ACs for block %d reset", block_num);
                    memcpy(write_ctx->mfc_data_target->block[block_num].data, block.data, 16);
                }
            }
        }

        // Figure out which key to use for writing
        write_ctx->write_key =
            gen2_poller_get_key_type_to_write(write_ctx->mfc_data_target, block_num);

        // Get the key to use for writing from the target data
        MfClassicSectorTrailer* sec_tr = mf_classic_get_sector_trailer_by_sector(
            write_ctx->mfc_data_target, mf_classic_get_sector_by_block(block_num));
        if(write_ctx->write_key == MfClassicKeyTypeA) {
            write_ctx->auth_key = sec_tr->key_a;
        } else {
            write_ctx->auth_key = sec_tr->key_b;
        }

        // Write the default block depending on the block type
        if(block_num == 0) {
            // Block 0 announces the card via SAK/ATQA. Preserve what the card actually reported at
            // activation rather than deriving from the detected MfClassicType: a magic CUID card
            // that answers 4K-only blocks gets mis-detected as 4K (the firmware type probe auths
            // block 254), but its real SAK still says 1K -- deriving from type would stamp a 1K
            // card as 4K. Preserving also keeps the card's real ATQA and the ISO14443-4/ATS bit.
            MfClassicBlock block_0 = gen2_poller_default_block_0;
            const Iso14443_3aData* iso3 = write_ctx->mfc_data_target->iso14443_3a_data;
            block_0.data[5] = iso3->sak;
            block_0.data[6] = iso3->atqa[0];
            block_0.data[7] = iso3->atqa[1];
            error = gen2_poller_write_block_handler(instance, block_num, &block_0);
        } else if(mf_classic_is_sector_trailer(block_num)) {
            error = gen2_poller_write_block_handler(
                instance, block_num, &gen2_poller_default_sector_trailer_block);
        } else {
            error = gen2_poller_write_block_handler(
                instance, block_num, &gen2_poller_default_empty_block);
        }
        if(error != Gen2PollerErrorNone) {
            FURI_LOG_E(TAG, "Couldn't write block %d", block_num);
            block_failed = true;
        }
    } while(false);

    // Each block is visited once, so just record the ones that failed -- a partial wipe then lists
    // exactly which blocks stayed untouched (e.g. only block 0 of a genuine MFC).
    if(block_failed) {
        furi_assert(block_num < GEN2_POLLER_MAX_BLOCKS);
        write_ctx->failed_block_bitmap[block_num >> 3] |= (1u << (block_num & 7u));
        write_ctx->failed_block_count++;
    }

    write_ctx->current_block++;

    uint16_t total_blocks = mf_classic_get_total_block_num(write_ctx->mfc_data_target->type);
    if(write_ctx->current_block == total_blocks) {
        if(write_ctx->failed_block_count == 0) {
            instance->state = Gen2PollerStateSuccess;
        } else if(write_ctx->failed_block_count >= total_blocks) {
            // Every block failed -> nothing was wiped: hard fail, offer a retry.
            instance->state = Gen2PollerStateFail;
        } else {
            instance->state = Gen2PollerStatePartial;
        }
    }

    return command;
}

NfcCommand gen2_poller_write_handler(Gen2Poller* instance) {
    NfcCommand command = NfcCommandContinue;
    Gen2PollerError error = Gen2PollerErrorNone;
    Gen2PollerWriteContext* write_ctx = &instance->mode_ctx.write_ctx;
    uint16_t block_num = write_ctx->current_block;
    bool block_failed = false;

    do {
        // Nothing to write here -> benign skip, not a failure.
        if(!mf_classic_is_block_read(write_ctx->mfc_data_source, block_num)) {
            // FURI_LOG_E(TAG, "Block %d not present in source data", block_num);
            break;
        }

        // Source has data for this block but the dict attack found no key for its sector -> we
        // can't auth, so it can't be written. Count it as a failure rather than skipping silently,
        // so a clone that wrote nothing reports Fail (was masked as Success).
        if(!mf_classic_is_block_read(
               write_ctx->mfc_data_target,
               mf_classic_get_sector_trailer_num_by_block(block_num))) {
            FURI_LOG_E(TAG, "Sector trailer for block %d not present in target data", block_num);
            block_failed = true;
            break;
        }

        // Check whether ACs need to be reset and whether they can be reset
        if(!gen2_poller_can_write_block(write_ctx->mfc_data_target, block_num)) {
            if(!gen2_can_reset_access_conditions(write_ctx->mfc_data_target, block_num)) {
                FURI_LOG_E(TAG, "Block %d cannot be written", block_num);
                block_failed = true;
                break;
            } else {
                FURI_LOG_D(TAG, "Resetting ACs for block %d", block_num);
                // Generate a block with old keys and default ACs (0xFF, 0x07, 0x80)
                MfClassicBlock block;
                memset(&block, 0, sizeof(block));
                memcpy(block.data, write_ctx->mfc_data_target->block[block_num].data, 16);
                memcpy(block.data + 6, "\xFF\x07\x80", 3);

                error = gen2_poller_write_block_handler(instance, block_num, &block);
                if(error != Gen2PollerErrorNone) {
                    FURI_LOG_E(TAG, "Failed to reset ACs for block %d", block_num);
                    block_failed = true;
                    break;
                } else {
                    FURI_LOG_D(TAG, "ACs for block %d reset", block_num);
                    memcpy(write_ctx->mfc_data_target->block[block_num].data, block.data, 16);
                }
            }
        }

        // Figure out which key to use for writing
        write_ctx->write_key =
            gen2_poller_get_key_type_to_write(write_ctx->mfc_data_target, block_num);

        // Get the key to use for writing from the target data
        MfClassicSectorTrailer* sec_tr = mf_classic_get_sector_trailer_by_sector(
            write_ctx->mfc_data_target, mf_classic_get_sector_by_block(block_num));
        if(write_ctx->write_key == MfClassicKeyTypeA) {
            write_ctx->auth_key = sec_tr->key_a;
        } else {
            write_ctx->auth_key = sec_tr->key_b;
        }

        // Write the block
        error = gen2_poller_write_block_handler(
            instance, block_num, &write_ctx->mfc_data_source->block[block_num]);
        if(error != Gen2PollerErrorNone) {
            FURI_LOG_E(TAG, "Couldn't write block %d", block_num);
            block_failed = true;
        }
    } while(false);

    // Each block is visited once, so just record the ones that failed -> a partial write lists
    // exactly which blocks it couldn't write (mirrors the wipe handler).
    if(block_failed) {
        furi_assert(block_num < GEN2_POLLER_MAX_BLOCKS);
        write_ctx->failed_block_bitmap[block_num >> 3] |= (1u << (block_num & 7u));
        write_ctx->failed_block_count++;
    }

    write_ctx->current_block++;

    // Always evaluate the terminal condition (current_block is post-incremented, so returning
    // early on the last block would overshoot total -> the poller would spin in Write forever).
    uint16_t total_blocks = mf_classic_get_total_block_num(write_ctx->mfc_data_source->type);
    if(write_ctx->current_block == total_blocks) {
        if(write_ctx->failed_block_count == 0) {
            instance->state = Gen2PollerStateSuccess;
        } else if(write_ctx->failed_block_count >= total_blocks) {
            // Every block in the dump failed (e.g. no keys found) -> hard fail, offer a retry. (A
            // sparse source can't reach this -- its absent blocks aren't counted -- so a clone that
            // couldn't write any of its present blocks surfaces as Partial instead.)
            instance->state = Gen2PollerStateFail;
        } else {
            instance->state = Gen2PollerStatePartial;
        }
    }

    return command;
}

NfcCommand gen2_poller_success_handler(Gen2Poller* instance) {
    furi_assert(instance);

    NfcCommand command = NfcCommandContinue;

    instance->gen2_event.type = Gen2PollerEventTypeSuccess;
    command = instance->callback(instance->gen2_event, instance->context);
    instance->state = Gen2PollerStateIdle;

    return command;
}

NfcCommand gen2_poller_partial_handler(Gen2Poller* instance) {
    furi_assert(instance);

    NfcCommand command = NfcCommandContinue;
    Gen2PollerWriteContext* write_ctx = &instance->mode_ctx.write_ctx;

    // Blocks total comes from whichever dump drove the iteration: the card (wipe) or the source
    // dump (write) -- they can differ (e.g. a 1K dump written onto a 4K-detected card).
    const MfClassicData* ref = (instance->mode == Gen2PollerModeWipe) ?
                                   write_ctx->mfc_data_target :
                                   write_ctx->mfc_data_source;

    instance->gen2_event.type = Gen2PollerEventTypePartial;
    instance->gen2_event_data.partial.blocks_total = mf_classic_get_total_block_num(ref->type);
    instance->gen2_event_data.partial.failed_count = write_ctx->failed_block_count;
    memcpy(
        instance->gen2_event_data.partial.failed_bitmap,
        write_ctx->failed_block_bitmap,
        sizeof(instance->gen2_event_data.partial.failed_bitmap));
    command = instance->callback(instance->gen2_event, instance->context);
    instance->state = Gen2PollerStateIdle;

    return command;
}

NfcCommand gen2_poller_fail_handler(Gen2Poller* instance) {
    furi_assert(instance);

    NfcCommand command = NfcCommandContinue;

    instance->gen2_event.type = Gen2PollerEventTypeFail;
    command = instance->callback(instance->gen2_event, instance->context);
    instance->state = Gen2PollerStateIdle;

    return command;
}

static const Gen2PollerStateHandler gen2_poller_state_handlers[Gen2PollerStateNum] = {
    [Gen2PollerStateIdle] = gen2_poller_idle_handler,
    [Gen2PollerStateRequestMode] = gen2_poller_request_mode_handler,
    [Gen2PollerStateWipe] = gen2_poller_wipe_handler,
    [Gen2PollerStateWriteSourceDataRequest] = gen2_poller_write_source_data_request_handler,
    [Gen2PollerStateWriteTargetDataRequest] = gen2_poller_write_target_data_request_handler,
    [Gen2PollerStateWrite] = gen2_poller_write_handler,
    [Gen2PollerStateSuccess] = gen2_poller_success_handler,
    [Gen2PollerStatePartial] = gen2_poller_partial_handler,
    [Gen2PollerStateFail] = gen2_poller_fail_handler,
};

NfcCommand gen2_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    furi_assert(event.event_data);
    furi_assert(event.instance);

    NfcCommand command = NfcCommandContinue;
    Gen2Poller* instance = context;
    instance->iso3_poller = event.instance;
    Iso14443_3aPollerEvent* iso3_event = event.event_data;

    if(iso3_event->type == Iso14443_3aPollerEventTypeReady) {
        command = gen2_poller_state_handlers[instance->state](instance);
    }

    return command;
}

void gen2_poller_start(Gen2Poller* instance, Gen2PollerCallback callback, void* context) {
    furi_assert(instance);
    furi_assert(callback);

    instance->callback = callback;
    instance->context = context;

    nfc_poller_start(instance->poller, gen2_poller_callback, instance);
    return;
}

void gen2_poller_stop(Gen2Poller* instance) {
    furi_assert(instance);

    FURI_LOG_D(TAG, "Stopping Gen2 poller");
    nfc_poller_stop(instance->poller);
    return;
}

Gen2PollerWriteProblems gen2_poller_check_target_problems(NfcDevice* target_dev) {
    furi_assert(target_dev);

    Gen2PollerWriteProblems problems = {0};
    const MfClassicData* mfc_data = nfc_device_get_data(target_dev, NfcProtocolMfClassic);

    if(mfc_data) {
        uint16_t total_block_num = mf_classic_get_total_block_num(mfc_data->type);
        for(uint16_t i = 0; i < total_block_num; i++) {
            if(mf_classic_is_sector_trailer(i)) {
                problems.all_problems |=
                    gen2_poller_can_write_sector_trailer(mfc_data, i).all_problems;
            } else {
                problems.all_problems |=
                    gen2_poller_can_write_data_block(mfc_data, i).all_problems;
            }
        }
    } else {
        problems.no_data = true;
    }

    return problems;
}

Gen2PollerWriteProblems gen2_poller_check_source_problems(NfcDevice* source_dev) {
    furi_assert(source_dev);

    Gen2PollerWriteProblems problems = {0};
    const MfClassicData* mfc_data = nfc_device_get_data(source_dev, NfcProtocolMfClassic);

    if(mfc_data) {
        uint16_t total_block_num = mf_classic_get_total_block_num(mfc_data->type);
        for(uint16_t i = 0; i < total_block_num; i++) {
            if(!mf_classic_is_block_read(mfc_data, i)) {
                problems.missing_source_data = true;
            }
        }
    }

    return problems;
}
