#include "dfc_der.h"

#include <string.h>

#define FILE_TYPE_STANDARD 0x00u
#define FILE_TYPE_BACKUP   0x01u
#define FILE_TYPE_VALUE    0x02u
#define FILE_TYPE_LINEAR   0x03u
#define FILE_TYPE_CYCLIC   0x04u
#define FILE_TYPE_TMAC     0x05u

#define U24_MAX           16777215u
#define TAG_COUNT(values) (sizeof(values) / sizeof((values)[0]))

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
#define CARD_GENERATION       P(0)
#define CARD_STORAGE          P(1)
#define CARD_UID              P(2)
#define CARD_PROVENANCE       P(3)
#define CARD_SIGNATURE        P(4)
#define CARD_HARDWARE_VERSION P(5)
#define CARD_SOFTWARE_VERSION P(6)

// Picc
#define PICC_KS1              P(0)
#define PICC_KS2              P(1)
#define PICC_AUTH             P(2)
#define PICC_RANDOM_ID        P(3)
#define PICC_FORMAT_DIS       P(4)
#define PICC_ATS              P(5)
#define PICC_SAK              P(6)
#define PICC_ATQA             P(7)
#define PICC_SM_DISABLE       P(8)
#define PICC_KEYS             C(9)
#define PICC_FILES            C(10)
#define PICC_EV2_CAPS         P(11)
#define PICC_PROXIMITY        C(12)
#define PICC_VCARD            C(13)
#define PICC_DAM              C(14)
#define PICC_PREFERRED_AUTH   P(15)
#define PICC_RANDOM_ID_LEGACY P(16)

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
#define APP_AID            P(0)
#define APP_ISO_FID        P(1)
#define APP_DF_NAME        P(2)
#define APP_KS1            P(3)
#define APP_KS2            P(4)
#define APP_AUTH           P(5)
#define APP_KEYS           C(6)
#define APP_FILES          C(7)
#define APP_KEY_SETS       C(8)
#define APP_CAPABILITY     P(9)
#define APP_DELEGATED      C(10)
#define APP_SM_DISABLE     P(11)
#define APP_PREFERRED_AUTH P(12)

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
#define DEL_SLOT            P(0)
#define DEL_VERSION         P(1)
#define DEL_QUOTA           P(2)
#define DEL_FREE            P(3)
#define DEL_FORMAT_DISABLED P(4)

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
#define VAL_LIMIT   P(4)

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

#if DFC_ENABLE_DER_ENCODER

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
        if(w->len > w->cap || n > w->cap - w->len) {
            w->overflow = true;
            return;
        }
        memcpy(w->buf + w->len, bytes, n);
    } else if(n > SIZE_MAX - w->len) {
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

// A constructed component whose body is produced by a callback. Reserve the
// shortest header, emit the body once, then make room for a long-form length if
// needed. This keeps output canonical without recursively probing every body.
typedef void (*BodyFn)(Writer*, const void*);

static void w_constructed(Writer* w, uint8_t tag, BodyFn fn, const void* ctx) {
    if(w->overflow) return;
    size_t start = w->len;
    w_byte(w, tag);
    w_byte(w, 0); // short-form placeholder
    if(w->overflow) return;

    size_t body_start = w->len;
    fn(w, ctx);
    if(w->overflow) return;
    size_t body_len = w->len - body_start;

    size_t extra = 0;
    if(body_len >= 256)
        extra = 2;
    else if(body_len >= 128)
        extra = 1;
    if(body_len > UINT16_MAX || extra > SIZE_MAX - w->len ||
       (w->buf && (w->len > w->cap || extra > w->cap - w->len))) {
        w->overflow = true;
        return;
    }

    if(w->buf) {
        if(extra) memmove(w->buf + body_start + extra, w->buf + body_start, body_len);
        if(extra == 0) {
            w->buf[start + 1] = (uint8_t)body_len;
        } else if(extra == 1) {
            w->buf[start + 1] = 0x81;
            w->buf[start + 2] = (uint8_t)body_len;
        } else {
            w->buf[start + 1] = 0x82;
            w->buf[start + 2] = (uint8_t)(body_len >> 8);
            w->buf[start + 3] = (uint8_t)body_len;
        }
    }
    w->len += extra;
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
    uint32_t declared_size;
    if(!dfc_file_get_declared_size(f, &declared_size)) {
        w->overflow = true;
        return;
    }
    w_int(w, DATA_SIZE, (int64_t)declared_size);
    if(known && f->data_len > 0) w_tlv(w, DATA_KNOWN, known, f->data_len);
    w_bool(w, DATA_COMPLETE, f->contents_complete);
}

static void body_value_contents(Writer* w, const void* p) {
    const DfcFile* f = ((const Ctx*)p)->file;
    w_int(w, VAL_LOWER, f->state.value.lower_limit);
    w_int(w, VAL_UPPER, f->state.value.upper_limit);
    w_int(w, VAL_CURRENT, f->state.value.current);
    w_tlv(w, VAL_LIMITED, &f->state.value.limited_credit, 1);
    if(f->state.value.limited_credit_value != 0)
        w_int(w, VAL_LIMIT, f->state.value.limited_credit_value);
}

static void body_records(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcFile* f = x->file;
    const uint8_t* known = dfc_file_data_const(x->c, f);
    size_t count = f->state.record.size ? f->data_len / f->state.record.size : 0;
    if(count > f->state.record.count) count = f->state.record.count;
    for(size_t i = 0; i < count; i++) {
        w_tlv(w, TAG_OCTETS, known + i * f->state.record.size, f->state.record.size);
    }
}

static void body_record_contents(Writer* w, const void* p) {
    const Ctx* x = p;
    const DfcFile* f = x->file;
    w_int(w, REC_SIZE, (int64_t)f->state.record.size);
    w_int(w, REC_MAX, (int64_t)f->state.record.max_records);
    w_int(w, REC_CURRENT, (int64_t)f->state.record.count);
    w_bool(w, REC_COMPLETE, f->contents_complete);
    w_constructed(w, REC_RECORDS, body_records, x);
}

#if DFC_ENABLE_TRANSACTION_MAC
static void body_transaction_mac_contents(Writer* w, const void* p) {
    const DfcFile* f = ((const Ctx*)p)->file;
    w_int(w, TMAC_COUNTER, (int64_t)f->state.transaction_mac.counter);
    w_tlv(w, TMAC_VALUE, f->state.transaction_mac.mac, sizeof(f->state.transaction_mac.mac));
    w_tlv(w, TMAC_KEY_TYPE, &f->state.transaction_mac.key_type, 1);
    w_tlv(w, TMAC_KEY_VERSION, &f->state.transaction_mac.key_version, 1);
    w_tlv(w, TMAC_KEY, f->state.transaction_mac.key, sizeof(f->state.transaction_mac.key));
    w_tlv(
        w,
        TMAC_READER_ID,
        f->state.transaction_mac.previous_reader_id,
        sizeof(f->state.transaction_mac.previous_reader_id));
}
#endif

#if DFC_ENABLE_SDM
static void body_sdm(Writer* w, const void* p) {
    const DfcFile* f = ((const Ctx*)p)->file;
    uint8_t rights[2] = {
        (uint8_t)(f->state.data.sdm.access_rights >> 8),
        (uint8_t)(f->state.data.sdm.access_rights & 0xFF)};
    w_tlv(w, SDM_OPTIONS, &f->state.data.sdm.options, 1);
    w_tlv(w, SDM_RIGHTS, rights, 2);
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagUidOffset))
        w_int(w, SDM_UID_OFF, (int64_t)f->state.data.sdm.uid_offset);
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagCounterOffset))
        w_int(w, SDM_COUNTER_OFF, (int64_t)f->state.data.sdm.counter_offset);
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagPiccDataOffset))
        w_int(w, SDM_PICC_OFF, (int64_t)f->state.data.sdm.picc_data_offset);
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagMacInputOffset))
        w_int(w, SDM_MAC_IN_OFF, (int64_t)f->state.data.sdm.mac_input_offset);
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagMacOffset))
        w_int(w, SDM_MAC_OFF, (int64_t)f->state.data.sdm.mac_offset);
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagEncryptedFile)) {
        w_int(w, SDM_ENC_OFF, (int64_t)f->state.data.sdm.encrypted_file_offset);
        w_int(w, SDM_ENC_LEN, (int64_t)f->state.data.sdm.encrypted_file_length);
    }
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagCounterLimit))
        w_int(w, SDM_LIMIT, (int64_t)f->state.data.sdm.counter_limit);
    w_int(w, SDM_READ_COUNTER, (int64_t)f->state.data.sdm_read_counter);
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
    if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagEnabled)) w_constructed(w, FILE_SDM, body_sdm, x);
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
    if(c->card.has_hardware_version) {
        w_tlv(
            w, CARD_HARDWARE_VERSION, c->card.hardware_version, sizeof(c->card.hardware_version));
    }
    if(c->card.has_software_version) {
        w_tlv(
            w, CARD_SOFTWARE_VERSION, c->card.software_version, sizeof(c->card.software_version));
    }
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
    w_int(w, PICC_AUTH, dfc_credential_picc_auth_commands(c));
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
    if(c->picc_has_preferred_auth_command) {
        w_int(w, PICC_PREFERRED_AUTH, c->picc_preferred_auth_command);
    }
    if(c->picc_random_id_legacy) w_bool(w, PICC_RANDOM_ID_LEGACY, true);
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
    const uint8_t* version_ptr =
        dfc_credential_key_version_in_set_const(k->c, k->app, k->set, k->slot);
    const uint8_t* type = dfc_credential_key_set_type_const(k->c, k->app, k->set);
    if(!version_ptr || !type) {
        w->overflow = true;
        return;
    }
    uint8_t version = *version_ptr;
    w_int(w, KEY_SLOT, (int64_t)k->slot);
    size_t key_len = dfc_key_set_type_length(*type);
    w_tlv(w, KEY_VALUE, value, key_len);
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
    const uint8_t* version = dfc_credential_key_set_version_const(k->c, a, k->set);
    const uint8_t* type = dfc_credential_key_set_type_const(k->c, a, k->set);
    if(!version || !type) {
        w->overflow = true;
        return;
    }
    w_int(w, SET_NUMBER, (int64_t)k->set);
    w_tlv(w, SET_VERSION, version, 1);
    w_int(w, SET_TYPE, (int64_t)*type);
    // Required, so FALSE is encoded rather than omitted.
    w_bool(w, SET_INITIALIZED, dfc_credential_key_set_is_initialized(k->c, a, k->set));
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
    if(a->delegated_format_disabled) w_bool(w, DEL_FORMAT_DISABLED, true);
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
    w_int(w, APP_AUTH, dfc_credential_app_auth_commands(x->c, a));
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
    if(a->has_sm_disable) w_tlv(w, APP_SM_DISABLE, &a->sm_disable, 1);
    if(a->has_preferred_auth_command) {
        w_int(w, APP_PREFERRED_AUTH, a->preferred_auth_command);
    }
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

