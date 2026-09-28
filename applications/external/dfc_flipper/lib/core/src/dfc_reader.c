#include "dfc_reader.h"

#include <string.h>

#include "dfc_ev2_crypto.h"

#if DFC_ENABLE_READER

enum {
    KindCommand = 1,
    KindAuthenticate,
    KindAuthenticateEv2,
    KindAuthenticateIso7816,
};

enum {
    PhaseStart = 0,
    // A command frame of a chained EV2 command is out; the card asks for more.
    PhaseCommandFrames,
    // The command is sent; answers are collected until the chain ends.
    PhaseResponse,
    PhaseAuthChallenge,
    PhaseAuthConfirmation,
    PhaseAuthExternal,
    PhaseDone,
};

// How the final answer is checked and unwrapped.
enum {
    PlanPlain = 0,
    // EV1: one CMAC over the whole reassembled answer and its final status.
    PlanEv1Cmac,
    // EV1 GetVersion: each frame is secured on its own, and each continuation
    // advances the IV.
    PlanEv1CmacPerFrame,
    // EV1 Full read: the answer is enciphered with its status-bound CRC32 and
    // carries no CMAC.
    PlanEv1Decrypt,
    // Legacy read under MAC or Full: the file's own wrapping.
    PlanD40Mac,
    PlanD40Decrypt,
    PlanEv2Mac,
    PlanEv2MacDecrypt,
};

#define FILE_HEADER_LENGTH          7
#define UPDATE_RECORD_HEADER_LENGTH 10
#define VALUE_HEADER_LENGTH         1
#define SET_CONFIGURATION_HEADER    1
#define CREATE_TMAC_HEADER          5
#define COMMIT_READER_ID_HEADER     0
#define CHANGE_KEY_EV2_HEADER       2
#define CHANGE_KEY_HEADER           1

#define WRAPPED_SW1 0x91
#define DFC_READER_STREAM_CHUNK 152

enum {
    StreamNone = 0,
    StreamWriteData,
    StreamWriteRecord,
    StreamUpdateRecord,
};

const char* dfc_reader_status_name(DfcReaderStatus status) {
    switch(status) {
    case DfcReaderOk:
        return "ok";
    case DfcReaderPending:
        return "pending";
    case DfcReaderInvalid:
        return "invalid";
    case DfcReaderBufferTooSmall:
        return "buffer too small";
    case DfcReaderProtocolError:
        return "protocol error";
    case DfcReaderIntegrityError:
        return "integrity error";
    case DfcReaderCardError:
        return "card error";
    case DfcReaderUnsupported:
        return "unsupported";
    }
    return "unknown";
}

// Comparison whose duration does not depend on where the inputs differ.
static bool ct_equal(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t difference = 0;
    for(size_t i = 0; i < len; i++) difference |= a[i] ^ b[i];
    return difference == 0;
}

// ------------------------------------------------------------ session ---

static void wipe(void* p, size_t n) {
    volatile uint8_t* bytes = p;
    while(n--) *bytes++ = 0;
}

void dfc_reader_session_init(DfcReaderSession* session) {
    if(!session) return;
    wipe(session, sizeof(*session));
    session->sm.pcd = true;
}

void dfc_reader_session_clear(DfcReaderSession* session) {
    dfc_reader_session_init(session);
}

bool dfc_reader_session_is_authenticated(const DfcReaderSession* session) {
    return session && session->auth_mode != DFC_READER_AUTH_NONE;
}

static bool session_is_ev1(const DfcReaderSession* session) {
    return session->auth_mode == DFC_CMD_AUTHENTICATE_ISO ||
           session->auth_mode == DFC_CMD_AUTHENTICATE_AES;
}

static void open_legacy_session(
    DfcReaderSession* session,
    uint8_t cipher,
    uint8_t key_no,
    const uint8_t* session_key,
    size_t session_key_len) {
    dfc_reader_session_init(session);
    session->auth_mode = cipher;
    session->key_no = key_no;
    session->sm.cipher = cipher;
    memcpy(session->sm.session_key, session_key, session_key_len);
    session->sm.session_key_len = session_key_len;
    // Every EV1 session starts from a zero IV; a legacy session never chains.
    memset(session->sm.iv, 0, sizeof(session->sm.iv));
}

size_t dfc_reader_challenge_length(uint8_t cipher, size_t key_len) {
    if(cipher == DFC_CMD_AUTHENTICATE_AES) return 16;
    if(cipher == DFC_CMD_AUTHENTICATE_ISO && key_len == 24) return 16;
    return 8;
}

// ------------------------------------------------------------ crypto ---

static size_t block_size(uint8_t cipher) {
    return cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
}

static void cbc_encrypt(
    uint8_t cipher,
    const uint8_t* key,
    size_t key_len,
    uint8_t* iv,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_encrypt(key, key_len, iv, len, in, out);
    } else {
        dfc_worker_des_cbc_encrypt(key, key_len, iv, len, in, out);
    }
}

static void cbc_decrypt(
    uint8_t cipher,
    const uint8_t* key,
    size_t key_len,
    uint8_t* iv,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_decrypt(key, key_len, iv, len, in, out);
    } else {
        dfc_worker_des_cbc_decrypt(key, key_len, iv, len, in, out);
    }
}

// A legacy reader decrypts in both directions: what it sends is
// C[i] = D(P[i] ^ C[i-1]), from a zero IV.
static void d40_send(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    uint8_t previous[8] = {0};
    for(size_t off = 0; off + 8 <= len; off += 8) {
        uint8_t block[8];
        for(size_t i = 0; i < 8; i++) block[i] = in[off + i] ^ previous[i];
        uint8_t iv[8] = {0};
        dfc_worker_des_cbc_decrypt(key, key_len, iv, 8, block, out + off);
        memcpy(previous, out + off, 8);
    }
}

static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i] ^ (crc & 0xFF);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ ((uint16_t)byte << 8) ^ ((uint16_t)byte << 3) ^ ((uint16_t)byte >> 4);
    }
    return crc;
}

static uint32_t crc32_extend(uint32_t crc, const uint8_t* data, size_t len) {
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static void put_le32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

// The full EV1 CMAC of `data` chained from the session IV. It becomes the next
// IV, and its first eight octets are what goes on the wire.
static bool ev1_cmac(DfcReaderSession* session, const uint8_t* data, size_t len, uint8_t* full) {
    DfcSecureMessaging* sm = &session->sm;
    if(sm->cipher == DFC_CMD_AUTHENTICATE_AES) {
        if(!aes_cmac_with_iv(
               sm->session_key, sm->session_key_len, (uint8_t*)data, len, sm->iv, full))
            return false;
        memcpy(sm->iv, full, 16);
        return true;
    }
    if(!des_cmac_with_iv(sm->session_key, sm->session_key_len, (uint8_t*)data, len, sm->iv, full))
        return false;
    memset(sm->iv, 0, sizeof(sm->iv));
    memcpy(sm->iv, full, 8);
    return true;
}

// Advance the IV over a command: CMAC(INS || data).
static bool ev1_command_cmac(
    DfcReaderSession* session,
    uint8_t ins,
    const uint8_t* data,
    size_t len,
    uint8_t* full) {
    uint8_t input[1 + DFC_COMMAND_MAX_DATA];
    if(len > DFC_COMMAND_MAX_DATA) return false;
    input[0] = ins;
    if(len) memcpy(input + 1, data, len);
    return ev1_cmac(session, input, len + 1, full);
}

// ----------------------------------------------------------- framing ---

static DfcReaderStatus emit(
    DfcReaderExchange* ex,
    uint8_t ins,
    const uint8_t* data,
    size_t len,
    uint8_t* out,
    size_t cap,
    size_t* out_len) {
    DfcCommand command;
    if(dfc_command_encode_raw(&command, ins, data, len) != DfcCommandOk) {
        return DfcReaderBufferTooSmall;
    }
    DfcCommandStatus st = ex->framing == DfcReaderFramingIso7816 ?
                              dfc_command_to_apdu(&command, out, cap, out_len) :
                              dfc_command_to_frame(&command, out, cap, out_len);
    return st == DfcCommandOk ? DfcReaderPending : DfcReaderBufferTooSmall;
}

// Split an answer into its status and data, whichever framing carried it.
static bool parse_answer(
    const DfcReaderExchange* ex,
    const uint8_t* response,
    size_t len,
    uint8_t* status,
    const uint8_t** data,
    size_t* data_len) {
    if(!response || len < 1) return false;
    if(ex->framing == DfcReaderFramingIso7816) {
        if(len < 2 || response[len - 2] != WRAPPED_SW1) return false;
        *status = response[len - 1];
        *data = response;
        *data_len = len - 2;
        return true;
    }
    *status = response[0];
    *data = response + 1;
    *data_len = len - 1;
    return true;
}

static bool append_data(DfcReaderExchange* ex, const uint8_t* data, size_t len) {
    if(len > sizeof(ex->data) - ex->data_len) return false;
    if(len) memcpy(ex->data + ex->data_len, data, len);
    ex->data_len += len;
    return true;
}

static void begin_common(DfcReaderExchange* ex, DfcReaderSession* session, DfcReaderFraming framing) {
    memset(ex, 0, sizeof(*ex));
    ex->session = session;
    ex->framing = framing;
}

// -------------------------------------------------------- authenticate ---

DfcReaderStatus dfc_reader_authenticate_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t cipher,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* random_a,
    size_t random_a_len) {
    if(!exchange || !session || !key || !random_a) return DfcReaderInvalid;
    bool legacy = cipher == DFC_CMD_AUTHENTICATE_LEGACY;
    bool iso = cipher == DFC_CMD_AUTHENTICATE_ISO;
    bool aes = cipher == DFC_CMD_AUTHENTICATE_AES;
    if(!legacy && !iso && !aes) return DfcReaderInvalid;
#if !DFC_ENABLE_AUTH_D40
    if(legacy) return DfcReaderUnsupported;
#endif
#if !DFC_ENABLE_AUTH_ISO
    if(iso) return DfcReaderUnsupported;
#endif
#if !DFC_ENABLE_AUTH_AES
    if(aes) return DfcReaderUnsupported;
#endif
    if(aes && key_len != 16) return DfcReaderInvalid;
    if(legacy && key_len != 8 && key_len != 16) return DfcReaderInvalid;
    if(iso && key_len != 8 && key_len != 16 && key_len != 24) return DfcReaderInvalid;
    if(random_a_len != dfc_reader_challenge_length(cipher, key_len)) return DfcReaderInvalid;

    begin_common(exchange, session, framing);
    exchange->kind = KindAuthenticate;
    exchange->auth_cipher = cipher;
    exchange->auth_key_no = key_no;
    memcpy(exchange->auth_key, key, key_len);
    exchange->auth_key_len = key_len;
    exchange->auth_challenge_len = random_a_len;
    memcpy(exchange->auth_random_a, random_a, random_a_len);
    exchange->ins = cipher;
    return DfcReaderOk;
}

