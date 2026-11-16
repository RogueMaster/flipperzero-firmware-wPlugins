#include "dfc_emulator_i.h"
#include "dfc_emulator_commands_i.h"
#include "dfc_credential_i.h"
#include "dfc_ev2.h"
#include "dfc_ev2_crypto.h"

#if DFC_ENABLE_EMULATOR

#define TAG                          DFC_EMULATOR_TAG
#define ISO14443_4A_CID_MASK         DFC_ISO14443_4A_CID_MASK
#define ISO14443_4A_NAD_MASK         DFC_ISO14443_4A_NAD_MASK
#define emulator_current_app         dfc_emulator_current_app
#define emulator_accepts_auth_cipher dfc_emulator_accepts_auth_cipher
#define emulator_key_len             dfc_emulator_key_len
#define emulator_num_keys            dfc_emulator_num_keys
#define emulator_key                 dfc_emulator_key
#define emulator_key_version         dfc_emulator_key_version
#define read_uint24_le               dfc_emulator_read_uint24_le
#define crc16_iso14443               dfc_crc16_iso14443
#define crc32_dfc                    dfc_crc32
#define d40_receive_plain            dfc_emulator_d40_receive_plain
#define des_key_version              dfc_emulator_des_key_version

void dfc_emulator_command_apply_ev1_response_secure_messaging(
    DfcEmulator* emulator,
    uint8_t cmd,
    DfcByteBuf* tx_buffer);

int32_t dfc_emulator_command_read_int32_le(const uint8_t* data) {
    return (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                     ((uint32_t)data[3] << 24));
}

void dfc_emulator_command_append_int32_le(DfcByteBuf* tx_buffer, int32_t value) {
    uint32_t raw = (uint32_t)value;
    dfc_bytebuf_append_byte(tx_buffer, (uint8_t)(raw & 0xFF));
    dfc_bytebuf_append_byte(tx_buffer, (uint8_t)((raw >> 8) & 0xFF));
    dfc_bytebuf_append_byte(tx_buffer, (uint8_t)((raw >> 16) & 0xFF));
    dfc_bytebuf_append_byte(tx_buffer, (uint8_t)((raw >> 24) & 0xFF));
}

#if DFC_ENABLE_TRANSACTION_MAC
#endif

static bool require_selected_application(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(emulator->selected_application == DfcEmulatorSelectedApplicationApp) {
        return true;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
    return false;
}

bool dfc_emulator_command_is_authenticated_as_master(const DfcEmulator* emulator) {
    return dfc_emulator_is_authenticated_with_key(emulator, 0x00);
}

uint8_t dfc_emulator_command_current_key_settings_1(DfcEmulator* emulator) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    if(app) return app->key_settings_1;
    return emulator->credential->picc_key_settings_1;
}

uint8_t dfc_emulator_command_current_key_settings_2(DfcEmulator* emulator) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    if(app) return app->key_settings_2;
    return emulator->credential->picc_key_settings_2;
}

// A slot with no key entry carries the factory
// default, every octet zero at the length the key type gives. The default is
// supplied here, where the key is used, so a credential that omits the entry is
// never rewritten to carry it. `factory` is the caller's scratch, used only when
// the credential holds no material for this slot.
const uint8_t* dfc_emulator_command_auth_key(
    DfcEmulator* emulator,
    uint8_t key_no,
    uint8_t* factory,
    size_t factory_cap,
    size_t* out_key_len) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc &&
       emulator->credential->picc_has_dam_keys && key_no == DFC_DAM_AUTH_KEY_NUMBER) {
        *out_key_len = DFC_AES_KEY_LENGTH;
        return emulator->credential->picc_dam_auth_key;
    }
#endif
    size_t key_len = emulator_key_len(emulator);
    if(key_len == 0 || key_len > factory_cap) {
        key_len = dfc_credential_key_length(dfc_emulator_command_current_key_settings_2(emulator));
    }
    if(key_len > factory_cap) key_len = factory_cap;
    *out_key_len = key_len;

    const uint8_t* key = emulator_key(emulator, key_no);
    if(key) return key;
    memset(factory, 0, key_len);
    return factory;
}

#if DFC_ENABLE_SPECIAL_KEY_STATUS
bool dfc_emulator_command_picc_disabled_special_key(DfcEmulator* emulator, uint8_t key_no) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationPicc) return false;
    if(key_no == DFC_DAM_AUTH_KEY_NUMBER) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
        return !emulator->credential->picc_has_dam_keys;
#else
        return true;
#endif
    }
    if(key_no == DFC_VC_CONFIGURATION_KEY_NUMBER) {
        return true;
    }
    return false;
}
#endif

