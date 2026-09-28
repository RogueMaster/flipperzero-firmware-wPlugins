#include "dfc_ffi.h"

#include <stdlib.h>
#include <string.h>

#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_port_ffi.h"
#include "dfc_text.h"

#if DFC_ENABLE_EMULATOR
#include "dfc_virtual_picc.h"
#endif
#if DFC_ENABLE_READER
#include "dfc_reader.h"
#endif

#if !DFC_ENABLE_FFI
#error "Compile ffi/ with DFC_ENABLE_FFI=1"
#endif

struct DfcFfiCredential {
    DfcCredential model;
};

#if DFC_ENABLE_EMULATOR
struct DfcFfiPicc {
    DfcCredential model;
    DfcVirtualPiccSession* session;
    DfcFfiRandomCallback random;
    void* random_context;
};
#endif

#if DFC_ENABLE_READER
struct DfcFfiReaderSession {
    DfcReaderSession session;
};

struct DfcFfiReaderExchange {
    DfcReaderExchange exchange;
};
#endif

static void wipe(void* p, size_t n) {
    volatile uint8_t* bytes = p;
    while(n--) *bytes++ = 0;
}

static void set_error(DfcFfiError* error, int32_t status, uint32_t line, const char* message) {
    if(!error) return;
    error->status = status;
    error->line = line;
    memset(error->message, 0, sizeof(error->message));
    if(message) {
        size_t n = strlen(message);
        if(n >= sizeof(error->message)) n = sizeof(error->message) - 1;
        memcpy(error->message, message, n);
    }
}

static DfcFfiCredential* credential_alloc(void) {
    DfcFfiCredential* credential = calloc(1, sizeof(*credential));
    if(credential) dfc_credential_clear(&credential->model);
    return credential;
}

// ------------------------------------------------------------ library ---

uint32_t dfc_ffi_abi_version(void) {
    return DFC_FFI_ABI_VERSION;
}

int32_t dfc_ffi_random_fill(uint8_t* out, size_t len) {
    if(len > 0 && !out) return DFC_FFI_INVALID_ARGUMENT;
    if(len > 0) dfc_ffi_port_system_random(out, len);
    return DFC_FFI_OK;
}

bool dfc_ffi_fixed_time_equal(const uint8_t* left, const uint8_t* right, size_t len) {
    if(len > 0 && (!left || !right)) return false;
    volatile uint8_t difference = 0;
    for(size_t index = 0; index < len; index++) difference |= left[index] ^ right[index];
    return difference == 0;
}

void dfc_ffi_secure_zero(void* buffer, size_t len) {
    if(buffer) wipe(buffer, len);
}

void dfc_ffi_capabilities(DfcFfiCapabilities* out) {
    if(!out) return;
    memset(out, 0, sizeof(*out));
    DfcBuildCapabilities c = dfc_build_capabilities();
    out->abi_version = DFC_FFI_ABI_VERSION;
    out->build_role = DFC_BUILD_ROLE;
    out->build_profile = DFC_BUILD_PROFILE;
    out->generation_ev1 = c.generation_ev1;
    out->generation_ev2 = c.generation_ev2;
    out->generation_ev3 = c.generation_ev3;
    out->storage_2k = c.storage_2k;
    out->storage_4k = c.storage_4k;
    out->storage_8k = c.storage_8k;
    out->ev2_secure_messaging = c.ev2_secure_messaging;
    out->iso7816 = c.iso7816;
    out->transaction_mac = c.transaction_mac;
    out->secure_dynamic_messaging = c.secure_dynamic_messaging;
    out->proximity_check = c.proximity_check;
    out->virtual_card = c.virtual_card;
    out->key_sets = c.key_sets;
    out->delegated_applications = c.delegated_applications;
    out->transaction_timer = c.transaction_timer;
    out->application_capability_data = c.application_capability_data;
    out->static_signature = c.static_signature;
    out->auth_d40 = c.auth_d40;
    out->auth_iso = c.auth_iso;
    out->auth_aes = c.auth_aes;
    out->emulator = c.emulator;
    out->reader = c.reader;
    out->der_decoder = c.der_decoder;
    out->der_encoder = c.der_encoder;
    out->text_codec = c.text_codec;
    out->max_applications = DFC_MAX_APPS;
    out->max_files = DFC_MAX_FILES;
    out->max_keys = DFC_MAX_KEYS;
    out->file_pool_size = DFC_FILE_POOL_SIZE;
    out->key_pool_size = DFC_KEY_POOL_SIZE;
}

void dfc_ffi_struct_sizes(DfcFfiStructSizes* out) {
    if(!out) return;
    out->capabilities = sizeof(DfcFfiCapabilities);
    out->error = sizeof(DfcFfiError);
    out->card = sizeof(DfcFfiCard);
    out->picc = sizeof(DfcFfiPiccSettings);
    out->application = sizeof(DfcFfiApplication);
    out->file = sizeof(DfcFfiFile);
    out->key = sizeof(DfcFfiKey);
    out->activation = sizeof(DfcFfiActivation);
    out->snapshot = sizeof(DfcFfiSnapshot);
    out->reader_state = sizeof(DfcFfiReaderState);
}

// --------------------------------------------------------- credential I/O ---

int32_t dfc_ffi_credential_parse_text(
    const char* text,
    size_t len,
    DfcFfiCredential** out,
    DfcFfiError* error) {
    set_error(error, DfcDerOk, 0, NULL);
    if(!out || (!text && len)) {
        set_error(error, DFC_FFI_INVALID_ARGUMENT, 0, "invalid argument");
        return DFC_FFI_INVALID_ARGUMENT;
    }
    *out = NULL;
#if DFC_ENABLE_TEXT_CODEC
    DfcFfiCredential* credential = credential_alloc();
    if(!credential) return DFC_FFI_OUT_OF_MEMORY;
    DfcTextError detail = {0};
    DfcTextStatus st = dfc_text_parse(&credential->model, text, len, &detail);
    if(st != DfcTextOk) {
        set_error(error, st, (uint32_t)detail.line, detail.message);
        dfc_ffi_credential_free(credential);
        return st;
    }
    *out = credential;
    return DfcDerOk;
#else
    DFC_UNUSED(text);
    DFC_UNUSED(len);
    set_error(error, DFC_FFI_UNSUPPORTED, 0, "this build carries no text codec");
    return DFC_FFI_UNSUPPORTED;
#endif
}

int32_t dfc_ffi_credential_decode(
    const uint8_t* dfcb,
    size_t len,
    DfcFfiCredential** out,
    DfcFfiError* error) {
    set_error(error, DfcDerOk, 0, NULL);
    if(!out || (!dfcb && len)) {
        set_error(error, DFC_FFI_INVALID_ARGUMENT, 0, "invalid argument");
        return DFC_FFI_INVALID_ARGUMENT;
    }
    *out = NULL;
    DfcFfiCredential* credential = credential_alloc();
    if(!credential) return DFC_FFI_OUT_OF_MEMORY;
    DfcDerStatus st = dfc_der_decode(&credential->model, dfcb, len);
    if(st != DfcDerOk) {
        set_error(error, st, 0, dfc_der_status_name(st));
        dfc_ffi_credential_free(credential);
        return st;
    }
    *out = credential;
    return DfcDerOk;
}