#if DFC_ENABLE_ISO7816_AUTH
DfcReaderStatus dfc_reader_authenticate_iso7816_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    uint8_t key_reference,
    const uint8_t* key,
    size_t key_len,
    uint8_t algorithm,
    const uint8_t* random_first,
    const uint8_t* random_second,
    size_t random_len) {
    if(!exchange || !session || !key || !random_first || !random_second) return DfcReaderInvalid;
    if((key_reference & (uint8_t)~(DFC_ISO7816_AUTH_APP_REFERENCE |
                                   DFC_ISO7816_AUTH_KEY_NUMBER_MASK)) != 0)
        return DfcReaderInvalid;
    bool aes = algorithm == DFC_ISO7816_AUTH_ALGORITHM_AES;
    bool tdea3 = algorithm == DFC_ISO7816_AUTH_ALGORITHM_3TDEA;
    bool tdea2 = algorithm == DFC_ISO7816_AUTH_ALGORITHM_2TDEA;
    if((aes && (key_len != DFC_AES_KEY_LENGTH || random_len != DFC_ISO7816_AUTH_CHALLENGE_LONG)) ||
       (tdea3 && (key_len != DFC_MAX_KEY_LEN || random_len != DFC_ISO7816_AUTH_CHALLENGE_LONG)) ||
       (tdea2 && (key_len != DFC_AES_KEY_LENGTH || random_len != DFC_ISO7816_AUTH_CHALLENGE_2TDEA)) ||
       (!aes && !tdea3 && !tdea2)) return DfcReaderInvalid;
    begin_common(exchange, session, DfcReaderFramingIso7816);
    exchange->kind = KindAuthenticateIso7816;
    exchange->auth_cipher = aes ? DFC_CMD_AUTHENTICATE_AES : DFC_CMD_AUTHENTICATE_ISO;
    exchange->auth_algorithm = algorithm;
    exchange->auth_reference = key_reference;
    exchange->auth_key_no = key_reference & DFC_ISO7816_AUTH_KEY_NUMBER_MASK;
    memcpy(exchange->auth_key, key, key_len);
    exchange->auth_key_len = key_len;
    exchange->auth_challenge_len = random_len;
    memcpy(exchange->auth_random_a, random_first, random_len);
    memcpy(exchange->auth_random_second, random_second, random_len);
    return DfcReaderOk;
}
#endif

DfcReaderStatus dfc_reader_authenticate_ev2_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    bool first,
    uint8_t key_no,
    const uint8_t key[DFC_AES_KEY_LENGTH],
    const uint8_t random_a[DFC_EV2_RANDOM_LENGTH],
    const uint8_t* capabilities,
    size_t capabilities_len) {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(!exchange || !session || !key || !random_a) return DfcReaderInvalid;
    if(capabilities ? capabilities_len != DFC_EV2_CAPABILITY_LENGTH : capabilities_len != 0)
        return DfcReaderInvalid;
    if(!first && (capabilities || session->auth_mode != DFC_READER_AUTH_EV2))
        return DfcReaderInvalid;
    begin_common(exchange, session, framing);
    exchange->kind = KindAuthenticateEv2;
    exchange->auth_first = first;
    exchange->auth_key_no = key_no;
    memcpy(exchange->auth_key, key, DFC_AES_KEY_LENGTH);
    exchange->auth_key_len = DFC_AES_KEY_LENGTH;
    memcpy(exchange->auth_random_a, random_a, DFC_EV2_RANDOM_LENGTH);
    if(capabilities) memcpy(exchange->auth_capabilities, capabilities, capabilities_len);
    exchange->auth_challenge_len = capabilities_len;
    exchange->ins = first ? DFC_CMD_AUTHENTICATE_EV2_FIRST : DFC_CMD_AUTHENTICATE_EV2_NON_FIRST;
    return DfcReaderOk;
#else
    DFC_UNUSED(exchange);
    DFC_UNUSED(session);
    DFC_UNUSED(framing);
    DFC_UNUSED(first);
    DFC_UNUSED(key_no);
    DFC_UNUSED(key);
    DFC_UNUSED(random_a);
    DFC_UNUSED(capabilities);
    DFC_UNUSED(capabilities_len);
    return DfcReaderUnsupported;
#endif
}

static DfcReaderStatus fail_auth(DfcReaderExchange* ex, DfcReaderStatus status) {
    // A refused or broken handshake leaves the card unauthenticated.
    dfc_reader_session_clear(ex->session);
    wipe(ex->auth_key, sizeof(ex->auth_key));
    ex->phase = PhaseDone;
    return status;
}

#if DFC_ENABLE_ISO7816_AUTH
enum {
    IsoApduHeaderLength = 5,
    IsoApduStatusLength = 2,
    IsoApduLeLength = 1,
};

static DfcReaderStatus emit_iso_auth_apdu(
    const uint8_t* apdu, size_t length, uint8_t* out, size_t capacity, size_t* out_len) {
    if(capacity < length) return DfcReaderBufferTooSmall;
    memcpy(out, apdu, length);
    *out_len = length;
    return DfcReaderPending;
}

static bool iso_auth_answer_ok(const uint8_t* answer, size_t length, size_t expected_data) {
    return answer && length == expected_data + IsoApduStatusLength &&
           answer[length - IsoApduStatusLength] == DFC_ISO7816_SW_OK_HI &&
           answer[length - 1] == DFC_ISO7816_SW_OK_LO;
}

