#include "dfc_der.h"

#include <string.h>

// Tag numbers, from the assignment tables in section 2.2.3. Under implicit
// tagging the identifier octet is 0x80 for a primitive component and 0xA0 for a
// constructed one, plus the tag number.

#if DFC_ENABLE_BINARY_CODEC

#define P(n) (uint8_t)(0x80u + (n))
#define C(n) (uint8_t)(0xA0u + (n))

#define TAG_CREDENTIAL 0x60u
#define TAG_SEQUENCE   0x30u
#define TAG_OCTETS     0x04u

// Credential
#define CRED_VERSION P(0)
#define CRED_CARD    C(1)
#define CRED_PICC    C(2)
#define CRED_APPS    C(3)

// Card
#define CARD_GENERATION P(0)
#define CARD_STORAGE    P(1)
#define CARD_UID        P(2)
#define CARD_PROVENANCE P(3)
#define CARD_SIGNATURE  P(4)

// Picc
#define PICC_KS1        P(0)
#define PICC_KS2        P(1)
#define PICC_AUTH       P(2)
#define PICC_RANDOM_ID  P(3)
#define PICC_FORMAT_DIS P(4)
#define PICC_ATS        P(5)
#define PICC_SAK        P(6)
#define PICC_ATQA       P(7)
#define PICC_SM_DISABLE P(8)
#define PICC_KEYS       C(9)
#define PICC_FILES      C(10)
#define PICC_EV2_CAPS   P(11)
#define PICC_PROXIMITY  C(12)
#define PICC_VCARD      C(13)
#define PICC_DAM        C(14)

// ProximityContents
#define PROX_KEY       P(0)
#define PROX_OPTION    P(1)
#define PROX_PUBLISHED P(2)
#define PROX_BITRATE   P(3)

// VirtualCardContents
#define VC_INSTALL_ID P(0)
#define VC_INFO       P(1)
#define VC_CAPS       P(2)
#define VC_UID        P(3)
#define VC_MAC_KEY    P(4)
#define VC_ENC_KEY    P(5)
#define VC_AUTH_REQ   P(6)
#define VC_PROX_REQ   P(7)

// DamContents
#define DAM_AUTH P(0)
#define DAM_MAC  P(1)
#define DAM_ENC  P(2)

// Application
#define APP_AID     P(0)
#define APP_ISO_FID P(1)
#define APP_DF_NAME P(2)
#define APP_KS1     P(3)
#define APP_KS2     P(4)
#define APP_AUTH    P(5)
#define APP_KEYS    C(6)
#define APP_FILES   C(7)
#define APP_KEY_SETS   C(8)
#define APP_CAPABILITY P(9)
#define APP_DELEGATED  C(10)

// KeySetsContents
#define KS_KEY_COUNT P(0)
#define KS_MAX_SIZE  P(1)
#define KS_SETTINGS  P(2)
#define KS_SETS      C(3)

// One key set inside KS_SETS
#define SET_NUMBER      P(0)
#define SET_VERSION     P(1)
#define SET_TYPE        P(2)
#define SET_INITIALIZED P(3)
#define SET_KEYS        C(4)

// DelegatedContents
#define DEL_SLOT    P(0)
#define DEL_VERSION P(1)
#define DEL_QUOTA   P(2)
#define DEL_FREE    P(3)

// Key
#define KEY_SLOT    P(0)
#define KEY_VALUE   P(1)
#define KEY_VERSION P(2)

// File
#define FILE_NUMBER  P(0)
#define FILE_TYPE    P(1)
#define FILE_COMM    P(2)
#define FILE_RIGHTS  P(3)
#define FILE_ISO_FID P(4)
#define FILE_DATA    C(5)
#define FILE_VALUE   C(6)
#define FILE_RECORD  C(7)
#define FILE_TMAC    C(8)
#define FILE_SDM     C(9)

// SdmContents
#define SDM_OPTIONS      P(0)
#define SDM_RIGHTS       P(1)
#define SDM_UID_OFF      P(2)
#define SDM_COUNTER_OFF  P(3)
#define SDM_PICC_OFF     P(4)
#define SDM_MAC_IN_OFF   P(5)
#define SDM_MAC_OFF      P(6)
#define SDM_ENC_OFF      P(7)
#define SDM_ENC_LEN      P(8)
#define SDM_LIMIT        P(9)
#define SDM_READ_COUNTER P(10)

// DataContents
#define DATA_SIZE     P(0)
#define DATA_KNOWN    P(1)
#define DATA_COMPLETE P(2)

// ValueContents
#define VAL_LOWER   P(0)
#define VAL_UPPER   P(1)
#define VAL_CURRENT P(2)
#define VAL_LIMITED P(3)

// RecordContents
#define REC_SIZE     P(0)
#define REC_MAX      P(1)
#define REC_CURRENT  P(2)
#define REC_COMPLETE P(3)
#define REC_RECORDS  C(4)

// TransactionMacContents
#define TMAC_COUNTER     P(0)
#define TMAC_VALUE       P(1)
#define TMAC_KEY_TYPE    P(2)
#define TMAC_KEY_VERSION P(3)
#define TMAC_KEY         P(4)
#define TMAC_READER_ID   P(5)

#define FILE_TYPE_STANDARD 0x00u
#define FILE_TYPE_BACKUP   0x01u
#define FILE_TYPE_VALUE    0x02u
#define FILE_TYPE_LINEAR   0x03u
#define FILE_TYPE_CYCLIC   0x04u
#define FILE_TYPE_TMAC     0x05u

#define U24_MAX 16777215u

const char* dfc_der_status_name(DfcDerStatus status) {
    switch(status) {
    case DfcDerOk:
        return "ok";
    case DfcDerMalformed:
        return "malformed";
    case DfcDerUnsupported:
        return "unsupported";
    case DfcDerCapacity:
        return "capacity";
    }
    return "unknown";
}

static bool file_is_data(uint8_t type) {
    return type == FILE_TYPE_STANDARD || type == FILE_TYPE_BACKUP;
}

static bool file_is_record(uint8_t type) {
    return type == FILE_TYPE_LINEAR || type == FILE_TYPE_CYCLIC;
}

// ------------------------------------------------------------------ writer ---

// Collects octets, or just counts them when `buf` is NULL. Sticky overflow, so
// one check at the end covers the whole encode.
typedef struct {
    uint8_t* buf;
    size_t cap;
    size_t len;
    bool overflow;
} Writer;

static void w_raw(Writer* w, const uint8_t* bytes, size_t n) {
    if(w->buf) {
        if(w->len + n > w->cap) {
            w->overflow = true;
            return;
        }
        memcpy(w->buf + w->len, bytes, n);
    } else if(w->len + n < w->len) {
        w->overflow = true;
        return;
    }
    w->len += n;
}

static void w_byte(Writer* w, uint8_t b) {
    w_raw(w, &b, 1);
}

static void w_len(Writer* w, size_t n) {
    if(n < 128) {
        w_byte(w, (uint8_t)n);
    } else if(n < 256) {
        w_byte(w, 0x81);
        w_byte(w, (uint8_t)n);
    } else if(n <= 0xFFFF) {
        w_byte(w, 0x82);
        w_byte(w, (uint8_t)(n >> 8));
        w_byte(w, (uint8_t)(n & 0xFF));
    } else {
        w->overflow = true;
    }
}

static void w_tlv(Writer* w, uint8_t tag, const uint8_t* body, size_t n) {
    w_byte(w, tag);
    w_len(w, n);
    w_raw(w, body, n);
}

// Minimal two's-complement INTEGER, per section 2.2.1.
static void w_int(Writer* w, uint8_t tag, int64_t v) {
    uint8_t tmp[8];
    size_t n = 0;
    // Emit big-endian, then trim redundant leading octets.
    for(int shift = 56; shift >= 0; shift -= 8) {
        tmp[n++] = (uint8_t)((uint64_t)v >> shift);
    }
    size_t first = 0;
    while(first + 1 < n) {
        bool redundant = (tmp[first] == 0x00 && (tmp[first + 1] & 0x80) == 0) ||
                         (tmp[first] == 0xFF && (tmp[first + 1] & 0x80) != 0);
        if(!redundant) break;
        first++;
    }
    w_tlv(w, tag, tmp + first, n - first);
}

static void w_bool(Writer* w, uint8_t tag, bool v) {
    uint8_t b = v ? 0xFF : 0x00;
    w_tlv(w, tag, &b, 1);
}

// A constructed component whose body is produced by a callback. Two passes: one
// to size the body, one to emit it, so lengths are exact without scratch space.
typedef void (*BodyFn)(Writer*, const void*);

static void w_constructed(Writer* w, uint8_t tag, BodyFn fn, const void* ctx) {
    Writer probe = {NULL, 0, 0, false};
    fn(&probe, ctx);
    if(probe.overflow) {
        w->overflow = true;
        return;
    }
    w_byte(w, tag);
    w_len(w, probe.len);
    fn(w, ctx);
}

typedef struct {
    const DfcCredential* c;
    const DfcApplication* app;
    const DfcFile* file;
    size_t owner;
} Ctx;

static void body_key(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcCredential* c = x->c;
    const DfcApplication* app = x->app;
    size_t slot = x->owner;
    const uint8_t* value = dfc_credential_key_const(c, app, slot);
    uint8_t version = app ? app->key_versions[slot] : c->picc_key_versions[slot];
    size_t key_len = app ? app->key_len : c->picc_key_len;
    // Section 1.4 fixes the stored length by key type; 8-octet DES is stored as
    // 16, which is what the model already holds.
    key_len = dfc_credential_stored_key_length(key_len);
    if(!value) {
        w->overflow = true;
        return;
    }
    w_int(w, KEY_SLOT, (int64_t)slot);
    w_tlv(w, KEY_VALUE, value, key_len);
    w_tlv(w, KEY_VERSION, &version, 1);
}