int32_t dfc_ffi_credential_load(
    const uint8_t* content,
    size_t len,
    DfcFfiCredential** out,
    DfcFfiError* error) {
    if(content && len > 0 && content[0] == DFC_DER_CREDENTIAL_TAG) {
        return dfc_ffi_credential_decode(content, len, out, error);
    }
    return dfc_ffi_credential_parse_text((const char*)content, len, out, error);
}

int32_t dfc_ffi_credential_encode(
    const DfcFfiCredential* credential,
    uint8_t* out,
    size_t cap,
    size_t* len) {
    if(!credential || !len || (!out && cap)) return DFC_FFI_INVALID_ARGUMENT;
#if DFC_ENABLE_DER_ENCODER
    if(!out) return dfc_der_encoded_size(&credential->model, len);
    DfcDerStatus st = dfc_der_encoded_size(&credential->model, len);
    if(st != DfcDerOk) return st;
    if(*len > cap) return DFC_FFI_BUFFER_TOO_SMALL;
    return dfc_der_encode(&credential->model, out, cap, len);
#else
    DFC_UNUSED(out);
    DFC_UNUSED(cap);
    return DFC_FFI_UNSUPPORTED;
#endif
}

int32_t dfc_ffi_credential_write_text(
    const DfcFfiCredential* credential,
    char* out,
    size_t cap,
    size_t* len) {
    if(!credential || !len || (!out && cap)) return DFC_FFI_INVALID_ARGUMENT;
#if DFC_ENABLE_TEXT_CODEC
    DfcTextStatus st = dfc_text_write(&credential->model, out, cap, len);
    if(st == DfcTextCapacity && out) return DFC_FFI_BUFFER_TOO_SMALL;
    if(st == DfcTextCapacity && !out) return DfcDerOk;
    return st;
#else
    DFC_UNUSED(out);
    DFC_UNUSED(cap);
    return DFC_FFI_UNSUPPORTED;
#endif
}

int32_t dfc_ffi_credential_validate(const DfcFfiCredential* credential) {
    if(!credential) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_der_validate_model(&credential->model);
}

void dfc_ffi_credential_free(DfcFfiCredential* credential) {
    if(!credential) return;
    wipe(credential, sizeof(*credential));
    free(credential);
}

// ------------------------------------------------------ model: reading ---

int32_t dfc_ffi_credential_get_card(const DfcFfiCredential* credential, DfcFfiCard* out) {
    if(!credential || !out) return DFC_FFI_INVALID_ARGUMENT;
    const DfcCredential* c = &credential->model;
    memset(out, 0, sizeof(*out));
    out->generation = (uint32_t)c->card.generation;
    out->storage = c->card.storage;
    out->uid_provenance = (uint32_t)c->card.uid_provenance;
    out->uid_len = (uint32_t)c->uid_len;
    memcpy(out->uid, c->uid, c->uid_len);
    out->has_hardware_version = c->card.has_hardware_version;
    memcpy(out->hardware_version, c->card.hardware_version, sizeof(out->hardware_version));
    out->has_software_version = c->card.has_software_version;
    memcpy(out->software_version, c->card.software_version, sizeof(out->software_version));
#if DFC_ENABLE_STATIC_SIGNATURE
    out->has_static_signature = c->picc_has_static_signature;
    memcpy(out->static_signature, c->picc_static_signature, sizeof(out->static_signature));
#endif
    return DfcDerOk;
}

int32_t dfc_ffi_credential_get_picc(const DfcFfiCredential* credential, DfcFfiPiccSettings* out) {
    if(!credential || !out) return DFC_FFI_INVALID_ARGUMENT;
    const DfcCredential* c = &credential->model;
    memset(out, 0, sizeof(*out));
    out->num_keys = (uint32_t)c->picc_num_keys;
    out->key_len = (uint32_t)c->picc_key_len;
    out->key_settings_1 = c->picc_key_settings_1;
    out->key_settings_2 = c->picc_key_settings_2;
    out->auth_command = c->picc_auth_command;
    out->has_auth_commands = c->picc_has_auth_commands;
    out->auth_commands = c->picc_auth_commands;
    out->has_preferred_auth_command = c->picc_has_preferred_auth_command;
    out->preferred_auth_command = c->picc_preferred_auth_command;
    out->random_id = c->picc_random_id;
    out->format_disabled = c->picc_format_disabled;
    out->ats_len = (uint8_t)c->picc_ats_len;
    memcpy(out->ats, c->picc_ats, c->picc_ats_len);
    out->has_sak = c->picc_has_sak;
    out->sak = c->picc_sak;
    out->has_atqa = c->picc_has_atqa;
    memcpy(out->atqa, c->picc_atqa, sizeof(out->atqa));
    out->has_sm_disable = c->picc_has_sm_disable;
    out->sm_disable = c->picc_sm_disable;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    out->has_ev2_capabilities = c->picc_has_ev2_capabilities;
    memcpy(out->ev2_capabilities, c->picc_ev2_capabilities, sizeof(out->ev2_capabilities));
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    out->has_proximity_key = c->picc_has_proximity_key;
    memcpy(out->proximity_key, c->picc_proximity_key, sizeof(out->proximity_key));
    out->proximity_option = c->picc_proximity_option;
    out->proximity_published_response_time = c->picc_proximity_published_response_time;
    out->has_proximity_bitrate = c->picc_has_proximity_bitrate;
    out->proximity_bitrate = c->picc_proximity_bitrate;
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    out->virtual_card_configured = c->virtual_card_configured;
    out->virtual_card_installation_id_len = (uint8_t)c->virtual_card_installation_id_len;
    memcpy(
        out->virtual_card_installation_id,
        c->virtual_card_installation_id,
        sizeof(out->virtual_card_installation_id));
    out->virtual_card_information = c->virtual_card_information;
    memcpy(
        out->virtual_card_capabilities,
        c->virtual_card_capabilities,
        sizeof(out->virtual_card_capabilities));
    out->virtual_card_uid_len = (uint8_t)c->virtual_card_uid_len;
    memcpy(out->virtual_card_uid, c->virtual_card_uid, sizeof(out->virtual_card_uid));
    memcpy(
        out->virtual_card_select_mac_key,
        c->virtual_card_select_mac_key,
        sizeof(out->virtual_card_select_mac_key));
    memcpy(
        out->virtual_card_select_encryption_key,
        c->virtual_card_select_encryption_key,
        sizeof(out->virtual_card_select_encryption_key));
    out->virtual_card_authentication_mandatory = c->virtual_card_authentication_mandatory;
    out->virtual_card_proximity_mandatory = c->virtual_card_proximity_mandatory;
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    out->has_dam_keys = c->picc_has_dam_keys;
    memcpy(out->dam_auth_key, c->picc_dam_auth_key, sizeof(out->dam_auth_key));
    memcpy(out->dam_mac_key, c->picc_dam_mac_key, sizeof(out->dam_mac_key));
    memcpy(out->dam_encryption_key, c->picc_dam_encryption_key, sizeof(out->dam_encryption_key));
#endif
    return DfcDerOk;
}