static bool allows_free_directory_access(uint8_t key_settings_1) {
    return (key_settings_1 & DFC_KS1_FREE_DIRECTORY_ACCESS) != 0;
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
bool dfc_emulator_command_is_available_authentication_key(DfcEmulator* emulator, uint8_t key_no) {
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    if(emulator->selected_application == DfcEmulatorSelectedApplicationPicc &&
       emulator->credential->picc_has_dam_keys && key_no == DFC_DAM_AUTH_KEY_NUMBER)
        return true;
#endif
    return key_no < emulator_num_keys(emulator);
}
#endif

bool dfc_emulator_command_require_directory_access(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(allows_free_directory_access(dfc_emulator_command_current_key_settings_1(emulator)) ||
       dfc_emulator_command_is_authenticated_as_master(emulator)) {
        return true;
    }

    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
    return false;
}

// DfcFile::access_rights holds the logical nibble order:
// read at shift 12, write at shift 8, read&write at shift 4, change at shift 0.
// The wire carries the same value least-significant octet first, so the APDU
// boundary swaps the two octets on the way in (dfc_emulator_command_access_rights_from_wire) and back
// out (dfc_emulator_command_access_rights_to_wire); nothing between those two points sees wire order.
// AIDs use the same boundary conversion. Every access-rights decision below
// goes through these four accessors; never a bare shift.
static uint8_t access_read_nibble(uint16_t access_rights) {
    return dfc_access_nibble(access_rights, 12);
}

static uint8_t access_write_nibble(uint16_t access_rights) {
    return dfc_access_nibble(access_rights, 8);
}

static uint8_t access_read_write_nibble(uint16_t access_rights) {
    return dfc_access_nibble(access_rights, 4);
}

uint8_t dfc_emulator_command_access_change_nibble(uint16_t access_rights) {
    return dfc_access_nibble(access_rights, 0);
}

bool dfc_emulator_command_key_references_exist(
    uint32_t references,
    size_t reference_count,
    size_t key_count) {
    while(reference_count-- > 0) {
        uint8_t key = (uint8_t)(references & 0x0F);
        if(key < DFC_ACCESS_FREE && key >= key_count) return false;
        references >>= 4;
    }
    return true;
}

// APDU boundary conversions. `first` is the first access-rights octet on the
// wire, which is the low octet of the logical value.
uint16_t dfc_emulator_command_access_rights_from_wire(uint8_t first, uint8_t second) {
    return (uint16_t)(((uint16_t)second << 8) | first);
}

void dfc_emulator_command_access_rights_to_wire(uint16_t access_rights, uint8_t out[2]) {
    out[0] = (uint8_t)(access_rights & 0xFF);
    out[1] = (uint8_t)(access_rights >> 8);
}

// The nibble that governs `write` when true and `read` when false. The
// read&write nibble is a second, independent path to either and is checked
// alongside it, never in place of it.
static uint8_t access_primary_nibble(uint16_t access_rights, bool write) {
    return write ? access_write_nibble(access_rights) : access_read_nibble(access_rights);
}

bool dfc_emulator_command_file_allows_read(const DfcFile* file, const DfcEmulator* emulator) {
    return dfc_emulator_file_access_allowed(emulator, file, DfcFileAccessRead);
}

bool dfc_emulator_command_file_allows_write(const DfcFile* file, const DfcEmulator* emulator) {
    return dfc_emulator_file_access_allowed(emulator, file, DfcFileAccessWrite);
}

#if DFC_ENABLE_RECORD_FILES
bool dfc_emulator_command_file_allows_read_write(const DfcFile* file, const DfcEmulator* emulator) {
    return dfc_emulator_file_access_allowed(emulator, file, DfcFileAccessReadWrite);
}
#endif

// Status for an operation the file's access rights refuse. Genuine silicon
// separates two reasons, so this must too, and it consults exactly the nibbles
// dfc_emulator_command_file_allows_read / dfc_emulator_command_file_allows_write consulted.
//
// A nibble naming a key (0x0..0xD) means the caller could have been let in by
// authenticating with that key and simply is not; a card answers that with the
// authentication error. Measured on a genuine EV1 4K: ReadData on a file whose
// access rights are 0x1230 (read = key 1, read&write = key 3), with no session
// open, answers 0xAE.
//
// A nibble of 0xF permits nobody, so no authentication can ever satisfy it and
// the authentication error would invite a retry that cannot succeed. That case
// keeps permission denied. There is no hardware measurement behind it — it is
// the status this engine has always returned and is left alone deliberately.
//
// A refusal because the operation does not fit the object at all (a value
// command on a standard data file, LimitedCredit on a file without the flag) is
// not an access-rights refusal and does not come here; those stay permission
// denied, which hardware confirms.
uint8_t dfc_emulator_command_file_access_refusal_status(const DfcFile* file, bool write) {
    uint8_t primary = access_primary_nibble(file->access_rights, write);
    uint8_t read_write = access_read_write_nibble(file->access_rights);
    if(primary < DFC_ACCESS_FREE || read_write < DFC_ACCESS_FREE) {
        return DFC_STATUS_AUTHENTICATION_ERR;
    }
    return DFC_STATUS_PERMISSION_DENIED;
}

// True when every nibble that can authorize `write` is free (0xE) — no key-gated path.
// EV1/EV3: if only free access applies but file is Full, treat as Plain-with-auth.
void dfc_emulator_command_clear_pending_chain(DfcEmulator* emulator) {
    if(emulator->pending_chain_len <= sizeof(emulator->pending_chain)) {
        dfc_secure_wipe(emulator->pending_chain, emulator->pending_chain_len);
    }
    emulator->pending_chain_len = 0;
    emulator->pending_chain_offset = 0;
    emulator->pending_chain_frame = 0;
    emulator->pending_chain_last = 0;
    emulator->pending_chain_chunk_count = 0;
    emulator->pending_chain_chunk_index = 0;
    emulator->pending_chain_zero = false;
    emulator->df_names_pending = false;
    emulator->df_names_next = 0;
    emulator->get_version_frame = 0;
}

bool dfc_emulator_command_has_pending_additional_work(const DfcEmulator* emulator) {
    return emulator->awaiting_step2 ||
#if DFC_ENABLE_DELEGATED_APPLICATIONS
           emulator->delegated_creation_pending ||
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
           emulator->ev2_authentication_pending ||
#endif
           emulator->get_version_frame != 0 || emulator->df_names_pending ||
           emulator->pending_chain_len > emulator->pending_chain_offset;
}

void dfc_emulator_command_append_status_payload(
    DfcByteBuf* tx_buffer,
    uint8_t status,
    const uint8_t* payload,
    size_t payload_len) {
    dfc_bytebuf_append_byte(tx_buffer, status);
    if(payload_len > 0 && payload) {
        dfc_bytebuf_append_bytes(tx_buffer, payload, payload_len);
    }
}

// Encrypted responses end intermediate frames on cipher-block boundaries.
// Clear and MACed listings use every octet they can without splitting entries.
static size_t ev1_frame_payload(const DfcEmulator* emulator, size_t entry, bool block_aligned) {
    size_t limit = emulator->credential->card.generation == DfcGenerationEv3 ?
                       DFC_EV3_MAX_RESPONSE_PAYLOAD :
                       DFC_EV1_MAX_FRAME_PAYLOAD;
    if(block_aligned) {
        size_t block = emulator->secure_messaging->cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
        return (limit / block) * block;
    }
    if(entry == 0) entry = 1;
    return (limit / entry) * entry;
}

// `entry` is the size of one listing entry, so no frame splits one; 1 for data.
// `comm` is the communication mode the answer is returned under: an enciphered
// file's records come back enciphered, and a legacy session secures by that mode.
void dfc_emulator_command_emit_payload_with_chaining_comm(
    DfcEmulator* emulator,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    const uint8_t* payload,
    size_t payload_len,
    size_t entry,
    uint8_t comm,
    bool length_unknown) {
    bool iso_transport = emulator->pending_chain_last == DFC_ISO_DEP_RESPONSE_PENDING;
    dfc_emulator_command_clear_pending_chain(emulator);

    // Secure the whole answer once, then split the result; intermediate frames
    // carry secured octets and the final frame carries the MAC where there is one.
    bool secured_in_pending = false;
    size_t pending_used = 0;
    bool ev1 = dfc_emulator_native_authentication_active(emulator) &&
               dfc_secure_messaging_applies_ev1(emulator->secure_messaging, cmd);
    bool legacy = dfc_emulator_native_authentication_active(emulator) && !ev1;
    bool is_secured = false;
    bool block_aligned = false;
    if(ev1 && comm == DFC_COMM_ENCIPHERED) {
        if(payload_len > sizeof(emulator->pending_chain) - DFC_SM_MAX_CIPHER_BLOCK_SIZE) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
        pending_used = payload_len + DFC_SM_MAX_CIPHER_BLOCK_SIZE;
        payload_len = dfc_secure_messaging_generate_response_with_length_marker(
            emulator->secure_messaging,
            DFC_COMM_ENCIPHERED,
            DFC_STATUS_OK,
            payload,
            payload_len,
            emulator->pending_chain,
            sizeof(emulator->pending_chain),
            length_unknown);
        if(payload_len == 0) goto crypto_failure;
        pending_used = payload_len;
        payload = emulator->pending_chain;
        secured_in_pending = true;
        is_secured = true;
        block_aligned = true;
    } else if(ev1) {
        if(payload_len > sizeof(emulator->pending_chain) - DFC_WORKER_CMAC_SIZE) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
        memcpy(emulator->pending_chain, payload, payload_len);
        pending_used = payload_len + DFC_WORKER_CMAC_SIZE;
        payload_len = dfc_secure_messaging_generate_ev1_response_in_place(
            emulator->secure_messaging,
            DFC_STATUS_OK,
            emulator->pending_chain,
            payload_len,
            sizeof(emulator->pending_chain));
        if(payload_len == 0) goto crypto_failure;
        pending_used = payload_len;
        payload = emulator->pending_chain;
        secured_in_pending = true;
        is_secured = true;
        block_aligned = comm == DFC_COMM_ENCIPHERED;
    } else if(legacy && comm != DFC_COMM_PLAIN) {
        if(payload_len > sizeof(emulator->pending_chain) - DFC_SM_MAX_CIPHER_BLOCK_SIZE) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
        pending_used = payload_len + DFC_SM_MAX_CIPHER_BLOCK_SIZE;
        payload_len = comm == DFC_COMM_MAC ?
                          dfc_secure_messaging_generate_d40_response_mac(
                              emulator->secure_messaging,
                              payload,
                              payload_len,
                              emulator->pending_chain,
                              sizeof(emulator->pending_chain)) :
                          dfc_secure_messaging_generate_response_with_length_marker(
                              emulator->secure_messaging,
                              DFC_COMM_ENCIPHERED,
                              DFC_STATUS_OK,
                              payload,
                              payload_len,
                              emulator->pending_chain,
                              sizeof(emulator->pending_chain),
                              length_unknown);
        if(payload_len == 0) goto crypto_failure;
        pending_used = payload_len;
        payload = emulator->pending_chain;
        secured_in_pending = true;
        is_secured = true;
    }
    if(is_secured) emulator->response_secured = true;

    // The alternate ReadRecords instruction delegates continuation to ISO-DEP.
    // Return its complete logical response so the transport can split I-blocks.
    if(cmd == DFC_CMD_READ_RECORDS_ISO) {
        if(!iso_transport) {
            size_t used = dfc_bytebuf_get_size_bytes(tx_buffer);
            if(used < DFC_BYTEBUF_MAX && payload_len < DFC_BYTEBUF_MAX - used)
                dfc_emulator_command_append_status_payload(
                    tx_buffer, DFC_STATUS_OK, payload, payload_len);
            else
                dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            if(secured_in_pending) dfc_secure_wipe(emulator->pending_chain, pending_used);
            return;
        }
        if(payload_len > sizeof(emulator->pending_chain) - DFC_ISO7816_STATUS_WORD_LENGTH) {
            if(secured_in_pending) dfc_secure_wipe(emulator->pending_chain, pending_used);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
        if(!secured_in_pending) memcpy(emulator->pending_chain, payload, payload_len);
        emulator->pending_chain_len = payload_len;
        emulator->pending_chain_frame = DFC_ISO_DEP_RESPONSE_PENDING;
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        return;
    }

    if((ev1 || (legacy && comm != DFC_COMM_PLAIN)) &&
       (cmd == DFC_CMD_READ_DATA || cmd == DFC_CMD_READ_RECORDS)) {
        block_aligned = true;
    }

    size_t frame = ev1_frame_payload(emulator, entry, block_aligned);
    // The EV3 card caps the ISO file-ID list at 27 two-octet entries per frame.
    if(cmd == DFC_CMD_GET_ISO_FILE_IDS &&
       emulator->credential->card.generation == DfcGenerationEv3 &&
       frame > DFC_EV3_ISO_FIDS_PER_FRAME * DFC_ISO_FID_SIZE) {
        frame = DFC_EV3_ISO_FIDS_PER_FRAME * DFC_ISO_FID_SIZE;
    }
    // A secured answer runs its final frame to the wire limit, carrying the MAC
    // after the last block; an unsecured one ends on a whole entry.
    size_t last = is_secured ? (emulator->credential->card.generation == DfcGenerationEv3 ?
                                    DFC_EV3_MAX_RESPONSE_PAYLOAD :
                                    DFC_EV1_MAX_FRAME_PAYLOAD) :
                               frame;
    emulator->pending_chain_frame = frame;
    emulator->pending_chain_last = last;
    if(payload_len <= last) {
        dfc_emulator_command_append_status_payload(tx_buffer, DFC_STATUS_OK, payload, payload_len);
        if(secured_in_pending) dfc_secure_wipe(emulator->pending_chain, pending_used);
        return;
    }

    size_t remaining = payload_len - frame;
    if(secured_in_pending) {
        emulator->pending_chain_len = payload_len;
        emulator->pending_chain_offset = frame;
        dfc_emulator_command_append_status_payload(
            tx_buffer, DFC_CMD_ADDITIONAL_FRAME, payload, frame);
        return;
    }
    if(remaining > sizeof(emulator->pending_chain)) {
        remaining = sizeof(emulator->pending_chain);
    }
    if(payload == emulator->pending_chain) {
        dfc_emulator_command_append_status_payload(
            tx_buffer, DFC_CMD_ADDITIONAL_FRAME, payload, frame);
        memmove(emulator->pending_chain, payload + frame, remaining);
        emulator->pending_chain_len = remaining;
        emulator->pending_chain_offset = 0;
        return;
    }
    memcpy(emulator->pending_chain, payload + frame, remaining);
    emulator->pending_chain_len = remaining;
    emulator->pending_chain_offset = 0;
    dfc_emulator_command_append_status_payload(
        tx_buffer, DFC_CMD_ADDITIONAL_FRAME, payload, frame);
    return;

crypto_failure:
    dfc_secure_wipe(emulator->pending_chain, pending_used);
    dfc_emulator_command_clear_pending_chain(emulator);
    dfc_emulator_reset_session(emulator);
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
}

// Most answers are returned plain or under the session default; a file read
// passes its own communication mode through the *_comm form above.
void dfc_emulator_command_emit_payload_with_chaining(
    DfcEmulator* emulator,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    const uint8_t* payload,
    size_t payload_len,
    size_t entry) {
    dfc_emulator_command_emit_payload_with_chaining_comm(
        emulator, tx_buffer, cmd, payload, payload_len, entry, DFC_COMM_PLAIN, false);
}

bool dfc_emulator_command_emit_zero_payload_with_chaining_comm(
    DfcEmulator* emulator,
    DfcByteBuf* tx_buffer,
    uint8_t cmd,
    size_t payload_len,
    uint8_t comm) {
    if(payload_len <= sizeof(emulator->pending_chain)) {
        memset(emulator->pending_chain, 0, payload_len);
        dfc_emulator_command_emit_payload_with_chaining_comm(
            emulator, tx_buffer, cmd, emulator->pending_chain, payload_len, 1, comm, false);
        return true;
    }
    if(dfc_emulator_native_authentication_active(emulator) || comm != DFC_COMM_PLAIN ||
       cmd == DFC_CMD_READ_RECORDS_ISO)
        return false;

    static const uint8_t zero_frame[DFC_EV3_MAX_RESPONSE_PAYLOAD] = {0};
    dfc_emulator_command_clear_pending_chain(emulator);
    size_t frame = ev1_frame_payload(emulator, 1, false);
    if(frame == 0 || frame > sizeof(zero_frame) || payload_len <= frame) return false;
    emulator->pending_chain_frame = frame;
    emulator->pending_chain_last = frame;
    emulator->pending_chain_len = payload_len;
    emulator->pending_chain_offset = frame;
    emulator->pending_chain_zero = true;
    dfc_emulator_command_append_status_payload(
        tx_buffer, DFC_CMD_ADDITIONAL_FRAME, zero_frame, frame);
    return true;
}

static void handle_pending_chain_continuation(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    if(emulator->pending_chain_frame == DFC_ISO_DEP_RESPONSE_PENDING) {
        dfc_emulator_command_clear_pending_chain(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }
    if(emulator->pending_chain_offset >= emulator->pending_chain_len) {
        dfc_emulator_command_clear_pending_chain(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return;
    }

    size_t frame = emulator->pending_chain_frame ? emulator->pending_chain_frame :
                                                   ev1_frame_payload(emulator, 1, false);
    size_t last = emulator->pending_chain_last ? emulator->pending_chain_last : frame;
    size_t remaining = emulator->pending_chain_len - emulator->pending_chain_offset;
    size_t chunk;
    if(emulator->pending_chain_chunk_index < emulator->pending_chain_chunk_count) {
        chunk = emulator->pending_chain_chunks[emulator->pending_chain_chunk_index++];
        if(chunk > remaining) chunk = remaining;
    } else {
        chunk = remaining <= last ? remaining : frame;
    }
    bool final_frame = chunk == remaining;
    // These octets were secured as one response before being split.
    if(dfc_emulator_native_authentication_active(emulator)) emulator->response_secured = true;
    uint8_t status = final_frame ? DFC_STATUS_OK : DFC_CMD_ADDITIONAL_FRAME;
    if(emulator->pending_chain_zero) {
        static const uint8_t zero_frame[DFC_EV3_MAX_RESPONSE_PAYLOAD] = {0};
        if(chunk > sizeof(zero_frame)) {
            dfc_emulator_command_clear_pending_chain(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
            return;
        }
        dfc_emulator_command_append_status_payload(tx_buffer, status, zero_frame, chunk);
    } else {
        dfc_emulator_command_append_status_payload(
            tx_buffer, status, emulator->pending_chain + emulator->pending_chain_offset, chunk);
    }
    emulator->pending_chain_offset += chunk;
    if(status == DFC_STATUS_OK) {
        dfc_emulator_command_clear_pending_chain(emulator);
    }
}

// The next GetDFNames record, one application per frame: AID, little-endian
// ISO file ID and DF name, for each application with an ISO file ID.
static void handle_get_card_uid(DfcEmulator* emulator, DfcByteBuf* tx_buffer) {
    bool authenticated = dfc_emulator_native_authentication_active(emulator);
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    authenticated = authenticated || emulator->ev2_session_active;
#endif
    if(!authenticated) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        return;
    }

    uint8_t response[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
    size_t uid_len = emulator->credential->uid_len;
    size_t response_len = uid_len;
    size_t uid_offset = 0;
    if(uid_len != DFC_DESFIRE_UID_LEN) {
        response[0] = 0x00;
        response[1] = (uint8_t)uid_len;
        uid_offset = 2;
        response_len += 2;
    }
    memcpy(response + uid_offset, emulator->credential->uid, uid_len);

#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(emulator->ev2_session_active) {
        // The transport layer applies EV2 Full protection after dispatch.
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        dfc_bytebuf_append_bytes(tx_buffer, response, response_len);
        dfc_secure_wipe(response, sizeof(response));
        return;
    }
#endif

    response_len = dfc_secure_messaging_generate_response(
        emulator->secure_messaging,
        DFC_COMM_ENCIPHERED,
        DFC_STATUS_OK,
        response,
        response_len,
        response,
        sizeof(response));
    if(response_len == 0) {
        dfc_secure_wipe(response, sizeof(response));
        dfc_emulator_reset_session(emulator);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
    dfc_bytebuf_append_bytes(tx_buffer, response, response_len);
    emulator->response_secured = true;
    dfc_secure_wipe(response, sizeof(response));
}

#if DFC_ENABLE_DELEGATED_APPLICATIONS
uint16_t
    dfc_emulator_command_delegated_application_overhead_blocks(uint8_t num_keys, size_t key_length);

#endif

// Unwraps commands that carry a clear header followed by a protected body.
// Unmodified bodies are borrowed from apdu. Transformed bodies use scratch,
// and scratch_len tells the caller how much of that buffer it must wipe.
// The caller owns command-IV advancement, so these commands must be deferred
// by the dispatcher. SIZE_MAX means the body failed authentication.
size_t dfc_emulator_command_unwrap_write_payload(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    size_t header_len,
    uint8_t comm,
    uint8_t* scratch,
    size_t scratch_cap,
    const uint8_t** clear,
    size_t* scratch_len) {
    size_t cmd_header = 1 + header_len;
    if(apdu_len < cmd_header) return SIZE_MAX;
    const uint8_t* wrapped = apdu + cmd_header;
    size_t wrapped_len = apdu_len - cmd_header;
    *clear = wrapped;
    *scratch_len = 0;
    if(!dfc_emulator_native_authentication_active(emulator)) {
        return wrapped_len;
    }
    bool ev1_sm = dfc_secure_messaging_applies_ev1(emulator->secure_messaging, apdu[0]);
    if(ev1_sm && comm == DFC_COMM_MAC && dfc_secure_messaging_ev1_transmits_command_mac(apdu[0])) {
        size_t clear_body = dfc_secure_messaging_verify_ev1_transmitted_command_mac(
            emulator->secure_messaging, apdu[0], apdu + 1, apdu_len - 1);
        if(clear_body == SIZE_MAX || clear_body < header_len) return SIZE_MAX;
        return clear_body - header_len;
    }
    if(ev1_sm && comm == DFC_COMM_ENCIPHERED) {
        *clear = scratch;
        *scratch_len = DFC_MIN(wrapped_len, scratch_cap);
        size_t data_len = dfc_secure_messaging_verify_command(
            emulator->secure_messaging,
            DFC_COMM_ENCIPHERED,
            apdu,
            cmd_header,
            wrapped,
            wrapped_len,
            scratch,
            scratch_cap);
        if(data_len == 0 && wrapped_len > 0) return SIZE_MAX;
        return data_len;
    }
    if(ev1_sm) {
        if(!dfc_secure_messaging_update_ev1_command(
               emulator->secure_messaging, apdu[0], apdu + 1, apdu_len - 1)) {
            return SIZE_MAX;
        }
        return wrapped_len;
    }
    if(comm == DFC_COMM_PLAIN) {
        return wrapped_len;
    }
    if(comm == DFC_COMM_MAC) {
        size_t data_len = 0;
        *clear = scratch;
        *scratch_len = DFC_MIN(wrapped_len, scratch_cap);
        return dfc_secure_messaging_verify_d40_command_mac(
                   emulator->secure_messaging,
                   wrapped,
                   wrapped_len,
                   scratch,
                   scratch_cap,
                   &data_len) ?
                   data_len :
                   SIZE_MAX;
    }
    *clear = scratch;
    *scratch_len = DFC_MIN(wrapped_len, scratch_cap);
    size_t data_len = dfc_secure_messaging_verify_command(
        emulator->secure_messaging,
        DFC_COMM_ENCIPHERED,
        apdu,
        cmd_header,
        wrapped,
        wrapped_len,
        scratch,
        scratch_cap);
    if(data_len == 0 && wrapped_len > 0) return SIZE_MAX;
    return data_len;
}

/* Validate the slot before any type-specific allocation or state change. */
/* Publish a fully initialized file, or remove it if snapshotting cannot proceed. */
#if DFC_ENABLE_RECORD_FILES
#endif

#if DFC_ENABLE_TRANSACTION_MAC
#endif

#if DFC_ENABLE_TRANSACTION_MAC
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
uint16_t dfc_emulator_command_delegated_application_overhead_blocks(
    uint8_t num_keys,
    size_t key_length) {
    if(num_keys == 0)
        return DFC_APPLICATION_GENERAL_OVERHEAD_BLOCKS + DFC_DELEGATED_APPLICATION_OVERHEAD_BLOCKS;

    uint16_t key_storage_units = (uint16_t)num_keys * (key_length > DFC_AES_KEY_LENGTH ?
                                                           DFC_KEY_STORAGE_UNITS_PER_24_BYTE_KEY :
                                                           DFC_KEY_STORAGE_UNITS_PER_16_BYTE_KEY);
    uint16_t key_data_blocks =
        (uint16_t)((key_storage_units + DFC_KEY_STORAGE_KEYS_PER_DATA_BLOCK - 1) /
                   DFC_KEY_STORAGE_KEYS_PER_DATA_BLOCK);
    uint16_t key_index_blocks = DFC_KEY_STORAGE_INDEX_BLOCKS;
    if(num_keys > DFC_KEY_STORAGE_SECOND_INDEX_THRESHOLD) key_index_blocks++;
    if(num_keys > DFC_KEY_STORAGE_THIRD_INDEX_THRESHOLD) key_index_blocks++;

    return DFC_APPLICATION_GENERAL_OVERHEAD_BLOCKS + DFC_DELEGATED_APPLICATION_OVERHEAD_BLOCKS +
           key_index_blocks + key_data_blocks;
}

static void handle_create_delegated_application(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(buffer_len < DFC_DELEGATED_CREATE_HEADER_LENGTH ||
       buffer_len > DFC_DELEGATED_CREATE_MAX_HEADER_LENGTH) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    memcpy(emulator->delegated_creation_header, buffer, buffer_len);
    emulator->delegated_creation_header_length = buffer_len;
    emulator->delegated_creation_pending = true;
    dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
}

static void handle_create_delegated_application_continuation(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    uint8_t chained_data
        [DFC_DELEGATED_CREATE_MAX_HEADER_LENGTH - 1 + DFC_DELEGATED_ENCRYPTED_KEY_LENGTH +
         DFC_DELEGATED_MAC_LENGTH] = {0};
    uint8_t dam_input[DFC_DELEGATED_CREATE_MAX_HEADER_LENGTH + DFC_DELEGATED_ENCRYPTED_KEY_LENGTH] =
        {0};
    uint8_t expected_dam_mac[DFC_DELEGATED_MAC_LENGTH] = {0};
    uint8_t initial_key_data[DFC_DELEGATED_ENCRYPTED_KEY_LENGTH] = {0};
    uint8_t zero_iv[DFC_AES_KEY_LENGTH] = {0};
    uint8_t secured_response[1 + DFC_WIRE_MAC_LENGTH] = {0};
    size_t created_app_index = SIZE_MAX;
    bool credential_was_dirty = emulator->credential->dirty;
    bool success = false;
    emulator->delegated_creation_pending = false;
    if(buffer_len != DFC_DELEGATED_SECOND_FRAME_LENGTH + DFC_WIRE_MAC_LENGTH) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        goto cleanup;
    }
    if(!emulator->credential->picc_has_dam_keys || !emulator->ev2_session_active ||
       emulator->ev2_authenticated_key_no != DFC_DAM_AUTH_KEY_NUMBER) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_AUTHENTICATION_ERR);
        goto cleanup;
    }

    const uint8_t* encrypted_key = buffer + 1;
    const uint8_t* dam_mac = encrypted_key + DFC_DELEGATED_ENCRYPTED_KEY_LENGTH;
    const uint8_t* secure_mac = dam_mac + DFC_DELEGATED_MAC_LENGTH;
    size_t chained_data_len = emulator->delegated_creation_header_length - 1;
    memcpy(chained_data, emulator->delegated_creation_header + 1, chained_data_len);
    memcpy(
        chained_data + chained_data_len,
        encrypted_key,
        DFC_DELEGATED_ENCRYPTED_KEY_LENGTH + DFC_DELEGATED_MAC_LENGTH);
    chained_data_len += DFC_DELEGATED_ENCRYPTED_KEY_LENGTH + DFC_DELEGATED_MAC_LENGTH;
    if(!dfc_ev2_verify_chained_command_mac(
           emulator,
           DFC_CMD_CREATE_DELEGATED_APPLICATION,
           chained_data,
           chained_data_len,
           secure_mac)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup;
    }

    size_t dam_input_len = emulator->delegated_creation_header_length;
    memcpy(dam_input, emulator->delegated_creation_header, dam_input_len);
    memcpy(dam_input + dam_input_len, encrypted_key, DFC_DELEGATED_ENCRYPTED_KEY_LENGTH);
    dam_input_len += DFC_DELEGATED_ENCRYPTED_KEY_LENGTH;
    if(!dfc_aes_wire_mac(
           emulator->credential->picc_dam_mac_key, dam_input, dam_input_len, expected_dam_mac) ||
       !dfc_ct_equal(expected_dam_mac, dam_mac, sizeof(expected_dam_mac))) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup;
    }

    if(!dfc_worker_aes_cbc_decrypt(
           emulator->credential->picc_dam_encryption_key,
           DFC_AES_KEY_LENGTH,
           zero_iv,
           sizeof(initial_key_data),
           encrypted_key,
           initial_key_data)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup;
    }

    const uint8_t* header = emulator->delegated_creation_header;
    uint8_t key_settings_2 = header[DFC_DELEGATED_KEY_SETTINGS_2_OFFSET];
    uint8_t num_keys = key_settings_2 & DFC_NUM_KEYS_MASK;
    size_t key_length = (key_settings_2 & DFC_KEY_TYPE_MASK) == DFC_KEY_TYPE_3K3DES ?
                            DFC_MAX_KEY_LEN :
                            DFC_AES_KEY_LENGTH;
    uint16_t quota_limit = (uint16_t)(header[DFC_DELEGATED_QUOTA_OFFSET] |
                                      ((uint16_t)header[DFC_DELEGATED_QUOTA_OFFSET + 1] << 8));
    uint16_t overhead_blocks =
        dfc_emulator_command_delegated_application_overhead_blocks(num_keys, key_length);
    if(quota_limit < overhead_blocks) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        goto cleanup;
    }
    uint8_t create_command[] = {
        DFC_CMD_CREATE_APPLICATION,
        header[DFC_DELEGATED_AID_OFFSET],
        header[DFC_DELEGATED_AID_OFFSET + 1],
        header[DFC_DELEGATED_AID_OFFSET + 2],
        header[DFC_DELEGATED_KEY_SETTINGS_1_OFFSET],
        key_settings_2,
    };
    // The nested create produces one status byte. Use the command response
    // buffer directly, then clear that internal status before protecting the
    // delegated response. This command starts with an empty response buffer.
    dfc_emulator_command_create_application(
        emulator, create_command, sizeof(create_command), tx_buffer);
    const size_t create_response_length = dfc_bytebuf_get_size_bytes(tx_buffer);
    const uint8_t create_status = create_response_length == 1 ?
                                      dfc_bytebuf_get_byte(tx_buffer, 0) :
                                      DFC_STATUS_PARAMETER_ERROR;
    if(create_response_length != 1 || create_status != DFC_STATUS_OK) {
        if(create_response_length != 1) {
            dfc_bytebuf_reset(tx_buffer);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PARAMETER_ERROR);
        }
        goto cleanup;
    }
    dfc_bytebuf_reset(tx_buffer);
    if(emulator->credential->num_apps == 0) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        goto cleanup;
    }
    created_app_index = emulator->credential->num_apps - 1;

    DfcApplication* app = dfc_credential_find_application_desfire_order(
        emulator->credential, header + DFC_DELEGATED_AID_OFFSET);
    if(!app) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        goto cleanup;
    }
    const uint8_t* initial_key = initial_key_data + DFC_DELEGATED_RANDOM_PREFIX_LENGTH;
    uint8_t initial_version = initial_key_data[DFC_DELEGATED_ENCRYPTED_KEY_LENGTH - 1];
    for(size_t key_number = 0; key_number < app->num_keys; key_number++) {
        uint8_t* key = dfc_credential_key(emulator->credential, app, key_number);
        if(key) memcpy(key, initial_key, app->key_len);
        app->key_versions[key_number] = initial_version;
    }
    app->delegated = true;
    app->delegated_slot_number =
        (uint16_t)(header[DFC_DELEGATED_SLOT_OFFSET] |
                   ((uint16_t)header[DFC_DELEGATED_SLOT_OFFSET + 1] << 8));
    app->delegated_slot_version = header[DFC_DELEGATED_SLOT_VERSION_OFFSET];
    app->delegated_quota_limit = quota_limit;
    app->delegated_free_blocks = quota_limit - overhead_blocks;
    const uint8_t clear_command[] = {DFC_CMD_CREATE_DELEGATED_APPLICATION};
    const uint8_t clear_response[] = {DFC_STATUS_OK};
    size_t secured_response_len = 0;
    if(!dfc_ev2_protect_response(
           emulator,
           clear_command,
           sizeof(clear_command),
           clear_response,
           sizeof(clear_response),
           secured_response,
           sizeof(secured_response),
           &secured_response_len)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        goto cleanup;
    }
    dfc_bytebuf_append_bytes(tx_buffer, secured_response, secured_response_len);
    success = true;

