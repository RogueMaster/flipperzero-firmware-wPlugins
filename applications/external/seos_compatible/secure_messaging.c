#include "secure_messaging.h"

#include "seos_tlv.h"

/* The hash calls were renamed between mbedTLS releases: 2.x deprecated the
 * plain names in favour of an _ret suffix, and 3.x dropped the suffix again.
 * Only 3.x carries the version macro into the hash headers, so its absence
 * marks the older spelling. */
#if !defined(MBEDTLS_VERSION_MAJOR) || MBEDTLS_VERSION_MAJOR < 3
#define seos_sha1_starts(ctx)          mbedtls_sha1_starts_ret(ctx)
#define seos_sha1_update(ctx, b, n)    mbedtls_sha1_update_ret(ctx, b, n)
#define seos_sha1_finish(ctx, out)     mbedtls_sha1_finish_ret(ctx, out)
#define seos_sha256_starts(ctx, is224) mbedtls_sha256_starts_ret(ctx, is224)
#define seos_sha256_update(ctx, b, n)  mbedtls_sha256_update_ret(ctx, b, n)
#define seos_sha256_finish(ctx, out)   mbedtls_sha256_finish_ret(ctx, out)
#else
#define seos_sha1_starts(ctx)          mbedtls_sha1_starts(ctx)
#define seos_sha1_update(ctx, b, n)    mbedtls_sha1_update(ctx, b, n)
#define seos_sha1_finish(ctx, out)     mbedtls_sha1_finish(ctx, out)
#define seos_sha256_starts(ctx, is224) mbedtls_sha256_starts(ctx, is224)
#define seos_sha256_update(ctx, b, n)  mbedtls_sha256_update(ctx, b, n)
#define seos_sha256_finish(ctx, out)   mbedtls_sha256_finish(ctx, out)
#endif

#define TAG "SecureMessaging"

