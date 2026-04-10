#include "../mfp_app.h"
#include "../mfp_keys.h"
#include "../mfp_storage.h"
#include "../mfp_dump_view.h"
#include "mfp_scene_config.h"

#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <gui/scene_manager.h>
#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define TAG "MfpReadAll"

/* High values to avoid collision with other scenes' custom events.
 * The NFC poller thread may send events just as the scene is being
 * torn down; those stray events can reach the next-active scene
 * (typically More), so their numeric values must not overlap with
 * MoreCustomEvent or any other scene we might navigate back to. */
typedef enum {
    ReadAllEventProgress = 0x1000,
    ReadAllEventComplete,
    ReadAllEventCardLost,
} ReadAllCustomEvent;

/* ---- NFC callback (NFC thread) ---- */

/* Holds a pointer to a key candidate + where it came from, so we can
 * remember the last successful match across sectors. */
typedef struct {
    int      idx;           /* -1 = no value yet */
    bool     from_default;
} LastKeyRef;

/* Try one (sector, key_type, key) combination. Returns:
 *  +1 if auth succeeded
 *   0 if auth failed (wrong key)
 *  -1 if comm error — caller must abort and bubble up */
static int try_auth(
    Iso14443_4aPoller* poller,
    MfpApp* app,
    uint8_t sector,
    MfpKeyType key_type,
    const uint8_t* key) {
    MfpError err = mfp_poller_auth(poller, sector, key_type, key, &app->session);
    if(err == MfpOk) return 1;
    if(err == MfpErrorComm) return -1;
    return 0;
}