cleanup:
    if(!success && created_app_index != SIZE_MAX) {
        dfc_credential_delete_application_at(emulator->credential, created_app_index);
        emulator->credential->dirty = credential_was_dirty;
    }
    dfc_secure_wipe(chained_data, sizeof(chained_data));
    dfc_secure_wipe(dam_input, sizeof(dam_input));
    dfc_secure_wipe(expected_dam_mac, sizeof(expected_dam_mac));
    dfc_secure_wipe(initial_key_data, sizeof(initial_key_data));
    dfc_secure_wipe(zero_iv, sizeof(zero_iv));
    dfc_secure_wipe(secured_response, sizeof(secured_response));
    dfc_secure_wipe(
        emulator->delegated_creation_header, sizeof(emulator->delegated_creation_header));
    emulator->delegated_creation_header_length = 0;
}

static void handle_get_delegated_info(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    if(buffer_len != DFC_DELEGATED_INFO_COMMAND_LENGTH) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return;
    }
    if(emulator->selected_application != DfcEmulatorSelectedApplicationPicc) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_PERMISSION_DENIED);
        return;
    }
    uint16_t slot_number = (uint16_t)(buffer[1] | ((uint16_t)buffer[2] << 8));
    for(size_t index = 0; index < emulator->credential->num_apps; index++) {
        const DfcApplication* app = &emulator->credential->apps[index];
        if(!app->delegated || app->delegated_slot_number != slot_number) continue;
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OK);
        dfc_bytebuf_append_byte(tx_buffer, app->delegated_slot_version);
        dfc_bytebuf_append_byte(tx_buffer, (uint8_t)app->delegated_quota_limit);
        dfc_bytebuf_append_byte(tx_buffer, (uint8_t)(app->delegated_quota_limit >> 8));
        dfc_bytebuf_append_byte(tx_buffer, (uint8_t)app->delegated_free_blocks);
        dfc_bytebuf_append_byte(tx_buffer, (uint8_t)(app->delegated_free_blocks >> 8));
        dfc_bytebuf_append_byte(tx_buffer, app->aid[2]);
        dfc_bytebuf_append_byte(tx_buffer, app->aid[1]);
        dfc_bytebuf_append_byte(tx_buffer, app->aid[0]);
        return;
    }
    dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_APPLICATION_NOT_FOUND);
}
#endif