static void body_data_contents(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcFile* f = x->file;
    const uint8_t* known = dfc_file_data_const(x->c, f);
    w_int(w, DATA_SIZE, (int64_t)f->declared_size);
    if(known && f->data_len > 0) w_tlv(w, DATA_KNOWN, known, f->data_len);
    w_bool(w, DATA_COMPLETE, f->contents_complete);
}

static void body_value_contents(Writer* w, const void* p) {
    const DfcFile* f = ((const Ctx*)p)->file;
    w_int(w, VAL_LOWER, f->value_lower_limit);
    w_int(w, VAL_UPPER, f->value_upper_limit);
    w_int(w, VAL_CURRENT, f->value);
    w_tlv(w, VAL_LIMITED, &f->limited_credit, 1);
}

static void body_records(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcFile* f = x->file;
    const uint8_t* known = dfc_file_data_const(x->c, f);
    size_t count = f->record_size ? f->data_len / f->record_size : 0;
    for(size_t i = 0; i < count; i++) {
        w_tlv(w, TAG_OCTETS, known + i * f->record_size, f->record_size);
    }
}

static void body_record_contents(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcFile* f = x->file;
    w_int(w, REC_SIZE, (int64_t)f->record_size);
    w_int(w, REC_MAX, (int64_t)f->max_records);
    w_int(w, REC_CURRENT, (int64_t)f->record_count);
    w_bool(w, REC_COMPLETE, f->contents_complete);
    w_constructed(w, REC_RECORDS, body_records, x);
}

#if DFC_ENABLE_TRANSACTION_MAC
static void body_transaction_mac_contents(Writer* w, const void* p) {
    const DfcFile* f = ((const Ctx*)p)->file;
    w_int(w, TMAC_COUNTER, (int64_t)f->transaction_counter);
    w_tlv(w, TMAC_VALUE, f->transaction_mac, sizeof(f->transaction_mac));
    w_tlv(w, TMAC_KEY_TYPE, &f->transaction_mac_key_type, 1);
    w_tlv(w, TMAC_KEY_VERSION, &f->transaction_mac_key_version, 1);
    w_tlv(w, TMAC_KEY, f->transaction_mac_key, sizeof(f->transaction_mac_key));
    w_tlv(w, TMAC_READER_ID, f->previous_reader_id, sizeof(f->previous_reader_id));
}
#endif

#if DFC_ENABLE_SDM
static void body_sdm(Writer* w, const void* p) {
    const DfcFile* f = ((const Ctx*)p)->file;
    uint8_t rights[2] = {
        (uint8_t)(f->sdm_access_rights >> 8), (uint8_t)(f->sdm_access_rights & 0xFF)};
    w_tlv(w, SDM_OPTIONS, &f->sdm_options, 1);
    w_tlv(w, SDM_RIGHTS, rights, 2);
    if(f->sdm_has_uid_offset) w_int(w, SDM_UID_OFF, (int64_t)f->sdm_uid_offset);
    if(f->sdm_has_counter_offset) w_int(w, SDM_COUNTER_OFF, (int64_t)f->sdm_counter_offset);
    if(f->sdm_has_picc_data_offset) w_int(w, SDM_PICC_OFF, (int64_t)f->sdm_picc_data_offset);
    if(f->sdm_has_mac_input_offset) w_int(w, SDM_MAC_IN_OFF, (int64_t)f->sdm_mac_input_offset);
    if(f->sdm_has_mac_offset) w_int(w, SDM_MAC_OFF, (int64_t)f->sdm_mac_offset);
    if(f->sdm_has_encrypted_file_offset) {
        w_int(w, SDM_ENC_OFF, (int64_t)f->sdm_encrypted_file_offset);
        w_int(w, SDM_ENC_LEN, (int64_t)f->sdm_encrypted_file_length);
    }
    if(f->sdm_has_counter_limit) w_int(w, SDM_LIMIT, (int64_t)f->sdm_counter_limit);
    w_int(w, SDM_READ_COUNTER, (int64_t)f->sdm_read_counter);
}
#endif

static void body_file(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcFile* f = x->file;
    uint8_t rights[2] = {(uint8_t)(f->access_rights >> 8), (uint8_t)(f->access_rights & 0xFF)};
    w_int(w, FILE_NUMBER, f->number);
    w_int(w, FILE_TYPE, f->type);
    w_tlv(w, FILE_COMM, &f->comm_settings, 1);
    w_tlv(w, FILE_RIGHTS, rights, 2);
    if(f->has_iso_file_id) {
        uint8_t fid[2] = {(uint8_t)(f->iso_file_id >> 8), (uint8_t)(f->iso_file_id & 0xFF)};
        w_tlv(w, FILE_ISO_FID, fid, 2);
    }
    if(file_is_data(f->type)) {
        w_constructed(w, FILE_DATA, body_data_contents, x);
    } else if(f->type == FILE_TYPE_VALUE) {
        w_constructed(w, FILE_VALUE, body_value_contents, x);
    } else if(file_is_record(f->type)) {
        w_constructed(w, FILE_RECORD, body_record_contents, x);
#if DFC_ENABLE_TRANSACTION_MAC
    } else if(f->type == FILE_TYPE_TMAC) {
        w_constructed(w, FILE_TMAC, body_transaction_mac_contents, x);
#endif
    } else {
        // A type with no Contents alternative cannot be encoded.
        w->overflow = true;
    }
#if DFC_ENABLE_SDM
    if(f->sdm_enabled) w_constructed(w, FILE_SDM, body_sdm, x);
#endif
}

static void body_files(Writer* w, const void* p) {
    const Ctx* x = p;
    for(size_t i = 0; i < x->c->num_files; i++) {
        const DfcFile* f = &x->c->files[i];
        if(f->app_index != x->owner) continue;
        Ctx inner = {x->c, x->app, f, x->owner};
        w_constructed(w, TAG_SEQUENCE, body_file, &inner);
    }
}

static void body_keys(Writer* w, const void* p) {
    const Ctx* x = p;
    size_t n = x->app ? x->app->num_keys : x->c->picc_num_keys;
    for(size_t slot = 0; slot < n; slot++) {
        Ctx inner = {x->c, x->app, NULL, slot};
        w_constructed(w, TAG_SEQUENCE, body_key, &inner);
    }
}

static void body_card(Writer* w, const void* p) {
    const DfcCredential* c = ((const Ctx*)p)->c;
    w_int(w, CARD_GENERATION, (int64_t)c->card.generation);
    w_int(w, CARD_STORAGE, (int64_t)c->card.storage);
    w_tlv(w, CARD_UID, c->uid, c->uid_len);
    w_int(w, CARD_PROVENANCE, (int64_t)c->card.uid_provenance);
#if DFC_ENABLE_STATIC_SIGNATURE
    if(c->picc_has_static_signature) {
        w_tlv(w, CARD_SIGNATURE, c->picc_static_signature, sizeof(c->picc_static_signature));
    }
#endif
}

static uint8_t auth_mode_code(uint8_t auth_command) {
    if(auth_command == DFC_CMD_AUTHENTICATE_AES) return 2;
    if(auth_command == DFC_CMD_AUTHENTICATE_ISO) return 1;
    return 0;
}

#if DFC_ENABLE_PROXIMITY_CHECK
static void body_proximity(Writer* w, const void* p) {
    const DfcCredential* c = ((const Ctx*)p)->c;
    w_tlv(w, PROX_KEY, c->picc_proximity_key, sizeof(c->picc_proximity_key));
    w_tlv(w, PROX_OPTION, &c->picc_proximity_option, 1);
    w_int(w, PROX_PUBLISHED, (int64_t)c->picc_proximity_published_response_time);
    if(c->picc_has_proximity_bitrate) w_tlv(w, PROX_BITRATE, &c->picc_proximity_bitrate, 1);
}
#endif

#if DFC_ENABLE_VIRTUAL_CARD
static void body_virtual_card(Writer* w, const void* p) {
    const DfcCredential* c = ((const Ctx*)p)->c;
    w_tlv(w, VC_INSTALL_ID, c->virtual_card_installation_id, c->virtual_card_installation_id_len);
    w_tlv(w, VC_INFO, &c->virtual_card_information, 1);
    w_tlv(w, VC_CAPS, c->virtual_card_capabilities, sizeof(c->virtual_card_capabilities));
    w_tlv(w, VC_UID, c->virtual_card_uid, c->virtual_card_uid_len);
    w_tlv(w, VC_MAC_KEY, c->virtual_card_select_mac_key, sizeof(c->virtual_card_select_mac_key));
    w_tlv(
        w,
        VC_ENC_KEY,
        c->virtual_card_select_encryption_key,
        sizeof(c->virtual_card_select_encryption_key));
    // Both flags are required, so FALSE is encoded rather than omitted.
    w_bool(w, VC_AUTH_REQ, c->virtual_card_authentication_mandatory);
    w_bool(w, VC_PROX_REQ, c->virtual_card_proximity_mandatory);
}
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
static void body_dam(Writer* w, const void* p) {
    const DfcCredential* c = ((const Ctx*)p)->c;
    w_tlv(w, DAM_AUTH, c->picc_dam_auth_key, sizeof(c->picc_dam_auth_key));
    w_tlv(w, DAM_MAC, c->picc_dam_mac_key, sizeof(c->picc_dam_mac_key));
    w_tlv(w, DAM_ENC, c->picc_dam_encryption_key, sizeof(c->picc_dam_encryption_key));
}
#endif