#endif // DFC_ENABLE_DER_ENCODER

#endif // DFC_ENABLE_BINARY_CODEC

// Model validation is part of the credential API, not either codec. Keep it
// available even when both codec directions are compiled out.

static bool model_file_is_data(uint8_t type) {
    return type == FILE_TYPE_STANDARD || type == FILE_TYPE_BACKUP;
}

static bool
    model_slice_is_valid(size_t offset, size_t length, size_t used, size_t capacity, size_t none) {
    if(used > capacity) return false;
    if(length == 0) return offset == none;
    return offset != none && offset <= used && length <= used - offset;
}

static bool model_slices_overlap(
    size_t left_offset,
    size_t left_length,
    size_t right_offset,
    size_t right_length) {
    if(left_length == 0 || right_length == 0) return false;
    return left_offset < right_offset + right_length && right_offset < left_offset + left_length;
}

static bool model_key_length_is_valid(uint8_t key_settings_2, size_t key_length) {
    switch(key_settings_2 & DFC_KEY_TYPE_MASK) {
    case DFC_KEY_TYPE_DES_2K3DES:
        return key_length == 8 || key_length == 16;
    case DFC_KEY_TYPE_3K3DES:
        return key_length == 24;
    case DFC_KEY_TYPE_AES:
        return key_length == 16;
    default:
        return false;
    }
}

static bool model_key_slice_is_valid(const DfcCredential* c, const DfcApplication* app) {
    size_t count = app ? app->num_keys : c->picc_num_keys;
    size_t key_length = app ? app->key_len : c->picc_key_len;
    size_t offset = app ? app->key_offset : c->picc_key_offset;
    size_t pool_length = app ? app->key_pool_len : c->picc_key_pool_len;
    uint8_t settings = app ? app->key_settings_2 : c->picc_key_settings_2;
    if(count > DFC_MAX_KEYS) return false;
    if(count > 0 && !model_key_length_is_valid(settings, key_length)) return false;

    size_t allocated = count;
    size_t storage_length = key_length;
#if DFC_ENABLE_KEY_SETS
    if(app) {
        if(app->num_key_sets == 0 || app->num_key_sets > DFC_MAX_KEY_SETS) return false;
        if(app->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT) {
            if(count == 0 || !dfc_credential_key_set_is_initialized(c, app, 0)) return false;
            const uint8_t* active_type = dfc_credential_key_set_type_const(c, app, 0);
            if(!active_type) return false;
            if(app->max_key_size != DFC_KEY_SET_MAXIMUM_16_BYTE &&
               app->max_key_size != DFC_KEY_SET_MAXIMUM_24_BYTE)
                return false;
            if(app->key_storage_len != app->max_key_size) return false;
            if(key_length > app->max_key_size ||
               dfc_key_set_type_length(*active_type) != key_length ||
               (settings & DFC_KEY_TYPE_MASK) != dfc_key_type_for_set_type(*active_type))
                return false;
            for(size_t set = 0; set < app->num_key_sets; set++) {
                const uint8_t* type = dfc_credential_key_set_type_const(c, app, set);
                if(!type) return false;
                size_t width = dfc_key_set_type_length(*type);
                if(width == 0 || width > app->max_key_size) return false;
            }
            if(count > SIZE_MAX / app->num_key_sets) return false;
            allocated = count * app->num_key_sets;
            storage_length = app->key_storage_len;
        } else if(app->num_key_sets != 1) {
            return false;
        }
    }
#endif
    size_t stride = dfc_credential_stored_key_length(storage_length);
    if(allocated > SIZE_MAX / stride) return false;
    size_t expected = allocated * stride;
    if(pool_length != expected) return false;
    return model_slice_is_valid(
        offset, pool_length, c->key_pool_used, DFC_KEY_POOL_SIZE, DFC_KEY_POOL_NONE);
}

static bool model_pools_are_valid(const DfcCredential* c) {
    if(c->key_pool_used > DFC_KEY_POOL_SIZE || c->file_pool_used > DFC_FILE_POOL_SIZE
#if DFC_ENABLE_KEY_SETS
       || c->key_set_metadata_pool_used > DFC_KEY_SET_METADATA_POOL_SIZE
#endif
    ) {
        return false;
    }
    if(!model_key_slice_is_valid(c, NULL)) return false;
    size_t key_total = c->picc_key_pool_len;
    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* app = &c->apps[i];
        if(!model_key_slice_is_valid(c, app)) return false;
        if(key_total > SIZE_MAX - app->key_pool_len) return false;
        key_total += app->key_pool_len;
        if(model_slices_overlap(
               c->picc_key_offset, c->picc_key_pool_len, app->key_offset, app->key_pool_len))
            return false;
        for(size_t j = 0; j < i; j++) {
            if(model_slices_overlap(
                   c->apps[j].key_offset,
                   c->apps[j].key_pool_len,
                   app->key_offset,
                   app->key_pool_len))
                return false;
        }
    }
    if(key_total != c->key_pool_used) return false;

#if DFC_ENABLE_KEY_SETS
    size_t metadata_total = 0;
    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* app = &c->apps[i];
        if(app->num_key_sets < DFC_KEY_SET_MINIMUM_COUNT) {
            if(app->key_set_metadata_offset != DFC_KEY_SET_METADATA_POOL_NONE ||
               app->key_set_metadata_len != 0)
                return false;
            continue;
        }
        if(!dfc_credential_key_set_version_const(c, app, 0)) return false;
        if(metadata_total > SIZE_MAX - app->key_set_metadata_len) return false;
        metadata_total += app->key_set_metadata_len;
        for(size_t j = 0; j < i; j++) {
            if(model_slices_overlap(
                   c->apps[j].key_set_metadata_offset,
                   c->apps[j].key_set_metadata_len,
                   app->key_set_metadata_offset,
                   app->key_set_metadata_len))
                return false;
        }
    }
    if(metadata_total != c->key_set_metadata_pool_used) return false;
#endif

    size_t file_total = 0;
    for(size_t i = 0; i < c->num_files; i++) {
        const DfcFile* file = &c->files[i];
        if(!model_slice_is_valid(
               file->data_offset,
               file->data_len,
               c->file_pool_used,
               DFC_FILE_POOL_SIZE,
               DFC_FILE_POOL_NONE))
            return false;
        if(file_total > SIZE_MAX - file->data_len) return false;
        file_total += file->data_len;
        for(size_t j = 0; j < i; j++) {
            if(model_slices_overlap(
                   c->files[j].data_offset,
                   c->files[j].data_len,
                   file->data_offset,
                   file->data_len))
                return false;
        }
    }
    return file_total == c->file_pool_used;
}

