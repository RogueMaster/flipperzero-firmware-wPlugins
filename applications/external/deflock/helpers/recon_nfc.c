// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "recon_nfc.h"
#include "../recon_app_i.h"

#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller_sync.h>
#include <toolbox/keys_dict.h>

#include <string.h>

#define RECON_NFC_MAX_PROTO 8

/* Universal default keys, used only if neither stock dictionary is on the SD.
 * Kept tiny; the on-SD mf_classic_dict has the full ~1k-entry list. */
static const uint8_t recon_nfc_default_keys[][MF_CLASSIC_KEY_SIZE] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5},
    {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5},
    {0x4D, 0x3A, 0x99, 0xC3, 0x51, 0xDD},
    {0x1A, 0x98, 0x2C, 0x7E, 0x45, 0x9A},
    {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
};

#define RECON_NFC_DICT_SYSTEM EXT_PATH("nfc/assets/mf_classic_dict.nfc")
#define RECON_NFC_DICT_USER   EXT_PATH("nfc/assets/mf_classic_dict_user.nfc")

struct ReconNfc {
    ReconApp* app;
    Nfc* nfc;
    NfcScanner* scanner;
    FuriMutex* lock;
    bool running;
    bool detected;
    size_t proto_num;
    NfcProtocol protos[RECON_NFC_MAX_PROTO];

    ReconMfcResult mfc;
    FuriThread* probe_thread;
    volatile bool probing;
    volatile bool probe_dirty; /**< status changed -> scene re-renders */
};

/** Depth of a protocol in its parent hierarchy (more parents = more specific). */
static int proto_depth(NfcProtocol p) {
    int depth = 0;
    NfcProtocol cur = p;
    while(true) {
        NfcProtocol parent = nfc_protocol_get_parent(cur);
        if(parent == NfcProtocolInvalid) break;
        depth++;
        cur = parent;
        if(depth > 8) break; // safety
    }
    return depth;
}

static void recon_nfc_grade(NfcProtocol p, const char** grade, const char** detail) {
    switch(p) {
    case NfcProtocolMfClassic:
        *grade = "WEAK";
        *detail =
            "MIFARE Classic\nCrypto1 is broken.\nKeys recoverable via\nnested/darkside/mfkey.\nAvoid for access control.";
        break;
    case NfcProtocolMfUltralight:
        *grade = "WEAK";
        *detail =
            "MIFARE Ultralight\nLittle/no auth on most\nvariants; easily cloned.\nUL-C/EV1 better if PWD set.";
        break;
    case NfcProtocolSt25tb:
        *grade = "WEAK";
        *detail = "ST25TB / SRT\nNo crypto; UID-based.\nCloneable.";
        break;
    case NfcProtocolSlix:
        *grade = "WEAK";
        *detail = "ICODE SLIX (15693)\nVicinity tag; weak/optional\nprotection. Often cloneable.";
        break;
    case NfcProtocolMfPlus:
        *grade = "MEDIUM";
        *detail =
            "MIFARE Plus\nStrong in SL3 (AES),\nweak if left in SL1.\nVerify security level.";
        break;
    case NfcProtocolMfDesfire:
        *grade = "STRONG";
        *detail =
            "MIFARE DESFire\nAES/3DES if configured.\nStrong when keys are\nnot left at default.";
        break;
    case NfcProtocolIso14443_4a:
    case NfcProtocolIso14443_4b:
        // Covers ISO14443-4 smartcards including EMV / Type4 / NTAG4xx, whose
        // dedicated enum values are firmware-specific (Momentum) and absent in
        // stock OFW. Grading them here keeps the app portable across both.
        *grade = "INFO";
        *detail =
            "ISO14443-4 smartcard\nEMV/DESFire/Type4 etc.\nSecurity depends on applet.\nInspect further.";
        break;
    case NfcProtocolFelica:
        *grade = "INFO";
        *detail = "FeliCa\nSecurity is service-defined.";
        break;
    case NfcProtocolIso15693_3:
        *grade = "WEAK";
        *detail = "ISO15693 vicinity tag\nUID-only; cloneable if used\nalone for access.";
        break;
    case NfcProtocolIso14443_3a:
        *grade = "WEAK";
        *detail =
            "ISO14443-3A (UID only)\nNo app-layer auth detected.\nUID is cloneable; weak if\nused alone for access.";
        break;
    case NfcProtocolIso14443_3b:
        *grade = "INFO";
        *detail = "ISO14443-3B\nBase layer; inspect apps.";
        break;
    default:
        *grade = "INFO";
        *detail = "Unclassified protocol.";
        break;
    }
}