static DfcReaderStatus step_authenticate_iso7816(
    DfcReaderExchange* ex, const uint8_t* response, size_t response_len,
    uint8_t* out, size_t cap, size_t* out_len) {
    size_t challenge = ex->auth_challenge_len;
    size_t block = block_size(ex->auth_cipher);
    if(ex->phase == PhaseStart) {
        const uint8_t apdu[IsoApduHeaderLength] = {
            DFC_ISO7816_CLA_STANDARD, DFC_ISO7816_INS_GET_CHALLENGE,
            0, 0, (uint8_t)challenge};
        ex->phase = PhaseAuthChallenge;
        return emit_iso_auth_apdu(apdu, sizeof(apdu), out, cap, out_len);
    }
    if(!response || response_len < IsoApduStatusLength) return fail_auth(ex, DfcReaderProtocolError);
    ex->status = response[response_len - 1];
    if(response[response_len - 2] != DFC_ISO7816_SW_OK_HI ||
       ex->status != DFC_ISO7816_SW_OK_LO) return fail_auth(ex, DfcReaderCardError);

    if(ex->phase == PhaseAuthChallenge) {
        if(!iso_auth_answer_ok(response, response_len, challenge))
            return fail_auth(ex, DfcReaderProtocolError);
        memcpy(ex->auth_random_b, response, challenge);
        uint8_t clear[DFC_ISO7816_AUTH_CHALLENGE_LONG * 2];
        memcpy(clear, ex->auth_random_a, challenge);
        memcpy(clear + challenge, ex->auth_random_b, challenge);
        uint8_t apdu[IsoApduHeaderLength + sizeof(clear)];
        apdu[0] = DFC_ISO7816_CLA_STANDARD;
        apdu[1] = DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE;
        apdu[2] = ex->auth_algorithm;
        apdu[3] = ex->auth_reference;
        apdu[4] = (uint8_t)(challenge * 2);
        uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
        cbc_encrypt(ex->auth_cipher, ex->auth_key, ex->auth_key_len,
                    iv, clear, challenge * 2, apdu + IsoApduHeaderLength);
        memcpy(ex->auth_iv, apdu + IsoApduHeaderLength + challenge * 2 - block, block);
        ex->phase = PhaseAuthExternal;
        return emit_iso_auth_apdu(apdu, IsoApduHeaderLength + challenge * 2, out, cap, out_len);
    }
    if(ex->phase == PhaseAuthExternal) {
        if(!iso_auth_answer_ok(response, response_len, 0))
            return fail_auth(ex, DfcReaderProtocolError);
        uint8_t apdu[IsoApduHeaderLength + DFC_ISO7816_AUTH_CHALLENGE_LONG + IsoApduLeLength];
        apdu[0] = DFC_ISO7816_CLA_STANDARD;
        apdu[1] = DFC_ISO7816_INS_INTERNAL_AUTHENTICATE;
        apdu[2] = ex->auth_algorithm;
        apdu[3] = ex->auth_reference;
        apdu[4] = (uint8_t)challenge;
        memcpy(apdu + IsoApduHeaderLength, ex->auth_random_second, challenge);
        apdu[IsoApduHeaderLength + challenge] = 0;
        ex->phase = PhaseAuthConfirmation;
        return emit_iso_auth_apdu(apdu,
                                  IsoApduHeaderLength + challenge + IsoApduLeLength,
                                  out, cap, out_len);
    }
    if(ex->phase != PhaseAuthConfirmation) return DfcReaderInvalid;
    if(!iso_auth_answer_ok(response, response_len, challenge * 2))
        return fail_auth(ex, DfcReaderProtocolError);
    uint8_t clear[DFC_ISO7816_AUTH_CHALLENGE_LONG * 2];
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    memcpy(iv, ex->auth_iv, block);
    cbc_decrypt(ex->auth_cipher, ex->auth_key, ex->auth_key_len,
                iv, response, challenge * 2, clear);
    if(!ct_equal(clear + challenge, ex->auth_random_second, challenge))
        return fail_auth(ex, DfcReaderIntegrityError);
    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    dfc_derive_session_key(ex->auth_cipher, ex->auth_key, ex->auth_key_len,
                           ex->auth_random_a, clear, session_key, &session_key_len);
    open_legacy_session(ex->session, ex->auth_cipher, ex->auth_key_no,
                        session_key, session_key_len);
    wipe(session_key, sizeof(session_key));
    wipe(ex->auth_key, sizeof(ex->auth_key));
    ex->phase = PhaseDone;
    return DfcReaderOk;
}
#endif