#if DFC_ENABLE_SDM
#endif

#if DFC_ENABLE_SDM
#endif

#if DFC_ENABLE_RECORD_FILES
static void
    handle_read_records(DfcEmulator* emulator, const uint8_t* apdu, DfcByteBuf* tx_buffer) {
    DfcFile* file = dfc_credential_find_file_in_app(
        emulator->credential, emulator->selected_app_index, apdu[1]);
    if(!file ||
       (file->type != DFC_FILE_TYPE_LINEAR_RECORD && file->type != DFC_FILE_TYPE_CYCLIC_RECORD)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    if(!dfc_emulator_command_file_allows_read(file, emulator)) {
        dfc_bytebuf_append_byte(
            tx_buffer, dfc_emulator_command_file_access_refusal_status(file, false));
        return;
    }

    uint32_t record_count = file->state.record.count;
    const uint8_t* data = dfc_file_data_const(emulator->credential, file);
#if DFC_ENABLE_TRANSACTIONAL_DATA_FILES
    if(emulator->transaction_snapshot_active &&
       emulator->transaction_snapshot_app_index == emulator->selected_app_index) {
        size_t index = dfc_emulator_command_transaction_file_index(emulator, file);
        bool committed_data_is_zero = false;
        record_count = dfc_emulator_transaction_snapshot_committed_record_count(emulator, index);
        data = dfc_emulator_transaction_snapshot_committed_data(
            emulator, index, &committed_data_is_zero);
        if(committed_data_is_zero) data = NULL;
    }
#endif

    uint32_t first_record = read_uint24_le(apdu + 2);
    uint32_t requested_records = read_uint24_le(apdu + 5);
    if(record_count == 0 || first_record >= record_count) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
    size_t available = record_count - first_record;
    size_t count = requested_records == 0 ? available : requested_records;
    if(count > available || file->state.record.size == 0 ||
       count > DFC_MAX_FILE_DATA / file->state.record.size) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
    if(!data) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_FILE_NOT_FOUND);
        return;
    }
    // The requested records occupy one contiguous range in stored oldest-first
    // order, even though record number zero names the newest one.
    size_t payload_len = count * file->state.record.size;
    size_t stored_index = record_count - first_record - count;