static void body_picc(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcCredential* c = x->c;
    w_tlv(w, PICC_KS1, &c->picc_key_settings_1, 1);
    w_tlv(w, PICC_KS2, &c->picc_key_settings_2, 1);
    w_int(w, PICC_AUTH, auth_mode_code(c->picc_auth_command));
    // DEFAULT FALSE: emit only when true (section 2.2.4 rule 14).
    if(c->picc_random_id) w_bool(w, PICC_RANDOM_ID, true);
    if(c->picc_format_disabled) w_bool(w, PICC_FORMAT_DIS, true);
    if(c->picc_ats_len > 0) w_tlv(w, PICC_ATS, c->picc_ats, c->picc_ats_len);
    if(c->picc_has_sak) w_tlv(w, PICC_SAK, &c->picc_sak, 1);
    if(c->picc_has_atqa) w_tlv(w, PICC_ATQA, c->picc_atqa, 2);
    if(c->picc_has_sm_disable) w_tlv(w, PICC_SM_DISABLE, &c->picc_sm_disable, 1);
    Ctx keys = {c, NULL, NULL, 0};
    w_constructed(w, PICC_KEYS, body_keys, &keys);
    Ctx files = {c, NULL, NULL, DFC_FILE_OWNER_PICC};
    w_constructed(w, PICC_FILES, body_files, &files);
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(c->picc_has_ev2_capabilities) {
        w_tlv(w, PICC_EV2_CAPS, c->picc_ev2_capabilities, sizeof(c->picc_ev2_capabilities));
    }
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    if(c->picc_has_proximity_key) w_constructed(w, PICC_PROXIMITY, body_proximity, x);
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    if(c->virtual_card_configured) w_constructed(w, PICC_VCARD, body_virtual_card, x);
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    if(c->picc_has_dam_keys) w_constructed(w, PICC_DAM, body_dam, x);
#endif
}

#if DFC_ENABLE_KEY_SETS
// A key set and the slot inside it. The writer needs both indices at once, which
// the shared Ctx cannot carry.
typedef struct {
    const DfcCredential* c;
    const DfcApplication* app;
    size_t set;
    size_t slot;
} KeySetCtx;

static void body_key_set_key(Writer* w, const void* p) {
    const KeySetCtx* k = p;
    const uint8_t* value = dfc_credential_key_in_set_const(k->c, k->app, k->set, k->slot);
    if(!value) {
        w->overflow = true;
        return;
    }
    // Set zero keeps its versions in the ordinary list; later sets follow it.
    uint8_t version = k->set == 0 ? k->app->key_versions[k->slot] :
                                    k->app->additional_key_versions[k->set - 1][k->slot];
    w_int(w, KEY_SLOT, (int64_t)k->slot);
    w_tlv(w, KEY_VALUE, value, dfc_credential_stored_key_length(k->app->key_len));
    w_tlv(w, KEY_VERSION, &version, 1);
}

static void body_key_set_keys(Writer* w, const void* p) {
    const KeySetCtx* k = p;
    for(size_t slot = 0; slot < k->app->num_keys; slot++) {
        KeySetCtx inner = {k->c, k->app, k->set, slot};
        w_constructed(w, TAG_SEQUENCE, body_key_set_key, &inner);
    }
}

static void body_key_set(Writer* w, const void* p) {
    const KeySetCtx* k = p;
    const DfcApplication* a = k->app;
    w_int(w, SET_NUMBER, (int64_t)k->set);
    w_tlv(w, SET_VERSION, &a->key_set_versions[k->set], 1);
    w_int(w, SET_TYPE, (int64_t)a->key_set_types[k->set]);
    // Required, so FALSE is encoded rather than omitted.
    w_bool(w, SET_INITIALIZED, a->key_set_initialized[k->set]);
    w_constructed(w, SET_KEYS, body_key_set_keys, k);
}

static void body_key_set_list(Writer* w, const void* p) {
    const Ctx* x = p;
    for(size_t set = 0; set < x->app->num_key_sets; set++) {
        KeySetCtx inner = {x->c, x->app, set, 0};
        w_constructed(w, TAG_SEQUENCE, body_key_set, &inner);
    }
}

static void body_key_sets(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcApplication* a = x->app;
    w_int(w, KS_KEY_COUNT, (int64_t)a->num_keys);
    w_int(w, KS_MAX_SIZE, (int64_t)a->max_key_size);
    w_tlv(w, KS_SETTINGS, &a->key_set_settings, 1);
    w_constructed(w, KS_SETS, body_key_set_list, x);
}
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
static void body_delegated(Writer* w, const void* p) {
    const DfcApplication* a = ((const Ctx*)p)->app;
    w_int(w, DEL_SLOT, (int64_t)a->delegated_slot_number);
    w_tlv(w, DEL_VERSION, &a->delegated_slot_version, 1);
    w_int(w, DEL_QUOTA, (int64_t)a->delegated_quota_limit);
    w_int(w, DEL_FREE, (int64_t)a->delegated_free_blocks);
}
#endif

static void body_app(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcApplication* a = x->app;
    w_tlv(w, APP_AID, a->aid, 3);
    if(a->has_iso_file_id) {
        uint8_t fid[2] = {(uint8_t)(a->iso_file_id >> 8), (uint8_t)(a->iso_file_id & 0xFF)};
        w_tlv(w, APP_ISO_FID, fid, 2);
    }
    if(a->iso_aid_len > 0) w_tlv(w, APP_DF_NAME, a->iso_aid, a->iso_aid_len);
    w_tlv(w, APP_KS1, &a->key_settings_1, 1);
    w_tlv(w, APP_KS2, &a->key_settings_2, 1);
    w_int(w, APP_AUTH, auth_mode_code(a->auth_command));
#if DFC_ENABLE_KEY_SETS
    // Exactly one key storage alternative is encoded.
    if(a->num_key_sets < DFC_KEY_SET_MINIMUM_COUNT) w_constructed(w, APP_KEYS, body_keys, x);
#else
    w_constructed(w, APP_KEYS, body_keys, x);
#endif
    w_constructed(w, APP_FILES, body_files, x);
#if DFC_ENABLE_KEY_SETS
    if(a->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT) {
        w_constructed(w, APP_KEY_SETS, body_key_sets, x);
    }
#endif
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
    if(a->has_capability_data) {
        w_tlv(w, APP_CAPABILITY, a->capability_data, sizeof(a->capability_data));
    }
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    if(a->delegated) w_constructed(w, APP_DELEGATED, body_delegated, x);
#endif
}

static void body_apps(Writer* w, const void* p) {
    const Ctx* x = p;
    for(size_t i = 0; i < x->c->num_apps; i++) {
        Ctx inner = {x->c, &x->c->apps[i], NULL, i};
        w_constructed(w, TAG_SEQUENCE, body_app, &inner);
    }
}

static void body_credential(Writer* w, const void* p) {
    const Ctx* x = p;
    w_int(w, CRED_VERSION, DFC_FORMAT_VERSION);
    w_constructed(w, CRED_CARD, body_card, x);
    w_constructed(w, CRED_PICC, body_picc, x);
    w_constructed(w, CRED_APPS, body_apps, x);
}

