// sli_writer.c
// Writes Magic ISO15693 SLI / SLIX-L / Special cards from a Flipper .nfc file.
//
// Three write modes:
//   Normal  — non-addressed write, then UID via 0x40/0x41
//   SaveUID — inventory only, save detected UID as "special (factory) UID"
//   Special — restore factory UID, write blocks addressed (flags=0x62), set target UID
//
// Special card write sequence:
//   1. magic_write_uid(factory_uid)  — restore factory UID so blocks are writable
//   2. write_blocks_addressed()      — flags=0x62 (high rate + addressed + option)
//   3. magic_write_uid(target_uid)   — set the UID from the .nfc file

#include "sli_writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "SLI_Writer"

/* ============================================================================
 *  ISO15693 / Magic command constants
 * ========================================================================== */

#define ISO15693_CMD_WRITE_BLOCK 0x21

/* Gen2 magic UID commands (Proxmark: hf 15 csetuid --v2)
 *   HIGH: 02 E0 09 40 uid[7..4]
 *   LOW:  02 E0 09 41 uid[3..0]  (card stores UID LSB-first) */
#define SLI_MAGIC_SET_UID_HIGH 0x40
#define SLI_MAGIC_SET_UID_LOW  0x41

/* FWT in carrier cycles — 500000 fc ≈ 37ms */
#define ISO15693_FWT_FC 500000

/* ============================================================================
 *  LED notification sequences
 * ========================================================================== */