/* Dynamic grade for a MIFARE Classic card after a deep check has run: the
 * default-keyed sector count makes the "trivially cloneable" verdict concrete.
 * Writes into the caller's buffer (detail text is built per-call). */
static void recon_nfc_grade_mfc(
    const ReconMfcResult* r,
    const char** grade,
    char* detail,
    size_t detail_len) {
    *grade = "WEAK";
    if(r->default_keyed == 0) {
        snprintf(
            detail,
            detail_len,
            "MIFARE Classic\nCrypto1 broken; no DEFAULT\nkeys found (still recoverable\nvia mfkey/nested).");
    } else {
        snprintf(
            detail,
            detail_len,
            "MIFARE Classic\n%u/%u sectors open with\nDEFAULT keys - trivially\ncloneable.",
            r->default_keyed,
            r->total_sectors);
    }
}

static void recon_nfc_scanner_cb(NfcScannerEvent event, void* context) {
    ReconNfc* n = context;
    if(event.type == NfcScannerEventTypeDetected) {
        furi_mutex_acquire(n->lock, FuriWaitForever);
        n->proto_num = MIN(event.data.protocol_num, (size_t)RECON_NFC_MAX_PROTO);
        for(size_t i = 0; i < n->proto_num; i++) {
            n->protos[i] = event.data.protocols[i];
        }
        n->detected = (n->proto_num > 0);
        furi_mutex_release(n->lock);
    }
}

ReconNfc* recon_nfc_alloc(void* app) {
    ReconNfc* n = malloc(sizeof(ReconNfc));
    memset(n, 0, sizeof(ReconNfc));
    n->app = app;
    n->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    return n;
}

void recon_nfc_free(ReconNfc* n) {
    furi_assert(n);
    if(n->running) recon_nfc_stop(n);
    furi_mutex_free(n->lock);
    free(n);
}

void recon_nfc_start(ReconNfc* n) {
    if(n->running) return;
    n->detected = false;
    n->proto_num = 0;
    n->nfc = nfc_alloc();
    n->scanner = nfc_scanner_alloc(n->nfc);
    nfc_scanner_start(n->scanner, recon_nfc_scanner_cb, n);
    n->running = true;
}

void recon_nfc_stop(ReconNfc* n) {
    if(!n->running) return;
    // A running deep check owns the radio + scanner; let it finish and reap it
    // before we tear the scanner/nfc down underneath it.
    if(n->probe_thread) {
        furi_thread_join(n->probe_thread);
        furi_thread_free(n->probe_thread);
        n->probe_thread = NULL;
    }
    nfc_scanner_stop(n->scanner);
    nfc_scanner_free(n->scanner);
    nfc_free(n->nfc);
    n->scanner = NULL;
    n->nfc = NULL;
    n->running = false;
}

bool recon_nfc_get(ReconNfc* n, FuriString* title, FuriString* grade, FuriString* detail) {
    furi_mutex_acquire(n->lock, FuriWaitForever);
    bool detected = n->detected;
    NfcProtocol best = NfcProtocolInvalid;
    int best_depth = -1;
    for(size_t i = 0; i < n->proto_num; i++) {
        int d = proto_depth(n->protos[i]);
        if(d > best_depth) {
            best_depth = d;
            best = n->protos[i];
        }
    }
    ReconMfcResult mfc = n->mfc;
    furi_mutex_release(n->lock);

    if(!detected || best == NfcProtocolInvalid) {
        return false;
    }

    const char* g = "INFO";
    const char* d = "";
    char mfc_detail[160];
    if(best == NfcProtocolMfClassic && mfc.valid) {
        // A deep check ran: replace the static notes with a concrete verdict.
        recon_nfc_grade_mfc(&mfc, &g, mfc_detail, sizeof(mfc_detail));
        d = mfc_detail;
    } else {
        recon_nfc_grade(best, &g, &d);
    }

    if(title) furi_string_set(title, nfc_device_get_protocol_name(best));
    if(grade) furi_string_set(grade, g);
    if(detail) furi_string_set(detail, d);
    return true;
}