static DfcReaderStatus step_authenticate(
    DfcReaderExchange* ex,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t cap,
    size_t* out_len) {
    uint8_t cipher = ex->auth_cipher;
    size_t block = block_size(cipher);
    size_t challenge = ex->auth_challenge_len;

    if(ex->phase == PhaseStart) {
        ex->phase = PhaseAuthChallenge;
        return emit(ex, cipher, &ex->auth_key_no, 1, out, cap, out_len);
    }

    uint8_t status;
    const uint8_t* data;
    size_t data_len;
    if(!parse_answer(ex, response, response_len, &status, &data, &data_len)) {
        return fail_auth(ex, DfcReaderProtocolError);
    }
    ex->status = status;

    if(ex->phase == PhaseAuthChallenge) {
        if(status != DFC_CMD_ADDITIONAL_FRAME) {
            return fail_auth(ex, status == DFC_STATUS_OK ? DfcReaderProtocolError : DfcReaderCardError);
        }
        if(data_len != challenge) return fail_auth(ex, DfcReaderProtocolError);

        uint8_t iv[16] = {0};
        cbc_decrypt(cipher, ex->auth_key, ex->auth_key_len, iv, data, challenge, ex->auth_random_b);

        uint8_t token[32];
        memcpy(token, ex->auth_random_a, challenge);
        memcpy(token + challenge, ex->auth_random_b, challenge);
        dfc_rotate_left(token + challenge, challenge);

        uint8_t encrypted[32];
        if(cipher == DFC_CMD_AUTHENTICATE_LEGACY) {
            d40_send(ex->auth_key, ex->auth_key_len, token, challenge * 2, encrypted);
        } else {
            // The card chains from the last block of its own challenge.
            uint8_t chain[16] = {0};
            memcpy(chain, data + challenge - block, block);
            cbc_encrypt(cipher, ex->auth_key, ex->auth_key_len, chain, token, challenge * 2, encrypted);
            memcpy(ex->auth_iv, encrypted + challenge * 2 - block, block);
        }
        ex->phase = PhaseAuthConfirmation;
        return emit(ex, DFC_CMD_ADDITIONAL_FRAME, encrypted, challenge * 2, out, cap, out_len);
    }

    if(ex->phase != PhaseAuthConfirmation) return DfcReaderInvalid;
    if(status != DFC_STATUS_OK) return fail_auth(ex, DfcReaderCardError);
    if(data_len != challenge) return fail_auth(ex, DfcReaderProtocolError);

    uint8_t rotated[16];
    uint8_t iv[16] = {0};
    if(cipher != DFC_CMD_AUTHENTICATE_LEGACY) memcpy(iv, ex->auth_iv, block);
    cbc_decrypt(cipher, ex->auth_key, ex->auth_key_len, iv, data, challenge, rotated);
    uint8_t expected[16];
    memcpy(expected, ex->auth_random_a, challenge);
    dfc_rotate_left(expected, challenge);
    uint8_t difference = 0;
    for(size_t i = 0; i < challenge; i++) difference |= rotated[i] ^ expected[i];
    if(difference != 0) return fail_auth(ex, DfcReaderIntegrityError);

    uint8_t session_key[DFC_MAX_KEY_LEN];
    size_t session_key_len = 0;
    // The derivation reads sixteen octets of each challenge for 3K3DES and AES.
    uint8_t random_a[16] = {0};
    uint8_t random_b[16] = {0};
    memcpy(random_a, ex->auth_random_a, challenge);
    memcpy(random_b, ex->auth_random_b, challenge);
    dfc_derive_session_key(
        cipher, ex->auth_key, ex->auth_key_len, random_a, random_b, session_key, &session_key_len);
    open_legacy_session(ex->session, cipher, ex->auth_key_no, session_key, session_key_len);
    wipe(session_key, sizeof(session_key));
    wipe(ex->auth_key, sizeof(ex->auth_key));
    ex->phase = PhaseDone;
    return DfcReaderOk;
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
static DfcReaderStatus step_authenticate_ev2(
    DfcReaderExchange* ex,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t cap,
    size_t* out_len) {
    if(ex->phase == PhaseStart) {
        ex->phase = PhaseAuthChallenge;
        if(ex->auth_first) {
            uint8_t head[2 + DFC_EV2_CAPABILITY_LENGTH];
            head[0] = ex->auth_key_no;
            head[1] = (uint8_t)ex->auth_challenge_len;
            memcpy(head + 2, ex->auth_capabilities, ex->auth_challenge_len);
            return emit(ex, ex->ins, head, 2 + ex->auth_challenge_len, out, cap, out_len);
        }
        return emit(ex, ex->ins, &ex->auth_key_no, 1, out, cap, out_len);
    }

    uint8_t status;
    const uint8_t* data;
    size_t data_len;
    if(!parse_answer(ex, response, response_len, &status, &data, &data_len)) {
        return fail_auth(ex, DfcReaderProtocolError);
    }
    ex->status = status;

    if(ex->phase == PhaseAuthChallenge) {
        if(status != DFC_CMD_ADDITIONAL_FRAME) {
            return fail_auth(ex, status == DFC_STATUS_OK ? DfcReaderProtocolError : DfcReaderCardError);
        }
        if(data_len != DFC_EV2_RANDOM_LENGTH) return fail_auth(ex, DfcReaderProtocolError);
        uint8_t iv[16] = {0};
        dfc_worker_aes_cbc_decrypt(
            ex->auth_key, DFC_AES_KEY_LENGTH, iv, DFC_EV2_RANDOM_LENGTH, data, ex->auth_random_b);
        uint8_t token[2 * DFC_EV2_RANDOM_LENGTH];
        memcpy(token, ex->auth_random_a, DFC_EV2_RANDOM_LENGTH);
        dfc_ev2_rotate_left(ex->auth_random_b, token + DFC_EV2_RANDOM_LENGTH);
        uint8_t encrypted[sizeof(token)];
        memset(iv, 0, sizeof(iv));
        dfc_worker_aes_cbc_encrypt(ex->auth_key, DFC_AES_KEY_LENGTH, iv, sizeof(token), token, encrypted);
        ex->phase = PhaseAuthConfirmation;
        return emit(ex, DFC_CMD_ADDITIONAL_FRAME, encrypted, sizeof(encrypted), out, cap, out_len);
    }

    if(ex->phase != PhaseAuthConfirmation) return DfcReaderInvalid;
    if(status != DFC_STATUS_OK) return fail_auth(ex, DfcReaderCardError);
    size_t expected_len = ex->auth_first ? DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH : DFC_EV2_RANDOM_LENGTH;
    if(data_len != expected_len) return fail_auth(ex, DfcReaderProtocolError);

    uint8_t clear[DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH];
    uint8_t iv[16] = {0};
    dfc_worker_aes_cbc_decrypt(ex->auth_key, DFC_AES_KEY_LENGTH, iv, data_len, data, clear);
    const uint8_t* rotated = ex->auth_first ? clear + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH : clear;
    uint8_t expected[DFC_EV2_RANDOM_LENGTH];
    dfc_ev2_rotate_left(ex->auth_random_a, expected);
    if(!ct_equal(rotated, expected, DFC_EV2_RANDOM_LENGTH)) {
        return fail_auth(ex, DfcReaderIntegrityError);
    }

    DfcReaderSession* session = ex->session;
    uint8_t transaction[DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH];
    uint8_t card_caps[DFC_EV2_CAPABILITY_LENGTH];
    uint8_t reader_caps[DFC_EV2_CAPABILITY_LENGTH];
    if(ex->auth_first) {
        memcpy(transaction, clear, sizeof(transaction));
        memcpy(card_caps, clear + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH + DFC_EV2_RANDOM_LENGTH, sizeof(card_caps));
        memcpy(
            reader_caps,
            clear + DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH + DFC_EV2_RANDOM_LENGTH +
                DFC_EV2_CAPABILITY_LENGTH,
            sizeof(reader_caps));
    } else {
        memcpy(transaction, session->ev2_transaction_identifier, sizeof(transaction));
        memcpy(card_caps, session->ev2_card_capabilities, sizeof(card_caps));
        memcpy(reader_caps, session->ev2_reader_capabilities, sizeof(reader_caps));
    }

    uint8_t encryption_key[DFC_AES_KEY_LENGTH];
    uint8_t mac_key[DFC_AES_KEY_LENGTH];
    if(!dfc_ev2_derive_session_keys(
           ex->auth_key, ex->auth_random_a, ex->auth_random_b, encryption_key, mac_key)) {
        return fail_auth(ex, DfcReaderIntegrityError);
    }
    dfc_reader_session_init(session);
    session->auth_mode = DFC_READER_AUTH_EV2;
    session->key_no = ex->auth_key_no;
    memcpy(session->ev2_transaction_identifier, transaction, sizeof(transaction));
    session->ev2_command_counter = 0;
    memcpy(session->ev2_encryption_key, encryption_key, sizeof(encryption_key));
    memcpy(session->ev2_mac_key, mac_key, sizeof(mac_key));
    memcpy(session->ev2_card_capabilities, card_caps, sizeof(card_caps));
    memcpy(session->ev2_reader_capabilities, reader_caps, sizeof(reader_caps));
    wipe(encryption_key, sizeof(encryption_key));
    wipe(mac_key, sizeof(mac_key));
    wipe(ex->auth_key, sizeof(ex->auth_key));
    ex->phase = PhaseDone;
    return DfcReaderOk;
}
#endif

// ----------------------------------------------------------- command ---

static bool is_file_read(uint8_t ins) {
    return ins == DFC_CMD_READ_DATA || ins == DFC_CMD_READ_RECORDS || ins == 0xAB ||
           ins == DFC_CMD_GET_VALUE;
}

static bool is_file_command(uint8_t ins) {
    switch(ins) {
    case DFC_CMD_READ_DATA:
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_READ_RECORDS:
    case DFC_CMD_WRITE_RECORD:
    case DFC_CMD_UPDATE_RECORD:
    case DFC_CMD_UPDATE_RECORD_ISO:
    case DFC_CMD_GET_VALUE:
    case DFC_CMD_CREDIT:
    case DFC_CMD_DEBIT:
    case DFC_CMD_LIMITED_CREDIT:
        return true;
    default:
        return false;
    }
}

// Clear head of a command whose data field a Full file encrypts, counted after
// the instruction. SIZE_MAX when the instruction has no encrypted field.
static size_t file_header_length(uint8_t ins) {
    switch(ins) {
    case DFC_CMD_WRITE_DATA:
    case DFC_CMD_WRITE_RECORD:
        return FILE_HEADER_LENGTH;
    case DFC_CMD_UPDATE_RECORD:
    case DFC_CMD_UPDATE_RECORD_ISO:
        return UPDATE_RECORD_HEADER_LENGTH;
    case DFC_CMD_CREDIT:
    case DFC_CMD_DEBIT:
    case DFC_CMD_LIMITED_CREDIT:
        return VALUE_HEADER_LENGTH;
    default:
        return SIZE_MAX;
    }
}

static bool invalidates_session(const DfcReaderSession* session, const DfcCommand* command) {
    switch(command->ins) {
    case DFC_CMD_SELECT_APPLICATION:
    case DFC_CMD_DELETE_APPLICATION:
    case DFC_CMD_FORMAT_PICC:
        return true;
    case DFC_CMD_CHANGE_KEY:
        return command->data_len > 0 && (command->data[0] & 0x3F) == (session->key_no & 0x3F);
    case DFC_CMD_CHANGE_KEY_EV2:
        // Key set 0 holds the keys in use, so changing the authenticated one
        // there ends the session.
        return session->auth_mode == DFC_READER_AUTH_EV2 && command->data_len > 1 &&
               (command->data[0] & DFC_KEY_SET_NUMBER_MASK) == 0 &&
               command->data[1] == session->key_no;
    default:
        return false;
    }
}

static bool is_special_success(uint8_t ins, uint8_t status) {
    if(status != DFC_STATUS_SPECIAL_SUCCESS) return false;
    return ins == DFC_CMD_READ_SIGNATURE || ins == DFC_CMD_PREPARE_PROXIMITY_CHECK ||
           ins == DFC_CMD_PROXIMITY_CHECK || ins == DFC_CMD_VERIFY_PROXIMITY_CHECK;
}

static bool uses_native_command_chaining(uint8_t ins) {
    return ins == DFC_CMD_WRITE_DATA || ins == DFC_CMD_WRITE_RECORD ||
           ins == DFC_CMD_UPDATE_RECORD;
}

static bool set_frame(DfcReaderExchange* ex, const uint8_t* a, size_t a_len, const uint8_t* b, size_t b_len) {
    if(a_len > sizeof(ex->frame) || b_len > sizeof(ex->frame) - a_len) return false;
    if(a_len) memcpy(ex->frame, a, a_len);
    if(b_len) memcpy(ex->frame + a_len, b, b_len);
    ex->frame_len = a_len + b_len;
    return true;
}

static DfcReaderStatus prepare_legacy(DfcReaderExchange* ex, const DfcCommand* command, size_t header_len) {
    DfcReaderSession* session = ex->session;
    uint8_t ins = command->ins;
    bool file_write = is_file_command(ins) && !is_file_read(ins);
    if(file_write && ex->comm_mode != DFC_COMM_PLAIN) {
        if(header_len == SIZE_MAX || header_len > command->data_len) return DfcReaderInvalid;
        uint8_t wrapped[DFC_SM_MAX_SIZE];
        const uint8_t* body = command->data + header_len;
        size_t body_len = command->data_len - header_len;
        if(body_len + 16 > sizeof(wrapped)) return DfcReaderBufferTooSmall;
        uint8_t head[1 + UPDATE_RECORD_HEADER_LENGTH];
        head[0] = ins;
        memcpy(head + 1, command->data, header_len);
        size_t wrapped_len = dfc_secure_messaging_wrap(
            &session->sm, ex->comm_mode, head, header_len + 1, body, body_len, wrapped);
        if(!set_frame(ex, command->data, header_len, wrapped, wrapped_len)) {
            return DfcReaderBufferTooSmall;
        }
        return DfcReaderOk;
    }
    if(is_file_read(ins) && ex->comm_mode == DFC_COMM_MAC) ex->response_plan = PlanD40Mac;
    if(is_file_read(ins) && ex->comm_mode == DFC_COMM_ENCIPHERED) {
        ex->response_plan = PlanD40Decrypt;
    }
    return set_frame(ex, command->data, command->data_len, NULL, 0) ? DfcReaderOk :
                                                                     DfcReaderBufferTooSmall;
}

static DfcReaderStatus prepare_ev1(DfcReaderExchange* ex, const DfcCommand* command, size_t header_len) {
    DfcReaderSession* session = ex->session;
    uint8_t ins = command->ins;
    if(!dfc_secure_messaging_applies_ev1(&session->sm, ins)) {
        return set_frame(ex, command->data, command->data_len, NULL, 0) ? DfcReaderOk :
                                                                         DfcReaderBufferTooSmall;
    }
    ex->response_plan = ins == DFC_CMD_GET_VERSION ? PlanEv1CmacPerFrame : PlanEv1Cmac;
    uint8_t full[16];

    if(ins == DFC_CMD_CHANGE_KEY) {
        // Clear key number, then a cryptogram already chained from the session
        // IV. Its last block is the IV the card continues from.
        size_t block = block_size(session->sm.cipher);
        if(command->data_len <= CHANGE_KEY_HEADER ||
           (command->data_len - CHANGE_KEY_HEADER) % block != 0) {
            return DfcReaderInvalid;
        }
        memset(session->sm.iv, 0, sizeof(session->sm.iv));
        memcpy(session->sm.iv, command->data + command->data_len - block, block);
        return set_frame(ex, command->data, command->data_len, NULL, 0) ? DfcReaderOk :
                                                                         DfcReaderBufferTooSmall;
    }

    bool file_write = is_file_command(ins) && !is_file_read(ins);
    if(file_write && ex->comm_mode == DFC_COMM_ENCIPHERED) {
        if(header_len == SIZE_MAX || header_len > command->data_len) return DfcReaderInvalid;
        uint8_t wrapped[DFC_SM_MAX_SIZE];
        const uint8_t* body = command->data + header_len;
        size_t body_len = command->data_len - header_len;
        if(body_len + 20 > sizeof(wrapped)) return DfcReaderBufferTooSmall;
        uint8_t head[1 + UPDATE_RECORD_HEADER_LENGTH];
        head[0] = ins;
        memcpy(head + 1, command->data, header_len);
        size_t wrapped_len = dfc_secure_messaging_wrap(
            &session->sm, DFC_COMM_ENCIPHERED, head, header_len + 1, body, body_len, wrapped);
        if(!set_frame(ex, command->data, header_len, wrapped, wrapped_len)) {
            return DfcReaderBufferTooSmall;
        }
        return DfcReaderOk;
    }

    if(!ev1_command_cmac(session, ins, command->data, command->data_len, full)) {
        return DfcReaderBufferTooSmall;
    }
    if(ex->comm_mode == DFC_COMM_MAC && dfc_secure_messaging_ev1_transmits_command_mac(ins)) {
        return set_frame(ex, command->data, command->data_len, full, DFC_WORKER_CMAC_SIZE) ?
                   DfcReaderOk :
                   DfcReaderBufferTooSmall;
    }
    if(is_file_read(ins) && ex->comm_mode == DFC_COMM_ENCIPHERED) {
        ex->response_plan = PlanEv1Decrypt;
        ex->command_head[0] = ins;
        ex->command_head_len = 1;
    }
    return set_frame(ex, command->data, command->data_len, NULL, 0) ? DfcReaderOk :
                                                                     DfcReaderBufferTooSmall;
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
// EV2: instructions whose data field is encrypted whatever the file says.
static size_t ev2_always_encrypted_header(uint8_t ins) {
    switch(ins) {
    case DFC_CMD_SET_CONFIGURATION:
        return SET_CONFIGURATION_HEADER;
    case DFC_CMD_CREATE_TRANSACTION_MAC_FILE:
        return CREATE_TMAC_HEADER;
    case DFC_CMD_COMMIT_READER_ID:
        return COMMIT_READER_ID_HEADER;
    case DFC_CMD_CHANGE_KEY_EV2:
        return CHANGE_KEY_EV2_HEADER;
    default:
        return SIZE_MAX;
    }
}

static bool ev2_bypasses(uint8_t ins, uint8_t comm_mode) {
    switch(ins) {
    case DFC_CMD_ADDITIONAL_FRAME:
    case DFC_CMD_AUTHENTICATE_EV2_FIRST:
    case DFC_CMD_AUTHENTICATE_EV2_NON_FIRST:
    case DFC_CMD_CREATE_DELEGATED_APPLICATION:
        return true;
    default:
        break;
    }
    return is_file_command(ins) && comm_mode == DFC_COMM_PLAIN;
}

static DfcReaderStatus prepare_ev2(DfcReaderExchange* ex, const DfcCommand* command, size_t header_len) {
    DfcReaderSession* session = ex->session;
    uint8_t ins = command->ins;
    if(ev2_bypasses(ins, ex->comm_mode)) {
        return set_frame(ex, command->data, command->data_len, NULL, 0) ? DfcReaderOk :
                                                                         DfcReaderBufferTooSmall;
    }

    size_t encrypted_header = ev2_always_encrypted_header(ins);
    if(encrypted_header == SIZE_MAX && ex->comm_mode == DFC_COMM_ENCIPHERED) {
        encrypted_header = header_len;
    }
    ex->response_plan =
        is_file_read(ins) && ex->comm_mode == DFC_COMM_ENCIPHERED ? PlanEv2MacDecrypt : PlanEv2Mac;

    uint8_t secured[DFC_READER_MAX_COMMAND];
    size_t secured_len = 0;
    if(encrypted_header != SIZE_MAX && !is_file_read(ins)) {
        if(encrypted_header >= command->data_len) return DfcReaderInvalid;
        memcpy(secured, command->data, encrypted_header);
        size_t encrypted_len = 0;
        if(!dfc_ev2_encrypt_data(
               session->ev2_encryption_key,
               DfcEv2DirectionCommand,
               session->ev2_command_counter,
               session->ev2_transaction_identifier,
               command->data + encrypted_header,
               command->data_len - encrypted_header,
               secured + encrypted_header,
               sizeof(secured) - encrypted_header - DFC_WIRE_MAC_LENGTH,
               &encrypted_len)) {
            return DfcReaderBufferTooSmall;
        }
        secured_len = encrypted_header + encrypted_len;
    } else {
        memcpy(secured, command->data, command->data_len);
        secured_len = command->data_len;
    }
    uint8_t mac[DFC_WIRE_MAC_LENGTH];
    if(!dfc_ev2_mac(
           session->ev2_mac_key,
           ins,
           session->ev2_command_counter,
           session->ev2_transaction_identifier,
           secured,
           secured_len,
           NULL,
           0,
           mac)) {
        return DfcReaderBufferTooSmall;
    }
    return set_frame(ex, secured, secured_len, mac, sizeof(mac)) ? DfcReaderOk :
                                                                  DfcReaderBufferTooSmall;
}
#endif

DfcReaderStatus dfc_reader_exchange_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    const DfcCommand* command,
    const DfcReaderOptions* options) {
    if(!exchange || !session || !command || command->data_len > DFC_COMMAND_MAX_DATA) {
        return DfcReaderInvalid;
    }
    DfcReaderOptions plain = {0};
    if(!options) options = &plain;
    if(options->comm_mode != DFC_COMM_PLAIN && options->comm_mode != DFC_COMM_MAC &&
       options->comm_mode != DFC_COMM_ENCIPHERED) {
        return DfcReaderInvalid;
    }
    if(command->ins == DFC_CMD_AUTHENTICATE_LEGACY || command->ins == DFC_CMD_AUTHENTICATE_ISO ||
       command->ins == DFC_CMD_AUTHENTICATE_AES || command->ins == DFC_CMD_AUTHENTICATE_EV2_FIRST ||
       command->ins == DFC_CMD_AUTHENTICATE_EV2_NON_FIRST) {
        // Authentication has its own entry points, which keep the session right.
        return DfcReaderInvalid;
    }

    begin_common(exchange, session, framing);
    exchange->kind = KindCommand;
    exchange->ins = command->ins;
    exchange->comm_mode = options->comm_mode;
    exchange->invalidates_session = invalidates_session(session, command);
    size_t header_len = options->has_header_len ? options->header_len : file_header_length(command->ins);

    DfcReaderStatus st = DfcReaderOk;
    if(session->auth_mode == DFC_READER_AUTH_NONE) {
        st = set_frame(exchange, command->data, command->data_len, NULL, 0) ? DfcReaderOk :
                                                                            DfcReaderBufferTooSmall;
    } else if(session->auth_mode == DFC_CMD_AUTHENTICATE_LEGACY) {
        st = prepare_legacy(exchange, command, header_len);
    } else if(session_is_ev1(session)) {
        st = prepare_ev1(exchange, command, header_len);
    } else {
#if DFC_ENABLE_EV2_SECURE_MESSAGING
        st = prepare_ev2(exchange, command, header_len);
#else
        st = DfcReaderUnsupported;
#endif
    }
    if(st != DfcReaderOk) exchange->phase = PhaseDone;
    if(st == DfcReaderOk && session->auth_mode != DFC_READER_AUTH_EV2 &&
       uses_native_command_chaining(command->ins) &&
       exchange->frame_len > DFC_EV1_MAX_FRAME_PAYLOAD) {
        exchange->first_frame_len = DFC_EV1_MAX_FRAME_PAYLOAD;
        exchange->command_frame_len = DFC_EV1_MAX_FRAME_PAYLOAD;
    }
    return st;
}

static DfcReaderStatus begin_stream_chunk(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    const DfcReaderStream* stream) {
    size_t remaining = stream->length - stream->offset;
    size_t chunk = remaining > DFC_READER_STREAM_CHUNK ? DFC_READER_STREAM_CHUNK : remaining;
    uint32_t offset = stream->base_offset + (uint32_t)stream->offset;
    const uint8_t* data = chunk ? stream->data + stream->offset : NULL;
    DfcCommand command;
    DfcCommandStatus encoded;
    switch(stream->kind) {
    case StreamWriteData:
        encoded = dfc_command_write_data(&command, stream->file_number, offset, data, chunk);
        break;
    case StreamWriteRecord:
        encoded = dfc_command_write_record(
            &command, stream->file_number, offset, data, chunk, false);
        break;
    case StreamUpdateRecord:
        encoded = dfc_command_update_record(
            &command,
            stream->file_number,
            stream->record_number,
            offset,
            data,
            chunk,
            false);
        break;
    default:
        return DfcReaderInvalid;
    }
    if(encoded != DfcCommandOk) return DfcReaderInvalid;
    DfcReaderOptions options = {.comm_mode = stream->comm_mode};
    DfcReaderStatus status = dfc_reader_exchange_begin(
        exchange, session, framing, &command, &options);
    if(status == DfcReaderOk) {
        exchange->stream = *stream;
        exchange->stream.offset += chunk;
    }
    return status;
}

static DfcReaderStatus begin_stream(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t kind,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    if(!exchange || !session || (!data && data_len) ||
       (comm_mode != DFC_COMM_PLAIN && comm_mode != DFC_COMM_MAC &&
        comm_mode != DFC_COMM_ENCIPHERED) ||
       offset > DFC_COMMAND_UINT24_MAX || data_len > DFC_MAX_FILE_DATA ||
       data_len > DFC_COMMAND_UINT24_MAX - offset ||
       record_number > DFC_COMMAND_UINT24_MAX) {
        return DfcReaderInvalid;
    }
    DfcReaderStream stream = {
        .data = data,
        .length = data_len,
        .base_offset = offset,
        .record_number = record_number,
        .file_number = file_number,
        .comm_mode = comm_mode,
        .kind = kind,
    };
    return begin_stream_chunk(exchange, session, framing, &stream);
}

DfcReaderStatus dfc_reader_write_data_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    return begin_stream(
        exchange, session, framing, StreamWriteData, file_number, 0, offset, data, data_len, comm_mode);
}