#if DFC_ENABLE_TRANSACTION_MAC
    if(!dfc_emulator_command_transaction_mac_add_data(
           emulator,
           apdu[0],
           apdu[1],
           first_record,
           (uint32_t)count,
           data + stored_index * file->state.record.size,
           payload_len)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
#endif
    uint8_t comm = dfc_emulator_file_effective_comm_settings(
        emulator, file, DfcFileAccessRead, file->comm_settings);
    dfc_emulator_command_emit_payload_with_chaining_comm(
        emulator,
        tx_buffer,
        apdu[0],
        data + stored_index * file->state.record.size,
        payload_len,
        1,
        comm,
        requested_records == 0);
}

#endif

void dfc_emulator_command_apply_ev1_response_secure_messaging(
    DfcEmulator* emulator,
    uint8_t cmd,
    DfcByteBuf* tx_buffer) {
    if(!dfc_emulator_native_authentication_active(emulator) ||
       !dfc_secure_messaging_applies_ev1(emulator->secure_messaging, cmd))
        return;
    // Already secured as one response before being split into frames.
    if(emulator->response_secured) return;

    size_t tx_len = dfc_bytebuf_get_size_bytes(tx_buffer);
    const uint8_t* tx_data = dfc_bytebuf_get_data(tx_buffer);
    size_t offset = 0;
    if(tx_len > 1 && tx_data[0] != DFC_STATUS_OK && tx_data[0] != DFC_CMD_ADDITIONAL_FRAME &&
       tx_data[0] != DFC_STATUS_FILE_NOT_FOUND && tx_data[0] != DFC_STATUS_APPLICATION_NOT_FOUND &&
       tx_data[0] != DFC_STATUS_PARAMETER_ERROR && tx_data[0] != DFC_STATUS_PERMISSION_DENIED &&
       tx_data[0] != DFC_STATUS_AUTHENTICATION_ERR && (tx_data[0] & 0xC0) == 0x00) {
        offset = 1;
        if(tx_data[0] & ISO14443_4A_CID_MASK) offset++;
        if(tx_data[0] & ISO14443_4A_NAD_MASK) offset++;
    }
    if(tx_len <= offset) return;

    uint8_t status = tx_data[offset];
    size_t payload_len = tx_len - offset - 1;
    if(status == DFC_STATUS_SPECIAL_SUCCESS ||
       (status == DFC_CMD_ADDITIONAL_FRAME && payload_len == 0))
        return;
    // GetVersion's first two response bodies remain plain. The three command
    // CMACs advance the IV; only the terminal response body and status are
    // authenticated, and that response CMAC becomes the next command's IV.
    if(cmd == DFC_CMD_GET_VERSION && status == DFC_CMD_ADDITIONAL_FRAME) return;
    if(status != DFC_STATUS_OK && status != DFC_CMD_ADDITIONAL_FRAME) {
        dfc_emulator_reset_session(emulator);
        return;
    }

    if(payload_len > DFC_WORKER_MAX_BUFFER_SIZE - (offset + 1) - DFC_WIRE_MAC_LENGTH) {
        dfc_emulator_reset_session(emulator);
        dfc_bytebuf_reset(tx_buffer);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_BOUNDARY_ERROR);
        return;
    }
    uint8_t mac[DFC_WIRE_MAC_LENGTH];
    if(!dfc_secure_messaging_generate_ev1_response_mac(
           emulator->secure_messaging, status, tx_data + offset + 1, payload_len, mac)) {
        dfc_secure_wipe(mac, sizeof(mac));
        dfc_emulator_reset_session(emulator);
        dfc_bytebuf_reset(tx_buffer);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
        return;
    }

    dfc_bytebuf_append_bytes(tx_buffer, mac, sizeof(mac));
    emulator->response_secured = true;
    dfc_secure_wipe(mac, sizeof(mac));
}