static uint8_t padding[16] =
    {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* Secure messaging data object tags. */
#define DO_CRYPTOGRAM 0x85
#define DO_CHECKSUM   0x8e
#define DO_LE         0x97
#define DO_STATUS     0x99

/* Computes the checksum over the sequence counter, the command header if there
 * is one, and the protected data objects, each group padded to a block
 * boundary. Both directions use the same scope, so both use this. */
static bool checksum_objects(
    SecureMessaging* secure_messaging,
    const uint8_t* header,
    size_t header_len,
    const uint8_t* objects,
    size_t objects_len,
    uint8_t* cmac) {
    uint8_t cipher = secure_messaging->cipher;
    size_t block_size = seos_cipher_block_size(cipher);
    uint8_t* context = cipher == AES_128_CBC ? secure_messaging->aesContext :
                                               secure_messaging->desContext;

    /* The counter, then the header and the objects, each group padded to a
     * block boundary. Sized from what is actually being checksummed rather
     * than from a ceiling every caller would pay for. */
    size_t input_cap = block_size + header_len + objects_len + 2 * block_size;
    uint8_t* input = malloc(input_cap);
    if(!input) {
        FURI_LOG_W(TAG, "No room for %zu bytes of checksum input", input_cap);
        return false;
    }
    size_t input_len = 0;

    memcpy(input, context, block_size);
    input_len += block_size;

    if(header_len > 0) {
        memcpy(input + input_len, header, header_len);
        input_len += header_len;
        size_t remainder = input_len % block_size;
        memcpy(input + input_len, padding, block_size - remainder);
        input_len += block_size - remainder;
    }

    memcpy(input + input_len, objects, objects_len);
    input_len += objects_len;
    size_t remainder = input_len % block_size;
    memcpy(input + input_len, padding, block_size - remainder);
    input_len += block_size - remainder;

    bool ok = seos_cipher_cmac(
        cipher,
        secure_messaging->CMACKey,
        sizeof(secure_messaging->CMACKey),
        input,
        input_len,
        cmac);

    free(input);
    return ok;
}

/* Walks the data objects from `body_offset` to the checksum object.
 *
 * The checksum covers everything ahead of it, so its position also gives the
 * length of the protected span. */
static bool find_checksum(
    const uint8_t* data,
    size_t data_len,
    size_t body_offset,
    size_t* objects_len,
    size_t* checksum_offset) {
    SeosTlvCursor cursor;
    seos_tlv_cursor_init(&cursor, data, data_len);
    cursor.offset = body_offset;

    while(!seos_tlv_cursor_done(&cursor)) {
        SeosTlvObject object;
        if(!seos_tlv_read(&cursor, &object)) return false;
        if(object.tag == DO_CHECKSUM) {
            if(object.value_len != SEOS_WORKER_CMAC_SIZE) return false;
            *objects_len = object.header_offset - body_offset;
            *checksum_offset = object.value_offset;
            return true;
        }
    }
    return false;
}

/* Recomputes the checksum of a received message and compares it.
 *
 * Records the status word the caller should answer with: a message with no
 * checksum is missing an object, one that fails to match is incorrect. */
static bool verify_checksum(
    SecureMessaging* secure_messaging,
    const uint8_t* data,
    size_t data_len,
    size_t body_offset,
    const uint8_t* header,
    size_t header_len,
    size_t* objects_len) {
    size_t checksum_offset = 0;
    if(!find_checksum(data, data_len, body_offset, objects_len, &checksum_offset)) {
        FURI_LOG_W(TAG, "No checksum in secure message");
        secure_messaging->last_error_sw = SECURE_MESSAGING_SW_MISSING_DO;
        return false;
    }

    uint8_t expected[16];
    if(!checksum_objects(
           secure_messaging, header, header_len, data + body_offset, *objects_len, expected)) {
        secure_messaging->last_error_sw = SECURE_MESSAGING_SW_INCORRECT_DO;
        return false;
    }

    if(memcmp(expected, data + checksum_offset, SEOS_WORKER_CMAC_SIZE) != 0) {
        FURI_LOG_W(TAG, "Checksum mismatch");
        secure_messaging->last_error_sw = SECURE_MESSAGING_SW_INCORRECT_DO;
        return false;
    }
    return true;
}

/* Copies a message into a padded plaintext block, returning the padded length.
 * Returns 0 if the message does not leave room for the mandatory pad byte. */
/* Padded length of a message: at least one pad byte, rounded up to a block. */
static size_t padded_size(size_t message_len, size_t block_size) {
    return ((message_len / block_size) + 1) * block_size;
}

static size_t pad_message(
    const uint8_t* message,
    size_t message_len,
    size_t block_size,
    uint8_t* clear,
    size_t clear_cap) {
    /* The pad byte is mandatory, so a message filling the buffer exactly is
     * still too long. */
    if(message_len >= clear_cap) {
        return 0;
    }

    size_t clear_len = padded_size(message_len, block_size);
    if(clear_len > clear_cap) {
        return 0;
    }

    memset(clear, 0, clear_cap);
    memcpy(clear, message, message_len);
    clear[message_len] = 0x80;
    return clear_len;
}

/* Recovers the plaintext from the cryptogram at `offset`.
 *
 * Returns false if the message is not well formed. Every length here comes off
 * the wire, so each is checked against the buffer before it is used. The
 * length is reported separately because an empty plaintext is a valid result.
 */
static bool unwrap_cryptogram(
    SecureMessaging* secure_messaging,
    const uint8_t* data,
    size_t data_len,
    size_t offset,
    uint8_t* clear,
    size_t clear_cap,
    size_t* clear_len_out) {
    SeosTlvObject cryptogram;
    if(!seos_tlv_read_at(data, data_len, offset, &cryptogram) || cryptogram.tag != DO_CRYPTOGRAM) {
        FURI_LOG_W(TAG, "No cryptogram to unwrap");
        return false;
    }
    size_t value_offset = cryptogram.value_offset;
    size_t value_len = cryptogram.value_len;

    size_t block_size = seos_cipher_block_size(secure_messaging->cipher);
    if(value_len == 0 || value_len > clear_cap || (value_len % block_size) != 0) {
        FURI_LOG_W(TAG, "Invalid cryptogram length (%d)", value_len);
        return false;
    }

    const uint8_t* encrypted = data + value_offset;
    memset(clear, 0, clear_cap);

    if(!seos_cipher_decrypt(
           secure_messaging->cipher, secure_messaging->PrivacyKey, value_len, encrypted, clear)) {
        return false;
    }

    /* Strip the pad: trailing zeroes, then the one mandatory 0x80. Its absence
     * means the padding is wrong, which is a decryption failure in disguise. */
    size_t clear_len = value_len;
    while(clear_len > 0 && clear[clear_len - 1] == 0x00) {
        clear_len--;
    }
    if(clear_len == 0 || clear[clear_len - 1] != 0x80) {
        FURI_LOG_W(TAG, "Bad cryptogram padding");
        return false;
    }

    *clear_len_out = clear_len - 1;
    return true;
}

SecureMessaging* secure_messaging_alloc(AuthParameters* params) {
    SecureMessaging* secure_messaging = malloc(sizeof(SecureMessaging));
    if(!secure_messaging) return NULL;
    memset(secure_messaging, 0, sizeof(SecureMessaging));

    secure_messaging->cipher = params->cipher;
    if(params->cipher == AES_128_CBC) {
        memcpy(secure_messaging->aesContext, params->rndICC, 8);
        memcpy(secure_messaging->aesContext + 8, params->UID, 8);
    } else if(params->cipher == TWO_KEY_3DES_CBC_MODE) {
        memcpy(secure_messaging->desContext, params->rndICC, 4);
        memcpy(secure_messaging->desContext + 4, params->UID, 4);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched (%d)", params->cipher);
        free(secure_messaging);
        return NULL;
    }

    size_t index = 0;
    uint8_t buffer[38];
    memset(buffer, 0, sizeof(buffer));
    index += 4; // skip 4 bytes where iteration will be put
    memcpy(buffer + index, params->cNonce, 8);
    index += 8;
    memcpy(buffer + index, params->rNonce, 8);
    index += 8;
    buffer[index++] = params->cipher;
    buffer[index++] = params->cipher;
    memcpy(buffer + index, params->rndICC, 8);
    index += 8;
    memcpy(buffer + index, params->UID, 8);
    index += 8;

    size_t iterations = 1;
    size_t unit = 0;
    if(params->hash == SHA1) {
        unit = 160 / 8;
    } else if(params->hash == SHA256) {
        unit = 256 / 8;
    } else {
        /* A hash we do not know leaves the step at zero, which would spin the
         * derivation loop forever. */
        FURI_LOG_W(TAG, "Unknown hash algorithm (%d)", params->hash);
        free(secure_messaging);
        return NULL;
    }
    // FURI_LOG_D(TAG, "secure_messaging_alloc hash %d unit %d", hash, unit);

    // More than enough space for the hash
    uint8_t accumulator[64];
    memset(accumulator, 0, sizeof(accumulator));
    for(size_t i = 0; i < 32; i += unit) {
        buffer[3] = iterations++;
        if(params->hash == SHA1) {
            mbedtls_sha1_context ctx;
            mbedtls_sha1_init(&ctx);
            seos_sha1_starts(&ctx);
            seos_sha1_update(&ctx, buffer, index);
            seos_sha1_finish(&ctx, accumulator + i);
            mbedtls_sha1_free(&ctx);
        } else if(params->hash == SHA256) {
            mbedtls_sha256_context ctx;
            mbedtls_sha256_init(&ctx);
            seos_sha256_starts(&ctx, 0);
            seos_sha256_update(&ctx, buffer, index);
            seos_sha256_finish(&ctx, accumulator + i);
            mbedtls_sha256_free(&ctx);
        } else {
            FURI_LOG_W(TAG, "Could not match hash algorithm");
        }
    }

    memcpy(secure_messaging->PrivacyKey, accumulator, 16);
    memcpy(secure_messaging->CMACKey, accumulator + 16, 16);

    return secure_messaging;
}

void secure_messaging_free(SecureMessaging* secure_messaging) {
    furi_assert(secure_messaging);
    secure_messaging_clear_pending(secure_messaging);

    /* The session keys and both cipher contexts live in here. Cleared before
     * the memory goes back, so they are not left for whatever allocates
     * next. */
    memset(secure_messaging, 0, sizeof(SecureMessaging));
    free(secure_messaging);
}

void secure_messaging_clear_pending(SecureMessaging* secure_messaging) {
    free(secure_messaging->pending);
    secure_messaging->pending = NULL;
    secure_messaging->pending_len = 0;
    secure_messaging->pending_offset = 0;
}

bool secure_messaging_set_pending(
    SecureMessaging* secure_messaging,
    const uint8_t* data,
    size_t len) {
    secure_messaging_clear_pending(secure_messaging);
    if(len == 0) return true;

    secure_messaging->pending = malloc(len);
    if(!secure_messaging->pending) return false;

    memcpy(secure_messaging->pending, data, len);
    secure_messaging->pending_len = len;
    return true;
}

size_t secure_messaging_take_pending(
    SecureMessaging* secure_messaging,
    uint8_t* out,
    size_t max_len,
    size_t* remaining) {
    size_t left = secure_messaging->pending_len - secure_messaging->pending_offset;
    size_t taken = left < max_len ? left : max_len;

    memcpy(out, secure_messaging->pending + secure_messaging->pending_offset, taken);
    secure_messaging->pending_offset += taken;

    *remaining = secure_messaging->pending_len - secure_messaging->pending_offset;
    if(*remaining == 0) {
        secure_messaging_clear_pending(secure_messaging);
    }
    return taken;
}

void secure_messaging_increment_context(SecureMessaging* secure_messaging) {
    uint8_t* context = NULL;
    size_t context_len = 0;
    if(secure_messaging->cipher == AES_128_CBC) {
        context = secure_messaging->aesContext;
        context_len = sizeof(secure_messaging->aesContext);
    } else if(secure_messaging->cipher == TWO_KEY_3DES_CBC_MODE) {
        context = secure_messaging->desContext;
        context_len = sizeof(secure_messaging->desContext);
    } else {
        FURI_LOG_W(TAG, "Cipher not matched");
        return;
    }
    do {
    } while(++context[--context_len] == 0 && context_len > 0);
}

bool secure_messaging_wrap_apdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    uint8_t* apdu_header,
    size_t apdu_header_len,
    bool expects_response,
    BitBuffer* tx_buffer) {
    uint8_t cipher = secure_messaging->cipher;
    size_t block_size = seos_cipher_block_size(cipher);
    if(block_size == 0) return false;

    uint8_t protected_le[] = {DO_LE, 0x00};
    size_t protected_le_len = expects_response ? sizeof(protected_le) : 0;
    uint8_t checksum_prefix[] = {DO_CHECKSUM, SEOS_WORKER_CMAC_SIZE};
    uint8_t Le[] = {0x00};

    size_t clear_cap = padded_size(message_len, block_size);
    size_t objects_cap = SEOS_TLV_HEADER_MAX + clear_cap + protected_le_len;

    /* The command states its own length in one byte, so the objects and the
     * checksum after them have to fit in that. A message past this needs the
     * transport to carry it in pieces. */
    if(objects_cap + sizeof(checksum_prefix) + SEOS_WORKER_CMAC_SIZE > 0xff) {
        FURI_LOG_W(TAG, "Message of %zu will not fit one command", message_len);
        return false;
    }

    uint8_t* clear = malloc(clear_cap);
    uint8_t* encrypted = malloc(clear_cap);
    uint8_t* objects = malloc(objects_cap);
    bool ok = false;

    do {
        if(!clear || !encrypted || !objects) {
            FURI_LOG_W(TAG, "No room to wrap %zu bytes", message_len);
            break;
        }

        size_t clear_len = pad_message(message, message_len, block_size, clear, clear_cap);
        if(clear_len == 0) break;

        secure_messaging_increment_context(secure_messaging);

        if(!seos_cipher_encrypt(cipher, secure_messaging->PrivacyKey, clear_len, clear, encrypted))
            break;

        /* Assemble the protected objects once. They are checksummed and then
         * sent as they stand, so the two cannot disagree. */
        size_t objects_len = seos_tlv_write_header(objects, DO_CRYPTOGRAM, clear_len);
        memcpy(objects + objects_len, encrypted, clear_len);
        objects_len += clear_len;
        if(protected_le_len > 0) {
            memcpy(objects + objects_len, protected_le, protected_le_len);
            objects_len += protected_le_len;
        }

        uint8_t cmac[16];
        if(!checksum_objects(
               secure_messaging, apdu_header, apdu_header_len, objects, objects_len, cmac))
            break;

        uint8_t apdu_len[] = {
            (uint8_t)(objects_len + sizeof(checksum_prefix) + SEOS_WORKER_CMAC_SIZE)};

        bit_buffer_reset(tx_buffer);
        bit_buffer_append_bytes(tx_buffer, apdu_header, apdu_header_len);
        bit_buffer_append_bytes(tx_buffer, apdu_len, sizeof(apdu_len));
        bit_buffer_append_bytes(tx_buffer, objects, objects_len);
        bit_buffer_append_bytes(tx_buffer, checksum_prefix, sizeof(checksum_prefix));
        bit_buffer_append_bytes(tx_buffer, cmac, SEOS_WORKER_CMAC_SIZE);
        bit_buffer_append_bytes(tx_buffer, Le, sizeof(Le));
        ok = true;
    } while(false);

    free(clear);
    free(encrypted);
    free(objects);
    return ok;
}