DfcReaderStatus dfc_reader_write_record_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t file_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    return begin_stream(
        exchange, session, framing, StreamWriteRecord, file_number, 0, offset, data, data_len, comm_mode);
}

DfcReaderStatus dfc_reader_update_record_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    uint8_t file_number,
    uint32_t record_number,
    uint32_t offset,
    const uint8_t* data,
    size_t data_len,
    uint8_t comm_mode) {
    return begin_stream(
        exchange,
        session,
        framing,
        StreamUpdateRecord,
        file_number,
        record_number,
        offset,
        data,
        data_len,
        comm_mode);
}

static DfcReaderStatus finish_command(DfcReaderExchange* ex, DfcReaderStatus status) {
    ex->phase = PhaseDone;
    return status;
}

static DfcReaderStatus card_error(DfcReaderExchange* ex) {
    DfcReaderSession* session = ex->session;
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    if(session->auth_mode == DFC_READER_AUTH_EV2 && ex->response_plan != PlanPlain) {
        // An EV2 card MACs its error answers too, and counts the command.
        if(ex->data_len >= DFC_WIRE_MAC_LENGTH) {
            uint16_t next = (uint16_t)(session->ev2_command_counter + 1);
            size_t body = ex->data_len - DFC_WIRE_MAC_LENGTH;
            uint8_t mac[DFC_WIRE_MAC_LENGTH];
            if(!dfc_ev2_mac(
                   session->ev2_mac_key,
                   ex->status,
                   next,
                   session->ev2_transaction_identifier,
                   ex->data,
                   body,
                   NULL,
                   0,
                   mac) ||
               !ct_equal(mac, ex->data + body, sizeof(mac))) {
                dfc_reader_session_clear(session);
                return finish_command(ex, DfcReaderIntegrityError);
            }
            session->ev2_command_counter = next;
            ex->data_len = body;
            return finish_command(ex, DfcReaderCardError);
        }
    }
#endif
    // Any other error ends the card's session.
    dfc_reader_session_clear(session);
    return finish_command(ex, DfcReaderCardError);
}