// Reject a model that would encode into something a conforming decoder refuses,
// so an invalid credential cannot leave this process.
DfcDerStatus dfc_der_validate_model(const DfcCredential* c) {
    if(c->uid_len != DFC_DESFIRE_UID_SHORT_LEN && c->uid_len != DFC_DESFIRE_UID_LEN &&
       c->uid_len != DFC_DESFIRE_UID_LONG_LEN) {
        return DfcDerMalformed;
    }
    if(c->card.generation < DfcGenerationEv1 || c->card.generation > DfcGenerationEv3) {
        return DfcDerMalformed;
    }
    if(c->card.uid_provenance > DfcUidProvenanceUnknown) return DfcDerMalformed;
    // A key list may be empty: an absent entry is the factory default
    // and an absent key slot encodes a required but empty
    // SEQUENCE OF as its tag with length zero.
    if(c->picc_num_keys > DFC_MAX_KEYS) return DfcDerMalformed;
    if(c->picc_ats_len > DFC_PICC_ATS_MAX) return DfcDerMalformed;

    // Generation gating. A feature that a generation does not define is
    // malformed, which is distinct from a feature this build omits.
    bool has_ev2 = false;
    bool has_ev3 = false;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    has_ev2 = has_ev2 || c->picc_has_ev2_capabilities;
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    has_ev2 = has_ev2 || c->virtual_card_configured;
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    has_ev2 = has_ev2 || c->picc_has_dam_keys;
#endif
#if DFC_ENABLE_STATIC_SIGNATURE
    has_ev3 = has_ev3 || c->picc_has_static_signature;
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    has_ev3 = has_ev3 || c->picc_has_proximity_key;
#endif
    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* a = &c->apps[i];
        (void)a;
#if DFC_ENABLE_KEY_SETS
        has_ev2 = has_ev2 || a->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT;
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        has_ev2 = has_ev2 || a->delegated;
#endif
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
        has_ev3 = has_ev3 || a->has_capability_data;
#endif
    }
    for(size_t i = 0; i < c->num_files; i++) {
        const DfcFile* f = &c->files[i];
        has_ev2 = has_ev2 || f->type == FILE_TYPE_TMAC;
#if DFC_ENABLE_SDM
        has_ev3 = has_ev3 || f->sdm_enabled;
#endif
    }
    if(has_ev2 && c->card.generation < DfcGenerationEv2) return DfcDerMalformed;
    if(has_ev3 && c->card.generation < DfcGenerationEv3) return DfcDerMalformed;

    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* a = &c->apps[i];
        if(a->num_keys > DFC_MAX_KEYS) return DfcDerMalformed;
        if(a->iso_aid_len > 16) return DfcDerMalformed;
        for(size_t j = i + 1; j < c->num_apps; j++) {
            if(memcmp(a->aid, c->apps[j].aid, 3) == 0) return DfcDerMalformed;
        }
    }

    for(size_t i = 0; i < c->num_files; i++) {
        const DfcFile* f = &c->files[i];
        if(f->app_index != DFC_FILE_OWNER_PICC && f->app_index >= c->num_apps) {
            return DfcDerMalformed;
        }
        if(f->number > DFC_EV1_MAX_FILE_NUMBER) return DfcDerMalformed;
#if DFC_ENABLE_TRANSACTION_MAC
        if(f->type > FILE_TYPE_TMAC) return DfcDerUnsupported;
#else
        if(f->type == FILE_TYPE_TMAC || f->type > FILE_TYPE_CYCLIC) return DfcDerUnsupported;
#endif
        for(size_t j = i + 1; j < c->num_files; j++) {
            if(c->files[j].app_index == f->app_index && c->files[j].number == f->number) {
                return DfcDerMalformed;
            }
        }
        if(file_is_data(f->type)) {
            if(f->declared_size == 0 || f->declared_size > U24_MAX) return DfcDerMalformed;
            if(f->data_len > f->declared_size) return DfcDerMalformed;
            if(f->contents_complete && f->data_len != f->declared_size) return DfcDerMalformed;
        } else if(f->type == FILE_TYPE_VALUE) {
            if(f->value_lower_limit > f->value || f->value > f->value_upper_limit) {
                return DfcDerMalformed;
            }
        } else if(f->type == FILE_TYPE_TMAC) {
            // A transaction-MAC file is an EV2 feature (section 2.2.4 rule 13).
            if(c->card.generation < DfcGenerationEv2) return DfcDerMalformed;
        } else {
            if(f->record_size == 0 || f->record_size > U24_MAX) return DfcDerMalformed;
            if(f->max_records == 0 || f->max_records > U24_MAX) return DfcDerMalformed;
            if(f->record_count > f->max_records) return DfcDerMalformed;
            if(f->data_len % f->record_size != 0) return DfcDerMalformed;
            size_t stored = f->data_len / f->record_size;
            if(stored > f->record_count) return DfcDerMalformed;
            if(f->contents_complete && stored != f->record_count) return DfcDerMalformed;
            if(f->type == FILE_TYPE_CYCLIC && f->max_records < 2) return DfcDerMalformed;
        }
        if(f->has_iso_file_id) {
            uint16_t v = f->iso_file_id;
            if(v == 0x0000 || v == 0x3F00 || v == 0x3FFF || v == 0xFFFF) return DfcDerMalformed;
        }
#if DFC_ENABLE_SDM
        if(f->sdm_enabled) {
            if(f->type != FILE_TYPE_STANDARD) return DfcDerMalformed;
            // Every offset addresses a position inside the file.
            const uint32_t size = f->declared_size;
            if(f->sdm_has_uid_offset && f->sdm_uid_offset >= size) return DfcDerMalformed;
            if(f->sdm_has_counter_offset && f->sdm_counter_offset >= size) return DfcDerMalformed;
            if(f->sdm_has_picc_data_offset && f->sdm_picc_data_offset >= size) {
                return DfcDerMalformed;
            }
            if(f->sdm_has_mac_input_offset && f->sdm_mac_input_offset >= size) {
                return DfcDerMalformed;
            }
            if(f->sdm_has_mac_offset && f->sdm_mac_offset >= size) return DfcDerMalformed;
            if(f->sdm_has_encrypted_file_offset) {
                if(f->sdm_encrypted_file_offset >= size) return DfcDerMalformed;
                if(f->sdm_encrypted_file_offset + f->sdm_encrypted_file_length > size) {
                    return DfcDerMalformed;
                }
            }
        }
#endif
    }
    return DfcDerOk;
}

DfcDerStatus dfc_der_encoded_size(const DfcCredential* credential, size_t* len) {
    if(!credential || !len) return DfcDerMalformed;
    DfcDerStatus st = dfc_der_validate_model(credential);
    if(st != DfcDerOk) return st;
    Ctx ctx = {credential, NULL, NULL, 0};
    Writer probe = {NULL, 0, 0, false};
    Writer outer = {NULL, 0, 0, false};
    body_credential(&probe, &ctx);
    if(probe.overflow) return DfcDerMalformed;
    w_byte(&outer, TAG_CREDENTIAL);
    w_len(&outer, probe.len);
    if(outer.overflow) return DfcDerCapacity;
    size_t total = outer.len + probe.len;
    if(total > DFC_DER_MAX_SIZE) return DfcDerMalformed;
    *len = total;
    return DfcDerOk;
}

DfcDerStatus
    dfc_der_encode(const DfcCredential* credential, uint8_t* out, size_t cap, size_t* len) {
    if(!credential || !out || !len) return DfcDerMalformed;
    size_t needed = 0;
    DfcDerStatus st = dfc_der_encoded_size(credential, &needed);
    if(st != DfcDerOk) return st;
    if(needed > cap) return DfcDerCapacity;

    Ctx ctx = {credential, NULL, NULL, 0};
    Writer w = {out, cap, 0, false};
    w_constructed(&w, TAG_CREDENTIAL, body_credential, &ctx);
    if(w.overflow) return DfcDerCapacity;
    *len = w.len;
    return DfcDerOk;
}

// ------------------------------------------------------------------ reader ---

typedef struct {
    const uint8_t* p;
    size_t len;
} Slice;

typedef struct {
    uint8_t tag;
    Slice body;
} Tlv;

// Read one TLV from `s`, advancing it. Enforces the section 2.2.1 subset.
static bool r_tlv(Slice* s, Tlv* out) {
    if(s->len < 2) return false;
    uint8_t tag = s->p[0];
    if((tag & 0x1F) == 0x1F) return false; // high-tag-number form
    size_t hdr = 2;
    size_t n = s->p[1];
    if(n & 0x80) {
        size_t count = n & 0x7F;
        if(count == 0 || count > 2) return false;
        if(s->len < 2 + count) return false;
        n = 0;
        for(size_t i = 0; i < count; i++) n = (n << 8) | s->p[2 + i];
        hdr = 2 + count;
        if(n < 128) return false; // non-minimal long form
        if(count == 2 && n < 256) return false;
    }
    if(s->len < hdr + n) return false;
    out->tag = tag;
    out->body.p = s->p + hdr;
    out->body.len = n;
    s->p += hdr + n;
    s->len -= hdr + n;
    return true;
}

// Components of one SEQUENCE, keyed by identifier octet, with declaration order
// enforced. `order` lists the identifiers in the order section 2.2.2 declares
// them; anything else, or out of order, is malformed.
#define MAX_COMPONENTS 16

typedef struct {
    uint8_t tags[MAX_COMPONENTS];
    Slice bodies[MAX_COMPONENTS];
    bool present[MAX_COMPONENTS];
    size_t count;
} Fields;

static bool
    r_fields(Slice body, const uint8_t* order, size_t order_len, Fields* out) {
    memset(out, 0, sizeof(*out));
    if(order_len > MAX_COMPONENTS) return false;
    out->count = order_len;
    for(size_t i = 0; i < order_len; i++) out->tags[i] = order[i];

    size_t cursor = 0;
    Tlv tlv;
    while(body.len > 0) {
        if(!r_tlv(&body, &tlv)) return false;
        size_t idx = order_len;
        for(size_t i = 0; i < order_len; i++) {
            if(order[i] == tlv.tag) {
                idx = i;
                break;
            }
        }
        if(idx == order_len) return false; // unknown tag: no extensions in v3
        if(out->present[idx]) return false; // duplicate
        if(idx < cursor) return false; // out of declaration order
        cursor = idx + 1;
        out->present[idx] = true;
        out->bodies[idx] = tlv.body;
    }
    return true;
}

static const Slice* f_get(const Fields* f, uint8_t tag) {
    for(size_t i = 0; i < f->count; i++) {
        if(f->tags[i] == tag && f->present[i]) return &f->bodies[i];
    }
    return NULL;
}

static bool r_int(const Slice* s, int64_t* out) {
    if(!s || s->len == 0 || s->len > 8) return false;
    if(s->len > 1) {
        bool redundant = (s->p[0] == 0x00 && (s->p[1] & 0x80) == 0) ||
                         (s->p[0] == 0xFF && (s->p[1] & 0x80) != 0);
        if(redundant) return false;
    }
    // Accumulate in an unsigned value, so shifting a negative integer's sign
    // extension is never undefined, and convert once at the end. The sign
    // extension fills the octets above the encoding with ones, so the two's
    // complement conversion reproduces the encoded value exactly.
    uint64_t v = (s->p[0] & 0x80) ? UINT64_MAX : 0;
    for(size_t i = 0; i < s->len; i++) v = (v << 8) | s->p[i];
    *out = (int64_t)v;
    return true;
}

static bool r_uint(const Slice* s, uint64_t max, uint64_t* out) {
    int64_t v;
    if(!r_int(s, &v)) return false;
    if(v < 0 || (uint64_t)v > max) return false;
    *out = (uint64_t)v;
    return true;
}