static const NotificationSequence seq_blink_start = {
    &message_blue_255,
    &message_delay_50,
    &message_blue_0,
    NULL,
};
static const NotificationSequence seq_success = {
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_error = {
    &message_red_255,
    &message_delay_100,
    &message_red_0,
    NULL,
};

/* ============================================================================
 *  BitBuffer helpers
 * ========================================================================== */

static BitBuffer* bb_alloc_from(const uint8_t* data, size_t len) {
    BitBuffer* bb = bit_buffer_alloc(len * 8);
    if(!bb) return NULL;
    bit_buffer_reset(bb);
    if(data && len) bit_buffer_append_bytes(bb, data, len);
    return bb;
}

static BitBuffer* bb_alloc_rx(size_t max_bytes) {
    BitBuffer* bb = bit_buffer_alloc(max_bytes * 8);
    if(!bb) return NULL;
    bit_buffer_set_size(bb, 0);
    return bb;
}

/* ============================================================================
 *  Low-level ISO15693 raw frame helper
 * ========================================================================== */

static bool iso_reply_ok(BitBuffer* rx) {
    if(bit_buffer_get_size_bytes(rx) < 1) return false;
    return (bit_buffer_get_byte(rx, 0) & 0x01) == 0;
}

static bool
    iso_send_raw(Iso15693_3Poller* iso, const uint8_t* frame, size_t frame_len, uint32_t fwt_fc) {
    BitBuffer* tx = bb_alloc_from(frame, frame_len);
    if(!tx) return false;
    BitBuffer* rx = bb_alloc_rx(16);
    if(!rx) {
        bit_buffer_free(tx);
        return false;
    }

    Iso15693_3Error err =
        iso15693_3_poller_send_frame(iso, tx, rx, fwt_fc ? fwt_fc : ISO15693_FWT_FC);
    bool ok = (err == Iso15693_3ErrorNone) && iso_reply_ok(rx);

    if(!ok) {
        size_t rxsz = bit_buffer_get_size_bytes(rx);
        if(rxsz >= 2)
            FURI_LOG_W(
                TAG,
                "iso_send_raw: err=%d rxbytes=%u resp=[%02X %02X]",
                (int)err,
                (unsigned)rxsz,
                bit_buffer_get_byte(rx, 0),
                bit_buffer_get_byte(rx, 1));
        else
            FURI_LOG_W(TAG, "iso_send_raw: err=%d rxbytes=%u", (int)err, (unsigned)rxsz);
    }
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return ok;
}

/* ============================================================================
 *  Magic UID write (non-addressed, used for both Normal and Special modes)
 * ========================================================================== */

static bool magic_write_uid(Iso15693_3Poller* iso, const uint8_t uid_msb[8]) {
    /* HIGH: bytes uid[0..3] sent in reverse (card stores LSB-first) */
    uint8_t frame_high[8] = {
        0x02,
        0xE0,
        0x09,
        SLI_MAGIC_SET_UID_HIGH,
        uid_msb[7],
        uid_msb[6],
        uid_msb[5],
        uid_msb[4],
    };
    /* LOW: bytes uid[4..7] sent in reverse */
    uint8_t frame_low[8] = {
        0x02,
        0xE0,
        0x09,
        SLI_MAGIC_SET_UID_LOW,
        uid_msb[3],
        uid_msb[2],
        uid_msb[1],
        uid_msb[0],
    };

    FURI_LOG_I(
        TAG,
        "magic_write_uid HIGH: %02X%02X%02X%02X",
        uid_msb[0],
        uid_msb[1],
        uid_msb[2],
        uid_msb[3]);
    bool ok = false;
    for(int i = 0; i < 3; i++) {
        if(iso_send_raw(iso, frame_high, sizeof(frame_high), ISO15693_FWT_FC)) {
            ok = true;
            break;
        }
        furi_delay_ms(10);
    }
    if(!ok) {
        FURI_LOG_E(TAG, "SET_UID_HIGH failed");
        return false;
    }
    furi_delay_ms(20);

    FURI_LOG_I(
        TAG,
        "magic_write_uid LOW:  %02X%02X%02X%02X",
        uid_msb[4],
        uid_msb[5],
        uid_msb[6],
        uid_msb[7]);
    ok = false;
    for(int i = 0; i < 3; i++) {
        if(iso_send_raw(iso, frame_low, sizeof(frame_low), ISO15693_FWT_FC)) {
            ok = true;
            break;
        }
        furi_delay_ms(10);
    }
    if(!ok) {
        FURI_LOG_E(TAG, "SET_UID_LOW failed");
        return false;
    }

    return true;
}

/* ============================================================================
 *  SELECT helper (puts card in selected state, non-fatal if no response)
 * ========================================================================== */

static void select_card(Iso15693_3Poller* iso, const uint8_t uid_msb[8]) {
    uint8_t frame[10];
    frame[0] = 0x22; /* high rate + addressed */
    frame[1] = 0x25; /* SELECT */
    for(int i = 0; i < 8; i++)
        frame[2 + i] = uid_msb[7 - i]; /* LSB-first */

    BitBuffer* tx = bb_alloc_from(frame, sizeof(frame));
    BitBuffer* rx = bb_alloc_rx(4);
    Iso15693_3Error err = iso15693_3_poller_send_frame(iso, tx, rx, ISO15693_FWT_FC);
    FURI_LOG_I(
        TAG, "SELECT resp: err=%d rxbytes=%u", (int)err, (unsigned)bit_buffer_get_size_bytes(rx));
    bit_buffer_free(tx);
    bit_buffer_free(rx);
}

/* ============================================================================
 *  Normal block write (non-addressed, flags=0x02, auto-retry with 0x42)
 * ========================================================================== */

static bool write_blocks(
    Iso15693_3Poller* iso,
    const uint8_t* data,
    uint8_t block_count,
    uint8_t block_size) {
    if(block_size == 0) block_size = 4;
    if(block_size > 4) block_size = 4;
    size_t blocks = (block_count > SLI_MAGIC_MAX_BLOCKS) ? SLI_MAGIC_MAX_BLOCKS : block_count;

    FURI_LOG_I(
        TAG,
        "write_blocks: %u blocks x %u bytes (non-addressed)",
        (unsigned)blocks,
        (unsigned)block_size);

    for(size_t b = 0; b < blocks; b++) {
        uint8_t wframe[7];
        wframe[0] = 0x02;
        wframe[1] = ISO15693_CMD_WRITE_BLOCK;
        wframe[2] = (uint8_t)b;
        memcpy(&wframe[3], &data[b * block_size], block_size);
        if(block_size < 4) memset(&wframe[3 + block_size], 0x00, 4 - block_size);

        bool wrote = false;
        for(int attempt = 0; attempt < 5 && !wrote; attempt++) {
            wrote = iso_send_raw(iso, wframe, sizeof(wframe), ISO15693_FWT_FC);
            if(!wrote) {
                if(wframe[0] == 0x02) {
                    FURI_LOG_W(TAG, "Block %u: retrying with Option flag (0x42)", (unsigned)b);
                    wframe[0] = 0x42;
                }
                furi_delay_ms(20);
            }
        }
        if(!wrote) {
            FURI_LOG_E(TAG, "write_blocks: block %u failed", (unsigned)b);
            return false;
        }
        furi_delay_ms(20);
    }
    return true;
}

/* ============================================================================
 *  Special block write (addressed + option, flags=0x62)
 *  flags=0x62: high data rate(0x02) + addressed(0x20) + option(0x40)
 *
 *  With Option flag, NXP cards use 2-phase response — timeout is normal.
 *  We ignore no-response errors and only fail on explicit card error flag.
 * ========================================================================== */

static bool write_blocks_addressed(
    Iso15693_3Poller* iso,
    const uint8_t uid_lsb[8],
    const uint8_t* data,
    uint8_t block_count,
    uint8_t block_size) {
    if(block_size == 0) block_size = 4;
    if(block_size > 4) block_size = 4;
    size_t blocks = (block_count > SLI_MAGIC_MAX_BLOCKS) ? SLI_MAGIC_MAX_BLOCKS : block_count;

    FURI_LOG_I(TAG, "write_blocks_addressed: %u blocks (flags=0x62)", (unsigned)blocks);

    for(size_t b = 0; b < blocks; b++) {
        uint8_t wframe[15];
        wframe[0] = 0x62;
        wframe[1] = ISO15693_CMD_WRITE_BLOCK;
        for(int i = 0; i < 8; i++)
            wframe[2 + i] = uid_lsb[i];
        wframe[10] = (uint8_t)b;
        memcpy(&wframe[11], &data[b * block_size], block_size);
        if(block_size < 4) memset(&wframe[11 + block_size], 0x00, 4 - block_size);

        if(b == 0)
            FURI_LOG_I(
                TAG,
                "  frame[0]: %02X %02X %02X%02X%02X%02X%02X%02X%02X%02X %02X",
                wframe[0],
                wframe[1],
                wframe[2],
                wframe[3],
                wframe[4],
                wframe[5],
                wframe[6],
                wframe[7],
                wframe[8],
                wframe[9],
                wframe[10]);

        BitBuffer* tx = bb_alloc_from(wframe, sizeof(wframe));
        BitBuffer* rx = bb_alloc_rx(16);

        /* Extended FWT for Option flag (2-phase response) */
        Iso15693_3Error err = iso15693_3_poller_send_frame(iso, tx, rx, 2000000);
        size_t rxsz = bit_buffer_get_size_bytes(rx);

        FURI_LOG_I(TAG, "  block %u: err=%d rxbytes=%u", (unsigned)b, (int)err, (unsigned)rxsz);
        if(rxsz >= 2)
            FURI_LOG_I(
                TAG, "  resp=[%02X %02X]", bit_buffer_get_byte(rx, 0), bit_buffer_get_byte(rx, 1));

        /* Only fail on explicit card error response (bit0=1 in flags byte) */
        bool card_error = (rxsz >= 1) && ((bit_buffer_get_byte(rx, 0) & 0x01) != 0);

        bit_buffer_free(tx);
        bit_buffer_free(rx);

        if(card_error) {
            FURI_LOG_E(TAG, "Block %u: card error", (unsigned)b);
            return false;
        }
        /* err=6/timeout with rxbytes=0 is normal with option flag — continue */

        furi_delay_ms(25);
    }
    return true;
}

/* ============================================================================
 *  Persistent special UID storage
 * ========================================================================== */

bool sli_writer_save_special_uid(SliWriterApp* app) {
    /* Create directory if needed */
    storage_common_mkdir(app->storage, "/ext/apps_data/sli_writer");

    File* f = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(f, SLI_SPECIAL_UID_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = (storage_file_write(f, app->special_uid, 8) == 8);
        storage_file_close(f);
    }
    storage_file_free(f);
    if(ok)
        FURI_LOG_I(
            TAG,
            "Special UID saved: %02X%02X%02X%02X%02X%02X%02X%02X",
            app->special_uid[0],
            app->special_uid[1],
            app->special_uid[2],
            app->special_uid[3],
            app->special_uid[4],
            app->special_uid[5],
            app->special_uid[6],
            app->special_uid[7]);
    else
        FURI_LOG_E(TAG, "Special UID save failed");
    return ok;
}

bool sli_writer_load_special_uid(SliWriterApp* app) {
    File* f = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(f, SLI_SPECIAL_UID_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = (storage_file_read(f, app->special_uid, 8) == 8);
        storage_file_close(f);
    }
    storage_file_free(f);
    if(ok) {
        app->special_uid_saved = true;
        FURI_LOG_I(
            TAG,
            "Special UID loaded: %02X%02X%02X%02X%02X%02X%02X%02X",
            app->special_uid[0],
            app->special_uid[1],
            app->special_uid[2],
            app->special_uid[3],
            app->special_uid[4],
            app->special_uid[5],
            app->special_uid[6],
            app->special_uid[7]);
    }
    return ok;
}

/* ============================================================================
 *  .nfc file parser
 * ========================================================================== */

bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* path) {
    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t sz = storage_file_size(f);
        char* buf = malloc(sz + 1);
        if(buf) {
            size_t n = storage_file_read(f, buf, sz);
            buf[n] = '\0';

            memset(&app->nfc_data, 0x00, sizeof(app->nfc_data));
            app->nfc_data.block_size = 4;
            app->nfc_data.block_count = 0;

            char* p;

            if((p = strstr(buf, "UID: "))) {
                p += 5;
                char* q = p;
                for(int i = 0; i < 8; i++) {
                    while(*q == ' ')
                        q++;
                    app->nfc_data.uid[i] = (uint8_t)strtol(q, &q, 16);
                }

                if((p = strstr(buf, "Block Count: ")))
                    sscanf(p + 13, "%hhu", &app->nfc_data.block_count);

                if((p = strstr(buf, "Block Size: ")))
                    sscanf(p + 12, "%hhu", &app->nfc_data.block_size);

                if((p = strstr(buf, "Data Content: "))) {
                    p += 14;
                    size_t total = (size_t)app->nfc_data.block_count * app->nfc_data.block_size;
                    if(total > sizeof(app->nfc_data.data)) total = sizeof(app->nfc_data.data);
                    char* q2 = p;
                    for(size_t i = 0; i < total; i++) {
                        while(*q2 == ' ')
                            q2++;
                        app->nfc_data.data[i] = (uint8_t)strtol(q2, &q2, 16);
                    }
                }

                FURI_LOG_I(
                    TAG,
                    "Parsed: blocks=%u size=%u uid=%02X%02X%02X%02X%02X%02X%02X%02X",
                    app->nfc_data.block_count,
                    app->nfc_data.block_size,
                    app->nfc_data.uid[0],
                    app->nfc_data.uid[1],
                    app->nfc_data.uid[2],
                    app->nfc_data.uid[3],
                    app->nfc_data.uid[4],
                    app->nfc_data.uid[5],
                    app->nfc_data.uid[6],
                    app->nfc_data.uid[7]);
                ok = true;
            }
            free(buf);
        }
        storage_file_close(f);
    }
    storage_file_free(f);
    return ok;
}

