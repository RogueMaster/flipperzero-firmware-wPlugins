#include "payto.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool payto_prefix_equal(const char* value, const char* prefix) {
    while(*prefix) {
        if(tolower((unsigned char)*value++) != tolower((unsigned char)*prefix++)) return false;
    }
    return true;
}

static bool payto_address_char(char c) {
    return isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_' || c == '~';
}

static bool payto_hex_address(const char* address, size_t length) {
    if(strlen(address) != length) return false;
    for(size_t i = 0; i < length; i++) {
        if(!isxdigit((unsigned char)address[i])) return false;
    }
    return true;
}

static bool payto_base58_char(char c) {
    return strchr("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", c) != NULL;
}

static bool payto_base58_length(const char* address, size_t minimum, size_t maximum) {
    size_t length = strlen(address);
    if(length < minimum || length > maximum) return false;
    for(size_t i = 0; i < length; i++) {
        if(!payto_base58_char(address[i])) return false;
    }
    return true;
}

static bool payto_ican_checksum(const char* currency, const char* address) {
    if(strlen(address) != 44 || !payto_hex_address(address + 2, 42)) return false;
    const char* expected_prefix = NULL;
    if(strcasecmp(currency, "xcb") == 0)
        expected_prefix = "cb";
    else if(strcasecmp(currency, "xab") == 0)
        expected_prefix = "ab";
    else if(strcasecmp(currency, "xce") == 0)
        expected_prefix = "ce";
    else if(strcasecmp(currency, "ican") == 0)
        expected_prefix = NULL;
    else
        return false;
    if(expected_prefix && strncasecmp(address, expected_prefix, 2) != 0) return false;
    if(strncasecmp(address, "cb", 2) != 0 && strncasecmp(address, "ab", 2) != 0 &&
       strncasecmp(address, "ce", 2) != 0)
        return false;

    /* ICAN uses the IBAN MOD-97 algorithm on address[4:] + address[0:4]. */
    char rearranged[45];
    memcpy(rearranged, address + 4, 40);
    memcpy(rearranged + 40, address, 4);
    rearranged[44] = '\0';
    char expanded[96];
    size_t expanded_length = 0;
    for(size_t i = 0; i < 44; i++) {
        char c = (char)toupper((unsigned char)rearranged[i]);
        if(c >= 'A' && c <= 'Z') {
            int value = c - 'A' + 10;
            expanded[expanded_length++] = (char)('0' + value / 10);
            expanded[expanded_length++] = (char)('0' + value % 10);
        } else {
            expanded[expanded_length++] = c;
        }
    }
    int remainder = 0;
    for(size_t i = 0; i < expanded_length; i++) {
        if(!isdigit((unsigned char)expanded[i])) return false;
        remainder = (remainder * 10 + expanded[i] - '0') % 97;
    }
    return remainder == 1;
}

static bool payto_known_address_format(const char* currency, const char* address) {
    if(payto_ican_checksum(currency, address)) return true;
    if(strcasecmp(currency, "eth") == 0 || strcasecmp(currency, "evm") == 0 ||
       strcasecmp(currency, "base") == 0 || strcasecmp(currency, "pol") == 0) {
        return strncasecmp(address, "0x", 2) == 0 && payto_hex_address(address + 2, 40);
    }
    if(strcasecmp(currency, "algo") == 0) {
        if(strlen(address) != 58) return false;
        for(size_t i = 0; i < 58; i++) {
            if(strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", toupper((unsigned char)address[i])) ==
               NULL)
                return false;
        }
        return true;
    }
    if(strcasecmp(currency, "xlm") == 0) {
        if(strlen(address) != 56 || address[0] != 'G') return false;
        for(size_t i = 1; i < 56; i++) {
            if(strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ234567", toupper((unsigned char)address[i])) ==
               NULL)
                return false;
        }
        return true;
    }
    if(strcasecmp(currency, "btc") == 0 || strcasecmp(currency, "ltc") == 0 ||
       strcasecmp(currency, "sol") == 0 || strcasecmp(currency, "dot") == 0 ||
       strcasecmp(currency, "xrp") == 0 || strcasecmp(currency, "trx") == 0 ||
       strcasecmp(currency, "tron") == 0) {
        return payto_base58_length(address, 26, 90);
    }
    if(strcasecmp(currency, "ada") == 0) {
        return (strncasecmp(address, "addr1", 5) == 0 || strncasecmp(address, "stake1", 6) == 0 ||
                strncasecmp(address, "addr_test1", 10) == 0 ||
                strncasecmp(address, "stake_test1", 11) == 0) &&
               strlen(address) >= 50 && strlen(address) <= 120;
    }
    if(strcasecmp(currency, "atom") == 0) {
        return (strncasecmp(address, "cosmos1", 7) == 0 || strncasecmp(address, "osmo1", 5) == 0 ||
                strncasecmp(address, "axelar1", 7) == 0 || strncasecmp(address, "juno1", 5) == 0 ||
                strncasecmp(address, "stars1", 6) == 0) &&
               strlen(address) >= 44 && strlen(address) <= 46;
    }
    if(strcasecmp(currency, "bch") == 0) {
        return (
            strncasecmp(address, "bitcoincash:", 13) == 0 ?
                strlen(address) == 55 :
                (strlen(address) == 42 && (address[0] == 'q' || address[0] == 'p')));
    }
    return false;
}