uint32_t dfc_ffi_credential_application_count(const DfcFfiCredential* credential) {
    return credential ? (uint32_t)credential->model.num_apps : 0;
}

int32_t dfc_ffi_credential_get_application(
    const DfcFfiCredential* credential,
    uint32_t index,
    DfcFfiApplication* out) {
    if(!credential || !out || index >= credential->model.num_apps) return DFC_FFI_INVALID_ARGUMENT;
    const DfcApplication* a = &credential->model.apps[index];
    memset(out, 0, sizeof(*out));
    memcpy(out->aid, a->aid, sizeof(out->aid));
    out->has_iso_file_id = a->has_iso_file_id;
    out->iso_file_id = a->iso_file_id;
    out->df_name_len = (uint32_t)a->iso_aid_len;
    memcpy(out->df_name, a->iso_aid, a->iso_aid_len);
    out->key_settings_1 = a->key_settings_1;
    out->key_settings_2 = a->key_settings_2;
    out->auth_command = a->auth_command;
    out->has_auth_commands = a->has_auth_commands;
    out->auth_commands = a->auth_commands;
    out->has_preferred_auth_command = a->has_preferred_auth_command;
    out->preferred_auth_command = a->preferred_auth_command;
    out->has_sm_disable = a->has_sm_disable;
    out->sm_disable = a->sm_disable;
    out->num_keys = (uint32_t)a->num_keys;
    out->key_len = (uint32_t)a->key_len;
#if DFC_ENABLE_KEY_SETS
    if(a->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT) {
        out->num_key_sets = a->num_key_sets;
        out->key_set_max_size = a->max_key_size;
        out->key_set_settings = a->key_set_settings;
        for(size_t i = 0; i < DFC_MAX_KEY_SETS; i++) {
            out->key_set_versions[i] = a->key_set_versions[i];
            out->key_set_types[i] = a->key_set_types[i];
            out->key_set_initialized[i] = a->key_set_initialized[i];
        }
    }
#endif
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
    out->has_capability_data = a->has_capability_data;
    memcpy(out->capability_data, a->capability_data, sizeof(out->capability_data));
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    out->delegated = a->delegated;
    out->delegated_slot_number = a->delegated_slot_number;
    out->delegated_slot_version = a->delegated_slot_version;
    out->delegated_quota_limit = a->delegated_quota_limit;
    out->delegated_free_blocks = a->delegated_free_blocks;
#endif
    return DfcDerOk;
}

uint32_t dfc_ffi_credential_file_count(const DfcFfiCredential* credential) {
    return credential ? (uint32_t)credential->model.num_files : 0;
}

int32_t
    dfc_ffi_credential_get_file(const DfcFfiCredential* credential, uint32_t index, DfcFfiFile* out) {
    if(!credential || !out || index >= credential->model.num_files) return DFC_FFI_INVALID_ARGUMENT;
    const DfcFile* f = &credential->model.files[index];
    memset(out, 0, sizeof(*out));
    out->owner = f->app_index == DFC_FILE_OWNER_PICC ? DFC_FFI_OWNER_PICC : (int32_t)f->app_index;
    out->number = f->number;
    out->type = f->type;
    out->comm_settings = f->comm_settings;
    out->access_rights = f->access_rights;
    out->has_iso_file_id = f->has_iso_file_id;
    out->iso_file_id = f->iso_file_id;
    out->declared_size = f->declared_size;
    out->data_len = (uint32_t)f->data_len;
    out->contents_complete = f->contents_complete;
    out->transaction_pending = f->transaction_pending;
    out->value_lower_limit = f->value_lower_limit;
    out->value_upper_limit = f->value_upper_limit;
    out->value = f->value;
    out->limited_credit = f->limited_credit;
    out->value_pending = f->value_pending;
    out->value_pending_delta = f->value_pending_delta;
    out->record_size = f->record_size;
    out->max_records = f->max_records;
    out->record_count = f->record_count;
#if DFC_ENABLE_SDM
    out->sdm_enabled = f->sdm_enabled;
    out->sdm_options = f->sdm_options;
    out->sdm_access_rights = f->sdm_access_rights;
    out->sdm_has_uid_offset = f->sdm_has_uid_offset;
    out->sdm_uid_offset = f->sdm_uid_offset;
    out->sdm_has_counter_offset = f->sdm_has_counter_offset;
    out->sdm_counter_offset = f->sdm_counter_offset;
    out->sdm_has_picc_data_offset = f->sdm_has_picc_data_offset;
    out->sdm_picc_data_offset = f->sdm_picc_data_offset;
    out->sdm_has_mac_input_offset = f->sdm_has_mac_input_offset;
    out->sdm_mac_input_offset = f->sdm_mac_input_offset;
    out->sdm_has_mac_offset = f->sdm_has_mac_offset;
    out->sdm_mac_offset = f->sdm_mac_offset;
    out->sdm_has_encrypted_file_offset = f->sdm_has_encrypted_file_offset;
    out->sdm_encrypted_file_offset = f->sdm_encrypted_file_offset;
    out->sdm_encrypted_file_length = f->sdm_encrypted_file_length;
    out->sdm_has_counter_limit = f->sdm_has_counter_limit;
    out->sdm_counter_limit = f->sdm_counter_limit;
    out->sdm_read_counter = f->sdm_read_counter;
#endif
#if DFC_ENABLE_TRANSACTION_MAC
    memcpy(out->transaction_mac_key, f->transaction_mac_key, sizeof(out->transaction_mac_key));
    out->transaction_mac_key_type = f->transaction_mac_key_type;
    out->transaction_mac_key_version = f->transaction_mac_key_version;
    out->transaction_counter = f->transaction_counter;
    memcpy(out->transaction_mac, f->transaction_mac, sizeof(out->transaction_mac));
    memcpy(out->previous_reader_id, f->previous_reader_id, sizeof(out->previous_reader_id));
#endif
    return DfcDerOk;
}

int32_t dfc_ffi_credential_get_file_data(
    const DfcFfiCredential* credential,
    uint32_t index,
    uint8_t* out,
    size_t cap,
    size_t* len) {
    if(!credential || !len || (!out && cap) || index >= credential->model.num_files) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    const DfcFile* f = &credential->model.files[index];
    *len = f->data_len;
    if(!out) return DfcDerOk;
    if(f->data_len > cap) return DFC_FFI_BUFFER_TOO_SMALL;
    const uint8_t* data = dfc_file_data_const(&credential->model, f);
    if(f->data_len && data) memcpy(out, data, f->data_len);
    return DfcDerOk;
}

// The owner's application, or NULL for the PICC. False for a bad index.
static bool owner_application(
    const DfcCredential* c,
    int32_t owner,
    const DfcApplication** app) {
    if(owner == DFC_FFI_OWNER_PICC) {
        *app = NULL;
        return true;
    }
    if(owner < 0 || (size_t)owner >= c->num_apps) return false;
    *app = &c->apps[owner];
    return true;
}