/* ============================================================================
 *  Write operations
 * ========================================================================== */

/* Normal write: SELECT -> write blocks (non-addressed) -> write UID */
static bool do_normal_write(SliWriterApp* app, Iso15693_3Poller* iso) {
    select_card(iso, app->detected_uid);
    furi_delay_ms(10);

    FURI_LOG_I(TAG, "Normal write: %u blocks", app->nfc_data.block_count);
    if(!write_blocks(
           iso, app->nfc_data.data, app->nfc_data.block_count, app->nfc_data.block_size)) {
        furi_string_set(app->error_message, "Block write failed");
        return false;
    }
    FURI_LOG_I(TAG, "Blocks OK");

    static const uint8_t zero_uid[8] = {0};
    if(memcmp(app->nfc_data.uid, zero_uid, 8) != 0 &&
       memcmp(app->nfc_data.uid, app->detected_uid, 8) != 0) {
        FURI_LOG_I(TAG, "Writing target UID...");
        if(!magic_write_uid(iso, app->nfc_data.uid)) {
            furi_string_set(app->error_message, "UID write failed");
            return false;
        }
        FURI_LOG_I(TAG, "UID OK");
    } else {
        FURI_LOG_I(TAG, "UID skip (same or zero)");
    }
    return true;
}

/* Special write:
 *   1. Restore factory UID (so blocks become writable)
 *   2. Write blocks addressed (flags=0x62) using factory UID
 *   3. Set target UID from .nfc file */