static bool r_bool(const Slice* s, bool* out) {
    if(!s || s->len != 1) return false;
    if(s->p[0] != 0x00 && s->p[0] != 0xFF) return false;
    *out = s->p[0] == 0xFF;
    return true;
}

// A DEFAULT FALSE boolean: absent means false, and an explicit false is invalid
// because DER omits a component equal to its default.
static bool r_default_false(const Fields* f, uint8_t tag, bool* out) {
    const Slice* s = f_get(f, tag);
    if(!s) {
        *out = false;
        return true;
    }
    bool v;
    if(!r_bool(s, &v)) return false;
    if(!v) return false;
    *out = true;
    return true;
}

static bool r_octets(const Slice* s, size_t want, uint8_t* out) {
    if(!s || s->len != want) return false;
    memcpy(out, s->p, want);
    return true;
}

static uint8_t auth_command_for(uint64_t code) {
    if(code == 2) return DFC_CMD_AUTHENTICATE_AES;
    if(code == 1) return DFC_CMD_AUTHENTICATE_ISO;
    return DFC_CMD_AUTHENTICATE_LEGACY;
}

// `app` selects an application, NULL the PICC record. The pool slice has to be
// reserved before any key is copied into it, so the sequences are counted first
// and only then decoded.
#if DFC_ENABLE_SDM
// Reads one optional SDM offset. Returns false when the value is malformed.
static bool sdm_offset(const Fields* f, uint8_t tag, bool* present, uint32_t* out) {
    const Slice* found = f_get(f, tag);
    if(!found) {
        *present = false;
        return true;
    }
    uint64_t value;
    if(!r_uint(found, U24_MAX, &value)) return false;
    *present = true;
    *out = (uint32_t)value;
    return true;
}

static DfcDerStatus decode_sdm(DfcFile* file, Slice body) {
    static const uint8_t order[] = {
        SDM_OPTIONS,
        SDM_RIGHTS,
        SDM_UID_OFF,
        SDM_COUNTER_OFF,
        SDM_PICC_OFF,
        SDM_MAC_IN_OFF,
        SDM_MAC_OFF,
        SDM_ENC_OFF,
        SDM_ENC_LEN,
        SDM_LIMIT,
        SDM_READ_COUNTER};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, SDM_OPTIONS), 1, &file->sdm_options)) return DfcDerMalformed;
    uint8_t rights[2];
    if(!r_octets(f_get(&f, SDM_RIGHTS), 2, rights)) return DfcDerMalformed;
    file->sdm_access_rights = (uint16_t)((rights[0] << 8) | rights[1]);
    if(!sdm_offset(&f, SDM_UID_OFF, &file->sdm_has_uid_offset, &file->sdm_uid_offset) ||
       !sdm_offset(&f, SDM_COUNTER_OFF, &file->sdm_has_counter_offset, &file->sdm_counter_offset) ||
       !sdm_offset(&f, SDM_PICC_OFF, &file->sdm_has_picc_data_offset, &file->sdm_picc_data_offset) ||
       !sdm_offset(
           &f, SDM_MAC_IN_OFF, &file->sdm_has_mac_input_offset, &file->sdm_mac_input_offset) ||
       !sdm_offset(&f, SDM_MAC_OFF, &file->sdm_has_mac_offset, &file->sdm_mac_offset) ||
       !sdm_offset(
           &f,
           SDM_ENC_OFF,
           &file->sdm_has_encrypted_file_offset,
           &file->sdm_encrypted_file_offset)) {
        return DfcDerMalformed;
    }
    const Slice* enc_len = f_get(&f, SDM_ENC_LEN);
    // The encrypted offset and its length occur together.
    if((enc_len != NULL) != file->sdm_has_encrypted_file_offset) return DfcDerMalformed;
    if(enc_len) {
        uint64_t value;
        if(!r_uint(enc_len, U24_MAX, &value)) return DfcDerMalformed;
        file->sdm_encrypted_file_length = (uint32_t)value;
    }
    if(!sdm_offset(&f, SDM_LIMIT, &file->sdm_has_counter_limit, &file->sdm_counter_limit)) {
        return DfcDerMalformed;
    }
    uint64_t counter;
    if(!r_uint(f_get(&f, SDM_READ_COUNTER), U24_MAX, &counter)) return DfcDerMalformed;
    file->sdm_read_counter = (uint32_t)counter;
    file->sdm_enabled = true;
    return DfcDerOk;
}
#endif

#if DFC_ENABLE_KEY_SETS
// Returns the stored key length that a key-set type requires.
static size_t key_set_type_length(uint8_t type) {
    return type == DFC_KEY_SET_TYPE_3K3DES ? 24 : 16;
}

static DfcDerStatus decode_key_sets(DfcCredential* c, DfcApplication* app, Slice body) {
    static const uint8_t order[] = {KS_KEY_COUNT, KS_MAX_SIZE, KS_SETTINGS, KS_SETS};
    static const uint8_t sorder[] = {
        SET_NUMBER, SET_VERSION, SET_TYPE, SET_INITIALIZED, SET_KEYS};
    static const uint8_t korder[] = {KEY_SLOT, KEY_VALUE, KEY_VERSION};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;
    uint64_t key_count, max_size;
    if(!r_uint(f_get(&f, KS_KEY_COUNT), DFC_MAX_KEYS, &key_count) || key_count == 0) {
        return DfcDerMalformed;
    }
    if(!r_uint(f_get(&f, KS_MAX_SIZE), 24, &max_size)) return DfcDerMalformed;
    if(max_size != 16 && max_size != 24) return DfcDerMalformed;
    uint8_t settings;
    if(!r_octets(f_get(&f, KS_SETTINGS), 1, &settings)) return DfcDerMalformed;
    const Slice* sets = f_get(&f, KS_SETS);
    if(!sets) return DfcDerMalformed;

    Slice probe = *sets;
    Tlv skip;
    size_t count = 0;
    while(probe.len > 0) {
        if(!r_tlv(&probe, &skip)) return DfcDerMalformed;
        if(skip.tag != TAG_SEQUENCE) return DfcDerMalformed;
        count++;
    }
    if(count < DFC_KEY_SET_MINIMUM_COUNT) return DfcDerMalformed;
    if(count > DFC_MAX_KEY_SETS) return DfcDerCapacity;
    if(!dfc_credential_key_sets_resize(c, app, count, key_count, app->key_len, max_size)) {
        return DfcDerCapacity;
    }
    app->key_set_settings = settings;

    Slice walk = *sets;
    size_t index = 0;
    Tlv item;
    while(walk.len > 0) {
        if(!r_tlv(&walk, &item)) return DfcDerMalformed;
        Fields sf;
        if(!r_fields(item.body, sorder, sizeof(sorder), &sf)) return DfcDerMalformed;
        uint64_t number, type;
        if(!r_uint(f_get(&sf, SET_NUMBER), DFC_MAX_KEY_SETS - 1, &number)) return DfcDerMalformed;
        if(number != index) return DfcDerMalformed; // contiguous from zero
        if(!r_uint(f_get(&sf, SET_TYPE), 0xFF, &type)) return DfcDerMalformed;
        if(type > DFC_KEY_SET_TYPE_AES) return DfcDerMalformed;
        size_t required = key_set_type_length((uint8_t)type);
        if(required > max_size) return DfcDerMalformed;
        if(!r_octets(f_get(&sf, SET_VERSION), 1, &app->key_set_versions[index])) {
            return DfcDerMalformed;
        }
        app->key_set_types[index] = (uint8_t)type;
        // Required, so an absent flag is malformed.
        if(!r_bool(f_get(&sf, SET_INITIALIZED), &app->key_set_initialized[index])) {
            return DfcDerMalformed;
        }
        const Slice* keys = f_get(&sf, SET_KEYS);
        if(!keys) return DfcDerMalformed;
        Slice kwalk = *keys;
        size_t slot = 0;
        Tlv kitem;
        while(kwalk.len > 0) {
            if(!r_tlv(&kwalk, &kitem)) return DfcDerMalformed;
            if(kitem.tag != TAG_SEQUENCE) return DfcDerMalformed;
            if(slot >= key_count) return DfcDerMalformed;
            Fields kf;
            if(!r_fields(kitem.body, korder, sizeof(korder), &kf)) return DfcDerMalformed;
            uint64_t number_slot;
            if(!r_uint(f_get(&kf, KEY_SLOT), DFC_MAX_KEYS - 1, &number_slot)) {
                return DfcDerMalformed;
            }
            if(number_slot != slot) return DfcDerMalformed;
            const Slice* value = f_get(&kf, KEY_VALUE);
            if(!value || value->len != required) return DfcDerMalformed;
            uint8_t* dst = dfc_credential_key_in_set(c, app, index, slot);
            uint8_t* version = dfc_credential_key_version_in_set(app, index, slot);
            if(!dst || !version) return DfcDerCapacity;
            memcpy(dst, value->p, value->len);
            if(!r_octets(f_get(&kf, KEY_VERSION), 1, version)) return DfcDerMalformed;
            slot++;
        }
        index++;
    }
    if(!app->key_set_initialized[0]) return DfcDerMalformed;
    return DfcDerOk;
}
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
static DfcDerStatus decode_delegated(DfcApplication* app, Slice body) {
    static const uint8_t order[] = {DEL_SLOT, DEL_VERSION, DEL_QUOTA, DEL_FREE};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;
    uint64_t slot, quota, free_blocks;
    if(!r_uint(f_get(&f, DEL_SLOT), 0xFFFFu, &slot)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, DEL_VERSION), 1, &app->delegated_slot_version)) return DfcDerMalformed;
    if(!r_uint(f_get(&f, DEL_QUOTA), 0xFFFFu, &quota)) return DfcDerMalformed;
    if(!r_uint(f_get(&f, DEL_FREE), 0xFFFFu, &free_blocks)) return DfcDerMalformed;
    if(free_blocks > quota) return DfcDerMalformed;
    app->delegated_slot_number = (uint16_t)slot;
    app->delegated_quota_limit = (uint16_t)quota;
    app->delegated_free_blocks = (uint16_t)free_blocks;
    app->delegated = true;
    return DfcDerOk;
}
#endif