static NfcCommand mfp_read_all_poller_cb(NfcGenericEvent event, void* ctx) {
    MfpApp* app = ctx;
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    const Iso14443_4aPollerEvent* ev = event.event_data;

    if(ev->type == Iso14443_4aPollerEventTypeError) return NfcCommandContinue;
    if(ev->type != Iso14443_4aPollerEventTypeReady) return NfcCommandContinue;

    Iso14443_4aPoller* poller = event.instance;
    MfpCardSize card_size = app->version.size;
    uint8_t total = mfp_sector_count(card_size);

    FURI_LOG_I(TAG, "Scan start: %d sectors, %lu dict keys",
        (int)total, (unsigned long)app->dict_key_count);

    app->scan_total_sectors = total;
    app->scan_sectors_done = 0;
    app->scan_sectors_ok = 0;

    LastKeyRef last_a = {-1, true};
    LastKeyRef last_b = {-1, true};

    for(uint8_t sector = 0; sector < total; sector++) {
        if(app->scan_cancel_requested) break;

        MfpSectorResult* r = &app->sector_results[sector];
        r->status = MfpSectorAuthFail;
        r->blocks_read = 0;
        r->key_a_found = false;
        r->key_b_found = false;

        uint8_t first_block = mfp_sector_first_block(card_size, sector);
        uint8_t blk_count = mfp_sector_block_count(card_size, sector);

        /* Macro-esque helper: try a (sector, key_type, key) and on success
         * store into the result + update the "last successful" ref so the
         * next sector tries the same key first. Returns -1 on comm error. */
#define TRY_AND_STORE(kt_expr, key_ptr, from_def, idx_val)                  \
    do {                                                                    \
        MfpKeyType _kt = (kt_expr);                                         \
        const uint8_t* _k = (key_ptr);                                      \
        int _rc = try_auth(poller, app, sector, _kt, _k);                   \
        if(_rc < 0) {                                                       \
            app->scan_sectors_done = sector;                                \
            view_dispatcher_send_custom_event(                              \
                app->view_dispatcher, ReadAllEventCardLost);                \
            return NfcCommandStop;                                          \
        }                                                                   \
        if(_rc > 0) {                                                       \
            if(_kt == MfpKeyA && !r->key_a_found) {                         \
                memcpy(r->key_a, _k, MFP_AES_KEY_SIZE);                     \
                r->key_a_found = true;                                      \
                last_a.idx = (idx_val);                                     \
                last_a.from_default = (from_def);                           \
            } else if(_kt == MfpKeyB && !r->key_b_found) {                  \
                memcpy(r->key_b, _k, MFP_AES_KEY_SIZE);                     \
                r->key_b_found = true;                                      \
                last_b.idx = (idx_val);                                     \
                last_b.from_default = (from_def);                           \
            }                                                               \
        }                                                                   \
    } while(0)

        /* Phase 0: try last successful keys first (separately for A and B).
         * Index -1 means "no last key yet". */
        if(last_a.idx >= 0) {
            const uint8_t* lk = last_a.from_default
                                    ? mfp_default_keys[last_a.idx]
                                    : app->dict_buf + last_a.idx * MFP_AES_KEY_SIZE;
            TRY_AND_STORE(MfpKeyA, lk, last_a.from_default, last_a.idx);
        }
        if(last_b.idx >= 0 && !r->key_b_found) {
            const uint8_t* lk = last_b.from_default
                                    ? mfp_default_keys[last_b.idx]
                                    : app->dict_buf + last_b.idx * MFP_AES_KEY_SIZE;
            TRY_AND_STORE(MfpKeyB, lk, last_b.from_default, last_b.idx);
        }

        /* Phase 1: hardcoded defaults — try each as A then as B until we
         * have BOTH keys for this sector. */
        for(int ki = 0;
            ki < MFP_DEFAULT_KEY_COUNT && !(r->key_a_found && r->key_b_found);
            ki++) {
            if(!r->key_a_found) {
                TRY_AND_STORE(MfpKeyA, mfp_default_keys[ki], true, ki);
            }
            if(!r->key_b_found) {
                TRY_AND_STORE(MfpKeyB, mfp_default_keys[ki], true, ki);
            }
        }

        /* Phase 2: user dictionary. */
        if(!(r->key_a_found && r->key_b_found) && app->dict_buf && app->dict_key_count > 0) {
            for(uint32_t ki = 0;
                ki < app->dict_key_count && !(r->key_a_found && r->key_b_found);
                ki++) {
                const uint8_t* dk = app->dict_buf + ki * MFP_AES_KEY_SIZE;
                if(!r->key_a_found) {
                    TRY_AND_STORE(MfpKeyA, dk, false, (int)ki);
                }
                if(!r->key_b_found) {
                    TRY_AND_STORE(MfpKeyB, dk, false, (int)ki);
                }
            }
        }

#undef TRY_AND_STORE

        /* Read blocks using whichever key we have. Prefer KeyA since it
         * typically carries read permissions in MFP access rights. */
        bool any_key = r->key_a_found || r->key_b_found;
        if(any_key) {
            MfpKeyType use_kt = r->key_a_found ? MfpKeyA : MfpKeyB;
            const uint8_t* use_key = r->key_a_found ? r->key_a : r->key_b;
            /* Re-auth to get a fresh session (the last scan attempt may
             * have left a stale session from a failed try_auth call). */
            MfpError auth_err =
                mfp_poller_auth(poller, sector, use_kt, use_key, &app->session);
            if(auth_err == MfpErrorComm) {
                app->scan_sectors_done = sector;
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ReadAllEventCardLost);
                return NfcCommandStop;
            }
            if(auth_err != MfpOk) {
                /* Shouldn't happen — we just validated this key. */
                r->status = MfpSectorReadFail;
                app->scan_sectors_done = sector + 1;
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ReadAllEventProgress);
                continue;
            }

            r->status = MfpSectorOk;
            uint8_t read_ok = 0;
            for(uint8_t b = 0; b < blk_count; b++) {
                MfpError err = mfp_poller_read_block(
                    poller, first_block + b, &app->session, app->blocks[first_block + b]);
                if(err != MfpOk) {
                    if(err == MfpErrorComm) {
                        r->blocks_read = read_ok;
                        if(read_ok == 0) r->status = MfpSectorReadFail;
                        app->scan_sectors_done = sector + 1;
                        app->scan_sectors_ok += (read_ok > 0) ? 1 : 0;
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, ReadAllEventCardLost);
                        return NfcCommandStop;
                    }
                    r->status = MfpSectorReadFail;
                    break;
                }
                read_ok++;
            }
            r->blocks_read = read_ok;
            if(read_ok > 0) app->scan_sectors_ok++;
        }

        app->scan_sectors_done = sector + 1;
        view_dispatcher_send_custom_event(app->view_dispatcher, ReadAllEventProgress);
    }

    FURI_LOG_I(TAG, "Scan done: %d/%d OK",
        (int)app->scan_sectors_ok, (int)app->scan_total_sectors);
    view_dispatcher_send_custom_event(app->view_dispatcher, ReadAllEventComplete);
    return NfcCommandStop;
}