static bool do_special_write(SliWriterApp* app, Iso15693_3Poller* iso) {
    FURI_LOG_I(
        TAG,
        "Special write: factory=%02X%02X target=%02X%02X",
        app->special_uid[7],
        app->special_uid[6],
        app->nfc_data.uid[0],
        app->nfc_data.uid[1]);

    /* Step 1: restore factory UID (skip if card already has factory UID)
     * special_uid stored LSB-first — reverse to MSB-first for magic_write_uid */
    uint8_t factory_uid_msb[8];
    for(int i = 0; i < 8; i++)
        factory_uid_msb[i] = app->special_uid[7 - i];

    /* detected_uid is MSB-first, factory_uid_msb is also MSB-first — compare directly */
    bool already_factory = (memcmp(app->detected_uid, factory_uid_msb, 8) == 0);
    if(already_factory) {
        FURI_LOG_I(TAG, "Step 1: skip (card already has factory UID)");
    } else {
        FURI_LOG_I(
            TAG,
            "Step 1: restore factory UID (MSB: %02X%02X%02X%02X%02X%02X%02X%02X)",
            factory_uid_msb[0],
            factory_uid_msb[1],
            factory_uid_msb[2],
            factory_uid_msb[3],
            factory_uid_msb[4],
            factory_uid_msb[5],
            factory_uid_msb[6],
            factory_uid_msb[7]);
        if(!magic_write_uid(iso, factory_uid_msb)) {
            furi_string_set(app->error_message, "Restore factory UID failed");
            return false;
        }
        furi_delay_ms(50);
    } /* let card settle after UID change */

    /* RF resync only needed if we actually changed the UID */
    if(!already_factory) {
        furi_delay_ms(50);
        uint8_t uid_resync[8];
        Iso15693_3Error resync_err = iso15693_3_poller_inventory(iso, uid_resync);
        FURI_LOG_I(
            TAG,
            "Resync inventory: err=%d uid=%02X%02X%02X%02X%02X%02X%02X%02X",
            (int)resync_err,
            uid_resync[7],
            uid_resync[6],
            uid_resync[5],
            uid_resync[4],
            uid_resync[3],
            uid_resync[2],
            uid_resync[1],
            uid_resync[0]);
        if(resync_err != Iso15693_3ErrorNone) {
            furi_string_set(app->error_message, "Card lost after UID restore");
            return false;
        }
        furi_delay_ms(20);
    }

    /* Step 2: write blocks addressed using factory UID */
    FURI_LOG_I(TAG, "Step 2: write blocks addressed (0x62)");
    if(!write_blocks_addressed(
           iso,
           app->special_uid,
           app->nfc_data.data,
           app->nfc_data.block_count,
           app->nfc_data.block_size)) {
        furi_string_set(app->error_message, "Block write failed (special)");
        return false;
    }
    FURI_LOG_I(TAG, "Blocks OK");

    /* Step 3: set target UID */
    static const uint8_t zero_uid[8] = {0};
    if(memcmp(app->nfc_data.uid, zero_uid, 8) != 0 &&
       memcmp(app->nfc_data.uid, app->special_uid, 8) != 0) {
        FURI_LOG_I(TAG, "Step 3: write target UID");
        if(!magic_write_uid(iso, app->nfc_data.uid)) {
            furi_string_set(app->error_message, "Target UID write failed");
            return false;
        }
        FURI_LOG_I(TAG, "Target UID OK");
    } else {
        FURI_LOG_I(TAG, "Step 3: UID skip (same as factory or zero)");
    }
    return true;
}