int32_t dfc_ffi_credential_get_key(
    const DfcFfiCredential* credential,
    int32_t owner,
    uint32_t key_set,
    uint32_t slot,
    DfcFfiKey* out) {
    if(!credential || !out) return DFC_FFI_INVALID_ARGUMENT;
    const DfcCredential* c = &credential->model;
    const DfcApplication* app;
    if(!owner_application(c, owner, &app)) return DFC_FFI_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if(key_set == 0) {
        size_t num_keys = app ? app->num_keys : c->picc_num_keys;
        size_t key_len = app ? app->key_len : c->picc_key_len;
        if(slot >= num_keys) return DFC_FFI_INVALID_ARGUMENT;
        const uint8_t* key = dfc_credential_key_const(c, app, slot);
        out->len = (uint32_t)key_len;
        if(key) memcpy(out->value, key, key_len);
        out->version = app ? app->key_versions[slot] : c->picc_key_versions[slot];
        return DfcDerOk;
    }
#if DFC_ENABLE_KEY_SETS
    if(!app || key_set >= app->num_key_sets || slot >= app->num_keys) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    const uint8_t* key = dfc_credential_key_in_set_const(c, app, key_set, slot);
    uint8_t* version =
        dfc_credential_key_version_in_set((DfcApplication*)app, key_set, slot);
    size_t key_len = app->key_set_types[key_set] == DFC_KEY_SET_TYPE_3K3DES ? 24 : 16;
    out->len = (uint32_t)key_len;
    if(key) memcpy(out->value, key, key_len);
    if(version) out->version = *version;
    return DfcDerOk;
#else
    return DFC_FFI_UNSUPPORTED;
#endif
}

// ----------------------------------------------------- model: building ---

int32_t dfc_ffi_credential_new(DfcFfiCredential** out) {
    if(!out) return DFC_FFI_INVALID_ARGUMENT;
    *out = credential_alloc();
    if(!*out) return DFC_FFI_OUT_OF_MEMORY;
    // Start from nothing rather than the seeded master key, so the builder's
    // model is exactly what the caller sets.
    dfc_credential_keys_release(&(*out)->model, NULL);
    return DfcDerOk;
}

int32_t dfc_ffi_credential_set_card(DfcFfiCredential* credential, const DfcFfiCard* card) {
    if(!credential || !card || card->uid_len > DFC_DESFIRE_UID_MAX_LENGTH) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    DfcCredential* c = &credential->model;
    c->card.generation = (DfcGeneration)card->generation;
    c->card.storage = card->storage;
    c->card.uid_provenance = (DfcUidProvenance)card->uid_provenance;
    memcpy(c->uid, card->uid, card->uid_len);
    c->uid_len = card->uid_len;
    c->card.has_hardware_version = card->has_hardware_version != 0;
    memcpy(c->card.hardware_version, card->hardware_version, sizeof(c->card.hardware_version));
    c->card.has_software_version = card->has_software_version != 0;
    memcpy(c->card.software_version, card->software_version, sizeof(c->card.software_version));
    if(card->has_static_signature) {
#if DFC_ENABLE_STATIC_SIGNATURE
        c->picc_has_static_signature = true;
        memcpy(c->picc_static_signature, card->static_signature, sizeof(c->picc_static_signature));
#else
        return DfcDerUnsupported;
#endif
    }
    return DfcDerOk;
}

int32_t dfc_ffi_credential_set_picc(DfcFfiCredential* credential, const DfcFfiPiccSettings* p) {
    if(!credential || !p || p->ats_len > DFC_PICC_ATS_MAX || p->num_keys > DFC_MAX_KEYS ||
       p->key_len > DFC_MAX_KEY_LEN) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    DfcCredential* c = &credential->model;
    c->picc_key_settings_1 = p->key_settings_1;
    c->picc_key_settings_2 = p->key_settings_2;
    c->picc_auth_command = p->auth_command;
    c->picc_has_auth_commands = p->has_auth_commands != 0;
    c->picc_auth_commands = p->auth_commands;
    c->picc_has_preferred_auth_command = p->has_preferred_auth_command != 0;
    c->picc_preferred_auth_command = p->preferred_auth_command;
    c->picc_random_id = p->random_id != 0;
    c->picc_format_disabled = p->format_disabled != 0;
    c->picc_ats_len = p->ats_len;
    memcpy(c->picc_ats, p->ats, p->ats_len);
    c->picc_has_sak = p->has_sak != 0;
    c->picc_sak = p->sak;
    c->picc_has_atqa = p->has_atqa != 0;
    memcpy(c->picc_atqa, p->atqa, sizeof(c->picc_atqa));
    c->picc_has_sm_disable = p->has_sm_disable != 0;
    c->picc_sm_disable = p->sm_disable;
    if(p->has_ev2_capabilities) {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
        c->picc_has_ev2_capabilities = true;
        memcpy(c->picc_ev2_capabilities, p->ev2_capabilities, sizeof(c->picc_ev2_capabilities));
#else
        return DfcDerUnsupported;
#endif
    }
    if(p->has_proximity_key) {
#if DFC_ENABLE_PROXIMITY_CHECK
        c->picc_has_proximity_key = true;
        memcpy(c->picc_proximity_key, p->proximity_key, sizeof(c->picc_proximity_key));
        c->picc_proximity_option = p->proximity_option;
        c->picc_proximity_published_response_time = p->proximity_published_response_time;
        c->picc_has_proximity_bitrate = p->has_proximity_bitrate != 0;
        c->picc_proximity_bitrate = p->proximity_bitrate;
#else
        return DfcDerUnsupported;
#endif
    }
    if(p->virtual_card_configured) {
#if DFC_ENABLE_VIRTUAL_CARD
        if(p->virtual_card_installation_id_len > DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH ||
           p->virtual_card_uid_len > DFC_VIRTUAL_CARD_UID_MAX_LENGTH) {
            return DFC_FFI_INVALID_ARGUMENT;
        }
        c->virtual_card_configured = true;
        c->virtual_card_installation_id_len = p->virtual_card_installation_id_len;
        memcpy(
            c->virtual_card_installation_id,
            p->virtual_card_installation_id,
            sizeof(c->virtual_card_installation_id));
        c->virtual_card_information = p->virtual_card_information;
        memcpy(
            c->virtual_card_capabilities,
            p->virtual_card_capabilities,
            sizeof(c->virtual_card_capabilities));
        c->virtual_card_uid_len = p->virtual_card_uid_len;
        memcpy(c->virtual_card_uid, p->virtual_card_uid, sizeof(c->virtual_card_uid));
        memcpy(
            c->virtual_card_select_mac_key,
            p->virtual_card_select_mac_key,
            sizeof(c->virtual_card_select_mac_key));
        memcpy(
            c->virtual_card_select_encryption_key,
            p->virtual_card_select_encryption_key,
            sizeof(c->virtual_card_select_encryption_key));
        c->virtual_card_authentication_mandatory = p->virtual_card_authentication_mandatory != 0;
        c->virtual_card_proximity_mandatory = p->virtual_card_proximity_mandatory != 0;
#else
        return DfcDerUnsupported;
#endif
    }
    if(p->has_dam_keys) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        c->picc_has_dam_keys = true;
        memcpy(c->picc_dam_auth_key, p->dam_auth_key, sizeof(c->picc_dam_auth_key));
        memcpy(c->picc_dam_mac_key, p->dam_mac_key, sizeof(c->picc_dam_mac_key));
        memcpy(c->picc_dam_encryption_key, p->dam_encryption_key, sizeof(c->picc_dam_encryption_key));
#else
        return DfcDerUnsupported;
#endif
    }
    if(p->num_keys == 0) {
        dfc_credential_keys_release(c, NULL);
    } else if(!dfc_credential_keys_resize(c, NULL, p->num_keys, p->key_len)) {
        return DfcDerCapacity;
    }
    return DfcDerOk;
}