static size_t chained_write_header_len(uint8_t cmd) {
    switch(cmd) {
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_WRITE_RECORD:
        return 7;
    case DFC_CMD_UPDATE_RECORD:
        return 10;
    default:
        return 0;
    }
}

// Total logical native command length after all AF frames have been joined.
// SIZE_MAX means that the first frame does not yet contain enough header.
static size_t chained_write_expected_len(
    DfcEmulator* emulator,
    uint8_t cmd,
    const uint8_t* buffer,
    size_t buffer_len) {
    size_t header_len = chained_write_header_len(cmd);
    if(header_len == 0 || buffer_len < header_len + 1) return SIZE_MAX;
    size_t length_offset = cmd == DFC_CMD_UPDATE_RECORD ? 8 : 5;
    size_t clear_len = read_uint24_le(buffer + length_offset);
    if(clear_len > DFC_EMULATOR_CHAIN_BUFFER_SIZE - header_len - 1) return 0;

    DfcFile* file = NULL;
    if(emulator->selected_application == DfcEmulatorSelectedApplicationApp) {
        file = dfc_credential_find_file_in_app(
            emulator->credential, emulator->selected_app_index, buffer[1]);
    }
    // The command handler can reject this from the first frame without asking
    // the reader to send data for a file that does not exist.
    if(!file) return buffer_len;
    DfcFileAccess access = cmd == DFC_CMD_UPDATE_RECORD ? DfcFileAccessReadWrite :
                                                          DfcFileAccessWrite;
    uint8_t comm =
        dfc_emulator_file_effective_comm_settings(emulator, file, access, file->comm_settings);
    size_t wire_len;

#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(emulator->ev2_session_active && comm != DFC_COMM_PLAIN) {
        wire_len = dfc_secure_messaging_command_payload_length(NULL, true, comm, clear_len);
    } else