/* ============================================================================
 *  NFC poller callback
 * ========================================================================== */

static NfcCommand sli_poller_callback(NfcGenericEvent event, void* context) {
    SliWriterApp* app = context;

    if(event.protocol != NfcProtocolIso15693_3) return NfcCommandContinue;

    Iso15693_3PollerEvent* iso_event = event.event_data;
    if(iso_event->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;

    Iso15693_3Poller* iso = event.instance;
    bool success = false;

    uint8_t uid_lsb[8];
    Iso15693_3Error uid_err = iso15693_3_poller_inventory(iso, uid_lsb);
    if(uid_err != Iso15693_3ErrorNone) {
        FURI_LOG_E(TAG, "Inventory failed: err=%d", (int)uid_err);
        furi_string_set(app->error_message, "Card inventory failed");
        goto done;
    }

    for(int i = 0; i < 8; i++)
        app->detected_uid[i] = uid_lsb[7 - i];
    app->have_uid = true;
    FURI_LOG_I(
        TAG,
        "Card UID: %02X%02X%02X%02X%02X%02X%02X%02X",
        app->detected_uid[0],
        app->detected_uid[1],
        app->detected_uid[2],
        app->detected_uid[3],
        app->detected_uid[4],
        app->detected_uid[5],
        app->detected_uid[6],
        app->detected_uid[7]);
    furi_delay_ms(20);

    switch(app->write_mode) {
    case SliWriterModeSaveUid:
        for(int i = 0; i < 8; i++)
            app->special_uid[i] = app->detected_uid[7 - i];
        app->special_uid_saved = true;
        success = sli_writer_save_special_uid(app);
        if(!success) furi_string_set(app->error_message, "Save UID failed");
        break;
    case SliWriterModeSpecial:
        success = do_special_write(app, iso);
        break;
    case SliWriterModeNormal:
    default:
        success = do_normal_write(app, iso);
        break;
    }

done:;
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    if(success) {
        notification_message(notif, &seq_success);
    } else {
        notification_message(notif, &seq_error);
    }
    furi_record_close(RECORD_NOTIFICATION);

    uint32_t evt = success ? SliWriterCustomEventWriteSuccess : SliWriterCustomEventWriteError;
    if(app->write_mode == SliWriterModeSaveUid && success)
        evt = SliWriterCustomEventSaveUidSuccess;
    view_dispatcher_send_custom_event(app->view_dispatcher, evt);

    return NfcCommandStop;
}

/* ============================================================================
 *  UI Callbacks
 * ========================================================================== */

void sli_writer_submenu_callback(void* context, uint32_t index) {
    SliWriterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sli_writer_dialog_ex_callback(DialogExResult result, void* context) {
    SliWriterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

bool sli_writer_custom_event_callback(void* context, uint32_t event) {
    SliWriterApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool sli_writer_back_event_callback(void* context) {
    SliWriterApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ============================================================================
 *  Scenes
 * ========================================================================== */

/* --- Start --- */
void sli_writer_scene_start_on_enter(void* context) {
    SliWriterApp* app = context;
    submenu_reset(app->submenu);

    submenu_add_item(
        app->submenu,
        "Write NFC File",
        SliWriterSubmenuIndexWrite,
        sli_writer_submenu_callback,
        app);

    submenu_add_item(
        app->submenu,
        "Save Special UID",
        SliWriterSubmenuIndexSaveUid,
        sli_writer_submenu_callback,
        app);

    /* Show saved special UID as a non-clickable info line */
    if(app->special_uid_saved) {
        char uid_info[48];
        snprintf(
            uid_info,
            sizeof(uid_info),
            "  > %02X %02X %02X %02X %02X %02X %02X %02X",
            app->special_uid[0],
            app->special_uid[1],
            app->special_uid[2],
            app->special_uid[3],
            app->special_uid[4],
            app->special_uid[5],
            app->special_uid[6],
            app->special_uid[7]);
        /* Index 99 — ignored by event handler, just visual info */
        submenu_add_item(app->submenu, uid_info, 99, sli_writer_submenu_callback, app);
    }

    /* "Write Special" label — no UID shown here, it's on the line above */
    submenu_add_item(
        app->submenu,
        app->special_uid_saved ? "Write Special" : "Write Special (no UID saved)",
        SliWriterSubmenuIndexWriteSpec,
        sli_writer_submenu_callback,
        app);

    submenu_add_item(
        app->submenu, "About", SliWriterSubmenuIndexAbout, sli_writer_submenu_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewSubmenu);
}

bool sli_writer_scene_start_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == SliWriterSubmenuIndexWrite) {
        app->write_mode = SliWriterModeNormal;
        scene_manager_next_scene(app->scene_manager, SliWriterSceneFileSelect);
        return true;
    }

    if(event.event == SliWriterSubmenuIndexSaveUid) {
        app->write_mode = SliWriterModeSaveUid;
        scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
        return true;
    }

    if(event.event == SliWriterSubmenuIndexWriteSpec) {
        if(!app->special_uid_saved) {
            furi_string_set(
                app->error_message, "No special UID saved!\nUse 'Save Special UID' first.");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
            return true;
        }
        app->write_mode = SliWriterModeSpecial;
        scene_manager_next_scene(app->scene_manager, SliWriterSceneFileSelect);
        return true;
    }

    if(event.event == SliWriterSubmenuIndexAbout) {
        dialog_ex_reset(app->dialog_ex);
        dialog_ex_set_header(app->dialog_ex, "SLI Writer", 64, 0, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex,
            "ISO15693 magic card writer\nSLI / SLIX-L / Special",
            64,
            32,
            AlignCenter,
            AlignCenter);
        dialog_ex_set_left_button_text(app->dialog_ex, "Back");
        view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
        return true;
    }
    return false;
}

void sli_writer_scene_start_on_exit(void* context) {
    SliWriterApp* app = context;
    submenu_reset(app->submenu);
}

/* --- File Select --- */
void sli_writer_scene_file_select_on_enter(void* context) {
    SliWriterApp* app = context;
    furi_string_set(app->file_path, "/ext/nfc");

    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, SLI_WRITER_FILE_EXTENSION, NULL);

    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &opts)) {
        if(sli_writer_parse_nfc_file(app, furi_string_get_cstr(app->file_path))) {
            scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
        } else {
            furi_string_set(app->error_message, "Cannot parse .nfc file");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
        }
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool sli_writer_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    (void)context;
    (void)event;
    return false;
}

void sli_writer_scene_file_select_on_exit(void* context) {
    (void)context;
}

/* --- Write --- */
void sli_writer_scene_write_on_enter(void* context) {
    SliWriterApp* app = context;

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "SLI Writer", 64, 0, AlignCenter, AlignTop);

    if(app->write_mode == SliWriterModeSaveUid) {
        dialog_ex_set_text(
            app->dialog_ex, "Approach card\nto save UID...", 64, 32, AlignCenter, AlignCenter);
    } else {
        dialog_ex_set_text(
            app->dialog_ex, "Approach card\nto Flipper...", 64, 32, AlignCenter, AlignCenter);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);

    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notif, &seq_blink_start);
    furi_record_close(RECORD_NOTIFICATION);

    app->have_uid = false;
    furi_string_reset(app->error_message);

    app->nfc = nfc_alloc();
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    app->nfc_started = true;

    nfc_poller_start(app->poller, sli_poller_callback, app);
    FURI_LOG_I(TAG, "Poller started, mode=%d", (int)app->write_mode);
}