static DfcReaderStatus verify_ev1_frame(DfcReaderExchange* ex, size_t frame_start, uint8_t status);

// The reassembled answer is complete: check and unwrap it.
static DfcReaderStatus finish_answer(DfcReaderExchange* ex) {
    DfcReaderSession* session = ex->session;
    uint8_t status = ex->status;

    // A special success carries no EV1 MAC, but an EV2 card still MACs it and
    // counts the command.
    bool ev2_plan = ex->response_plan == PlanEv2Mac || ex->response_plan == PlanEv2MacDecrypt;
    if(ex->special_success && !ev2_plan) return finish_command(ex, DfcReaderOk);
    if(status != DFC_STATUS_OK && !ex->special_success) return card_error(ex);
    if(ex->invalidates_session) {
        dfc_reader_session_clear(session);
        return finish_command(ex, DfcReaderOk);
    }

    uint8_t clear[DFC_READER_MAX_RESPONSE];
    size_t clear_len = 0;
    switch(ex->response_plan) {
    case PlanPlain:
        return finish_command(ex, DfcReaderOk);
    case PlanEv1Cmac:
    case PlanEv1CmacPerFrame:
        return finish_command(ex, verify_ev1_frame(ex, 0, status));
    case PlanEv1Decrypt:
    case PlanD40Decrypt:
    case PlanD40Mac: {
        if(ex->data_len == 0 && ex->response_plan != PlanD40Mac) return finish_command(ex, DfcReaderOk);
        uint8_t comm = ex->response_plan == PlanD40Mac ? DFC_COMM_MAC : DFC_COMM_ENCIPHERED;
        if(ex->data_len > DFC_SM_MAX_CRYPTO_SIZE)
            return finish_command(ex, DfcReaderBufferTooSmall);
        clear_len = dfc_secure_messaging_unwrap(&session->sm, comm, status, ex->data, ex->data_len, clear);
        if(clear_len == 0 && ex->data_len > 0 && !(comm == DFC_COMM_MAC && ex->data_len == 4)) {
            dfc_reader_session_clear(session);
            return finish_command(ex, DfcReaderIntegrityError);
        }
        memcpy(ex->data, clear, clear_len);
        ex->data_len = clear_len;
        return finish_command(ex, DfcReaderOk);
    }
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    case PlanEv2Mac:
    case PlanEv2MacDecrypt: {
        if(ex->data_len < DFC_WIRE_MAC_LENGTH || session->ev2_command_counter == UINT16_MAX) {
            dfc_reader_session_clear(session);
            return finish_command(ex, DfcReaderProtocolError);
        }
        uint16_t next = (uint16_t)(session->ev2_command_counter + 1);
        size_t body = ex->data_len - DFC_WIRE_MAC_LENGTH;
        uint8_t mac[DFC_WIRE_MAC_LENGTH];
        if(!dfc_ev2_mac(
               session->ev2_mac_key,
               status,
               next,
               session->ev2_transaction_identifier,
               ex->data,
               body,
               NULL,
               0,
               mac) ||
           !ct_equal(mac, ex->data + body, sizeof(mac))) {
            dfc_reader_session_clear(session);
            return finish_command(ex, DfcReaderIntegrityError);
        }
        session->ev2_command_counter = next;
        ex->data_len = body;
        if(ex->response_plan == PlanEv2MacDecrypt && body > 0) {
            if(!dfc_ev2_decrypt_data(
                   session->ev2_encryption_key,
                   DfcEv2DirectionResponse,
                   next,
                   session->ev2_transaction_identifier,
                   ex->data,
                   body,
                   clear,
                   &clear_len,
                   false)) {
                dfc_reader_session_clear(session);
                return finish_command(ex, DfcReaderIntegrityError);
            }
            memcpy(ex->data, clear, clear_len);
            ex->data_len = clear_len;
        }
        return finish_command(ex, DfcReaderOk);
    }
#endif
    default:
        return finish_command(ex, DfcReaderInvalid);
    }
}

// Check the CMAC that closes the frame starting at `frame_start`, over the
// frame's data and its status, and drop the MAC from the collected data.
static DfcReaderStatus verify_ev1_frame(DfcReaderExchange* ex, size_t frame_start, uint8_t status) {
    size_t frame_len = ex->data_len - frame_start;
    if(frame_len < DFC_WORKER_CMAC_SIZE) {
        dfc_reader_session_clear(ex->session);
        return DfcReaderProtocolError;
    }
    size_t body = frame_len - DFC_WORKER_CMAC_SIZE;
    uint8_t input[DFC_READER_MAX_RESPONSE + 1];
    memcpy(input, ex->data + frame_start, body);
    input[body] = status;
    uint8_t full[16];
    if(!ev1_cmac(ex->session, input, body + 1, full) ||
       !ct_equal(full, ex->data + frame_start + body, DFC_WORKER_CMAC_SIZE)) {
        dfc_reader_session_clear(ex->session);
        return DfcReaderIntegrityError;
    }
    ex->data_len = frame_start + body;
    return DfcReaderOk;
}

static DfcReaderStatus emit_command_frame(
    DfcReaderExchange* ex,
    uint8_t* out,
    size_t cap,
    size_t* out_len) {
    size_t remaining = ex->frame_len - ex->frame_offset;
    size_t limit;
    if(ex->frame_offset == 0 && ex->first_frame_len) {
        limit = ex->first_frame_len;
    } else {
        limit = ex->command_frame_len ? ex->command_frame_len : DFC_COMMAND_MAX_DATA;
    }
    size_t chunk = remaining > limit ? limit : remaining;
    uint8_t ins = ex->frame_offset == 0 ? ex->ins : DFC_CMD_ADDITIONAL_FRAME;
    DfcReaderStatus st = emit(ex, ins, ex->frame + ex->frame_offset, chunk, out, cap, out_len);
    if(st != DfcReaderPending) return finish_command(ex, st);
    ex->frame_offset += chunk;
    ex->phase = ex->frame_offset < ex->frame_len ? PhaseCommandFrames : PhaseResponse;
    return DfcReaderPending;
}