/* Try one key (A then B) against every not-yet-cracked sector; record found
 * keys so a later key in the dictionary doesn't re-test an open sector. Returns
 * false (and sets *removed) if the card left the field. */
static bool recon_nfc_try_key(
    Nfc* nfc,
    const MfClassicKey* key,
    uint8_t total_sectors,
    MfClassicKey* found_a,
    MfClassicKey* found_b,
    bool* cracked_a,
    bool* cracked_b,
    ReconMfcResult* r,
    bool* removed) {
    for(uint8_t s = 0; s < total_sectors; s++) {
        if(cracked_a[s] && cracked_b[s]) continue;
        uint8_t block = mf_classic_get_sector_trailer_num_by_sector(s);
        bool sector_was_open = cracked_a[s] || cracked_b[s];

        for(int kt = 0; kt < 2; kt++) {
            MfClassicKeyType key_type = (kt == 0) ? MfClassicKeyTypeA : MfClassicKeyTypeB;
            if(kt == 0 && cracked_a[s]) continue;
            if(kt == 1 && cracked_b[s]) continue;

            MfClassicAuthContext ctx;
            MfClassicError err =
                mf_classic_poller_sync_auth(nfc, block, (MfClassicKey*)key, key_type, &ctx);
            if(err == MfClassicErrorNotPresent) {
                *removed = true;
                return false;
            }
            if(err != MfClassicErrorNone) continue;

            if(kt == 0) {
                cracked_a[s] = true;
                memcpy(found_a[s].data, key->data, MF_CLASSIC_KEY_SIZE);
            } else {
                cracked_b[s] = true;
                memcpy(found_b[s].data, key->data, MF_CLASSIC_KEY_SIZE);
            }
            r->recovered_keys++;
        }

        // First time this sector cracked with any default key.
        if(!sector_was_open && (cracked_a[s] || cracked_b[s])) {
            r->default_keyed++;
        }
    }
    return true;
}

