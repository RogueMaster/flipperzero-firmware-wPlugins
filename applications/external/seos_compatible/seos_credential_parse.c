#include "seos_credential_parse.h"

#define TAG "SeosCredentialParse"

/* A file may list its fields in any order, and the reader underneath searches
 * only forward from wherever it is: it never rewinds, and a miss leaves it at
 * the end of the file, after which nothing more can be found.
 *
 * So every read starts from the beginning. Reading two fields in an order the
 * file does not happen to use would otherwise step past the second and lose
 * it, and one absent field would consume every field after it. */
static bool read_field_hex(FlipperFormat* file, const char* key, uint8_t* data, size_t len) {
    flipper_format_rewind(file);
    return flipper_format_read_hex(file, key, data, (uint16_t)len);
}

static bool read_field_uint32(FlipperFormat* file, const char* key, uint32_t* value) {
    flipper_format_rewind(file);
    return flipper_format_read_uint32(file, key, value, 1);
}

bool seos_credential_parse_seos(FlipperFormat* file, SeosCredential* credential) {
    FuriString* header = furi_string_alloc();
    bool parsed = false;

    memset(credential->diversifier, 0, sizeof(credential->diversifier));
    memset(credential->sio, 0, sizeof(credential->sio));
    memset(credential->priv_key, 0, sizeof(credential->priv_key));
    memset(credential->auth_key, 0, sizeof(credential->auth_key));
    memset(credential->adf_response, 0, sizeof(credential->adf_response));
    credential->adf_oid_len = 0;

    do {
        uint32_t version = 0;
        flipper_format_rewind(file);
        if(!flipper_format_read_header(file, header, &version)) break;
        if(furi_string_cmp_str(header, seos_file_header) || (version != seos_file_version)) break;

        /* Each stated length decides how much is read into a fixed-size
         * field, so check it against the field first. */
        uint32_t diversifier_len = 0;
        if(!read_field_uint32(file, "Diversifier Length", &diversifier_len)) break;
        if(diversifier_len > sizeof(credential->diversifier)) {
            FURI_LOG_W(TAG, "Diversifier of %lu will not fit", diversifier_len);
            break;
        }
        credential->diversifier_len = diversifier_len;
        if(!read_field_hex(
               file, "Diversifier", credential->diversifier, credential->diversifier_len))
            break;

        uint32_t sio_len = 0;
        if(!read_field_uint32(file, "SIO Length", &sio_len)) break;
        if(sio_len > sizeof(credential->sio)) {
            FURI_LOG_W(TAG, "Credential of %lu will not fit", sio_len);
            break;
        }
        credential->sio_len = sio_len;
        if(!read_field_hex(file, "SIO", credential->sio, credential->sio_len)) break;

        /* Keys and the application identifier are optional. */
        read_field_hex(file, "Priv Key", credential->priv_key, sizeof(credential->priv_key));
        read_field_hex(file, "Auth Key", credential->auth_key, sizeof(credential->auth_key));
        read_field_hex(
            file, "ADF Response", credential->adf_response, sizeof(credential->adf_response));

        uint32_t adf_oid_len = 0;
        if(read_field_uint32(file, "ADF OID Length", &adf_oid_len)) {
            if(adf_oid_len > sizeof(credential->adf_oid)) {
                FURI_LOG_W(TAG, "Application identifier of %lu will not fit", adf_oid_len);
                break;
            }
            credential->adf_oid_len = adf_oid_len;
            read_field_hex(file, "ADF OID", credential->adf_oid, credential->adf_oid_len);
        }

        parsed = true;
    } while(false);

    furi_string_free(header);
    return parsed;
}

bool seos_credential_parse_seader(FlipperFormat* file, SeosCredential* credential) {
    const char* seader_file_header = "Flipper Seader Credential";
    const uint32_t seader_file_version = 1;

    FuriString* header = furi_string_alloc();
    bool parsed = false;

    memset(credential->diversifier, 0, sizeof(credential->diversifier));
    memset(credential->sio, 0, sizeof(credential->sio));

    do {
        uint32_t version = 0;
        flipper_format_rewind(file);
        if(!flipper_format_read_header(file, header, &version)) break;
        if(furi_string_cmp_str(header, seader_file_header) || (version != seader_file_version))
            break;

        /* This format states no lengths. Fields are read at their maximum
         * size and the real length is taken from the contents. A short read
         * leaves the remainder zero, so its return value is not conclusive. */
        credential->sio_len = 64;
        if(!read_field_hex(file, "SIO", credential->sio, credential->sio_len)) {
            /* A short read is still usable; an absent one is not. */
            if(credential->sio[0] == 0 && credential->sio[1] == 0) break;
        }
        /* Two bytes of tag and length before, two after. */
        credential->sio_len = credential->sio[1] + 4;
        if(credential->sio_len > sizeof(credential->sio)) {
            FURI_LOG_W(TAG, "Credential of %zu will not fit", credential->sio_len);
            break;
        }

        credential->diversifier_len = 8;
        if(!read_field_hex(
               file, "Diversifier", credential->diversifier, credential->diversifier_len)) {
            break;
        }
        uint8_t* end = memchr(credential->diversifier, 0, credential->diversifier_len);
        if(end) {
            credential->diversifier_len = end - credential->diversifier;
        }

        parsed = true;
    } while(false);

    furi_string_free(header);
    return parsed;
}