int32_t dfc_ffi_credential_add_application(
    DfcFfiCredential* credential,
    const DfcFfiApplication* a,
    uint32_t* index) {
    if(!credential || !a || a->df_name_len > 16 || a->num_keys > DFC_MAX_KEYS ||
       a->key_len > DFC_MAX_KEY_LEN) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    DfcCredential* c = &credential->model;
    if(c->num_apps >= DFC_MAX_APPS) return DfcDerCapacity;
    for(size_t i = 0; i < c->num_apps; i++) {
        if(memcmp(c->apps[i].aid, a->aid, 3) == 0) return DfcDerMalformed;
    }
    DfcApplication* app = &c->apps[c->num_apps];
    dfc_credential_reset_application(app);
    memcpy(app->aid, a->aid, sizeof(app->aid));
    app->has_iso_file_id = a->has_iso_file_id != 0;
    app->iso_file_id = a->iso_file_id;
    app->iso_aid_len = a->df_name_len;
    memcpy(app->iso_aid, a->df_name, a->df_name_len);
    app->key_settings_1 = a->key_settings_1;
    app->key_settings_2 = a->key_settings_2;
    app->auth_command = a->auth_command;
    app->has_auth_commands = a->has_auth_commands != 0;
    app->auth_commands = a->auth_commands;
    app->has_preferred_auth_command = a->has_preferred_auth_command != 0;
    app->preferred_auth_command = a->preferred_auth_command;
    app->has_sm_disable = a->has_sm_disable != 0;
    app->sm_disable = a->sm_disable;
    app->key_len = a->key_len;

    if(a->num_key_sets >= DFC_KEY_SET_MINIMUM_COUNT) {
#if DFC_ENABLE_KEY_SETS
        if(a->num_key_sets > DFC_MAX_KEY_SETS) return DfcDerCapacity;
        if(!dfc_credential_key_sets_resize(
               c, app, a->num_key_sets, a->num_keys, a->key_len, a->key_set_max_size)) {
            return DfcDerCapacity;
        }
        app->key_set_settings = a->key_set_settings;
        for(size_t i = 0; i < a->num_key_sets; i++) {
            app->key_set_versions[i] = a->key_set_versions[i];
            app->key_set_types[i] = a->key_set_types[i];
            app->key_set_initialized[i] = a->key_set_initialized[i] != 0;
        }
#else
        return DfcDerUnsupported;
#endif
    } else if(a->num_keys > 0) {
        if(!dfc_credential_keys_resize(c, app, a->num_keys, a->key_len)) return DfcDerCapacity;
    }
    if(a->has_capability_data) {
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA
        app->has_capability_data = true;
        memcpy(app->capability_data, a->capability_data, sizeof(app->capability_data));
#else
        return DfcDerUnsupported;
#endif
    }
    if(a->delegated) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        app->delegated = true;
        app->delegated_slot_number = a->delegated_slot_number;
        app->delegated_slot_version = a->delegated_slot_version;
        app->delegated_quota_limit = a->delegated_quota_limit;
        app->delegated_free_blocks = a->delegated_free_blocks;
#else
        return DfcDerUnsupported;
#endif
    }
    if(index) *index = (uint32_t)c->num_apps;
    c->num_apps++;
    return DfcDerOk;
}