static int32_t recon_nfc_probe_worker(void* ctx) {
    ReconNfc* n = ctx;

    // Take exclusive control of the radio for the duration of the probe.
    if(n->scanner) nfc_scanner_stop(n->scanner);

    ReconMfcResult res;
    memset(&res, 0, sizeof(res));

    // UID via the ISO14443-3A layer (MIFARE Classic is 14443A underneath).
    Iso14443_3aData* iso = iso14443_3a_alloc();
    if(iso14443_3a_poller_sync_read(n->nfc, iso) == Iso14443_3aErrorNone) {
        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(iso, &uid_len);
        if(uid && uid_len <= sizeof(res.uid)) {
            memcpy(res.uid, uid, uid_len);
            res.uid_len = (uint8_t)uid_len;
        }
    }
    iso14443_3a_free(iso);

    MfClassicType type = MfClassicType1k;
    MfClassicError derr = mf_classic_poller_sync_detect_type(n->nfc, &type);
    if(derr == MfClassicErrorNotPresent) {
        res.aborted = true;
    }
    res.type = type;
    res.total_sectors = mf_classic_get_total_sectors_num(type);
    if(res.total_sectors > MF_CLASSIC_TOTAL_SECTORS_MAX) {
        res.total_sectors = MF_CLASSIC_TOTAL_SECTORS_MAX;
    }

    // Per-sector found keys + cracked flags (heap to keep the worker stack small).
    MfClassicKey* found_a = malloc(sizeof(MfClassicKey) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    MfClassicKey* found_b = malloc(sizeof(MfClassicKey) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    bool* cracked_a = malloc(sizeof(bool) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    bool* cracked_b = malloc(sizeof(bool) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    if(!found_a || !found_b || !cracked_a || !cracked_b) {
        // Heap too tight for the deep check -- bail cleanly (free(NULL) is a no-op)
        // instead of dereferencing a NULL in the memsets below. Publish an aborted
        // result and resume passive scanning, mirroring the normal worker tail.
        free(found_a);
        free(found_b);
        free(cracked_a);
        free(cracked_b);
        res.aborted = true;
        res.valid = true;
        furi_mutex_acquire(n->lock, FuriWaitForever);
        n->mfc = res;
        n->probe_dirty = true;
        furi_mutex_release(n->lock);
        if(n->scanner) nfc_scanner_start(n->scanner, recon_nfc_scanner_cb, n);
        n->probing = false;
        return 0;
    }
    memset(found_a, 0, sizeof(MfClassicKey) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    memset(found_b, 0, sizeof(MfClassicKey) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    memset(cracked_a, 0, sizeof(bool) * MF_CLASSIC_TOTAL_SECTORS_MAX);
    memset(cracked_b, 0, sizeof(bool) * MF_CLASSIC_TOTAL_SECTORS_MAX);

    // Prefer the on-SD stock dictionaries; fall back to the hardcoded defaults.
    bool removed = res.aborted;
    KeysDict* dict = NULL;
    if(!removed) {
        const char* dict_path = NULL;
        if(keys_dict_check_presence(RECON_NFC_DICT_SYSTEM)) {
            dict_path = RECON_NFC_DICT_SYSTEM;
        } else if(keys_dict_check_presence(RECON_NFC_DICT_USER)) {
            dict_path = RECON_NFC_DICT_USER;
        }
        if(dict_path) {
            dict = keys_dict_alloc(dict_path, KeysDictModeOpenExisting, MF_CLASSIC_KEY_SIZE);
        }
    }

    if(!removed && dict) {
        MfClassicKey key;
        while(keys_dict_get_next_key(dict, key.data, MF_CLASSIC_KEY_SIZE)) {
            if(!recon_nfc_try_key(
                   n->nfc,
                   &key,
                   res.total_sectors,
                   found_a,
                   found_b,
                   cracked_a,
                   cracked_b,
                   &res,
                   &removed)) {
                break;
            }
        }
    } else if(!removed) {
        size_t num = sizeof(recon_nfc_default_keys) / MF_CLASSIC_KEY_SIZE;
        for(size_t i = 0; i < num; i++) {
            MfClassicKey key;
            memcpy(key.data, recon_nfc_default_keys[i], MF_CLASSIC_KEY_SIZE);
            if(!recon_nfc_try_key(
                   n->nfc,
                   &key,
                   res.total_sectors,
                   found_a,
                   found_b,
                   cracked_a,
                   cracked_b,
                   &res,
                   &removed)) {
                break;
            }
        }
    }

    if(dict) keys_dict_free(dict);
    free(found_a);
    free(found_b);
    free(cracked_a);
    free(cracked_b);

    res.aborted = removed;
    res.valid = true;

    furi_mutex_acquire(n->lock, FuriWaitForever);
    n->mfc = res;
    n->probe_dirty = true;
    furi_mutex_release(n->lock);

    // Resume passive scanning for the next card.
    if(n->scanner) nfc_scanner_start(n->scanner, recon_nfc_scanner_cb, n);

    n->probing = false;
    return 0;
}

void recon_nfc_deep_check_start(ReconNfc* n) {
    if(!n->running || n->probing) return;
    // Reap a previously completed worker (probing already cleared) so its handle
    // doesn't leak across repeated checks.
    if(n->probe_thread) {
        furi_thread_join(n->probe_thread);
        furi_thread_free(n->probe_thread);
        n->probe_thread = NULL;
    }
    n->probing = true;
    n->probe_dirty = true;
    n->probe_thread = furi_thread_alloc_ex("FlipDeFlockMfc", 2048, recon_nfc_probe_worker, n);
    furi_thread_start(n->probe_thread);
}

bool recon_nfc_deep_check_busy(ReconNfc* n) {
    return n->probing;
}

bool recon_nfc_deep_check_get(ReconNfc* n, ReconMfcResult* out) {
    furi_mutex_acquire(n->lock, FuriWaitForever);
    bool valid = n->mfc.valid;
    if(out) *out = n->mfc;
    n->probe_dirty = false;
    furi_mutex_release(n->lock);
    return valid;
}