bool secure_messaging_unwrap_rapdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer) {
    secure_messaging_increment_context(secure_messaging);
    secure_messaging->last_error_sw = 0;

    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    size_t data_len = bit_buffer_get_size_bytes(rx_buffer);

    size_t objects_len = 0;
    if(!verify_checksum(
           secure_messaging,
           data,
           data_len,
           SECURE_MESSAGING_RAPDU_BODY_OFFSET,
           NULL,
           0,
           &objects_len)) {
        return false;
    }

    /* Record the protected status word, and note that a response with no data
     * carries no cryptogram at all. */
    secure_messaging->last_response_sw = 0;
    uint8_t first_tag = data[SECURE_MESSAGING_RAPDU_BODY_OFFSET];
    if(first_tag == DO_STATUS) {
        if(data_len < SECURE_MESSAGING_RAPDU_BODY_OFFSET + 4) {
            secure_messaging->last_error_sw = SECURE_MESSAGING_SW_INCORRECT_DO;
            return false;
        }
        secure_messaging->last_response_sw =
            (uint16_t)((data[SECURE_MESSAGING_RAPDU_BODY_OFFSET + 2] << 8) |
                       data[SECURE_MESSAGING_RAPDU_BODY_OFFSET + 3]);
        bit_buffer_reset(rx_buffer);
        return true;
    }

    /* The plaintext is never longer than the message it came out of. */
    uint8_t* clear = malloc(data_len);
    if(!clear) return false;

    size_t clear_len = 0;
    if(!unwrap_cryptogram(
           secure_messaging,
           data,
           data_len,
           SECURE_MESSAGING_RAPDU_BODY_OFFSET,
           clear,
           data_len,
           &clear_len)) {
        secure_messaging->last_error_sw = SECURE_MESSAGING_SW_INCORRECT_DO;
        free(clear);
        return false;
    }

    /* The status object follows the cryptogram; objects_len spans both. */
    if(objects_len >= 4) {
        size_t status_offset = SECURE_MESSAGING_RAPDU_BODY_OFFSET + objects_len - 4;
        if(data[status_offset] == DO_STATUS) {
            secure_messaging->last_response_sw =
                (uint16_t)((data[status_offset + 2] << 8) | data[status_offset + 3]);
        }
    }

    bit_buffer_reset(rx_buffer);
    bit_buffer_append_bytes(rx_buffer, clear, clear_len);
    free(clear);
    return true;
}