// Reject a model that would encode into something a conforming decoder refuses,
// so an invalid credential cannot leave this process.
DfcModelStatus dfc_credential_validate_model(const DfcCredential* c) {
    if(!c || c->num_apps > DFC_MAX_APPS || c->num_files > DFC_MAX_FILES) {
        return DfcDerMalformed;
    }
    if(!model_pools_are_valid(c)) return DfcDerMalformed;
    if(!dfc_uid_length_is_valid(c->uid_len)) {
        return DfcDerMalformed;
    }
    if(c->card.generation < DfcGenerationEv1 || c->card.generation > DfcGenerationEv3) {
        return DfcDerMalformed;
    }
    if(c->card.uid_provenance > DfcUidProvenanceUnknown) return DfcDerMalformed;
    if(c->picc_random_id_legacy && !c->picc_random_id) return DfcDerMalformed;
    // A key list may be empty: an absent entry is the factory default
    // and an absent key slot encodes a required but empty
    // SEQUENCE OF as its tag with length zero.
    if(c->picc_num_keys > DFC_MAX_KEYS) return DfcDerMalformed;
    if(c->picc_ats_len > DFC_PICC_ATS_MAX) return DfcDerMalformed;
    uint8_t picc_commands = dfc_credential_picc_auth_commands(c);
    if(picc_commands & (uint8_t)~DFC_AUTH_COMMAND_ALL) return DfcDerMalformed;
    if(c->picc_has_preferred_auth_command &&
       (c->picc_preferred_auth_command == 0 ||
        (c->picc_preferred_auth_command & (c->picc_preferred_auth_command - 1u)) != 0 ||
        (c->picc_preferred_auth_command & DFC_AUTH_COMMAND_ALL) == 0 ||
        ((c->picc_has_auth_commands ?
              picc_commands :
              dfc_credential_possible_auth_commands(c->picc_key_settings_2, c->card.generation)) &
         c->picc_preferred_auth_command) == 0))
        return DfcDerMalformed;
    uint8_t picc_allowed =
        dfc_credential_possible_auth_commands(c->picc_key_settings_2, c->card.generation) |
        DFC_AUTH_COMMAND_ISO7816;
    if(picc_commands & (uint8_t)~picc_allowed) return DfcDerMalformed;
    if(picc_commands & (uint8_t)~dfc_credential_compiled_auth_commands()) {
        return DfcDerUnsupported;
    }
    if(c->picc_has_preferred_auth_command &&
       !(dfc_credential_compiled_auth_commands() & c->picc_preferred_auth_command)) {
        return DfcDerUnsupported;
    }
    if(c->card.generation < DfcGenerationEv2 &&
       (picc_commands & (DFC_AUTH_COMMAND_EV2_FIRST | DFC_AUTH_COMMAND_EV2_NON_FIRST))) {
        return DfcDerMalformed;
    }
    if(c->picc_has_sm_disable &&
       (c->picc_sm_disable &
        (uint8_t)~(DFC_SM_DISABLE_D40 | DFC_SM_DISABLE_EV1 | DFC_SM_DISABLE_EV2_CHAINED_WRITE))) {
        return DfcDerMalformed;
    }

    // Generation gating. A feature that a generation does not define is
    // malformed, which is distinct from a feature this build omits.
    bool has_ev2 = false;
    bool has_ev3 = false;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    has_ev2 = has_ev2 || c->picc_has_ev2_capabilities;
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    has_ev2 = has_ev2 || c->virtual_card_configured;
    if(c->virtual_card_configured) {
        if(c->virtual_card_installation_id_len > DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH ||
           !dfc_uid_length_is_valid(c->virtual_card_uid_len)) {
            return DfcDerMalformed;
        }
    }
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    has_ev2 = has_ev2 || c->picc_has_dam_keys;
#endif
#if DFC_ENABLE_STATIC_SIGNATURE
    has_ev2 = has_ev2 || c->picc_has_static_signature;
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    has_ev3 = has_ev3 || c->picc_has_proximity_key;
#endif
    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* a = &c->apps[i];
        uint8_t commands = dfc_credential_app_auth_commands(c, a);
        if(commands & (uint8_t)~DFC_AUTH_COMMAND_ALL) return DfcDerMalformed;
        if(a->has_preferred_auth_command &&
           (a->preferred_auth_command == 0 ||
            (a->preferred_auth_command & (a->preferred_auth_command - 1u)) != 0 ||
            (a->preferred_auth_command & DFC_AUTH_COMMAND_ALL) == 0 ||
            ((a->has_auth_commands ?
                  commands :
                  dfc_credential_possible_auth_commands(a->key_settings_2, c->card.generation)) &
             a->preferred_auth_command) == 0))
            return DfcDerMalformed;
        uint8_t allowed =
            dfc_credential_possible_auth_commands(a->key_settings_2, c->card.generation) |
            DFC_AUTH_COMMAND_ISO7816;
        if(commands & (uint8_t)~allowed) return DfcDerMalformed;
        if(commands & (uint8_t)~dfc_credential_compiled_auth_commands()) {
            return DfcDerUnsupported;
        }
        if(a->has_preferred_auth_command &&
           !(dfc_credential_compiled_auth_commands() & a->preferred_auth_command)) {
            return DfcDerUnsupported;
        }
        if(c->card.generation < DfcGenerationEv2 &&
           (commands & (DFC_AUTH_COMMAND_EV2_FIRST | DFC_AUTH_COMMAND_EV2_NON_FIRST))) {
            return DfcDerMalformed;
        }
        if(a->has_sm_disable && (a->sm_disable & (uint8_t)~(
                                                     DFC_SM_DISABLE_D40 | DFC_SM_DISABLE_EV1 |
                                                     DFC_SM_DISABLE_EV2_CHAINED_WRITE))) {
            return DfcDerMalformed;
        }
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
        has_ev3 = has_ev3 || dfc_file_sdm_flag_is_set(f, DfcSdmFlagEnabled);
#endif
    }
    if(has_ev2 && c->card.generation < DfcGenerationEv2) return DfcDerMalformed;
    if(has_ev3 && c->card.generation < DfcGenerationEv3) return DfcDerMalformed;

    for(size_t i = 0; i < c->num_apps; i++) {
        const DfcApplication* a = &c->apps[i];
        if(a->num_keys > DFC_MAX_KEYS) return DfcDerMalformed;
        if(a->iso_aid_len > 16) return DfcDerMalformed;
        if(a->has_iso_file_id && dfc_iso_file_id_is_reserved(a->iso_file_id)) {
            return DfcDerMalformed;
        }
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
        size_t key_count = f->app_index == DFC_FILE_OWNER_PICC ? c->picc_num_keys :
                                                                 c->apps[f->app_index].num_keys;
        if(!dfc_communication_settings_is_valid(f->comm_settings)) return DfcDerMalformed;
#if DFC_ENABLE_SDM
        if((f->comm_settings & DFC_SDM_ENABLED_MASK) != 0 &&
           !dfc_file_sdm_flag_is_set(f, DfcSdmFlagEnabled)) {
            return DfcDerMalformed;
        }
#endif
        if(f->type == FILE_TYPE_TMAC ?
               !dfc_transaction_mac_access_rights_are_valid(f->access_rights, key_count) :
               !dfc_access_rights_are_valid(f->access_rights, key_count)) {
            return DfcDerMalformed;
        }
        for(size_t j = i + 1; j < c->num_files; j++) {
            if(c->files[j].app_index == f->app_index && c->files[j].number == f->number) {
                return DfcDerMalformed;
            }
        }
        if(model_file_is_data(f->type)) {
            uint32_t declared_size;
            if(!dfc_file_get_declared_size(f, &declared_size) || declared_size == 0 ||
               declared_size > U24_MAX) {
                return DfcDerMalformed;
            }
            if(f->data_len > declared_size) return DfcDerMalformed;
            if(f->contents_complete && f->data_len != declared_size) return DfcDerMalformed;
        } else if(f->type == FILE_TYPE_VALUE) {
            if(f->state.value.lower_limit > f->state.value.current ||
               f->state.value.current > f->state.value.upper_limit) {
                return DfcDerMalformed;
            }
            if(f->state.value.limited_credit > 3 ||
               ((f->state.value.limited_credit & 0x01u) == 0 &&
                f->state.value.limited_credit_value != 0) ||
               f->state.value.limited_credit_value < 0) {
                return DfcDerMalformed;
            }
        } else if(f->type == FILE_TYPE_TMAC) {
            // A transaction-MAC file is an EV2 feature (section 2.2.4 rule 13).
            if(c->card.generation < DfcGenerationEv2) {
                return DfcDerMalformed;
            }
#if DFC_ENABLE_TRANSACTION_MAC
            if(!dfc_transaction_mac_key_type_is_valid(f->state.transaction_mac.key_type)) {
                return DfcDerMalformed;
            }
#endif
        } else {
            if(f->state.record.size == 0 || f->state.record.size > U24_MAX) return DfcDerMalformed;
            if(f->state.record.max_records == 0 || f->state.record.max_records > U24_MAX)
                return DfcDerMalformed;
            if(f->state.record.count > f->state.record.max_records) return DfcDerMalformed;
            if(f->data_len % f->state.record.size != 0) return DfcDerMalformed;
            size_t stored = f->data_len / f->state.record.size;
            bool reserved = stored == f->state.record.max_records;
            if(stored > f->state.record.count && !reserved) return DfcDerMalformed;
            if(f->contents_complete && stored != f->state.record.count && !reserved) {
                return DfcDerMalformed;
            }
            if(f->type == FILE_TYPE_CYCLIC && f->state.record.max_records < 2)
                return DfcDerMalformed;
        }
        if(f->has_iso_file_id && dfc_iso_file_id_is_reserved(f->iso_file_id)) {
            return DfcDerMalformed;
        }
#if DFC_ENABLE_SDM
        if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagEnabled)) {
            if(f->type != FILE_TYPE_STANDARD) return DfcDerMalformed;
            if((f->state.data.sdm.options &
                (uint8_t)~(
                    DFC_SDM_UID_MIRROR_MASK | DFC_SDM_COUNTER_MIRROR_MASK |
                    DFC_SDM_COUNTER_LIMIT_MASK | DFC_SDM_ENCRYPTED_FILE_MASK |
                    DFC_SDM_ASCII_ENCODING_MASK)) != 0 ||
               !dfc_sdm_access_rights_are_valid(f->state.data.sdm.access_rights, key_count)) {
                return DfcDerMalformed;
            }
            // Every offset addresses a position inside the file.
            uint32_t size;
            if(!dfc_file_get_declared_size(f, &size)) return DfcDerMalformed;
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagUidOffset) &&
               f->state.data.sdm.uid_offset >= size)
                return DfcDerMalformed;
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagCounterOffset) &&
               f->state.data.sdm.counter_offset >= size)
                return DfcDerMalformed;
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagPiccDataOffset) &&
               f->state.data.sdm.picc_data_offset >= size) {
                return DfcDerMalformed;
            }
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagMacInputOffset) &&
               f->state.data.sdm.mac_input_offset >= size) {
                return DfcDerMalformed;
            }
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagMacOffset) &&
               f->state.data.sdm.mac_offset >= size)
                return DfcDerMalformed;
            if(dfc_file_sdm_flag_is_set(f, DfcSdmFlagEncryptedFile)) {
                if(f->state.data.sdm.encrypted_file_offset >= size) return DfcDerMalformed;
                if(f->state.data.sdm.encrypted_file_length >
                   size - f->state.data.sdm.encrypted_file_offset) {
                    return DfcDerMalformed;
                }
            }
        }