static DfcDerStatus decode_proximity(DfcCredential* c, Slice body) {
#if DFC_ENABLE_PROXIMITY_CHECK
    static const uint8_t order[] = {PROX_KEY, PROX_OPTION, PROX_PUBLISHED, PROX_BITRATE};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, PROX_KEY), sizeof(c->picc_proximity_key), c->picc_proximity_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&f, PROX_OPTION), 1, &c->picc_proximity_option)) return DfcDerMalformed;
    uint64_t published;
    if(!r_uint(f_get(&f, PROX_PUBLISHED), 0xFFFFu, &published)) return DfcDerMalformed;
    c->picc_proximity_published_response_time = (uint16_t)published;
    const Slice* bitrate = f_get(&f, PROX_BITRATE);
    if(bitrate) {
        if(!r_octets(bitrate, 1, &c->picc_proximity_bitrate)) return DfcDerMalformed;
        c->picc_has_proximity_bitrate = true;
    }
    c->picc_has_proximity_key = true;
    return DfcDerOk;
#else
    (void)c;
    (void)body;
    // The field is recognized but the build omits the feature.
    return DfcDerUnsupported;
#endif
}

static DfcDerStatus decode_virtual_card(DfcCredential* c, Slice body) {
#if DFC_ENABLE_VIRTUAL_CARD
    static const uint8_t order[] = {
        VC_INSTALL_ID, VC_INFO, VC_CAPS, VC_UID, VC_MAC_KEY, VC_ENC_KEY, VC_AUTH_REQ, VC_PROX_REQ};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;
    const Slice* install = f_get(&f, VC_INSTALL_ID);
    if(!install || install->len == 0) return DfcDerMalformed;
    if(install->len > DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH) return DfcDerMalformed;
    memcpy(c->virtual_card_installation_id, install->p, install->len);
    c->virtual_card_installation_id_len = install->len;
    if(!r_octets(f_get(&f, VC_INFO), 1, &c->virtual_card_information)) return DfcDerMalformed;
    if(!r_octets(
           f_get(&f, VC_CAPS), sizeof(c->virtual_card_capabilities), c->virtual_card_capabilities)) {
        return DfcDerMalformed;
    }
    const Slice* uid = f_get(&f, VC_UID);
    if(!uid) return DfcDerMalformed;
    if(uid->len != 4 && uid->len != 7 && uid->len != 10) return DfcDerMalformed;
    memcpy(c->virtual_card_uid, uid->p, uid->len);
    c->virtual_card_uid_len = uid->len;
    if(!r_octets(
           f_get(&f, VC_MAC_KEY),
           sizeof(c->virtual_card_select_mac_key),
           c->virtual_card_select_mac_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(
           f_get(&f, VC_ENC_KEY),
           sizeof(c->virtual_card_select_encryption_key),
           c->virtual_card_select_encryption_key)) {
        return DfcDerMalformed;
    }
    // Both flags are required, so an absent one is malformed.
    if(!r_bool(f_get(&f, VC_AUTH_REQ), &c->virtual_card_authentication_mandatory)) {
        return DfcDerMalformed;
    }
    if(!r_bool(f_get(&f, VC_PROX_REQ), &c->virtual_card_proximity_mandatory)) {
        return DfcDerMalformed;
    }
    c->virtual_card_configured = true;
    return DfcDerOk;
#else
    (void)c;
    (void)body;
    return DfcDerUnsupported;
#endif
}

static DfcDerStatus decode_dam(DfcCredential* c, Slice body) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    static const uint8_t order[] = {DAM_AUTH, DAM_MAC, DAM_ENC};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, DAM_AUTH), sizeof(c->picc_dam_auth_key), c->picc_dam_auth_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&f, DAM_MAC), sizeof(c->picc_dam_mac_key), c->picc_dam_mac_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&f, DAM_ENC), sizeof(c->picc_dam_encryption_key), c->picc_dam_encryption_key)) {
        return DfcDerMalformed;
    }
    c->picc_has_dam_keys = true;
    return DfcDerOk;
#else
    (void)c;
    (void)body;
    return DfcDerUnsupported;
#endif
}

static DfcDerStatus decode_keys(
    DfcCredential* credential,
    DfcApplication* app,
    Slice body,
    size_t key_len,
    size_t max_keys) {
    static const uint8_t order[] = {KEY_SLOT, KEY_VALUE, KEY_VERSION};
    uint8_t* versions = app ? app->key_versions : credential->picc_key_versions;

    size_t count = 0;
    Slice probe = body;
    Tlv skip;
    while(probe.len > 0) {
        if(!r_tlv(&probe, &skip)) return DfcDerMalformed;
        if(skip.tag != TAG_SEQUENCE) return DfcDerMalformed;
        count++;
    }
    // An empty list is well formed: the slot then carries the factory default.
    if(count > max_keys) return DfcDerCapacity;
    if(count == 0) {
        // A cleared credential is seeded with a master key, so an empty list has
        // to take it away again rather than leave it standing.
        dfc_credential_keys_release(credential, app);
        return DfcDerOk;
    }
    if(!dfc_credential_keys_resize(credential, app, count, key_len)) return DfcDerCapacity;

    size_t n = 0;
    Tlv tlv;
    while(body.len > 0) {
        if(!r_tlv(&body, &tlv)) return DfcDerMalformed;
        if(tlv.tag != TAG_SEQUENCE) return DfcDerMalformed;
        Fields f;
        if(!r_fields(tlv.body, order, sizeof(order), &f)) return DfcDerMalformed;
        uint64_t slot;
        if(!r_uint(f_get(&f, KEY_SLOT), 13, &slot)) return DfcDerMalformed;
        if(slot != n) return DfcDerMalformed; // ascending, contiguous from 0
        const Slice* value = f_get(&f, KEY_VALUE);
        if(!value || value->len != key_len) return DfcDerMalformed;
        uint8_t* dst = dfc_credential_key(credential, app, n);
        if(!dst) return DfcDerCapacity;
        memcpy(dst, value->p, value->len);
        if(!r_octets(f_get(&f, KEY_VERSION), 1, &versions[n])) return DfcDerMalformed;
        n++;
    }
    return DfcDerOk;
}