// Assumes it is an iso14443a-4 and doesn't have framing bytes
/*
0ccb3fff
16
  8508
    4088b37ca72bc7ae
  9700
  8e08
    85345f0f5c44b980
00
*/
bool secure_messaging_unwrap_apdu(SecureMessaging* secure_messaging, BitBuffer* rx_buffer) {
    secure_messaging_increment_context(secure_messaging);
    secure_messaging->last_error_sw = 0;

    const uint8_t* data = bit_buffer_get_data(rx_buffer);
    size_t data_len = bit_buffer_get_size_bytes(rx_buffer);

    /* The command header is the four bytes ahead of the length byte, and is
     * covered by the checksum. */
    if(data_len < SECURE_MESSAGING_CAPDU_BODY_OFFSET) {
        FURI_LOG_W(TAG, "Command too short to unwrap");
        secure_messaging->last_error_sw = SECURE_MESSAGING_SW_MISSING_DO;
        return false;
    }

    size_t objects_len = 0;
    if(!verify_checksum(
           secure_messaging,
           data,
           data_len,
           SECURE_MESSAGING_CAPDU_BODY_OFFSET,
           data,
           SECURE_MESSAGING_APDU_HEADER_LEN,
           &objects_len)) {
        return false;
    }

    /* The plaintext is never longer than the message it came out of. */
    uint8_t* clear = malloc(data_len);
    if(!clear) return false;

    size_t clear_len = 0;
    if(!unwrap_cryptogram(
           secure_messaging,
           data,
           data_len,
           SECURE_MESSAGING_CAPDU_BODY_OFFSET,
           clear,
           data_len,
           &clear_len)) {
        secure_messaging->last_error_sw = SECURE_MESSAGING_SW_INCORRECT_DO;
        free(clear);
        return false;
    }

    bit_buffer_reset(rx_buffer);
    bit_buffer_append_bytes(rx_buffer, clear, clear_len);
    free(clear);
    return true;
}