#endif
    }
    return DfcDerOk;
}

#if DFC_ENABLE_BINARY_CODEC

DfcDerStatus dfc_der_validate_model(const DfcCredential* credential) {
    return dfc_credential_validate_model(credential);
}

#if DFC_ENABLE_DER_ENCODER

DfcDerStatus dfc_der_encoded_size(const DfcCredential* credential, size_t* len) {
    if(!credential || !len) return DfcDerMalformed;
    DfcDerStatus st = dfc_credential_validate_model(credential);
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

#endif // DFC_ENABLE_DER_ENCODER

#if DFC_ENABLE_DER_DECODER

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
        for(size_t i = 0; i < count; i++)
            n = (n << 8) | s->p[2 + i];
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

size_t dfc_der_length(const uint8_t* data, size_t capacity) {
    if(!data) return 0;
    Slice input = {data, capacity};
    Tlv tlv;
    if(!r_tlv(&input, &tlv) || tlv.tag != TAG_CREDENTIAL) return 0;
    size_t length = capacity - input.len;
    return length <= DFC_DER_MAX_SIZE ? length : 0;
}

// Components of one SEQUENCE, keyed by identifier octet, with declaration order
// enforced. `order` lists the identifiers in the order section 2.2.2 declares
// them; anything else, or out of order, is malformed.
#define MAX_COMPONENTS 17

_Static_assert(DFC_DER_MAX_SIZE <= UINT16_MAX, "DER offsets must fit uint16_t");

typedef struct {
    const uint8_t* base;
    const uint8_t* order;
    uint32_t present;
    uint16_t encoded_offset[MAX_COMPONENTS];
    uint8_t count;
} Fields;

#if DFC_BUILD_ROLE == DFC_ROLE_TARGET && SIZE_MAX == UINT32_MAX
_Static_assert(sizeof(Fields) <= 48, "DER field index must remain stack-bounded");
#endif

static bool r_fields(Slice body, const uint8_t* order, size_t order_len, Fields* out) {
    memset(out, 0, sizeof(*out));
    if(order_len > MAX_COMPONENTS) return false;
    out->base = body.p;
    out->order = order;
    out->count = (uint8_t)order_len;

    size_t cursor = 0;
    Tlv tlv = {0};
    while(body.len > 0) {
        const uint8_t* encoded = body.p;
        if(!r_tlv(&body, &tlv)) return false;
        size_t idx = order_len;
        for(size_t i = 0; i < order_len; i++) {
            if(order[i] == tlv.tag) {
                idx = i;
                break;
            }
        }
        if(idx == order_len) return false; // unknown tag: no extensions in v3
        uint32_t bit = UINT32_C(1) << idx;
        if(out->present & bit) return false; // duplicate
        if(idx < cursor) return false; // out of declaration order
        size_t offset = (size_t)(encoded - out->base);
        if(offset > UINT16_MAX) return false;
        cursor = idx + 1;
        out->present |= bit;
        out->encoded_offset[idx] = (uint16_t)offset;
    }
    return true;
}

static const uint8_t* f_get(const Fields* f, uint8_t tag) {
    for(size_t i = 0; i < f->count; i++) {
        if(f->order[i] == tag && (f->present & (UINT32_C(1) << i)))
            return f->base + f->encoded_offset[i];
    }
    return NULL;
}

// `field` points at a TLV already validated by r_fields. Reconstructing its
// body here keeps Fields compact without rescanning the parent sequence.
static Slice field_body(const uint8_t* field) {
    size_t header = 2;
    size_t length = field[1];
    if(length & 0x80) {
        size_t count = length & 0x7F;
        length = 0;
        for(size_t i = 0; i < count; i++)
            length = (length << 8) | field[2 + i];
        header += count;
    }
    Slice body = {field + header, length};
    return body;
}

static bool r_int(const uint8_t* field, int64_t* out) {
    if(!field) return false;
    Slice s = field_body(field);
    if(s.len == 0 || s.len > 8) return false;
    if(s.len > 1) {
        bool redundant = (s.p[0] == 0x00 && (s.p[1] & 0x80) == 0) ||
                         (s.p[0] == 0xFF && (s.p[1] & 0x80) != 0);
        if(redundant) return false;
    }
    // Accumulate in an unsigned value, so shifting a negative integer's sign
    // extension is never undefined, and convert once at the end. The sign
    // extension fills the octets above the encoding with ones, so the two's
    // complement conversion reproduces the encoded value exactly.
    uint64_t v = (s.p[0] & 0x80) ? UINT64_MAX : 0;
    for(size_t i = 0; i < s.len; i++)
        v = (v << 8) | s.p[i];
    *out = (int64_t)v;
    return true;
}

static bool r_uint(const uint8_t* field, uint64_t max, uint64_t* out) {
    int64_t v;
    if(!r_int(field, &v)) return false;
    if(v < 0 || (uint64_t)v > max) return false;
    *out = (uint64_t)v;
    return true;
}

static bool r_bool(const uint8_t* field, bool* out) {
    if(!field) return false;
    Slice s = field_body(field);
    if(s.len != 1) return false;
    if(s.p[0] != 0x00 && s.p[0] != 0xFF) return false;
    *out = s.p[0] == 0xFF;
    return true;
}

// A DEFAULT FALSE boolean: absent means false, and an explicit false is invalid
// because DER omits a component equal to its default.
static bool r_default_false(const Fields* f, uint8_t tag, bool* out) {
    const uint8_t* field = f_get(f, tag);
    if(!field) {
        *out = false;
        return true;
    }
    bool v;
    if(!r_bool(field, &v)) return false;
    if(!v) return false;
    *out = true;
    return true;
}

static bool r_octets(const uint8_t* field, size_t want, uint8_t* out) {
    if(!field) return false;
    Slice s = field_body(field);
    if(s.len != want) return false;
    memcpy(out, s.p, want);
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
    const uint8_t* found = f_get(f, tag);
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
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, SDM_OPTIONS), 1, &file->state.data.sdm.options)) return DfcDerMalformed;
    uint8_t rights[2];
    if(!r_octets(f_get(&f, SDM_RIGHTS), 2, rights)) return DfcDerMalformed;
    file->state.data.sdm.access_rights = (uint16_t)((rights[0] << 8) | rights[1]);
    bool has_uid = false;
    bool has_counter = false;
    bool has_picc = false;
    bool has_mac_input = false;
    bool has_mac = false;
    bool has_encrypted = false;
    if(!sdm_offset(&f, SDM_UID_OFF, &has_uid, &file->state.data.sdm.uid_offset) ||
       !sdm_offset(&f, SDM_COUNTER_OFF, &has_counter, &file->state.data.sdm.counter_offset) ||
       !sdm_offset(&f, SDM_PICC_OFF, &has_picc, &file->state.data.sdm.picc_data_offset) ||
       !sdm_offset(&f, SDM_MAC_IN_OFF, &has_mac_input, &file->state.data.sdm.mac_input_offset) ||
       !sdm_offset(&f, SDM_MAC_OFF, &has_mac, &file->state.data.sdm.mac_offset) ||
       !sdm_offset(&f, SDM_ENC_OFF, &has_encrypted, &file->state.data.sdm.encrypted_file_offset)) {
        return DfcDerMalformed;
    }
    dfc_file_sdm_flag_set(file, DfcSdmFlagUidOffset, has_uid);
    dfc_file_sdm_flag_set(file, DfcSdmFlagCounterOffset, has_counter);
    dfc_file_sdm_flag_set(file, DfcSdmFlagPiccDataOffset, has_picc);
    dfc_file_sdm_flag_set(file, DfcSdmFlagMacInputOffset, has_mac_input);
    dfc_file_sdm_flag_set(file, DfcSdmFlagMacOffset, has_mac);
    dfc_file_sdm_flag_set(file, DfcSdmFlagEncryptedFile, has_encrypted);
    const uint8_t* enc_len = f_get(&f, SDM_ENC_LEN);
    // The encrypted offset and its length occur together.
    if((enc_len != NULL) != has_encrypted) return DfcDerMalformed;
    if(enc_len) {
        uint64_t value;
        if(!r_uint(enc_len, U24_MAX, &value)) return DfcDerMalformed;
        file->state.data.sdm.encrypted_file_length = (uint32_t)value;
    }
    bool has_limit = false;
    if(!sdm_offset(&f, SDM_LIMIT, &has_limit, &file->state.data.sdm.counter_limit)) {
        return DfcDerMalformed;
    }
    dfc_file_sdm_flag_set(file, DfcSdmFlagCounterLimit, has_limit);
    uint64_t counter;
    if(!r_uint(f_get(&f, SDM_READ_COUNTER), U24_MAX, &counter)) return DfcDerMalformed;
    file->state.data.sdm_read_counter = (uint32_t)counter;
    dfc_file_sdm_flag_set(file, DfcSdmFlagEnabled, true);
    return DfcDerOk;
}
#endif