static DfcDerStatus
    decode_file(DfcCredential* c, size_t owner, Slice body) {
    static const uint8_t order[] = {
        FILE_NUMBER,
        FILE_TYPE,
        FILE_COMM,
        FILE_RIGHTS,
        FILE_ISO_FID,
        FILE_DATA,
        FILE_VALUE,
        FILE_RECORD,
        FILE_TMAC,
        FILE_SDM};
    Fields f;
    if(!r_fields(body, order, sizeof(order), &f)) return DfcDerMalformed;

    uint64_t number, type;
    if(!r_uint(f_get(&f, FILE_NUMBER), DFC_EV1_MAX_FILE_NUMBER, &number)) return DfcDerMalformed;
    if(!r_uint(f_get(&f, FILE_TYPE), 0xFF, &type)) return DfcDerMalformed;
    if(type > FILE_TYPE_TMAC) return DfcDerMalformed;

    if(c->num_files >= DFC_MAX_FILES) return DfcDerCapacity;
    for(size_t i = 0; i < c->num_files; i++) {
        if(c->files[i].app_index == owner && c->files[i].number == (uint8_t)number) {
            return DfcDerMalformed;
        }
    }

    DfcFile* file = &c->files[c->num_files];
    memset(file, 0, sizeof(*file));
    file->app_index = owner;
    file->number = (uint8_t)number;
    file->type = (uint8_t)type;
    file->data_offset = DFC_FILE_POOL_NONE;

    if(!r_octets(f_get(&f, FILE_COMM), 1, &file->comm_settings)) return DfcDerMalformed;
    uint8_t rights[2];
    if(!r_octets(f_get(&f, FILE_RIGHTS), 2, rights)) return DfcDerMalformed;
    file->access_rights = (uint16_t)((rights[0] << 8) | rights[1]);

    const Slice* fid = f_get(&f, FILE_ISO_FID);
    if(fid) {
        uint8_t raw[2];
        if(!r_octets(fid, 2, raw)) return DfcDerMalformed;
        uint16_t v = (uint16_t)((raw[0] << 8) | raw[1]);
        if(v == 0x0000 || v == 0x3F00 || v == 0x3FFF || v == 0xFFFF) return DfcDerMalformed;
        file->has_iso_file_id = true;
        file->iso_file_id = v;
    }

    const Slice* data = f_get(&f, FILE_DATA);
    const Slice* value = f_get(&f, FILE_VALUE);
    const Slice* record = f_get(&f, FILE_RECORD);
    const Slice* transaction_mac = f_get(&f, FILE_TMAC);
    size_t alternatives =
        (data ? 1 : 0) + (value ? 1 : 0) + (record ? 1 : 0) + (transaction_mac ? 1 : 0);
    if(alternatives != 1) return DfcDerMalformed;

    // The type selects the alternative (section 2.2.4 rule 5).
    if(file_is_data(file->type)) {
        if(!data) return DfcDerMalformed;
        static const uint8_t dorder[] = {DATA_SIZE, DATA_KNOWN, DATA_COMPLETE};
        Fields d;
        if(!r_fields(*data, dorder, sizeof(dorder), &d)) return DfcDerMalformed;
        uint64_t size;
        if(!r_uint(f_get(&d, DATA_SIZE), U24_MAX, &size) || size == 0) return DfcDerMalformed;
        file->declared_size = (uint32_t)size;
        if(!r_bool(f_get(&d, DATA_COMPLETE), &file->contents_complete)) return DfcDerMalformed;
        const Slice* known = f_get(&d, DATA_KNOWN);
        if(known) {
            if(known->len == 0 || known->len > size) return DfcDerMalformed;
            if(!dfc_file_resize(c, file, known->len)) return DfcDerCapacity;
            uint8_t* dst = dfc_file_data(c, file);
            if(!dst) return DfcDerCapacity;
            memcpy(dst, known->p, known->len);
        }
        if(file->contents_complete && file->data_len != size) return DfcDerMalformed;
    } else if(file->type == FILE_TYPE_VALUE) {
        if(!value) return DfcDerMalformed;
        static const uint8_t vorder[] = {VAL_LOWER, VAL_UPPER, VAL_CURRENT, VAL_LIMITED};
        Fields v;
        if(!r_fields(*value, vorder, sizeof(vorder), &v)) return DfcDerMalformed;
        int64_t lower, upper, current;
        if(!r_int(f_get(&v, VAL_LOWER), &lower)) return DfcDerMalformed;
        if(!r_int(f_get(&v, VAL_UPPER), &upper)) return DfcDerMalformed;
        if(!r_int(f_get(&v, VAL_CURRENT), &current)) return DfcDerMalformed;
        if(lower < INT32_MIN || upper > INT32_MAX || current < INT32_MIN || current > INT32_MAX) {
            return DfcDerMalformed;
        }
        if(lower > current || current > upper) return DfcDerMalformed;
        file->value_lower_limit = (int32_t)lower;
        file->value_upper_limit = (int32_t)upper;
        file->value = (int32_t)current;
        if(!r_octets(f_get(&v, VAL_LIMITED), 1, &file->limited_credit)) return DfcDerMalformed;
    } else if(file->type == FILE_TYPE_TMAC) {
        if(!transaction_mac) return DfcDerMalformed;
#if DFC_ENABLE_TRANSACTION_MAC
        static const uint8_t torder[] = {
            TMAC_COUNTER,
            TMAC_VALUE,
            TMAC_KEY_TYPE,
            TMAC_KEY_VERSION,
            TMAC_KEY,
            TMAC_READER_ID};
        Fields tm;
        if(!r_fields(*transaction_mac, torder, sizeof(torder), &tm)) return DfcDerMalformed;
        uint64_t counter;
        if(!r_uint(f_get(&tm, TMAC_COUNTER), UINT32_MAX, &counter)) return DfcDerMalformed;
        file->transaction_counter = (uint32_t)counter;
        if(!r_octets(f_get(&tm, TMAC_VALUE), sizeof(file->transaction_mac), file->transaction_mac)) {
            return DfcDerMalformed;
        }
        if(!r_octets(f_get(&tm, TMAC_KEY_TYPE), 1, &file->transaction_mac_key_type)) {
            return DfcDerMalformed;
        }
        if(!r_octets(f_get(&tm, TMAC_KEY_VERSION), 1, &file->transaction_mac_key_version)) {
            return DfcDerMalformed;
        }
        if(!r_octets(
               f_get(&tm, TMAC_KEY), sizeof(file->transaction_mac_key), file->transaction_mac_key)) {
            return DfcDerMalformed;
        }
        if(!r_octets(
               f_get(&tm, TMAC_READER_ID),
               sizeof(file->previous_reader_id),
               file->previous_reader_id)) {
            return DfcDerMalformed;
        }
#else
        // The field is recognized but the build omits the feature.
        return DfcDerUnsupported;
#endif
    } else {
        if(!record) return DfcDerMalformed;
        static const uint8_t rorder[] = {
            REC_SIZE, REC_MAX, REC_CURRENT, REC_COMPLETE, REC_RECORDS};
        Fields r;
        if(!r_fields(*record, rorder, sizeof(rorder), &r)) return DfcDerMalformed;
        uint64_t rsize, rmax, rcur;
        if(!r_uint(f_get(&r, REC_SIZE), U24_MAX, &rsize) || rsize == 0) return DfcDerMalformed;
        if(!r_uint(f_get(&r, REC_MAX), U24_MAX, &rmax) || rmax == 0) return DfcDerMalformed;
        if(!r_uint(f_get(&r, REC_CURRENT), U24_MAX, &rcur)) return DfcDerMalformed;
        if(rcur > rmax) return DfcDerMalformed;
        if(file->type == FILE_TYPE_CYCLIC && rmax < 2) return DfcDerMalformed;
        if(!r_bool(f_get(&r, REC_COMPLETE), &file->contents_complete)) return DfcDerMalformed;
        file->record_size = (uint32_t)rsize;
        file->max_records = (uint32_t)rmax;
        file->record_count = (uint32_t)rcur;

        const Slice* recs = f_get(&r, REC_RECORDS);
        if(!recs) return DfcDerMalformed;
        Slice walk = *recs;
        size_t stored = 0;
        Tlv item;
        while(walk.len > 0) {
            if(!r_tlv(&walk, &item)) return DfcDerMalformed;
            if(item.tag != TAG_OCTETS) return DfcDerMalformed;
            if(item.body.len != rsize) return DfcDerMalformed;
            stored++;
        }
        if(stored > rcur) return DfcDerMalformed;
        if(file->contents_complete && stored != rcur) return DfcDerMalformed;
        if(stored > 0) {
            if(!dfc_file_resize(c, file, stored * rsize)) return DfcDerCapacity;
            uint8_t* dst = dfc_file_data(c, file);
            if(!dst) return DfcDerCapacity;
            walk = *recs;
            size_t off = 0;
            while(walk.len > 0) {
                if(!r_tlv(&walk, &item)) return DfcDerMalformed;
                memcpy(dst + off, item.body.p, item.body.len);
                off += item.body.len;
            }
        }
    }

    const Slice* sdm = f_get(&f, FILE_SDM);
    if(sdm) {
#if DFC_ENABLE_SDM
        // Secure dynamic messaging belongs to a standard data file only.
        if(file->type != FILE_TYPE_STANDARD) return DfcDerMalformed;
        DfcDerStatus sdm_status = decode_sdm(file, *sdm);
        if(sdm_status != DfcDerOk) return sdm_status;
#else
        // The field is recognized but the build omits the feature.
        return DfcDerUnsupported;
#endif
    }

    c->num_files++;
    return DfcDerOk;
}

static DfcDerStatus decode_files(DfcCredential* c, size_t owner, Slice body) {
    Tlv tlv;
    while(body.len > 0) {
        if(!r_tlv(&body, &tlv)) return DfcDerMalformed;
        if(tlv.tag != TAG_SEQUENCE) return DfcDerMalformed;
        DfcDerStatus st = decode_file(c, owner, tlv.body);
        if(st != DfcDerOk) return st;
    }
    return DfcDerOk;
}