bool sli_writer_scene_write_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterCustomEventWriteSuccess ||
           event.event == SliWriterCustomEventSaveUidSuccess) {
            scene_manager_next_scene(app->scene_manager, SliWriterSceneSuccess);
            return true;
        }
        if(event.event == SliWriterCustomEventWriteError) {
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
            return true;
        }
    }
    return false;
}

void sli_writer_scene_write_on_exit(void* context) {
    SliWriterApp* app = context;
    if(app->nfc_started) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        nfc_free(app->nfc);
        app->poller = NULL;
        app->nfc = NULL;
        app->nfc_started = false;
    }
}

/* --- Success --- */
void sli_writer_scene_success_on_enter(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Success!", 64, 0, AlignCenter, AlignTop);

    if(app->write_mode == SliWriterModeSaveUid) {
        char msg[64];
        snprintf(
            msg,
            sizeof(msg),
            "UID saved:\n%02X %02X %02X %02X\n%02X %02X %02X %02X",
            app->special_uid[0],
            app->special_uid[1],
            app->special_uid[2],
            app->special_uid[3],
            app->special_uid[4],
            app->special_uid[5],
            app->special_uid[6],
            app->special_uid[7]);
        dialog_ex_set_text(app->dialog_ex, msg, 64, 32, AlignCenter, AlignCenter);
    } else {
        dialog_ex_set_text(
            app->dialog_ex, "Card written successfully", 64, 32, AlignCenter, AlignCenter);
    }
    dialog_ex_set_left_button_text(app->dialog_ex, "OK");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}