static DfcReaderStatus step_command(
    DfcReaderExchange* ex,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t cap,
    size_t* out_len) {
    if(ex->phase == PhaseStart) {
        if(ex->frame_len > DFC_COMMAND_MAX_DATA && ex->first_frame_len == 0 &&
           ex->session->auth_mode != DFC_READER_AUTH_EV2) {
            return finish_command(ex, DfcReaderBufferTooSmall);
        }
        return emit_command_frame(ex, out, cap, out_len);
    }

    uint8_t status;
    const uint8_t* data;
    size_t data_len;
    if(!parse_answer(ex, response, response_len, &status, &data, &data_len)) {
        return finish_command(ex, DfcReaderProtocolError);
    }

    if(ex->phase == PhaseCommandFrames) {
        if(status != DFC_CMD_ADDITIONAL_FRAME || data_len != 0) {
            ex->status = status;
            if(!append_data(ex, data, data_len)) return finish_command(ex, DfcReaderBufferTooSmall);
            return status == DFC_CMD_ADDITIONAL_FRAME ? finish_command(ex, DfcReaderProtocolError) :
                                                        card_error(ex);
        }
        return emit_command_frame(ex, out, cap, out_len);
    }

    if(ex->phase != PhaseResponse) return DfcReaderInvalid;
    ex->status = status;

    if(ex->response_plan == PlanEv1CmacPerFrame) {
        // Each GetVersion frame stands on its own: check it as it arrives, and
        // continue the chain with an additional frame that advances the IV.
        size_t frame_start = ex->data_len;
        if(!append_data(ex, data, data_len)) return finish_command(ex, DfcReaderBufferTooSmall);
        if(status != DFC_STATUS_OK && status != DFC_CMD_ADDITIONAL_FRAME) return card_error(ex);
        DfcReaderStatus st = verify_ev1_frame(ex, frame_start, status);
        if(st != DfcReaderOk) return finish_command(ex, st);
        if(status == DFC_STATUS_OK) return finish_command(ex, DfcReaderOk);
        uint8_t full[16];
        if(!ev1_command_cmac(ex->session, DFC_CMD_ADDITIONAL_FRAME, NULL, 0, full)) {
            return finish_command(ex, DfcReaderBufferTooSmall);
        }
        return emit(ex, DFC_CMD_ADDITIONAL_FRAME, NULL, 0, out, cap, out_len);
    }

    if(is_special_success(ex->ins, status)) ex->special_success = true;
    if(!append_data(ex, data, data_len)) return finish_command(ex, DfcReaderBufferTooSmall);
    if(status == DFC_CMD_ADDITIONAL_FRAME) {
        return emit(ex, DFC_CMD_ADDITIONAL_FRAME, NULL, 0, out, cap, out_len);
    }
    return finish_answer(ex);
}

DfcReaderStatus dfc_reader_step(
    DfcReaderExchange* exchange,
    const uint8_t* response,
    size_t response_len,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!exchange || !out || !out_len) return DfcReaderInvalid;
    *out_len = 0;
    if(exchange->phase == PhaseDone) return DfcReaderInvalid;
    if(exchange->phase != PhaseStart && !response) return DfcReaderInvalid;
    switch(exchange->kind) {
    case KindAuthenticate:
        return step_authenticate(exchange, response, response_len, out, out_cap, out_len);
#if DFC_ENABLE_ISO7816_AUTH
    case KindAuthenticateIso7816:
        return step_authenticate_iso7816(exchange, response, response_len,
                                         out, out_cap, out_len);
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    case KindAuthenticateEv2:
        return step_authenticate_ev2(exchange, response, response_len, out, out_cap, out_len);
#endif
    case KindCommand:
    {
        DfcReaderStatus status = step_command(exchange, response, response_len, out, out_cap, out_len);
        if(status != DfcReaderOk || exchange->stream.kind == StreamNone ||
           exchange->stream.offset >= exchange->stream.length) {
            return status;
        }
        DfcReaderStream stream = exchange->stream;
        status = begin_stream_chunk(
            exchange, exchange->session, exchange->framing, &stream);
        if(status != DfcReaderOk) return status;
        return step_command(exchange, NULL, 0, out, out_cap, out_len);
    }
    default:
        return DfcReaderInvalid;
    }
}

uint8_t dfc_reader_result_status(const DfcReaderExchange* exchange) {
    return exchange ? exchange->status : 0;
}

const uint8_t* dfc_reader_result_data(const DfcReaderExchange* exchange, size_t* len) {
    if(!exchange) {
        if(len) *len = 0;
        return NULL;
    }
    if(len) *len = exchange->data_len;
    return exchange->data;
}

// ------------------------------------------------------ key cryptograms ---

static void set_des_key_version(uint8_t* key, size_t key_len, uint8_t version) {
    for(size_t i = 0; i < 8 && i < key_len; i++) {
        uint8_t bit = (version >> (7 - i)) & 0x01;
        key[i] = (uint8_t)((key[i] & 0xFE) | bit);
        if(key_len == 16) key[i + 8] = (uint8_t)((key[i + 8] & 0xFE) | ((~bit) & 0x01));
    }
}

DfcReaderStatus dfc_reader_change_key_cryptogram(
    const DfcReaderSession* session,
    uint8_t key_no,
    const uint8_t* new_key,
    size_t key_len,
    const uint8_t* current_key,
    bool aes_key,
    uint8_t new_version,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!session || !new_key || !out || !out_len) return DfcReaderInvalid;
    if(key_len != 8 && key_len != 16 && key_len != 24) return DfcReaderInvalid;
    if(aes_key && key_len != 16) return DfcReaderInvalid;
    bool different = (key_no & 0x3F) != (session->key_no & 0x3F);
    if(different && !current_key) return DfcReaderInvalid;

    uint8_t stored[DFC_MAX_KEY_LEN];
    memcpy(stored, new_key, key_len);
    if(!aes_key) set_des_key_version(stored, key_len, new_version);

    uint8_t clear[48] = {0};
    size_t cursor = 0;
    for(size_t i = 0; i < key_len; i++) {
        clear[i] = different ? (uint8_t)(stored[i] ^ current_key[i]) : stored[i];
    }
    cursor = key_len;

    if(session->auth_mode == DFC_CMD_AUTHENTICATE_LEGACY) {
        uint16_t crc = crc16(clear, key_len);
        clear[cursor++] = (uint8_t)crc;
        clear[cursor++] = (uint8_t)(crc >> 8);
        if(different) {
            uint16_t new_crc = crc16(stored, key_len);
            clear[cursor++] = (uint8_t)new_crc;
            clear[cursor++] = (uint8_t)(new_crc >> 8);
        }
        size_t padded = ((cursor + 7) / 8) * 8;
        if(padded > out_cap) return DfcReaderBufferTooSmall;
        d40_send(session->sm.session_key, session->sm.session_key_len, clear, padded, out);
        *out_len = padded;
        return DfcReaderOk;
    }

    if(!session_is_ev1(session)) return DfcReaderInvalid;
    if(aes_key) clear[cursor++] = new_version;
    uint8_t crc_head[2] = {DFC_CMD_CHANGE_KEY, (uint8_t)(key_no & 0x3F)};
    uint32_t crc = crc32_extend(0xFFFFFFFFu, crc_head, sizeof(crc_head));
    crc = crc32_extend(crc, clear, cursor);
    put_le32(clear + cursor, crc);
    cursor += 4;
    if(different) {
        put_le32(clear + cursor, crc32_extend(0xFFFFFFFFu, stored, key_len));
        cursor += 4;
    }
    size_t block = block_size(session->sm.cipher);
    size_t padded = ((cursor + block - 1) / block) * block;
    if(padded > out_cap) return DfcReaderBufferTooSmall;
    uint8_t iv[16];
    memcpy(iv, session->sm.iv, sizeof(iv));
    cbc_encrypt(
        session->sm.cipher,
        session->sm.session_key,
        session->sm.session_key_len,
        iv,
        clear,
        padded,
        out);
    *out_len = padded;
    return DfcReaderOk;
}

// AES CMAC truncated to the odd-indexed octets, as every EV2-era MAC is.
static void wire_mac_8(const uint8_t key[16], const uint8_t* input, size_t len, uint8_t out[8]) {
    uint8_t full[DFC_AES_CMAC_LENGTH];
    aes_cmac((uint8_t*)key, DFC_AES_KEY_LENGTH, (uint8_t*)input, len, full);
    for(size_t i = 0; i < DFC_WIRE_MAC_LENGTH; i++) out[i] = full[i * 2 + 1];
}

// ------------------------------------------------------------ EV2 keys ---

#if DFC_ENABLE_EV2_SECURE_MESSAGING
DfcReaderStatus dfc_reader_change_key_ev2_command(
    const DfcReaderSession* session,
    uint8_t key_set_no,
    uint8_t key_no,
    const uint8_t new_key[DFC_AES_KEY_LENGTH],
    const uint8_t* current_key,
    uint8_t new_version,
    DfcCommand* command) {
    if(!session || !new_key || !command || session->auth_mode != DFC_READER_AUTH_EV2) {
        return DfcReaderInvalid;
    }
    bool same = (key_set_no & DFC_KEY_SET_NUMBER_MASK) == 0 && key_no == session->key_no;
    if(!same && !current_key) return DfcReaderInvalid;
    uint8_t clear[DFC_AES_KEY_LENGTH + 1 + 4];
    size_t len = 0;
    for(size_t i = 0; i < DFC_AES_KEY_LENGTH; i++) {
        clear[len++] = same ? new_key[i] : (uint8_t)(new_key[i] ^ current_key[i]);
    }
    clear[len++] = new_version;
    if(!same) {
        put_le32(clear + len, crc32_extend(0xFFFFFFFFu, new_key, DFC_AES_KEY_LENGTH));
        len += 4;
    }
    DfcCommandStatus st = dfc_command_change_key_ev2(command, key_set_no, key_no, clear, len);
    wipe(clear, sizeof(clear));
    return st == DfcCommandOk ? DfcReaderOk : DfcReaderInvalid;
}