DfcDerStatus dfc_der_decode(DfcCredential* credential, const uint8_t* in, size_t len) {
    if(!credential || !in) return DfcDerMalformed;
    // Section 2.2.1 bounds a .dfcb value at 65535 octets, so exceeding it is a
    // format violation rather than this build running out of room.
    if(len > DFC_DER_MAX_SIZE) return DfcDerMalformed;

    Slice top = {in, len};
    Tlv root;
    if(!r_tlv(&top, &root)) return DfcDerMalformed;
    if(root.tag != TAG_CREDENTIAL) return DfcDerMalformed;
    if(top.len != 0) return DfcDerMalformed; // trailing octets

    static const uint8_t corder[] = {CRED_VERSION, CRED_CARD, CRED_PICC, CRED_APPS};
    Fields cf;
    if(!r_fields(root.body, corder, sizeof(corder), &cf)) return DfcDerMalformed;

    uint64_t version;
    if(!r_uint(f_get(&cf, CRED_VERSION), 0xFF, &version)) return DfcDerMalformed;
    if(version != DFC_FORMAT_VERSION) return DfcDerUnsupported;

    dfc_credential_clear(credential);

    // Card
    static const uint8_t caorder[] = {
        CARD_GENERATION, CARD_STORAGE, CARD_UID, CARD_PROVENANCE, CARD_SIGNATURE};
    const Slice* card = f_get(&cf, CRED_CARD);
    Fields caf;
    if(!card || !r_fields(*card, caorder, sizeof(caorder), &caf)) return DfcDerMalformed;
    uint64_t generation, storage, provenance;
    if(!r_uint(f_get(&caf, CARD_GENERATION), 0xFF, &generation)) return DfcDerMalformed;
    if(generation < DfcGenerationEv1 || generation > DfcGenerationEv3) return DfcDerMalformed;
    if(!r_uint(f_get(&caf, CARD_STORAGE), 0xFFFFFFFFu, &storage)) return DfcDerMalformed;
    if(!r_uint(f_get(&caf, CARD_PROVENANCE), 0xFF, &provenance)) return DfcDerMalformed;
    if(provenance > DfcUidProvenanceUnknown) return DfcDerMalformed;
    const Slice* uid = f_get(&caf, CARD_UID);
    if(!uid) return DfcDerMalformed;
    if(uid->len != DFC_DESFIRE_UID_SHORT_LEN && uid->len != DFC_DESFIRE_UID_LEN &&
       uid->len != DFC_DESFIRE_UID_LONG_LEN) {
        return DfcDerMalformed;
    }
    memcpy(credential->uid, uid->p, uid->len);
    credential->uid_len = uid->len;
    credential->card.generation = (DfcGeneration)generation;
    credential->card.storage = (uint32_t)storage;
    credential->card.uid_provenance = (DfcUidProvenance)provenance;
    const Slice* signature = f_get(&caf, CARD_SIGNATURE);
    if(signature) {
#if DFC_ENABLE_STATIC_SIGNATURE
        if(!r_octets(
               signature, sizeof(credential->picc_static_signature), credential->picc_static_signature)) {
            return DfcDerMalformed;
        }
        credential->picc_has_static_signature = true;
#else
        // The field is recognized but the build omits the feature.
        return DfcDerUnsupported;
#endif
    }

    // Picc
    static const uint8_t porder[] = {
        PICC_KS1,
        PICC_KS2,
        PICC_AUTH,
        PICC_RANDOM_ID,
        PICC_FORMAT_DIS,
        PICC_ATS,
        PICC_SAK,
        PICC_ATQA,
        PICC_SM_DISABLE,
        PICC_KEYS,
        PICC_FILES,
        PICC_EV2_CAPS,
        PICC_PROXIMITY,
        PICC_VCARD,
        PICC_DAM};
    const Slice* picc = f_get(&cf, CRED_PICC);
    Fields pf;
    if(!picc || !r_fields(*picc, porder, sizeof(porder), &pf)) return DfcDerMalformed;
    if(!r_octets(f_get(&pf, PICC_KS1), 1, &credential->picc_key_settings_1)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&pf, PICC_KS2), 1, &credential->picc_key_settings_2)) {
        return DfcDerMalformed;
    }
    uint64_t picc_auth;
    if(!r_uint(f_get(&pf, PICC_AUTH), 2, &picc_auth)) return DfcDerMalformed;
    credential->picc_auth_command = auth_command_for(picc_auth);
    credential->picc_key_len = dfc_credential_key_length(credential->picc_key_settings_2);
    if(!r_default_false(&pf, PICC_RANDOM_ID, &credential->picc_random_id)) {
        return DfcDerMalformed;
    }
    if(!r_default_false(&pf, PICC_FORMAT_DIS, &credential->picc_format_disabled)) {
        return DfcDerMalformed;
    }
    const Slice* ats = f_get(&pf, PICC_ATS);
    if(ats) {
        if(ats->len == 0) return DfcDerMalformed;
        if(ats->len > DFC_PICC_ATS_MAX) return DfcDerCapacity;
        memcpy(credential->picc_ats, ats->p, ats->len);
        credential->picc_ats_len = ats->len;
    }
    const Slice* sak = f_get(&pf, PICC_SAK);
    if(sak) {
        if(!r_octets(sak, 1, &credential->picc_sak)) return DfcDerMalformed;
        credential->picc_has_sak = true;
    }
    const Slice* atqa = f_get(&pf, PICC_ATQA);
    if(atqa) {
        if(!r_octets(atqa, 2, credential->picc_atqa)) return DfcDerMalformed;
        credential->picc_has_atqa = true;
    }
    const Slice* smd = f_get(&pf, PICC_SM_DISABLE);
    if(smd) {
        if(!r_octets(smd, 1, &credential->picc_sm_disable)) return DfcDerMalformed;
        credential->picc_has_sm_disable = true;
    }
    const Slice* picc_keys = f_get(&pf, PICC_KEYS);
    if(!picc_keys) return DfcDerMalformed;
    DfcDerStatus st =
        decode_keys(credential, NULL, *picc_keys, credential->picc_key_len, DFC_MAX_KEYS);
    if(st != DfcDerOk) return st;

    const Slice* ev2_caps = f_get(&pf, PICC_EV2_CAPS);
    if(ev2_caps) {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
        if(!r_octets(
               ev2_caps,
               sizeof(credential->picc_ev2_capabilities),
               credential->picc_ev2_capabilities)) {
            return DfcDerMalformed;
        }
        credential->picc_has_ev2_capabilities = true;
#else
        return DfcDerUnsupported;
#endif
    }
    const Slice* proximity = f_get(&pf, PICC_PROXIMITY);
    if(proximity) {
        st = decode_proximity(credential, *proximity);
        if(st != DfcDerOk) return st;
    }
    const Slice* virtual_card = f_get(&pf, PICC_VCARD);
    if(virtual_card) {
        st = decode_virtual_card(credential, *virtual_card);
        if(st != DfcDerOk) return st;
    }
    const Slice* dam = f_get(&pf, PICC_DAM);
    if(dam) {
        st = decode_dam(credential, *dam);
        if(st != DfcDerOk) return st;
    }

    // Applications, before PICC files so app_index values resolve.
    const Slice* apps = f_get(&cf, CRED_APPS);
    if(!apps) return DfcDerMalformed;
    Slice walk = *apps;
    Tlv tlv;
    // The declared order is not numeric: APP_CAPABILITY follows APP_KEY_SETS.
    static const uint8_t aorder[] = {
        APP_AID,
        APP_ISO_FID,
        APP_DF_NAME,
        APP_KS1,
        APP_KS2,
        APP_AUTH,
        APP_KEYS,
        APP_FILES,
        APP_KEY_SETS,
        APP_CAPABILITY,
        APP_DELEGATED};
    while(walk.len > 0) {
        if(!r_tlv(&walk, &tlv)) return DfcDerMalformed;
        if(tlv.tag != TAG_SEQUENCE) return DfcDerMalformed;
        if(credential->num_apps >= DFC_MAX_APPS) return DfcDerCapacity;
        Fields af;
        if(!r_fields(tlv.body, aorder, sizeof(aorder), &af)) return DfcDerMalformed;

        DfcApplication* app = &credential->apps[credential->num_apps];
        dfc_credential_reset_application(app);
        if(!r_octets(f_get(&af, APP_AID), 3, app->aid)) return DfcDerMalformed;
        for(size_t i = 0; i < credential->num_apps; i++) {
            if(memcmp(credential->apps[i].aid, app->aid, 3) == 0) return DfcDerMalformed;
        }
        const Slice* afid = f_get(&af, APP_ISO_FID);
        if(afid) {
            uint8_t raw[2];
            if(!r_octets(afid, 2, raw)) return DfcDerMalformed;
            uint16_t v = (uint16_t)((raw[0] << 8) | raw[1]);
            if(v == 0x0000 || v == 0x3F00 || v == 0x3FFF || v == 0xFFFF) return DfcDerMalformed;
            app->has_iso_file_id = true;
            app->iso_file_id = v;
        }
        const Slice* df = f_get(&af, APP_DF_NAME);
        if(df) {
            if(df->len == 0 || df->len > sizeof(app->iso_aid)) return DfcDerMalformed;
            memcpy(app->iso_aid, df->p, df->len);
            app->iso_aid_len = df->len;
        }
        if(!r_octets(f_get(&af, APP_KS1), 1, &app->key_settings_1)) return DfcDerMalformed;
        if(!r_octets(f_get(&af, APP_KS2), 1, &app->key_settings_2)) return DfcDerMalformed;
        uint64_t app_auth;
        if(!r_uint(f_get(&af, APP_AUTH), 2, &app_auth)) return DfcDerMalformed;
        app->auth_command = auth_command_for(app_auth);
        app->key_len = dfc_credential_key_length(app->key_settings_2);

        const Slice* akeys = f_get(&af, APP_KEYS);
        const Slice* aksets = f_get(&af, APP_KEY_SETS);
        // Exactly one key storage alternative is required.
        if((akeys == NULL) == (aksets == NULL)) return DfcDerMalformed;
        if(akeys) {
            st = decode_keys(credential, app, *akeys, app->key_len, DFC_MAX_KEYS);
            if(st != DfcDerOk) return st;
        } else {
#if DFC_ENABLE_KEY_SETS
            st = decode_key_sets(credential, app, *aksets);
            if(st != DfcDerOk) return st;
#else
            // The field is recognized but the build omits the feature.
            return DfcDerUnsupported;
#endif
        }

        const Slice* capability = f_get(&af, APP_CAPABILITY);
        if(capability) {
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
            if(!r_octets(capability, sizeof(app->capability_data), app->capability_data)) {
                return DfcDerMalformed;
            }
            app->has_capability_data = true;
#else
            return DfcDerUnsupported;
#endif
        }
        const Slice* delegated = f_get(&af, APP_DELEGATED);
        if(delegated) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
            st = decode_delegated(app, *delegated);
            if(st != DfcDerOk) return st;
#else
            return DfcDerUnsupported;
#endif
        }

        size_t owner = credential->num_apps;
        credential->num_apps++;

        const Slice* afiles = f_get(&af, APP_FILES);
        if(!afiles) return DfcDerMalformed;
        st = decode_files(credential, owner, *afiles);
        if(st != DfcDerOk) return st;
    }

    const Slice* pfiles = f_get(&pf, PICC_FILES);
    if(!pfiles) return DfcDerMalformed;
    st = decode_files(credential, DFC_FILE_OWNER_PICC, *pfiles);
    if(st != DfcDerOk) return st;

    st = dfc_der_validate_model(credential);
    if(st != DfcDerOk) return st;

    credential->dirty = false;
    return DfcDerOk;
}

#endif // DFC_ENABLE_BINARY_CODEC