bool sli_writer_scene_success_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft)) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}

void sli_writer_scene_success_on_exit(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}

/* --- Error --- */
void sli_writer_scene_error_on_enter(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Error", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex, furi_string_get_cstr(app->error_message), 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "OK");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}

bool sli_writer_scene_error_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft)) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}

void sli_writer_scene_error_on_exit(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}

/* ============================================================================
 *  Scene manager dispatch tables
 * ========================================================================== */

static void (*const sli_scene_on_enter[])(void*) = {
    sli_writer_scene_start_on_enter,
    sli_writer_scene_file_select_on_enter,
    sli_writer_scene_write_on_enter,
    sli_writer_scene_success_on_enter,
    sli_writer_scene_error_on_enter,
};

static bool (*const sli_scene_on_event[])(void*, SceneManagerEvent) = {
    sli_writer_scene_start_on_event,
    sli_writer_scene_file_select_on_event,
    sli_writer_scene_write_on_event,
    sli_writer_scene_success_on_event,
    sli_writer_scene_error_on_event,
};

static void (*const sli_scene_on_exit[])(void*) = {
    sli_writer_scene_start_on_exit,
    sli_writer_scene_file_select_on_exit,
    sli_writer_scene_write_on_exit,
    sli_writer_scene_success_on_exit,
    sli_writer_scene_error_on_exit,
};