// ----------------------------------------------------- delegated apps ---


DfcReaderStatus dfc_reader_create_delegated_application_begin(
    DfcReaderExchange* exchange,
    DfcReaderSession* session,
    DfcReaderFraming framing,
    const DfcCommandCreateDelegatedApplication* app,
    const uint8_t dam_encryption_key[DFC_AES_KEY_LENGTH],
    const uint8_t dam_mac_key[DFC_AES_KEY_LENGTH],
    const uint8_t random_prefix[DFC_DELEGATED_RANDOM_PREFIX_LENGTH],
    const uint8_t* initial_key,
    size_t initial_key_len,
    uint8_t initial_version) {
    if(!exchange || !session || !app || !dam_encryption_key || !dam_mac_key || !random_prefix ||
       !initial_key || initial_key_len == 0 ||
       initial_key_len > DFC_DELEGATED_DEFAULT_KEY_LENGTH ||
       session->auth_mode != DFC_READER_AUTH_EV2) {
        return DfcReaderInvalid;
    }
    DfcCommand header;
    if(dfc_command_create_delegated_application(&header, app) != DfcCommandOk) {
        return DfcReaderInvalid;
    }

    // The initial key travels encrypted under the DAM key, beside a MAC that
    // binds it to the header.
    uint8_t key_data[DFC_DELEGATED_ENCRYPTED_KEY_LENGTH] = {0};
    memcpy(key_data, random_prefix, DFC_DELEGATED_RANDOM_PREFIX_LENGTH);
    memcpy(key_data + DFC_DELEGATED_RANDOM_PREFIX_LENGTH, initial_key, initial_key_len);
    key_data[DFC_DELEGATED_ENCRYPTED_KEY_LENGTH - 1] = initial_version;
    uint8_t body[DFC_DELEGATED_CREATE_HEADER_LENGTH - 1 + DFC_DELEGATED_ENCRYPTED_KEY_LENGTH +
                 DFC_DELEGATED_MAC_LENGTH];
    size_t body_len = 0;
    memcpy(body, header.data, header.data_len);
    body_len += header.data_len;
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    dfc_worker_aes_cbc_encrypt(
        dam_encryption_key, DFC_AES_KEY_LENGTH, iv, sizeof(key_data), key_data, body + body_len);
    wipe(key_data, sizeof(key_data));
    body_len += DFC_DELEGATED_ENCRYPTED_KEY_LENGTH;

    uint8_t dam_input[1 + sizeof(body)];
    dam_input[0] = DFC_CMD_CREATE_DELEGATED_APPLICATION;
    memcpy(dam_input + 1, body, body_len);
    wire_mac_8(dam_mac_key, dam_input, 1 + body_len, body + body_len);
    body_len += DFC_DELEGATED_MAC_LENGTH;

    uint8_t mac[DFC_WIRE_MAC_LENGTH];
    if(!dfc_ev2_mac(
           session->ev2_mac_key,
           DFC_CMD_CREATE_DELEGATED_APPLICATION,
           session->ev2_command_counter,
           session->ev2_transaction_identifier,
           body,
           body_len,
           NULL,
           0,
           mac)) {
        return DfcReaderBufferTooSmall;
    }

    begin_common(exchange, session, framing);
    exchange->kind = KindCommand;
    exchange->ins = DFC_CMD_CREATE_DELEGATED_APPLICATION;
    exchange->response_plan = PlanEv2Mac;
    // The header goes alone, and the card asks for the rest.
    exchange->first_frame_len = header.data_len;
    if(!set_frame(exchange, body, body_len, mac, sizeof(mac))) return DfcReaderBufferTooSmall;
    return DfcReaderOk;
}
#endif

// ---------------------------------------------------- proximity check ---

bool dfc_reader_proximity_check_mac(
    const uint8_t key[DFC_AES_KEY_LENGTH],
    bool from_card,
    const uint8_t* published,
    size_t published_len,
    const uint8_t* transcript,
    size_t transcript_len,
    uint8_t mac[DFC_WIRE_MAC_LENGTH]) {
    if(!key || !mac || (!published && published_len) || (!transcript && transcript_len))
        return false;
    uint8_t input[1 + 4 + DFC_PROXIMITY_TRANSCRIPT_MAX];
    if(published_len > 4 || transcript_len > DFC_PROXIMITY_TRANSCRIPT_MAX) return false;
    size_t len = 0;
    input[len++] = from_card ? DFC_STATUS_SPECIAL_SUCCESS : DFC_CMD_VERIFY_PROXIMITY_CHECK;
    if(published_len) memcpy(input + len, published, published_len);
    len += published_len;
    if(transcript_len) memcpy(input + len, transcript, transcript_len);
    len += transcript_len;
    wire_mac_8(key, input, len, mac);
    return true;
}

// ------------------------------------------------------- virtual card ---

#define VC_FCI_TAG 0x6F
#define VC_FCI_LENGTH 0x22
#define VC_DATA_TAG 0x85
#define VC_DATA_LENGTH 0x20

DfcReaderStatus dfc_reader_virtual_card_select_apdu(
    const uint8_t* installation_id,
    size_t installation_id_len,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!installation_id || !out || !out_len || installation_id_len == 0 ||
       installation_id_len > DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH) {
        return DfcReaderInvalid;
    }
    size_t needed = 5 + installation_id_len + 1;
    if(needed > out_cap) return DfcReaderBufferTooSmall;
    out[0] = DFC_ISO7816_CLA_STANDARD;
    out[1] = DFC_ISO7816_INS_SELECT;
    out[2] = DFC_ISO7816_SELECT_BY_DF_NAME;
    out[3] = 0x00;
    out[4] = (uint8_t)installation_id_len;
    memcpy(out + 5, installation_id, installation_id_len);
    out[5 + installation_id_len] = 0x00;
    *out_len = needed;
    return DfcReaderOk;
}

DfcReaderStatus dfc_reader_virtual_card_open(
    const uint8_t select_encryption_key[DFC_AES_KEY_LENGTH],
    const uint8_t* response,
    size_t response_len,
    uint8_t challenge[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH],
    uint8_t clear_data[DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH]) {
    const size_t cryptogram_len = DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH;
    if(!select_encryption_key || !response || !challenge || !clear_data) return DfcReaderInvalid;
    if(response_len != 4 + cryptogram_len + 2) return DfcReaderProtocolError;
    if(response[response_len - 2] != DFC_ISO7816_SW_OK_HI ||
       response[response_len - 1] != DFC_ISO7816_SW_OK_LO) {
        return DfcReaderCardError;
    }
    if(response[0] != VC_FCI_TAG || response[1] != VC_FCI_LENGTH || response[2] != VC_DATA_TAG ||
       response[3] != VC_DATA_LENGTH) {
        return DfcReaderProtocolError;
    }
    uint8_t clear[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH];
    uint8_t iv[DFC_AES_KEY_LENGTH] = {0};
    dfc_worker_aes_cbc_decrypt(
        select_encryption_key, DFC_AES_KEY_LENGTH, iv, cryptogram_len, response + 4, clear);
    memcpy(challenge, clear, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memcpy(clear_data, clear + DFC_VIRTUAL_CARD_CHALLENGE_LENGTH, DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    wipe(clear, sizeof(clear));
    return DfcReaderOk;
}

DfcReaderStatus dfc_reader_virtual_card_authenticate_apdu(
    const uint8_t select_mac_key[DFC_AES_KEY_LENGTH],
    const uint8_t challenge[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH],
    const uint8_t clear_data[DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH],
    uint8_t* out,
    size_t out_cap,
    size_t* out_len) {
    if(!select_mac_key || !challenge || !clear_data || !out || !out_len) return DfcReaderInvalid;
    if(out_cap < 5 + DFC_WIRE_MAC_LENGTH) return DfcReaderBufferTooSmall;
    uint8_t input[DFC_VIRTUAL_CARD_CHALLENGE_LENGTH + DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH];
    memcpy(input, challenge, DFC_VIRTUAL_CARD_CHALLENGE_LENGTH);
    memcpy(input + DFC_VIRTUAL_CARD_CHALLENGE_LENGTH, clear_data, DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH);
    out[0] = DFC_ISO7816_CLA_STANDARD;
    out[1] = DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE;
    out[2] = 0x00;
    out[3] = 0x00;
    out[4] = DFC_WIRE_MAC_LENGTH;
    wire_mac_8(select_mac_key, input, sizeof(input), out + 5);
    *out_len = 5 + DFC_WIRE_MAC_LENGTH;
    return DfcReaderOk;
}

#endif // DFC_ENABLE_READER