int32_t dfc_ffi_credential_add_file(DfcFfiCredential* credential, const DfcFfiFile* f, uint32_t* index) {
    if(!credential || !f) return DFC_FFI_INVALID_ARGUMENT;
    DfcCredential* c = &credential->model;
    size_t owner;
    if(f->owner == DFC_FFI_OWNER_PICC) {
        owner = DFC_FILE_OWNER_PICC;
    } else if(f->owner >= 0 && (size_t)f->owner < c->num_apps) {
        owner = (size_t)f->owner;
    } else {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    if(c->num_files >= DFC_MAX_FILES) return DfcDerCapacity;
    for(size_t i = 0; i < c->num_files; i++) {
        if(c->files[i].app_index == owner && c->files[i].number == f->number) return DfcDerMalformed;
    }
    DfcFile* file = &c->files[c->num_files];
    memset(file, 0, sizeof(*file));
    file->app_index = owner;
    file->data_offset = DFC_FILE_POOL_NONE;
    file->number = f->number;
    file->type = f->type;
    file->comm_settings = f->comm_settings;
    file->access_rights = f->access_rights;
    file->has_iso_file_id = f->has_iso_file_id != 0;
    file->iso_file_id = f->iso_file_id;
    file->declared_size = f->declared_size;
    file->contents_complete = f->contents_complete != 0;
    file->transaction_pending = f->transaction_pending != 0;
    file->value_lower_limit = f->value_lower_limit;
    file->value_upper_limit = f->value_upper_limit;
    file->value = f->value;
    file->limited_credit = f->limited_credit;
    file->value_pending = f->value_pending != 0;
    file->value_pending_delta = f->value_pending_delta;
    file->record_size = f->record_size;
    file->max_records = f->max_records;
    file->record_count = f->record_count;
    if(f->sdm_enabled) {
#if DFC_ENABLE_SDM
        file->sdm_enabled = true;
        file->sdm_options = f->sdm_options;
        file->sdm_access_rights = f->sdm_access_rights;
        file->sdm_has_uid_offset = f->sdm_has_uid_offset != 0;
        file->sdm_uid_offset = f->sdm_uid_offset;
        file->sdm_has_counter_offset = f->sdm_has_counter_offset != 0;
        file->sdm_counter_offset = f->sdm_counter_offset;
        file->sdm_has_picc_data_offset = f->sdm_has_picc_data_offset != 0;
        file->sdm_picc_data_offset = f->sdm_picc_data_offset;
        file->sdm_has_mac_input_offset = f->sdm_has_mac_input_offset != 0;
        file->sdm_mac_input_offset = f->sdm_mac_input_offset;
        file->sdm_has_mac_offset = f->sdm_has_mac_offset != 0;
        file->sdm_mac_offset = f->sdm_mac_offset;
        file->sdm_has_encrypted_file_offset = f->sdm_has_encrypted_file_offset != 0;
        file->sdm_encrypted_file_offset = f->sdm_encrypted_file_offset;
        file->sdm_encrypted_file_length = f->sdm_encrypted_file_length;
        file->sdm_has_counter_limit = f->sdm_has_counter_limit != 0;
        file->sdm_counter_limit = f->sdm_counter_limit;
        file->sdm_read_counter = f->sdm_read_counter;
#else
        return DfcDerUnsupported;
#endif
    }
    if(f->type == DFC_FILE_TYPE_TRANSACTION_MAC) {
#if DFC_ENABLE_TRANSACTION_MAC
        memcpy(file->transaction_mac_key, f->transaction_mac_key, sizeof(file->transaction_mac_key));
        file->transaction_mac_key_type = f->transaction_mac_key_type;
        file->transaction_mac_key_version = f->transaction_mac_key_version;
        file->transaction_counter = f->transaction_counter;
        memcpy(file->transaction_mac, f->transaction_mac, sizeof(file->transaction_mac));
        memcpy(file->previous_reader_id, f->previous_reader_id, sizeof(file->previous_reader_id));
#else
        return DfcDerUnsupported;
#endif
    }
    if(index) *index = (uint32_t)c->num_files;
    c->num_files++;
    return DfcDerOk;
}

int32_t dfc_ffi_credential_set_file_data(
    DfcFfiCredential* credential,
    uint32_t index,
    const uint8_t* data,
    size_t len) {
    if(!credential || (!data && len) || index >= credential->model.num_files) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    DfcCredential* c = &credential->model;
    DfcFile* file = &c->files[index];
    if(len == 0) {
        dfc_file_release(c, file);
        return DfcDerOk;
    }
    if(!dfc_file_resize(c, file, len)) return DfcDerCapacity;
    uint8_t* dst = dfc_file_data(c, file);
    if(!dst) return DfcDerCapacity;
    memcpy(dst, data, len);
    return DfcDerOk;
}

int32_t dfc_ffi_credential_set_key(
    DfcFfiCredential* credential,
    int32_t owner,
    uint32_t key_set,
    uint32_t slot,
    const DfcFfiKey* key) {
    if(!credential || !key || key->len > sizeof(key->value)) return DFC_FFI_INVALID_ARGUMENT;
    DfcCredential* c = &credential->model;
    const DfcApplication* found;
    if(!owner_application(c, owner, &found)) return DFC_FFI_INVALID_ARGUMENT;
    DfcApplication* app = (DfcApplication*)found;
    if(key_set == 0) {
        size_t num_keys = app ? app->num_keys : c->picc_num_keys;
        size_t key_len = app ? app->key_len : c->picc_key_len;
        if(slot >= num_keys || key->len != key_len) return DFC_FFI_INVALID_ARGUMENT;
        uint8_t* dst = dfc_credential_key(c, app, slot);
        if(!dst) return DfcDerCapacity;
        memcpy(dst, key->value, key_len);
        if(app) {
            app->key_versions[slot] = key->version;
        } else {
            c->picc_key_versions[slot] = key->version;
        }
        return DfcDerOk;
    }
#if DFC_ENABLE_KEY_SETS
    if(!app || key_set >= app->num_key_sets || slot >= app->num_keys) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    size_t key_len = app->key_set_types[key_set] == DFC_KEY_SET_TYPE_3K3DES ? 24 : 16;
    if(key->len != key_len) return DFC_FFI_INVALID_ARGUMENT;
    uint8_t* dst = dfc_credential_key_in_set(c, app, key_set, slot);
    uint8_t* version = dfc_credential_key_version_in_set(app, key_set, slot);
    if(!dst || !version) return DfcDerCapacity;
    memcpy(dst, key->value, key_len);
    *version = key->version;
    return DfcDerOk;
#else
    return DFC_FFI_UNSUPPORTED;
#endif
}

int32_t dfc_ffi_credential_finish(DfcFfiCredential* credential, DfcFfiError* error) {
    set_error(error, DfcDerOk, 0, NULL);
    if(!credential) return DFC_FFI_INVALID_ARGUMENT;
    DfcDerStatus st = dfc_der_validate_model(&credential->model);
    if(st != DfcDerOk) {
        set_error(error, st, 0, dfc_der_status_name(st));
        return st;
    }
    credential->model.dirty = false;
    return DfcDerOk;
}

// ---------------------------------------------------------- virtual PICC ---

#if DFC_ENABLE_EMULATOR
// Route the engine's randomness to this PICC for the rest of the call.
static void enter(const DfcFfiPicc* picc) {
    dfc_ffi_port_set_random(picc->random, picc->random_context);
}

static void leave(void) {
    dfc_ffi_port_set_random(NULL, NULL);
}

int32_t dfc_ffi_picc_create(
    const DfcFfiCredential* credential,
    DfcFfiRandomCallback random,
    void* random_context,
    DfcFfiPicc** out) {
    if(!credential || !out) return DFC_FFI_INVALID_ARGUMENT;
    *out = NULL;
    DfcDerStatus valid = dfc_der_validate_model(&credential->model);
    if(valid != DfcDerOk) return valid;
    DfcFfiPicc* picc = calloc(1, sizeof(*picc));
    if(!picc) return DFC_FFI_OUT_OF_MEMORY;
    dfc_credential_copy_model(&picc->model, &credential->model);
    picc->random = random;
    picc->random_context = random_context;
    enter(picc);
    picc->session = dfc_virtual_picc_session_alloc(&picc->model);
    leave();
    if(!picc->session) {
        free(picc);
        return DFC_FFI_OUT_OF_MEMORY;
    }
    *out = picc;
    return DfcVirtualPiccStatusOk;
}

void dfc_ffi_picc_free(DfcFfiPicc* picc) {
    if(!picc) return;
    if(picc->session) dfc_virtual_picc_session_free(picc->session);
    wipe(picc, sizeof(*picc));
    free(picc);
}

int32_t dfc_ffi_picc_activate(DfcFfiPicc* picc, DfcFfiActivation* out) {
    if(!picc || !out) return DFC_FFI_INVALID_ARGUMENT;
    DfcVirtualPiccActivation activation;
    memset(&activation, 0, sizeof(activation));
    enter(picc);
    DfcVirtualPiccStatus st = dfc_virtual_picc_scan_iso14443a(picc->session, &activation);
    leave();
    memset(out, 0, sizeof(*out));
    if(st != DfcVirtualPiccStatusOk) return st;
    out->uid_len = (uint32_t)activation.uid_len;
    memcpy(out->uid, activation.uid, activation.uid_len);
    out->ats_len = (uint32_t)activation.ats_len;
    memcpy(out->ats, activation.ats, activation.ats_len);
    out->atqa_len = (uint32_t)activation.atqa_len;
    memcpy(out->atqa, activation.atqa, activation.atqa_len);
    out->sak = activation.sak;
    out->protocol_len = (uint32_t)activation.protocol_len;
    memcpy(out->protocol, activation.protocol, activation.protocol_len);
    out->rf_detail_len = (uint32_t)activation.rf_detail_len;
    memcpy(out->rf_detail, activation.rf_detail, activation.rf_detail_len);
    return st;
}

int32_t dfc_ffi_picc_exchange(
    DfcFfiPicc* picc,
    const uint8_t* command,
    size_t command_len,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len) {
    if(!picc || !command || !response || !response_len) return DFC_FFI_INVALID_ARGUMENT;
    enter(picc);
    DfcVirtualPiccStatus st = dfc_virtual_picc_iso_dep_exchange(
        picc->session, command, command_len, response, response_cap, response_len);
    leave();
    return st;
}

int32_t dfc_ffi_picc_frame_exchange(
    DfcFfiPicc* picc,
    const uint8_t* frame,
    size_t frame_len,
    uint8_t* response,
    size_t response_cap,
    size_t* response_len) {
    if(!picc || !frame || !response || !response_len) return DFC_FFI_INVALID_ARGUMENT;
    enter(picc);
    DfcVirtualPiccStatus st = dfc_virtual_picc_iso_dep_frame_exchange(
        picc->session, frame, frame_len, response, response_cap, response_len);
    leave();
    return st;
}

int32_t dfc_ffi_picc_field_off(DfcFfiPicc* picc) {
    if(!picc) return DFC_FFI_INVALID_ARGUMENT;
    enter(picc);
    DfcVirtualPiccStatus st = dfc_virtual_picc_field_off(picc->session);
    leave();
    return st;
}

int32_t dfc_ffi_picc_reset_protocol(DfcFfiPicc* picc) {
    if(!picc) return DFC_FFI_INVALID_ARGUMENT;
    enter(picc);
    DfcVirtualPiccStatus st = dfc_virtual_picc_reset_protocol(picc->session);
    leave();
    return st;
}

int32_t dfc_ffi_picc_advance_time(DfcFfiPicc* picc, uint32_t elapsed_milliseconds) {
    if(!picc) return DFC_FFI_INVALID_ARGUMENT;
    enter(picc);
    dfc_emulator_advance_time(picc->session->emulator, elapsed_milliseconds);
    leave();
    return DfcVirtualPiccStatusOk;
}

int32_t dfc_ffi_picc_snapshot(const DfcFfiPicc* picc, DfcFfiSnapshot* out) {
    if(!picc || !out) return DFC_FFI_INVALID_ARGUMENT;
    const DfcEmulator* e = picc->session->emulator;
    memset(out, 0, sizeof(*out));
    out->selected = (uint32_t)e->selected_application;
    out->selected_application_index = (uint32_t)e->selected_app_index;
    if(e->selected_application == DfcEmulatorSelectedApplicationApp &&
       e->selected_app_index < picc->model.num_apps) {
        memcpy(out->selected_aid, picc->model.apps[e->selected_app_index].aid, 3);
    }
    out->dirty = picc->model.dirty;
    if(e->secure_messaging) {
        out->auth_mode = e->secure_messaging->cipher;
        out->auth_key_no = e->auth_key_no;
    }
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    out->ev2_active = e->ev2_session_active;
    if(e->ev2_session_active) {
        out->ev2_key_no = e->ev2_authenticated_key_no;
        out->ev2_command_counter = e->ev2_command_counter;
        memcpy(out->ev2_transaction_identifier, e->ev2_transaction_identifier, 4);
        memcpy(out->ev2_encryption_key, e->ev2_session_encryption_key, 16);
        memcpy(out->ev2_mac_key, e->ev2_session_mac_key, 16);
    }
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    out->virtual_card_selected = e->virtual_card_selected;
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    out->proximity_verified = e->proximity_verified;
#endif
    return DfcVirtualPiccStatusOk;
}

int32_t dfc_ffi_picc_export(const DfcFfiPicc* picc, DfcFfiCredential** out) {
    if(!picc || !out) return DFC_FFI_INVALID_ARGUMENT;
    *out = credential_alloc();
    if(!*out) return DFC_FFI_OUT_OF_MEMORY;
    dfc_credential_copy_model(&(*out)->model, &picc->model);
    return DfcVirtualPiccStatusOk;
}

int32_t dfc_ffi_picc_mark_persisted(DfcFfiPicc* picc) {
    if(!picc) return DFC_FFI_INVALID_ARGUMENT;
    dfc_credential_clear_dirty(&picc->model);
    return DfcVirtualPiccStatusOk;
}
#else
int32_t dfc_ffi_picc_create(
    const DfcFfiCredential* credential,
    DfcFfiRandomCallback random,
    void* random_context,
    DfcFfiPicc** out) {
    DFC_UNUSED(credential);
    DFC_UNUSED(random);
    DFC_UNUSED(random_context);
    if(out) *out = NULL;
    return DFC_FFI_UNSUPPORTED;
}
void dfc_ffi_picc_free(DfcFfiPicc* picc) {
    DFC_UNUSED(picc);
}
#endif

// ---------------------------------------------------------------- reader ---

#if DFC_ENABLE_READER
int32_t dfc_ffi_reader_session_new(DfcFfiReaderSession** out) {
    if(!out) return DFC_FFI_INVALID_ARGUMENT;
    *out = calloc(1, sizeof(**out));
    if(!*out) return DFC_FFI_OUT_OF_MEMORY;
    dfc_reader_session_init(&(*out)->session);
    return DfcReaderOk;
}

void dfc_ffi_reader_session_free(DfcFfiReaderSession* session) {
    if(!session) return;
    wipe(session, sizeof(*session));
    free(session);
}

void dfc_ffi_reader_session_clear(DfcFfiReaderSession* session) {
    if(session) dfc_reader_session_clear(&session->session);
}

int32_t dfc_ffi_reader_session_state(const DfcFfiReaderSession* session, DfcFfiReaderState* out) {
    if(!session || !out) return DFC_FFI_INVALID_ARGUMENT;
    const DfcReaderSession* s = &session->session;
    memset(out, 0, sizeof(*out));
    out->auth_mode = s->auth_mode;
    out->key_no = s->key_no;
    out->ev2_command_counter = s->ev2_command_counter;
    memcpy(out->ev2_transaction_identifier, s->ev2_transaction_identifier, 4);
    memcpy(out->ev2_card_capabilities, s->ev2_card_capabilities, 6);
    memcpy(out->ev2_reader_capabilities, s->ev2_reader_capabilities, 6);
    return DfcReaderOk;
}

int32_t dfc_ffi_reader_exchange_new(DfcFfiReaderExchange** out) {
    if(!out) return DFC_FFI_INVALID_ARGUMENT;
    *out = calloc(1, sizeof(**out));
    return *out ? DfcReaderOk : DFC_FFI_OUT_OF_MEMORY;
}

void dfc_ffi_reader_exchange_free(DfcFfiReaderExchange* exchange) {
    if(!exchange) return;
    wipe(exchange, sizeof(*exchange));
    free(exchange);
}

int32_t dfc_ffi_reader_authenticate_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t cipher,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* random_a,
    size_t random_a_len) {
    if(!exchange || !session || framing > DfcReaderFramingNative) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_authenticate_begin(
        &exchange->exchange,
        &session->session,
        (DfcReaderFraming)framing,
        cipher,
        key_no,
        key,
        key_len,
        random_a,
        random_a_len);
}