static const SceneManagerHandlers sli_scene_handlers = {
    .on_enter_handlers = sli_scene_on_enter,
    .on_event_handlers = sli_scene_on_event,
    .on_exit_handlers = sli_scene_on_exit,
    .scene_num = SliWriterSceneNum,
};

/* ============================================================================
 *  App lifecycle
 * ========================================================================== */

SliWriterApp* sli_writer_app_alloc(void) {
    SliWriterApp* app = malloc(sizeof(SliWriterApp));
    furi_check(app);
    memset(app, 0x00, sizeof(SliWriterApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&sli_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, sli_writer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, sli_writer_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SliWriterViewSubmenu, submenu_get_view(app->submenu));

    app->dialog_ex = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, sli_writer_dialog_ex_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, SliWriterViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SliWriterViewLoading, loading_get_view(app->loading));

    app->file_path = furi_string_alloc();
    app->error_message = furi_string_alloc();

    /* Load special UID from SD if it was previously saved */
    sli_writer_load_special_uid(app);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, SliWriterSceneStart);

    return app;
}

void sli_writer_app_free(SliWriterApp* app) {
    furi_assert(app);

    if(app->nfc_started) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        nfc_free(app->nfc);
    }

    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewLoading);

    submenu_free(app->submenu);
    dialog_ex_free(app->dialog_ex);
    loading_free(app->loading);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->file_path);
    furi_string_free(app->error_message);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

/* ============================================================================
 *  Entry point
 * ========================================================================== */

int32_t sli_writer_app(void* p) {
    UNUSED(p);
    SliWriterApp* app = sli_writer_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    sli_writer_app_free(app);
    return 0;
}