#if DFC_ENABLE_KEY_SETS
// Returns the stored key length that a key-set type requires.
static DfcDerStatus decode_key_sets(DfcCredential* c, DfcApplication* app, Slice body) {
    static const uint8_t order[] = {KS_KEY_COUNT, KS_MAX_SIZE, KS_SETTINGS, KS_SETS};
    static const uint8_t sorder[] = {SET_NUMBER, SET_VERSION, SET_TYPE, SET_INITIALIZED, SET_KEYS};
    static const uint8_t korder[] = {KEY_SLOT, KEY_VALUE, KEY_VERSION};
    Fields f;
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
    uint64_t key_count, max_size;
    if(!r_uint(f_get(&f, KS_KEY_COUNT), DFC_MAX_KEYS, &key_count) || key_count == 0) {
        return DfcDerMalformed;
    }
    if(!r_uint(f_get(&f, KS_MAX_SIZE), 24, &max_size)) return DfcDerMalformed;
    if(max_size != 16 && max_size != 24) return DfcDerMalformed;
    uint8_t settings;
    if(!r_octets(f_get(&f, KS_SETTINGS), 1, &settings)) return DfcDerMalformed;
    const uint8_t* sets = f_get(&f, KS_SETS);
    if(!sets) return DfcDerMalformed;

    Slice probe = field_body(sets);
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

    Slice walk = field_body(sets);
    size_t index = 0;
    Tlv item;
    while(walk.len > 0) {
        if(!r_tlv(&walk, &item)) return DfcDerMalformed;
        Fields sf;
        if(!r_fields(item.body, sorder, TAG_COUNT(sorder), &sf)) return DfcDerMalformed;
        uint64_t number, type;
        if(!r_uint(f_get(&sf, SET_NUMBER), DFC_MAX_KEY_SETS - 1, &number)) return DfcDerMalformed;
        if(number != index) return DfcDerMalformed; // contiguous from zero
        if(!r_uint(f_get(&sf, SET_TYPE), 0xFF, &type)) return DfcDerMalformed;
        if(type > DFC_KEY_SET_TYPE_AES) return DfcDerMalformed;
        size_t required = dfc_key_set_type_length((uint8_t)type);
        if(required > max_size) return DfcDerMalformed;
        uint8_t* set_version = dfc_credential_key_set_version(c, app, index);
        uint8_t* set_type = dfc_credential_key_set_type(c, app, index);
        if(!set_version || !set_type || !r_octets(f_get(&sf, SET_VERSION), 1, set_version)) {
            return DfcDerMalformed;
        }
        *set_type = (uint8_t)type;
        // Required, so an absent flag is malformed.
        bool initialized;
        if(!r_bool(f_get(&sf, SET_INITIALIZED), &initialized) ||
           !dfc_credential_key_set_set_initialized(c, app, index, initialized)) {
            return DfcDerMalformed;
        }
        const uint8_t* keys = f_get(&sf, SET_KEYS);
        if(!keys) return DfcDerMalformed;
        Slice kwalk = field_body(keys);
        size_t slot = 0;
        Tlv kitem;
        while(kwalk.len > 0) {
            if(!r_tlv(&kwalk, &kitem)) return DfcDerMalformed;
            if(kitem.tag != TAG_SEQUENCE) return DfcDerMalformed;
            if(slot >= key_count) return DfcDerMalformed;
            Fields kf;
            if(!r_fields(kitem.body, korder, TAG_COUNT(korder), &kf)) return DfcDerMalformed;
            uint64_t number_slot;
            if(!r_uint(f_get(&kf, KEY_SLOT), DFC_MAX_KEYS - 1, &number_slot)) {
                return DfcDerMalformed;
            }
            if(number_slot != slot) return DfcDerMalformed;
            const uint8_t* value = f_get(&kf, KEY_VALUE);
            if(!value) return DfcDerMalformed;
            Slice value_body = field_body(value);
            if(value_body.len != required) return DfcDerMalformed;
            uint8_t* dst = dfc_credential_key_in_set(c, app, index, slot);
            uint8_t* version = dfc_credential_key_version_in_set(c, app, index, slot);
            if(!dst || !version) return DfcDerCapacity;
            memcpy(dst, value_body.p, value_body.len);
            if(!r_octets(f_get(&kf, KEY_VERSION), 1, version)) return DfcDerMalformed;
            slot++;
        }
        index++;
    }
    if(!dfc_credential_key_set_is_initialized(c, app, 0)) return DfcDerMalformed;
    return DfcDerOk;
}
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
static DfcDerStatus decode_delegated(DfcApplication* app, Slice body, uint64_t version) {
    static const uint8_t order[] = {
        DEL_SLOT, DEL_VERSION, DEL_QUOTA, DEL_FREE, DEL_FORMAT_DISABLED};
    Fields f;
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
    uint64_t slot, quota, free_blocks;
    if(!r_uint(f_get(&f, DEL_SLOT), 0xFFFFu, &slot)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, DEL_VERSION), 1, &app->delegated_slot_version)) return DfcDerMalformed;
    if(!r_uint(f_get(&f, DEL_QUOTA), 0xFFFFu, &quota)) return DfcDerMalformed;
    if(!r_uint(f_get(&f, DEL_FREE), 0xFFFFu, &free_blocks)) return DfcDerMalformed;
    if(free_blocks > quota) return DfcDerMalformed;
    app->delegated_slot_number = (uint16_t)slot;
    app->delegated_quota_limit = (uint16_t)quota;
    app->delegated_free_blocks = (uint16_t)free_blocks;
    if(version < 6 && f_get(&f, DEL_FORMAT_DISABLED)) return DfcDerMalformed;
    if(!r_default_false(&f, DEL_FORMAT_DISABLED, &app->delegated_format_disabled)) {
        return DfcDerMalformed;
    }
    app->delegated = true;
    return DfcDerOk;
}
#endif