/* ---- Scene handlers ---- */

void mfp_scene_read_all_on_enter(void* ctx) {
    MfpApp* app = ctx;

    /* Reset scan state */
    memset(app->sector_results, 0, sizeof(app->sector_results));
    app->scan_sectors_done = 0;
    app->scan_sectors_ok = 0;
    app->scan_total_sectors = mfp_sector_count(app->version.size);
    app->scan_cancel_requested = false;
    app->read_all_mode = true;

    /* Load user dictionary if path set */
    if(app->dict_buf) {
        free(app->dict_buf);
        app->dict_buf = NULL;
        app->dict_key_count = 0;
    }
    /* dict_path is set by the previous scene (CardInfo or DictSelect):
     *  - empty string → only hardcoded defaults
     *  - non-empty    → load that .dic file */
    if(!furi_string_empty(app->dict_path)) {
        app->dict_key_count = mfp_keys_load_dict(
            app->storage, furi_string_get_cstr(app->dict_path), &app->dict_buf);
    }

    /* Seed the custom dump view with card info + key counts */
    mfp_dump_view_reset(
        app->dump_view,
        app->version.size,
        app->scan_total_sectors,
        app->version.uid,
        app->version.uid_len,
        MFP_DEFAULT_KEY_COUNT,
        app->dict_key_count);
    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewDump);

    /* Start NFC poller */
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(app->poller, mfp_read_all_poller_cb, app);
}

bool mfp_scene_read_all_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;

    if(event.type == SceneManagerEventTypeBack) {
        /* Cancel the scan and jump straight back to the Start menu,
         * skipping the intermediate Read/ReadAllChoice scenes that
         * would otherwise restart card detection / dict choice on
         * their on_enter. on_exit will still drain the NFC thread. */
        app->scan_cancel_requested = true;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MfpSceneStart);
        return true;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ReadAllEventProgress) {
        mfp_dump_view_sync(app->dump_view, app);
        return true;
    }

    if(event.event == ReadAllEventComplete || event.event == ReadAllEventCardLost) {
        /* Final sync so the grid reflects the last sector iteration */
        mfp_dump_view_sync(app->dump_view, app);
        if(app->scan_sectors_ok > 0) {
            notification_message(app->notifications, &sequence_success);
            scene_manager_next_scene(app->scene_manager, MfpSceneReadAllResult);
        } else {
            notification_message(app->notifications, &sequence_error);
            mfp_dump_view_set_state(
                app->dump_view,
                (event.event == ReadAllEventCardLost) ? MfpDumpViewStateCardLost
                                                      : MfpDumpViewStateNoKeys);
        }
        return true;
    }

    return false;
}

void mfp_scene_read_all_on_exit(void* ctx) {
    MfpApp* app = ctx;

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    if(app->dict_buf) {
        free(app->dict_buf);
        app->dict_buf = NULL;
        app->dict_key_count = 0;
    }
}