#endif
        if(dfc_emulator_native_authentication_active(emulator)) {
        wire_len = dfc_secure_messaging_command_payload_length(
            emulator->secure_messaging, false, comm, clear_len);
    } else
        wire_len = clear_len;
    if(wire_len == SIZE_MAX) return 0;
    if(wire_len > DFC_EMULATOR_CHAIN_BUFFER_SIZE - header_len - 1) return 0;
    return 1 + header_len + wire_len;
}

static bool begin_or_reject_command_chain(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(emulator->ev2_session_active && buffer_len < DFC_EV1_MAX_FRAME_PAYLOAD + 1) return false;
#endif
    size_t expected = chained_write_expected_len(emulator, buffer[0], buffer, buffer_len);
    if(expected == SIZE_MAX) return false;
    if(expected == 0 || buffer_len > DFC_EV1_MAX_FRAME_PAYLOAD + 1 || buffer_len > expected) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
        return true;
    }
    if(buffer_len == expected) return false;
    emulator->command_chain = dfc_platform_alloc(expected, DfcAllocCommandChain);
    if(!emulator->command_chain) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_OUT_OF_EEPROM);
        return true;
    }
    memcpy(emulator->command_chain, buffer, buffer_len);
    emulator->command_chain_active = true;
    emulator->command_chain_len = buffer_len;
    emulator->command_chain_expected = expected;
    dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
    return true;
}

static bool update_ev1_command_if_active(
    DfcEmulator* emulator,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len) {
    if(dfc_emulator_native_authentication_active(emulator) &&
       dfc_secure_messaging_applies_ev1(emulator->secure_messaging, cmd)) {
        return dfc_secure_messaging_update_ev1_command(
            emulator->secure_messaging, cmd, data, data_len);
    }
    return true;
}

#if DFC_ENABLE_PROXIMITY_CHECK || DFC_ENABLE_EV2_SECURE_MESSAGING
static bool prepare_ev1_command_update(
    DfcEmulator* emulator,
    uint8_t cmd,
    const uint8_t* data,
    size_t data_len,
    uint8_t candidate_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE],
    bool* has_candidate) {
    *has_candidate = false;
    if(!dfc_emulator_native_authentication_active(emulator) ||
       !dfc_secure_messaging_applies_ev1(emulator->secure_messaging, cmd)) {
        return true;
    }
    uint8_t original_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE];
    memcpy(original_iv, emulator->secure_messaging->iv, sizeof(original_iv));
    if(!dfc_secure_messaging_update_ev1_command(emulator->secure_messaging, cmd, data, data_len)) {
        dfc_secure_wipe(original_iv, sizeof(original_iv));
        return false;
    }
    memcpy(candidate_iv, emulator->secure_messaging->iv, sizeof(original_iv));
    memcpy(emulator->secure_messaging->iv, original_iv, sizeof(original_iv));
    dfc_secure_wipe(original_iv, sizeof(original_iv));
    *has_candidate = true;
    return true;
}

static void commit_ev1_command_update(
    DfcEmulator* emulator,
    const uint8_t candidate_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE],
    bool has_candidate) {
    if(has_candidate && dfc_emulator_native_authentication_active(emulator)) {
        memcpy(
            emulator->secure_messaging->iv, candidate_iv, sizeof(emulator->secure_messaging->iv));
    }
}
#endif

static void finalize_native_command(
    DfcEmulator* emulator,
    bool preserve_transaction_on_failure,
    bool authentication_exchange,
    const DfcByteBuf* tx_buffer) {
    size_t response_len = dfc_bytebuf_get_size_bytes(tx_buffer);
    if(response_len == 0) return;
    uint8_t status = dfc_bytebuf_get_data(tx_buffer)[0];
    if(status == DFC_STATUS_OK || status == DFC_CMD_ADDITIONAL_FRAME ||
       status == DFC_STATUS_SPECIAL_SUCCESS)
        return;
    if(!preserve_transaction_on_failure) dfc_emulator_abort_transaction(emulator);
    // Authentication handlers retire the correct family at every failure
    // stage. The generic policy has a legacy D40 exception, so applying it a
    // second time would make failed D40 authentication retain its session.
    if(authentication_exchange) return;
    if(emulator->auth_cipher != DFC_CMD_AUTHENTICATE_LEGACY)
        dfc_emulator_clear_active_authentication(emulator);
}

static bool dispatch_native_command(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer,
    void* context) {
    Dfc* dfc = context;
    if(buffer_len < 1) return false;
    uint8_t cmd = buffer[0];
    emulator->response_secured = false;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    emulator->ev2_response_plain = false;
#endif
    if(emulator->command_chain_active) {
        if(cmd != DFC_CMD_ADDITIONAL_FRAME) {
            dfc_emulator_discard_command_chain(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_COMMAND_ABORTED);
            return true;
        }
        size_t continuation_len = buffer_len - 1;
        size_t remaining = emulator->command_chain_expected - emulator->command_chain_len;
        if(continuation_len == 0 || continuation_len > DFC_EV1_MAX_FRAME_PAYLOAD ||
           continuation_len > remaining) {
            dfc_emulator_discard_command_chain(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_LENGTH_ERROR);
            return true;
        }
        memcpy(
            emulator->command_chain + emulator->command_chain_len, buffer + 1, continuation_len);
        emulator->command_chain_len += continuation_len;
        if(emulator->command_chain_len < emulator->command_chain_expected) {
            dfc_bytebuf_append_byte(tx_buffer, DFC_CMD_ADDITIONAL_FRAME);
            return true;
        }
        size_t logical_len = emulator->command_chain_len;
        emulator->command_chain_active = false;
        emulator->command_chain_expected = SIZE_MAX;
        bool handled = dispatch_native_command(
            emulator, emulator->command_chain, logical_len, tx_buffer, context);
        if(handled
#if DFC_ENABLE_TRANSACTION_MAC
           && !emulator->transaction_mac_retryable_failure
#endif
        )
            dfc_emulator_command_apply_ev1_response_secure_messaging(
                emulator, emulator->command_chain[0], tx_buffer);
        dfc_emulator_discard_command_chain(emulator);
        return handled;
    }

    if(cmd != DFC_CMD_ADDITIONAL_FRAME && dfc_emulator_abort_pending_native_work(emulator)) {
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_COMMAND_ABORTED);
        return true;
    }

    if(emulator->command_chain_expected != SIZE_MAX && chained_write_header_len(cmd) != 0 &&
       begin_or_reject_command_chain(emulator, buffer, buffer_len, tx_buffer)) {
        return true;
    }

    // Handlers that own EV1 command IV updates themselves:
    // ChangeKey and ChangeFileSettings (Full cryptograms), plus WriteData /
    // Credit / Debit / LimitedCredit (MAC-on-wire or Full decrypt).
    bool defer_ev1_cmd_cmac = cmd == DFC_CMD_CHANGE_KEY || cmd == DFC_CMD_CHANGE_KEY_SETTINGS ||
                              cmd == DFC_CMD_SET_CONFIGURATION ||
                              cmd == DFC_CMD_CHANGE_FILE_SETTINGS ||
                              dfc_secure_messaging_ev1_transmits_command_mac(cmd);