int32_t dfc_ffi_reader_authenticate_iso7816_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint8_t key_reference,
    const uint8_t* key,
    size_t key_len,
    uint8_t algorithm,
    const uint8_t* random_first,
    const uint8_t* random_second,
    size_t random_len) {
    if(!exchange || !session) return DFC_FFI_INVALID_ARGUMENT;
#if DFC_ENABLE_ISO7816_AUTH
    return dfc_reader_authenticate_iso7816_begin(
        &exchange->exchange, &session->session, key_reference, key, key_len,
        algorithm, random_first, random_second, random_len);
#else
    (void)key_reference;
    (void)key;
    (void)key_len;
    (void)algorithm;
    (void)random_first;
    (void)random_second;
    (void)random_len;
    return DfcReaderUnsupported;
#endif
}

int32_t dfc_ffi_reader_authenticate_ev2_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t first,
    uint8_t key_no,
    const uint8_t* key,
    const uint8_t* random_a,
    const uint8_t* capabilities,
    size_t capabilities_len) {
    if(!exchange || !session || framing > DfcReaderFramingNative) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_authenticate_ev2_begin(
        &exchange->exchange,
        &session->session,
        (DfcReaderFraming)framing,
        first != 0,
        key_no,
        key,
        random_a,
        capabilities,
        capabilities_len);
}