static DfcDerStatus decode_proximity(DfcCredential* c, Slice body) {
#if DFC_ENABLE_PROXIMITY_CHECK
    static const uint8_t order[] = {PROX_KEY, PROX_OPTION, PROX_PUBLISHED, PROX_BITRATE};
    Fields f;
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, PROX_KEY), sizeof(c->picc_proximity_key), c->picc_proximity_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&f, PROX_OPTION), 1, &c->picc_proximity_option)) return DfcDerMalformed;
    uint64_t published;
    if(!r_uint(f_get(&f, PROX_PUBLISHED), 0xFFFFu, &published)) return DfcDerMalformed;
    c->picc_proximity_published_response_time = (uint16_t)published;
    const uint8_t* bitrate = f_get(&f, PROX_BITRATE);
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
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
    const uint8_t* install = f_get(&f, VC_INSTALL_ID);
    if(!install) return DfcDerMalformed;
    Slice install_body = field_body(install);
    if(install_body.len == 0) return DfcDerMalformed;
    if(install_body.len > DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH) return DfcDerMalformed;
    memcpy(c->virtual_card_installation_id, install_body.p, install_body.len);
    c->virtual_card_installation_id_len = (uint8_t)install_body.len;
    if(!r_octets(f_get(&f, VC_INFO), 1, &c->virtual_card_information)) return DfcDerMalformed;
    if(!r_octets(
           f_get(&f, VC_CAPS),
           sizeof(c->virtual_card_capabilities),
           c->virtual_card_capabilities)) {
        return DfcDerMalformed;
    }
    const uint8_t* uid = f_get(&f, VC_UID);
    if(!uid) return DfcDerMalformed;
    Slice uid_body = field_body(uid);
    if(uid_body.len != 4 && uid_body.len != 7 && uid_body.len != 10) return DfcDerMalformed;
    memcpy(c->virtual_card_uid, uid_body.p, uid_body.len);
    c->virtual_card_uid_len = (uint8_t)uid_body.len;
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
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
    if(!r_octets(f_get(&f, DAM_AUTH), sizeof(c->picc_dam_auth_key), c->picc_dam_auth_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&f, DAM_MAC), sizeof(c->picc_dam_mac_key), c->picc_dam_mac_key)) {
        return DfcDerMalformed;
    }
    if(!r_octets(
           f_get(&f, DAM_ENC), sizeof(c->picc_dam_encryption_key), c->picc_dam_encryption_key)) {
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
        if(!r_fields(tlv.body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;
        uint64_t slot;
        if(!r_uint(f_get(&f, KEY_SLOT), 13, &slot)) return DfcDerMalformed;
        if(slot != n) return DfcDerMalformed; // ascending, contiguous from 0
        const uint8_t* value = f_get(&f, KEY_VALUE);
        if(!value) return DfcDerMalformed;
        Slice value_body = field_body(value);
        if(value_body.len != key_len) return DfcDerMalformed;
        uint8_t* dst = dfc_credential_key(credential, app, n);
        if(!dst) return DfcDerCapacity;
        memcpy(dst, value_body.p, value_body.len);
        if(!r_octets(f_get(&f, KEY_VERSION), 1, &versions[n])) return DfcDerMalformed;
        n++;
    }
    return DfcDerOk;
}

static DfcDerStatus decode_file(DfcCredential* c, size_t owner, Slice body) {
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
    if(!r_fields(body, order, TAG_COUNT(order), &f)) return DfcDerMalformed;

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
    file->app_index = (uint8_t)owner;
    file->number = (uint8_t)number;
    file->type = (uint8_t)type;
    file->data_offset = DFC_FILE_POOL_NONE;

    if(!r_octets(f_get(&f, FILE_COMM), 1, &file->comm_settings)) return DfcDerMalformed;
    uint8_t rights[2];
    if(!r_octets(f_get(&f, FILE_RIGHTS), 2, rights)) return DfcDerMalformed;
    file->access_rights = (uint16_t)((rights[0] << 8) | rights[1]);

    const uint8_t* fid = f_get(&f, FILE_ISO_FID);
    if(fid) {
        uint8_t raw[2];
        if(!r_octets(fid, 2, raw)) return DfcDerMalformed;
        uint16_t v = (uint16_t)((raw[0] << 8) | raw[1]);
        if(dfc_iso_file_id_is_reserved(v)) return DfcDerMalformed;
        file->has_iso_file_id = true;
        file->iso_file_id = v;
    }

    const uint8_t* data = f_get(&f, FILE_DATA);
    const uint8_t* value = f_get(&f, FILE_VALUE);
    const uint8_t* record = f_get(&f, FILE_RECORD);
    const uint8_t* transaction_mac = f_get(&f, FILE_TMAC);
    size_t alternatives =
        (data ? 1 : 0) + (value ? 1 : 0) + (record ? 1 : 0) + (transaction_mac ? 1 : 0);
    if(alternatives != 1) return DfcDerMalformed;

    // The type selects the alternative (section 2.2.4 rule 5).
    if(file_is_data(file->type)) {
        if(!data) return DfcDerMalformed;
        static const uint8_t dorder[] = {DATA_SIZE, DATA_KNOWN, DATA_COMPLETE};
        Fields d;
        if(!r_fields(field_body(data), dorder, TAG_COUNT(dorder), &d)) return DfcDerMalformed;
        uint64_t size;
        if(!r_uint(f_get(&d, DATA_SIZE), U24_MAX, &size) || size == 0) return DfcDerMalformed;
        if(!dfc_file_set_declared_size(file, (uint32_t)size)) return DfcDerMalformed;
        if(!r_bool(f_get(&d, DATA_COMPLETE), &file->contents_complete)) return DfcDerMalformed;
        const uint8_t* known = f_get(&d, DATA_KNOWN);
        if(known) {
            Slice known_body = field_body(known);
            if(known_body.len == 0 || known_body.len > size) return DfcDerMalformed;
            if(!dfc_file_resize(c, file, known_body.len)) return DfcDerCapacity;
            uint8_t* dst = dfc_file_data(c, file);
            if(!dst) return DfcDerCapacity;
            memcpy(dst, known_body.p, known_body.len);
        }
        if(file->contents_complete && file->data_len != size) return DfcDerMalformed;
    } else if(file->type == FILE_TYPE_VALUE) {
        if(!value) return DfcDerMalformed;
        static const uint8_t vorder[] = {
            VAL_LOWER, VAL_UPPER, VAL_CURRENT, VAL_LIMITED, VAL_LIMIT};
        Fields v;
        if(!r_fields(field_body(value), vorder, TAG_COUNT(vorder), &v)) return DfcDerMalformed;
        int64_t lower, upper, current;
        if(!r_int(f_get(&v, VAL_LOWER), &lower)) return DfcDerMalformed;
        if(!r_int(f_get(&v, VAL_UPPER), &upper)) return DfcDerMalformed;
        if(!r_int(f_get(&v, VAL_CURRENT), &current)) return DfcDerMalformed;
        if(lower < INT32_MIN || upper > INT32_MAX || current < INT32_MIN || current > INT32_MAX) {
            return DfcDerMalformed;
        }
        if(lower > current || current > upper) return DfcDerMalformed;
        file->state.value.lower_limit = (int32_t)lower;
        file->state.value.upper_limit = (int32_t)upper;
        file->state.value.current = (int32_t)current;
        if(!r_octets(f_get(&v, VAL_LIMITED), 1, &file->state.value.limited_credit))
            return DfcDerMalformed;
        const uint8_t* limit_field = f_get(&v, VAL_LIMIT);
        if(limit_field) {
            int64_t limit;
            if(!r_int(limit_field, &limit) || limit < 0 || limit > INT32_MAX) {
                return DfcDerMalformed;
            }
            file->state.value.limited_credit_value = (int32_t)limit;
        }
    } else if(file->type == FILE_TYPE_TMAC) {
        if(!transaction_mac) return DfcDerMalformed;
#if DFC_ENABLE_TRANSACTION_MAC
        static const uint8_t torder[] = {
            TMAC_COUNTER, TMAC_VALUE, TMAC_KEY_TYPE, TMAC_KEY_VERSION, TMAC_KEY, TMAC_READER_ID};
        Fields tm;
        if(!r_fields(field_body(transaction_mac), torder, TAG_COUNT(torder), &tm)) {
            return DfcDerMalformed;
        }
        uint64_t counter;
        if(!r_uint(f_get(&tm, TMAC_COUNTER), UINT32_MAX, &counter)) return DfcDerMalformed;
        file->state.transaction_mac.counter = (uint32_t)counter;
        if(!r_octets(
               f_get(&tm, TMAC_VALUE),
               sizeof(file->state.transaction_mac.mac),
               file->state.transaction_mac.mac)) {
            return DfcDerMalformed;
        }
        if(!r_octets(f_get(&tm, TMAC_KEY_TYPE), 1, &file->state.transaction_mac.key_type)) {
            return DfcDerMalformed;
        }
        if(!r_octets(f_get(&tm, TMAC_KEY_VERSION), 1, &file->state.transaction_mac.key_version)) {
            return DfcDerMalformed;
        }
        if(!r_octets(
               f_get(&tm, TMAC_KEY),
               sizeof(file->state.transaction_mac.key),
               file->state.transaction_mac.key)) {
            return DfcDerMalformed;
        }
        if(!r_octets(
               f_get(&tm, TMAC_READER_ID),
               sizeof(file->state.transaction_mac.previous_reader_id),
               file->state.transaction_mac.previous_reader_id)) {
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
        if(!r_fields(field_body(record), rorder, TAG_COUNT(rorder), &r)) return DfcDerMalformed;
        uint64_t rsize, rmax, rcur;
        if(!r_uint(f_get(&r, REC_SIZE), U24_MAX, &rsize) || rsize == 0) return DfcDerMalformed;
        if(!r_uint(f_get(&r, REC_MAX), U24_MAX, &rmax) || rmax == 0) return DfcDerMalformed;
        if(!r_uint(f_get(&r, REC_CURRENT), U24_MAX, &rcur)) return DfcDerMalformed;
        if(rcur > rmax) return DfcDerMalformed;
        if(file->type == FILE_TYPE_CYCLIC && rmax < 2) return DfcDerMalformed;
        if(!r_bool(f_get(&r, REC_COMPLETE), &file->contents_complete)) return DfcDerMalformed;
        file->state.record.size = (uint32_t)rsize;
        file->state.record.max_records = (uint32_t)rmax;
        file->state.record.count = (uint32_t)rcur;

        const uint8_t* recs = f_get(&r, REC_RECORDS);
        if(!recs) return DfcDerMalformed;
        Slice walk = field_body(recs);
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
            walk = field_body(recs);
            size_t off = 0;
            while(walk.len > 0) {
                if(!r_tlv(&walk, &item)) return DfcDerMalformed;
                memcpy(dst + off, item.body.p, item.body.len);
                off += item.body.len;
            }
        }
    }

    const uint8_t* sdm = f_get(&f, FILE_SDM);
    if(sdm) {
#if DFC_ENABLE_SDM
        // Secure dynamic messaging belongs to a standard data file only.
        if(file->type != FILE_TYPE_STANDARD) return DfcDerMalformed;
        DfcDerStatus sdm_status = decode_sdm(file, field_body(sdm));
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

// Fills `credential` in place, field by field. A failure can arrive at any
// point, so this is only reached through dfc_der_decode, which owns the
// failure contract and discards whatever was written.
static DfcDerStatus decode_credential(DfcCredential* credential, const uint8_t* in, size_t len) {
    if(!in) return DfcDerMalformed;
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
    if(!r_fields(root.body, corder, TAG_COUNT(corder), &cf)) return DfcDerMalformed;

    uint64_t version;
    if(!r_uint(f_get(&cf, CRED_VERSION), 0xFF, &version)) return DfcDerMalformed;
    if(version < DFC_MIN_READ_FORMAT_VERSION || version > DFC_FORMAT_VERSION) {
        return DfcDerUnsupported;
    }

    dfc_credential_clear(credential);

    // Card
    static const uint8_t caorder_v4[] = {
        CARD_GENERATION, CARD_STORAGE, CARD_UID, CARD_PROVENANCE, CARD_SIGNATURE};
    static const uint8_t caorder_v5[] = {
        CARD_GENERATION,
        CARD_STORAGE,
        CARD_UID,
        CARD_PROVENANCE,
        CARD_SIGNATURE,
        CARD_HARDWARE_VERSION,
        CARD_SOFTWARE_VERSION};
    const uint8_t* card = f_get(&cf, CRED_CARD);
    Fields caf;
    const uint8_t* caorder = version == DFC_MIN_READ_FORMAT_VERSION ? caorder_v4 : caorder_v5;
    size_t caorder_len = version == DFC_MIN_READ_FORMAT_VERSION ? TAG_COUNT(caorder_v4) :
                                                                  TAG_COUNT(caorder_v5);
    if(!card || !r_fields(field_body(card), caorder, caorder_len, &caf)) return DfcDerMalformed;
    uint64_t generation, storage, provenance;
    if(!r_uint(f_get(&caf, CARD_GENERATION), 0xFF, &generation)) return DfcDerMalformed;
    if(generation < DfcGenerationEv1 || generation > DfcGenerationEv3) return DfcDerMalformed;
    if(!r_uint(f_get(&caf, CARD_STORAGE), 0xFFFFFFFFu, &storage)) return DfcDerMalformed;
    if(!r_uint(f_get(&caf, CARD_PROVENANCE), 0xFF, &provenance)) return DfcDerMalformed;
    if(provenance > DfcUidProvenanceUnknown) return DfcDerMalformed;
    const uint8_t* uid = f_get(&caf, CARD_UID);
    if(!uid) return DfcDerMalformed;
    Slice uid_body = field_body(uid);
    if(uid_body.len != DFC_DESFIRE_UID_SHORT_LEN && uid_body.len != DFC_DESFIRE_UID_LEN &&
       uid_body.len != DFC_DESFIRE_UID_LONG_LEN) {
        return DfcDerMalformed;
    }
    memcpy(credential->uid, uid_body.p, uid_body.len);
    credential->uid_len = (uint8_t)uid_body.len;
    credential->card.generation = (DfcGeneration)generation;
    credential->card.storage = (uint32_t)storage;
    credential->card.uid_provenance = (DfcUidProvenance)provenance;
    const uint8_t* hardware_version = f_get(&caf, CARD_HARDWARE_VERSION);
    if(hardware_version) {
        if(!r_octets(
               hardware_version,
               sizeof(credential->card.hardware_version),
               credential->card.hardware_version))
            return DfcDerMalformed;
        credential->card.has_hardware_version = true;
    }
    const uint8_t* software_version = f_get(&caf, CARD_SOFTWARE_VERSION);
    if(software_version) {
        if(!r_octets(
               software_version,
               sizeof(credential->card.software_version),
               credential->card.software_version))
            return DfcDerMalformed;
        credential->card.has_software_version = true;
    }
    const uint8_t* signature = f_get(&caf, CARD_SIGNATURE);
    if(signature) {
#if DFC_ENABLE_STATIC_SIGNATURE
        if(!r_octets(
               signature,
               sizeof(credential->picc_static_signature),
               credential->picc_static_signature)) {
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
        PICC_DAM,
        PICC_PREFERRED_AUTH,
        PICC_RANDOM_ID_LEGACY};
    const uint8_t* picc = f_get(&cf, CRED_PICC);
    Fields pf;
    if(!picc || !r_fields(field_body(picc), porder, TAG_COUNT(porder), &pf)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&pf, PICC_KS1), 1, &credential->picc_key_settings_1)) {
        return DfcDerMalformed;
    }
    if(!r_octets(f_get(&pf, PICC_KS2), 1, &credential->picc_key_settings_2)) {
        return DfcDerMalformed;
    }
    uint64_t picc_auth;
    if(!r_uint(f_get(&pf, PICC_AUTH), version >= 6 ? DFC_AUTH_COMMAND_ALL : 2, &picc_auth))
        return DfcDerMalformed;
    if(version >= 6) {
        credential->picc_auth_commands = (uint8_t)picc_auth;
        credential->picc_has_auth_commands = true;
        const uint8_t* preferred = f_get(&pf, PICC_PREFERRED_AUTH);
        if(preferred) {
            uint64_t value;
            if(!r_uint(preferred, DFC_AUTH_COMMAND_ALL, &value)) return DfcDerMalformed;
            credential->picc_preferred_auth_command = (uint8_t)value;
            credential->picc_has_preferred_auth_command = true;
        }
    } else {
        if(f_get(&pf, PICC_PREFERRED_AUTH)) return DfcDerMalformed;
        credential->picc_auth_command = auth_command_for(picc_auth);
        credential->picc_preferred_auth_command = (uint8_t)(1u << picc_auth);
        credential->picc_has_preferred_auth_command = true;
    }
    credential->picc_key_len = dfc_credential_key_length(credential->picc_key_settings_2);
    if(!r_default_false(&pf, PICC_RANDOM_ID, &credential->picc_random_id)) {
        return DfcDerMalformed;
    }
    if(version < 6 && f_get(&pf, PICC_RANDOM_ID_LEGACY)) return DfcDerMalformed;
    if(!r_default_false(&pf, PICC_RANDOM_ID_LEGACY, &credential->picc_random_id_legacy)) {
        return DfcDerMalformed;
    }
    if(!r_default_false(&pf, PICC_FORMAT_DIS, &credential->picc_format_disabled)) {
        return DfcDerMalformed;
    }
    const uint8_t* ats = f_get(&pf, PICC_ATS);
    if(ats) {
        Slice ats_body = field_body(ats);
        if(ats_body.len == 0) return DfcDerMalformed;
        if(ats_body.len > DFC_PICC_ATS_MAX) return DfcDerCapacity;
        memcpy(credential->picc_ats, ats_body.p, ats_body.len);
        credential->picc_ats_len = (uint8_t)ats_body.len;
    }
    const uint8_t* sak = f_get(&pf, PICC_SAK);
    if(sak) {
        if(!r_octets(sak, 1, &credential->picc_sak)) return DfcDerMalformed;
        credential->picc_has_sak = true;
    }
    const uint8_t* atqa = f_get(&pf, PICC_ATQA);
    if(atqa) {
        if(!r_octets(atqa, 2, credential->picc_atqa)) return DfcDerMalformed;
        credential->picc_has_atqa = true;
    }
    const uint8_t* smd = f_get(&pf, PICC_SM_DISABLE);
    if(smd) {
        if(!r_octets(smd, 1, &credential->picc_sm_disable)) return DfcDerMalformed;
        credential->picc_has_sm_disable = true;
    }
    const uint8_t* picc_keys = f_get(&pf, PICC_KEYS);
    if(!picc_keys) return DfcDerMalformed;
    DfcDerStatus st = decode_keys(
        credential, NULL, field_body(picc_keys), credential->picc_key_len, DFC_MAX_KEYS);
    if(st != DfcDerOk) return st;

    const uint8_t* ev2_caps = f_get(&pf, PICC_EV2_CAPS);
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
    const uint8_t* proximity = f_get(&pf, PICC_PROXIMITY);
    if(proximity) {
        st = decode_proximity(credential, field_body(proximity));
        if(st != DfcDerOk) return st;
    }
    const uint8_t* virtual_card = f_get(&pf, PICC_VCARD);
    if(virtual_card) {
        st = decode_virtual_card(credential, field_body(virtual_card));
        if(st != DfcDerOk) return st;
    }
    const uint8_t* dam = f_get(&pf, PICC_DAM);
    if(dam) {
        st = decode_dam(credential, field_body(dam));
        if(st != DfcDerOk) return st;
    }

    // Applications, before PICC files so app_index values resolve.
    const uint8_t* apps = f_get(&cf, CRED_APPS);
    if(!apps) return DfcDerMalformed;
    Slice walk = field_body(apps);
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
        APP_DELEGATED,
        APP_SM_DISABLE,
        APP_PREFERRED_AUTH};
    while(walk.len > 0) {
        if(!r_tlv(&walk, &tlv)) return DfcDerMalformed;
        if(tlv.tag != TAG_SEQUENCE) return DfcDerMalformed;
        if(credential->num_apps >= DFC_MAX_APPS) return DfcDerCapacity;
        Fields af;
        if(!r_fields(tlv.body, aorder, TAG_COUNT(aorder), &af)) return DfcDerMalformed;

        DfcApplication* app = &credential->apps[credential->num_apps];
        if(!r_octets(f_get(&af, APP_AID), 3, app->aid)) return DfcDerMalformed;
        for(size_t i = 0; i < credential->num_apps; i++) {
            if(memcmp(credential->apps[i].aid, app->aid, 3) == 0) return DfcDerMalformed;
        }
        const uint8_t* afid = f_get(&af, APP_ISO_FID);
        if(afid) {
            uint8_t raw[2];
            if(!r_octets(afid, 2, raw)) return DfcDerMalformed;
            uint16_t v = (uint16_t)((raw[0] << 8) | raw[1]);
            if(dfc_iso_file_id_is_reserved(v)) return DfcDerMalformed;
            app->has_iso_file_id = true;
            app->iso_file_id = v;
        }
        const uint8_t* df = f_get(&af, APP_DF_NAME);
        if(df) {
            Slice df_body = field_body(df);
            if(df_body.len == 0 || df_body.len > sizeof(app->iso_aid)) return DfcDerMalformed;
            memcpy(app->iso_aid, df_body.p, df_body.len);
            app->iso_aid_len = (uint8_t)df_body.len;
        }
        if(!r_octets(f_get(&af, APP_KS1), 1, &app->key_settings_1)) return DfcDerMalformed;
        if(!r_octets(f_get(&af, APP_KS2), 1, &app->key_settings_2)) return DfcDerMalformed;
        uint64_t app_auth;
        if(!r_uint(f_get(&af, APP_AUTH), version >= 6 ? DFC_AUTH_COMMAND_ALL : 2, &app_auth))
            return DfcDerMalformed;
        if(version >= 6) {
            app->auth_commands = (uint8_t)app_auth;
            app->has_auth_commands = true;
            const uint8_t* preferred = f_get(&af, APP_PREFERRED_AUTH);
            if(preferred) {
                uint64_t value;
                if(!r_uint(preferred, DFC_AUTH_COMMAND_ALL, &value)) return DfcDerMalformed;
                app->preferred_auth_command = (uint8_t)value;
                app->has_preferred_auth_command = true;
            }
        } else {
            if(f_get(&af, APP_PREFERRED_AUTH)) return DfcDerMalformed;
            app->auth_command = auth_command_for(app_auth);
            app->preferred_auth_command = (uint8_t)(1u << app_auth);
            app->has_preferred_auth_command = true;
        }
        app->key_len = dfc_credential_key_length(app->key_settings_2);

        const uint8_t* akeys = f_get(&af, APP_KEYS);
        const uint8_t* aksets = f_get(&af, APP_KEY_SETS);
        // Exactly one key storage alternative is required.
        if((akeys == NULL) == (aksets == NULL)) return DfcDerMalformed;
        if(akeys) {
            st = decode_keys(credential, app, field_body(akeys), app->key_len, DFC_MAX_KEYS);
            if(st != DfcDerOk) return st;
        } else {
#if DFC_ENABLE_KEY_SETS
            st = decode_key_sets(credential, app, field_body(aksets));
            if(st != DfcDerOk) return st;
#else
            // The field is recognized but the build omits the feature.
            return DfcDerUnsupported;
#endif
        }

        const uint8_t* capability = f_get(&af, APP_CAPABILITY);
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
        const uint8_t* delegated = f_get(&af, APP_DELEGATED);
        if(delegated) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
            st = decode_delegated(app, field_body(delegated), version);
            if(st != DfcDerOk) return st;
#else
            return DfcDerUnsupported;
#endif
        }
        const uint8_t* app_sm_disable = f_get(&af, APP_SM_DISABLE);
        if(app_sm_disable) {
            if(version < 6 || !r_octets(app_sm_disable, 1, &app->sm_disable)) {
                return DfcDerMalformed;
            }
            app->has_sm_disable = true;
        }

        size_t owner = credential->num_apps;
        credential->num_apps++;

        const uint8_t* afiles = f_get(&af, APP_FILES);
        if(!afiles) return DfcDerMalformed;
        st = decode_files(credential, owner, field_body(afiles));
        if(st != DfcDerOk) return st;
    }

    const uint8_t* pfiles = f_get(&pf, PICC_FILES);
    if(!pfiles) return DfcDerMalformed;
    st = decode_files(credential, DFC_FILE_OWNER_PICC, field_body(pfiles));
    if(st != DfcDerOk) return st;

    st = dfc_credential_validate_model(credential);
    if(st != DfcDerOk) return st;

    credential->dirty = false;
    return DfcDerOk;
}

DfcDerStatus dfc_der_decode(DfcCredential* credential, const uint8_t* in, size_t len) {
    if(!credential) return DfcDerMalformed;
    DfcDerStatus status = decode_credential(credential, in, len);
    // One exit for every failure: the credential is the empty credential, with
    // any key material decoded before the fault wiped, never a partial one.
    if(status != DfcDerOk) dfc_credential_clear(credential);
    return status;
}

#endif // DFC_ENABLE_DER_DECODER

#endif // DFC_ENABLE_BINARY_CODEC