#if DFC_ENABLE_PROXIMITY_CHECK
    defer_ev1_cmd_cmac = defer_ev1_cmd_cmac || cmd == DFC_CMD_PREPARE_PROXIMITY_CHECK;
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    defer_ev1_cmd_cmac = defer_ev1_cmd_cmac || cmd == DFC_CMD_AUTHENTICATE_EV2_FIRST ||
                         cmd == DFC_CMD_AUTHENTICATE_EV2_NON_FIRST;
#endif
#if DFC_ENABLE_SDM
    // SDM ReadData may need randomness. Keep the EV1 IV tentative until the
    // complete response exists, so an exhausted source is retryable.
    defer_ev1_cmd_cmac = defer_ev1_cmd_cmac || cmd == DFC_CMD_READ_DATA;
#endif
    if(!defer_ev1_cmd_cmac) {
        if(!update_ev1_command_if_active(emulator, cmd, buffer + 1, buffer_len - 1)) {
            dfc_emulator_reset_session(emulator);
            dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_INTEGRITY_ERROR);
            return true;
        }
    }

#if DFC_ENABLE_RECORD_FILES
    cmd = dfc_record_command_base(cmd);
#endif
    switch(cmd) {
#define DFC_NATIVE_COMMAND(command) case command:
#include "dfc_emulator_native_command_registry.inc"
#undef DFC_NATIVE_COMMAND
    default:
        DFC_LOG_I(TAG, "Unhandled command 0x%02x", cmd);
        dfc_bytebuf_append_byte(tx_buffer, DFC_STATUS_ILLEGAL_COMMAND_CODE);
        return true;
    }
}

bool dfc_emulator_handle_command(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer,
    void* context) {
    bool authentication_exchange = false;
    bool preserve_auth_transaction = false;
    if(buffer_len > 0) {
        uint8_t command = buffer[0];
        authentication_exchange = command == DFC_CMD_AUTHENTICATE_LEGACY ||
                                  command == DFC_CMD_AUTHENTICATE_ISO ||
                                  command == DFC_CMD_AUTHENTICATE_AES;
        preserve_auth_transaction = command == DFC_CMD_AUTHENTICATE_LEGACY;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
        authentication_exchange = authentication_exchange ||
                                  command == DFC_CMD_AUTHENTICATE_EV2_FIRST ||
                                  command == DFC_CMD_AUTHENTICATE_EV2_NON_FIRST;
#endif
        if(command == DFC_CMD_ADDITIONAL_FRAME) {
            authentication_exchange = emulator->awaiting_step2;
            preserve_auth_transaction = emulator->awaiting_step2 &&
                                        emulator->pending_auth_cipher ==
                                            DFC_CMD_AUTHENTICATE_LEGACY;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
            authentication_exchange = authentication_exchange ||
                                      emulator->ev2_authentication_pending;
#endif
        }
    }
    bool preserve_transaction_on_failure = false;
#if DFC_ENABLE_TRANSACTION_MAC
    if(buffer_len > 0 && buffer[0] == DFC_CMD_CREATE_TRANSACTION_MAC_FILE) {
        preserve_transaction_on_failure = true;
    } else if(
        buffer_len == 2 && buffer[0] == DFC_CMD_DELETE_FILE &&
        emulator->selected_application == DfcEmulatorSelectedApplicationApp) {
        DfcFile* target = dfc_credential_find_file_in_app(
            emulator->credential, emulator->selected_app_index, buffer[1]);
        preserve_transaction_on_failure = target && target->type == DFC_FILE_TYPE_TRANSACTION_MAC;
    }
#endif
    preserve_transaction_on_failure = preserve_transaction_on_failure || preserve_auth_transaction;
#if DFC_ENABLE_TRANSACTION_MAC
    uint8_t transaction_mac_prior_iv[DFC_SM_MAX_CIPHER_BLOCK_SIZE] = {0};
    bool transaction_mac_had_secure_messaging = emulator->secure_messaging != NULL;
    if(transaction_mac_had_secure_messaging)
        memcpy(
            transaction_mac_prior_iv,
            emulator->secure_messaging->iv,
            sizeof(transaction_mac_prior_iv));
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    uint16_t transaction_mac_prior_command_counter = emulator->ev2_command_counter;
#endif
#endif
    if(!dispatch_native_command(emulator, buffer, buffer_len, tx_buffer, context)) {
#if DFC_ENABLE_TRANSACTION_MAC
        dfc_secure_wipe(transaction_mac_prior_iv, sizeof(transaction_mac_prior_iv));
#endif
        return false;
    }
#if DFC_ENABLE_TRANSACTION_MAC
    if(emulator->transaction_mac_retryable_failure) {
        if(transaction_mac_had_secure_messaging && emulator->secure_messaging)
            memcpy(
                emulator->secure_messaging->iv,
                transaction_mac_prior_iv,
                sizeof(transaction_mac_prior_iv));
#if DFC_ENABLE_EV2_SECURE_MESSAGING
        emulator->ev2_command_counter = transaction_mac_prior_command_counter;
#endif
        emulator->transaction_mac_retryable_failure = false;
        dfc_secure_wipe(transaction_mac_prior_iv, sizeof(transaction_mac_prior_iv));
        return true;
    }
    dfc_secure_wipe(transaction_mac_prior_iv, sizeof(transaction_mac_prior_iv));
#endif
    // A caller-controlled source can be replenished and retried. Its adapter
    // replaces the provisional status, so do not publish failure side effects.
    if(dfc_random_failure_is_retryable()) return true;
    dfc_emulator_command_apply_ev1_response_secure_messaging(emulator, buffer[0], tx_buffer);
    finalize_native_command(
        emulator, preserve_transaction_on_failure, authentication_exchange, tx_buffer);
    return true;
}

bool dfc_emulator_handle_transport_command(
    DfcEmulator* emulator,
    const uint8_t* buffer,
    size_t buffer_len,
    DfcByteBuf* tx_buffer) {
    // Only this transport consumer accepts a lent ISO response in pending_chain.
    bool iso_read = buffer_len && buffer[0] == DFC_CMD_READ_RECORDS_ISO;
    if(iso_read) emulator->pending_chain_last = DFC_ISO_DEP_RESPONSE_PENDING;
    bool handled = dfc_emulator_handle_command(emulator, buffer, buffer_len, tx_buffer, NULL);
    if(iso_read && emulator->pending_chain_last == DFC_ISO_DEP_RESPONSE_PENDING)
        emulator->pending_chain_last = 0;
    return handled;
}

#endif // DFC_ENABLE_EMULATOR