int32_t dfc_ffi_reader_exchange_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t ins,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode,
    int32_t header_len) {
    if(!exchange || !session || framing > DfcReaderFramingNative || header_len > 0xFF) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    DfcCommand command;
    if(dfc_command_encode_raw(&command, ins, data, data_len) != DfcCommandOk) {
        return DfcReaderInvalid;
    }
    DfcReaderOptions options = {0};
    options.comm_mode = comm_mode;
    if(header_len >= 0) {
        options.has_header_len = true;
        options.header_len = (uint8_t)header_len;
    }
    return dfc_reader_exchange_begin(
        &exchange->exchange, &session->session, (DfcReaderFraming)framing, &command, &options);
}

int32_t dfc_ffi_reader_write_data_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    if(!exchange || !session || framing > DfcReaderFramingNative) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_write_data_begin(
        &exchange->exchange, &session->session, (DfcReaderFraming)framing,
        file_number, offset, data, data_len, comm_mode);
}

int32_t dfc_ffi_reader_write_record_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    if(!exchange || !session || framing > DfcReaderFramingNative) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_write_record_begin(
        &exchange->exchange, &session->session, (DfcReaderFraming)framing,
        file_number, offset, data, data_len, comm_mode);
}

int32_t dfc_ffi_reader_update_record_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    if(!exchange || !session || framing > DfcReaderFramingNative) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_update_record_begin(
        &exchange->exchange, &session->session, (DfcReaderFraming)framing,
        file_number, record_number, offset, data, data_len, comm_mode);
}

int32_t dfc_ffi_reader_create_delegated_application_begin(
    DfcFfiReaderExchange* exchange,
    DfcFfiReaderSession* session,
    uint32_t framing,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* dam_encryption_key,
    const uint8_t* dam_mac_key,
    const uint8_t* random_prefix,
    const uint8_t* initial_key,
    size_t initial_key_len,
    uint8_t initial_version) {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(!exchange || !session || !header || header_len != DFC_DELEGATED_CREATE_HEADER_LENGTH - 1 ||
       framing > DfcReaderFramingNative) {
        return DFC_FFI_INVALID_ARGUMENT;
    }
    DfcCommandCreateDelegatedApplication app;
    memcpy(app.aid, header, 3);
    app.slot_number = (uint16_t)(header[3] | (header[4] << 8));
    app.slot_version = header[5];
    app.quota_limit = (uint16_t)(header[6] | (header[7] << 8));
    app.key_settings_1 = header[8];
    app.key_settings_2 = header[9];
    return dfc_reader_create_delegated_application_begin(
        &exchange->exchange,
        &session->session,
        (DfcReaderFraming)framing,
        &app,
        dam_encryption_key,
        dam_mac_key,
        random_prefix,
        initial_key,
        initial_key_len,
        initial_version);
#else
    DFC_UNUSED(exchange);
    DFC_UNUSED(session);
    DFC_UNUSED(framing);
    DFC_UNUSED(header);
    DFC_UNUSED(header_len);
    DFC_UNUSED(dam_encryption_key);
    DFC_UNUSED(dam_mac_key);
    DFC_UNUSED(random_prefix);
    DFC_UNUSED(initial_key);
    DFC_UNUSED(initial_key_len);
    DFC_UNUSED(initial_version);
    return DFC_FFI_UNSUPPORTED;
#endif
}

int32_t dfc_ffi_reader_step(
    DfcFfiReaderExchange* exchange,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!exchange) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_step(&exchange->exchange, response, response_len, out, out_cap, out_len);
}

uint8_t dfc_ffi_reader_result_status(const DfcFfiReaderExchange* exchange) {
    return exchange ? dfc_reader_result_status(&exchange->exchange) : 0;
}

int32_t dfc_ffi_reader_result_data(
    const DfcFfiReaderExchange* exchange,
    uint8_t* out,
    size_t cap,
    size_t* len) {
    if(!exchange || !len || (!out && cap)) return DFC_FFI_INVALID_ARGUMENT;
    size_t data_len = 0;
    const uint8_t* data = dfc_reader_result_data(&exchange->exchange, &data_len);
    *len = data_len;
    if(!out) return DfcReaderOk;
    if(data_len > cap) return DFC_FFI_BUFFER_TOO_SMALL;
    if(data_len) memcpy(out, data, data_len);
    return DfcReaderOk;
}

int32_t dfc_ffi_reader_change_key_cryptogram(
    const DfcFfiReaderSession* session,
    uint8_t key_no,
    const uint8_t* new_key,
    size_t key_len,
    const uint8_t* current_key,
    uint8_t aes_key,
    uint8_t new_version,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!session) return DFC_FFI_INVALID_ARGUMENT;
    return dfc_reader_change_key_cryptogram(
        &session->session,
        key_no,
        new_key,
        key_len,
        current_key,
        aes_key != 0,
        new_version,
        out,
        out_cap,
        out_len);
}

int32_t dfc_ffi_reader_change_key_ev2_data(
    const DfcFfiReaderSession* session,
    uint8_t key_set_no,
    uint8_t key_no,
    const uint8_t* new_key,
    const uint8_t* current_key,
    uint8_t new_version,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(!session || !out || !out_len) return DFC_FFI_INVALID_ARGUMENT;
    DfcCommand command;
    DfcReaderStatus st = dfc_reader_change_key_ev2_command(
        &session->session, key_set_no, key_no, new_key, current_key, new_version, &command);
    if(st != DfcReaderOk) return st;
    if(command.data_len > out_cap) return DFC_FFI_BUFFER_TOO_SMALL;
    memcpy(out, command.data, command.data_len);
    *out_len = command.data_len;
    wipe(&command, sizeof(command));
    return DfcReaderOk;
#else
    DFC_UNUSED(session);
    DFC_UNUSED(key_set_no);
    DFC_UNUSED(key_no);
    DFC_UNUSED(new_key);
    DFC_UNUSED(current_key);
    DFC_UNUSED(new_version);
    DFC_UNUSED(out);
    DFC_UNUSED(out_cap);
    DFC_UNUSED(out_len);
    return DFC_FFI_UNSUPPORTED;
#endif
}
#endif