bool secure_messaging_wrap_rapdu(
    SecureMessaging* secure_messaging,
    uint8_t* message,
    size_t message_len,
    uint16_t status_word,
    BitBuffer* tx_buffer) {
    size_t block_size = seos_cipher_block_size(secure_messaging->cipher);
    if(block_size == 0) return false;

    uint8_t protected_status[] = {
        DO_STATUS, 0x02, (uint8_t)(status_word >> 8), (uint8_t)(status_word & 0xff)};

    /* A response with no data carries no cryptogram, only its status. */
    size_t clear_cap = message_len > 0 ? padded_size(message_len, block_size) : 0;
    size_t objects_cap = SEOS_TLV_HEADER_MAX + clear_cap + sizeof(protected_status);

    uint8_t* objects = malloc(objects_cap);
    uint8_t* clear = clear_cap > 0 ? malloc(clear_cap) : NULL;
    uint8_t* encrypted = clear_cap > 0 ? malloc(clear_cap) : NULL;
    size_t objects_len = 0;
    bool ok = false;

    do {
        if(!objects || (clear_cap > 0 && (!clear || !encrypted))) {
            FURI_LOG_W(TAG, "No room to wrap %zu bytes", message_len);
            break;
        }

        if(message_len > 0) {
            size_t clear_len = pad_message(message, message_len, block_size, clear, clear_cap);
            if(clear_len == 0) {
                FURI_LOG_W(TAG, "Message too long to wrap (%d)", message_len);
                break;
            }

            secure_messaging_increment_context(secure_messaging);

            if(!seos_cipher_encrypt(
                   secure_messaging->cipher,
                   secure_messaging->PrivacyKey,
                   clear_len,
                   clear,
                   encrypted)) {
                break;
            }

            objects_len = seos_tlv_write_header(objects, DO_CRYPTOGRAM, clear_len);
            memcpy(objects + objects_len, encrypted, clear_len);
            objects_len += clear_len;
        } else {
            secure_messaging_increment_context(secure_messaging);
        }

        memcpy(objects + objects_len, protected_status, sizeof(protected_status));
        objects_len += sizeof(protected_status);
        ok = true;
    } while(false);

    free(clear);
    free(encrypted);
    if(!ok) {
        free(objects);
        return false;
    }

    uint8_t cmac[16];
    if(!checksum_objects(secure_messaging, NULL, 0, objects, objects_len, cmac)) {
        free(objects);
        return false;
    }

    uint8_t checksum_prefix[] = {DO_CHECKSUM, SEOS_WORKER_CMAC_SIZE};
    bit_buffer_append_bytes(tx_buffer, objects, objects_len);
    bit_buffer_append_bytes(tx_buffer, checksum_prefix, sizeof(checksum_prefix));
    bit_buffer_append_bytes(tx_buffer, cmac, SEOS_WORKER_CMAC_SIZE);
    // The same status word is appended in the clear by the caller
    free(objects);
    return true;
}