bool payto_parse_uri(const char* uri, PaytoWallet* wallet) {
    if(!uri || !wallet || !payto_prefix_equal(uri, "payto://")) return false;
    const char* currency = uri + 8;
    const char* slash = strchr(currency, '/');
    if(!slash || slash == currency || (size_t)(slash - currency) >= sizeof(wallet->currency))
        return false;
    const char* address = slash + 1;
    if(!*address || strlen(uri) >= PAYTO_MAX_URI_LEN || strlen(address) >= sizeof(wallet->address))
        return false;
    char currency_name[sizeof(wallet->currency)] = {0};
    memcpy(currency_name, currency, slash - currency);
    for(const char* p = currency; p < slash; p++) {
        if(!isalpha((unsigned char)*p)) return false;
    }
    for(const char* p = address; *p; p++)
        if(!payto_address_char(*p)) return false;
    if(!payto_known_address_format(currency_name, address)) return false;
    memset(wallet, 0, sizeof(*wallet));
    snprintf(wallet->uri, sizeof(wallet->uri), "%s", uri);
    for(size_t i = 0; i < (size_t)(slash - currency); i++) {
        wallet->currency[i] = (char)toupper((unsigned char)currency[i]);
    }
    snprintf(wallet->address, sizeof(wallet->address), "%s", address);
    payto_format_name(wallet, wallet->short_name, sizeof(wallet->short_name));
    return true;
}

bool payto_parse_ndef(const unsigned char* data, size_t length, PaytoWallet* wallet) {
    size_t offset = 0;
    while(offset + 3 <= length) {
        const unsigned char header = data[offset++];
        const unsigned char type_length = data[offset++];
        const bool short_record = (header & 0x10) != 0;
        size_t payload_length;
        if(short_record) {
            if(offset >= length) return false;
            payload_length = data[offset++];
        } else {
            if(offset + 4 > length) return false;
            payload_length = (size_t)data[offset] << 24 | (size_t)data[offset + 1] << 16 |
                             (size_t)data[offset + 2] << 8 | data[offset + 3];
            offset += 4;
        }
        if(offset + type_length + payload_length > length) return false;
        const unsigned char* type = &data[offset];
        offset += type_length;
        const unsigned char* payload = &data[offset];
        offset += payload_length;
        if((header & 0x07) == 0x01 && type_length == 1 && type[0] == 'U' && payload_length > 1 &&
           payload[0] == 0) {
            char uri[PAYTO_MAX_URI_LEN];
            size_t uri_length = payload_length - 1;
            if(uri_length >= sizeof(uri)) uri_length = sizeof(uri) - 1;
            memcpy(uri, payload + 1, uri_length);
            uri[uri_length] = '\0';
            if(payto_parse_uri(uri, wallet)) return true;
        }
        if(header & 0x40) break;
    }
    return false;
}

void payto_format_name(const PaytoWallet* wallet, char* output, size_t output_size) {
    if(!wallet || !output || output_size == 0) return;
    size_t length = strlen(wallet->address);
    if(length >= 8) {
        snprintf(
            output,
            output_size,
            "%s: %.4s-%.4s",
            wallet->currency,
            wallet->address,
            wallet->address + length - 4);
    } else {
        snprintf(output, output_size, "%s: %s", wallet->currency, wallet->address);
    }
    for(char* p = output; *p; p++)
        *p = (char)toupper((unsigned char)*p);
}
