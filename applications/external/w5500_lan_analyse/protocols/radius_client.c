#include "radius_client.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <socket.h>
#include <string.h>

#define RADIUS_SOCK       3
#define RADIUS_PORT       1812
#define RADIUS_LOCAL_PORT 18120
#define RADIUS_TIMEOUT_MS 5000

/* RADIUS codes */
#define RADIUS_ACCESS_REQUEST   1
#define RADIUS_ACCESS_ACCEPT    2
#define RADIUS_ACCESS_REJECT    3
#define RADIUS_ACCESS_CHALLENGE 11

/* RADIUS attribute types */
#define RADIUS_ATTR_USER_NAME     1
#define RADIUS_ATTR_USER_PASSWORD 2
#define RADIUS_ATTR_NAS_IP        4
#define RADIUS_ATTR_NAS_PORT      5

/**
 * Simple MD5 implementation (RFC 1321).
 * Minimal for RADIUS password hiding (single block).
 */

/* MD5 context */
typedef struct {
    uint32_t state[4];
    uint8_t buffer[64];
    uint32_t count[2];
} MD5_CTX;

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define FF(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTL((a), (s));                           \
        (a) += (b);                                     \
    }
#define GG(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTL((a), (s));                           \
        (a) += (b);                                     \
    }
#define HH(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTL((a), (s));                           \
        (a) += (b);                                     \
    }
#define II(a, b, c, d, x, s, ac)                        \
    {                                                   \
        (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTL((a), (s));                           \
        (a) += (b);                                     \
    }

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];

    for(int i = 0; i < 16; i++) {
        x[i] = ((uint32_t)block[i * 4]) | ((uint32_t)block[i * 4 + 1] << 8) |
               ((uint32_t)block[i * 4 + 2] << 16) | ((uint32_t)block[i * 4 + 3] << 24);
    }

    FF(a, b, c, d, x[0], 7, 0xd76aa478);
    FF(d, a, b, c, x[1], 12, 0xe8c7b756);
    FF(c, d, a, b, x[2], 17, 0x242070db);
    FF(b, c, d, a, x[3], 22, 0xc1bdceee);
    FF(a, b, c, d, x[4], 7, 0xf57c0faf);
    FF(d, a, b, c, x[5], 12, 0x4787c62a);
    FF(c, d, a, b, x[6], 17, 0xa8304613);
    FF(b, c, d, a, x[7], 22, 0xfd469501);
    FF(a, b, c, d, x[8], 7, 0x698098d8);
    FF(d, a, b, c, x[9], 12, 0x8b44f7af);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1);
    FF(b, c, d, a, x[11], 22, 0x895cd7be);
    FF(a, b, c, d, x[12], 7, 0x6b901122);
    FF(d, a, b, c, x[13], 12, 0xfd987193);
    FF(c, d, a, b, x[14], 17, 0xa679438e);
    FF(b, c, d, a, x[15], 22, 0x49b40821);

    GG(a, b, c, d, x[1], 5, 0xf61e2562);
    GG(d, a, b, c, x[6], 9, 0xc040b340);
    GG(c, d, a, b, x[11], 14, 0x265e5a51);
    GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, x[5], 5, 0xd62f105d);
    GG(d, a, b, c, x[10], 9, 0x02441453);
    GG(c, d, a, b, x[15], 14, 0xd8a1e681);
    GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, x[9], 5, 0x21e1cde6);
    GG(d, a, b, c, x[14], 9, 0xc33707d6);
    GG(c, d, a, b, x[3], 14, 0xf4d50d87);
    GG(b, c, d, a, x[8], 20, 0x455a14ed);
    GG(a, b, c, d, x[13], 5, 0xa9e3e905);
    GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
    GG(c, d, a, b, x[7], 14, 0x676f02d9);
    GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

    HH(a, b, c, d, x[5], 4, 0xfffa3942);
    HH(d, a, b, c, x[8], 11, 0x8771f681);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122);
    HH(b, c, d, a, x[14], 23, 0xfde5380c);
    HH(a, b, c, d, x[1], 4, 0xa4beea44);
    HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
    HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
    HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    HH(a, b, c, d, x[13], 4, 0x289b7ec6);
    HH(d, a, b, c, x[0], 11, 0xeaa127fa);
    HH(c, d, a, b, x[3], 16, 0xd4ef3085);
    HH(b, c, d, a, x[6], 23, 0x04881d05);
    HH(a, b, c, d, x[9], 4, 0xd9d4d039);
    HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
    HH(b, c, d, a, x[2], 23, 0xc4ac5665);

    II(a, b, c, d, x[0], 6, 0xf4292244);
    II(d, a, b, c, x[7], 10, 0x432aff97);
    II(c, d, a, b, x[14], 15, 0xab9423a7);
    II(b, c, d, a, x[5], 21, 0xfc93a039);
    II(a, b, c, d, x[12], 6, 0x655b59c3);
    II(d, a, b, c, x[3], 10, 0x8f0ccc92);
    II(c, d, a, b, x[10], 15, 0xffeff47d);
    II(b, c, d, a, x[1], 21, 0x85845dd1);
    II(a, b, c, d, x[8], 6, 0x6fa87e4f);
    II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    II(c, d, a, b, x[6], 15, 0xa3014314);
    II(b, c, d, a, x[13], 21, 0x4e0811a1);
    II(a, b, c, d, x[4], 6, 0xf7537e82);
    II(d, a, b, c, x[11], 10, 0xbd3af235);
    II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
    II(b, c, d, a, x[9], 21, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

/**
 * Compute MD5 hash of data.
 * Output: 16 bytes in digest.
 */
static void md5_hash(const uint8_t* data, uint16_t len, uint8_t digest[16]) {
    uint32_t state[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    uint8_t block[64];
    uint16_t offset = 0;

    /* Process full 64-byte blocks */
    while(offset + 64 <= len) {
        md5_transform(state, data + offset);
        offset += 64;
    }

    /* Final block with padding */
    uint16_t remaining = len - offset;
    memcpy(block, data + offset, remaining);
    block[remaining] = 0x80;
    memset(block + remaining + 1, 0, 64 - remaining - 1);

    if(remaining >= 56) {
        md5_transform(state, block);
        memset(block, 0, 64);
    }

    /* Append length in bits (little-endian) */
    uint64_t bits = (uint64_t)len * 8;
    block[56] = (uint8_t)(bits);
    block[57] = (uint8_t)(bits >> 8);
    block[58] = (uint8_t)(bits >> 16);
    block[59] = (uint8_t)(bits >> 24);
    block[60] = (uint8_t)(bits >> 32);
    block[61] = (uint8_t)(bits >> 40);
    block[62] = (uint8_t)(bits >> 48);
    block[63] = (uint8_t)(bits >> 56);
    md5_transform(state, block);

    /* Output digest (little-endian) */
    for(int i = 0; i < 4; i++) {
        digest[i * 4] = (uint8_t)(state[i]);
        digest[i * 4 + 1] = (uint8_t)(state[i] >> 8);
        digest[i * 4 + 2] = (uint8_t)(state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(state[i] >> 24);
    }
}

/**
 * RADIUS password encoding per RFC 2865 section 5.2.
 * password_hidden = password XOR MD5(secret + authenticator)
 */
static uint8_t radius_encode_password(
    const char* password,
    const char* secret,
    const uint8_t authenticator[16],
    uint8_t* out) {
    uint8_t pw_len = (uint8_t)strlen(password);
    /* Pad to 16-byte boundary */
    uint8_t padded_len = ((pw_len + 15) / 16) * 16;
    if(padded_len == 0) padded_len = 16;
    if(padded_len > 128) padded_len = 128;

    uint8_t padded[128];
    memset(padded, 0, padded_len);
    memcpy(padded, password, pw_len);

    uint8_t secret_len = (uint8_t)strlen(secret);
    uint8_t hash_input[180]; /* secret + authenticator or previous block */

    /* First block: MD5(secret + authenticator) */
    memcpy(hash_input, secret, secret_len);
    memcpy(hash_input + secret_len, authenticator, 16);

    uint8_t digest[16];
    md5_hash(hash_input, secret_len + 16, digest);

    for(uint8_t i = 0; i < 16; i++) {
        out[i] = padded[i] ^ digest[i];
    }

    /* Subsequent blocks: MD5(secret + previous cipher block) */
    for(uint8_t block = 1; block < padded_len / 16; block++) {
        memcpy(hash_input, secret, secret_len);
        memcpy(hash_input + secret_len, &out[(block - 1) * 16], 16);
        md5_hash(hash_input, secret_len + 16, digest);

        for(uint8_t i = 0; i < 16; i++) {
            out[block * 16 + i] = padded[block * 16 + i] ^ digest[i];
        }
    }

    return padded_len;
}

bool radius_test(
    const uint8_t server_ip[4],
    const char* secret,
    const char* username,
    const char* password,
    RadiusResult* result) {
    memset(result, 0, sizeof(RadiusResult));

    close(RADIUS_SOCK);
    if(socket(RADIUS_SOCK, Sn_MR_UDP, RADIUS_LOCAL_PORT, 0) != RADIUS_SOCK) {
        strncpy(result->status_str, "Socket failed", sizeof(result->status_str));
        return false;
    }

    /* Build Access-Request */
    uint8_t* pkt = malloc(300);
    if(!pkt) {
        strncpy(result->status_str, "Memory failed", sizeof(result->status_str));
        close(RADIUS_SOCK);
        return false;
    }
    uint16_t idx = 0;

    /* Code: Access-Request */
    pkt[idx++] = RADIUS_ACCESS_REQUEST;

    /* Identifier */
    uint8_t identifier = (uint8_t)(furi_get_tick() & 0xFF);
    pkt[idx++] = identifier;

    /* Length placeholder (will fill later) */
    uint16_t len_offset = idx;
    idx += 2;

    /* Authenticator (16 random bytes) */
    uint8_t authenticator[16];
    furi_hal_random_fill_buf(authenticator, 16);
    memcpy(&pkt[idx], authenticator, 16);
    idx += 16;

    /* Attribute: User-Name */
    uint8_t uname_len = (uint8_t)strlen(username);
    pkt[idx++] = RADIUS_ATTR_USER_NAME;
    pkt[idx++] = 2 + uname_len;
    memcpy(&pkt[idx], username, uname_len);
    idx += uname_len;

    /* Attribute: User-Password (MD5-hidden) */
    uint8_t encoded_pw[128];
    uint8_t pw_enc_len = radius_encode_password(password, secret, authenticator, encoded_pw);
    pkt[idx++] = RADIUS_ATTR_USER_PASSWORD;
    pkt[idx++] = 2 + pw_enc_len;
    memcpy(&pkt[idx], encoded_pw, pw_enc_len);
    idx += pw_enc_len;

    /* Attribute: NAS-Port */
    pkt[idx++] = RADIUS_ATTR_NAS_PORT;
    pkt[idx++] = 6;
    pkt[idx++] = 0;
    pkt[idx++] = 0;
    pkt[idx++] = 0;
    pkt[idx++] = 0;

    /* Fill length */
    pkt[len_offset] = (uint8_t)(idx >> 8);
    pkt[len_offset + 1] = (uint8_t)(idx);

    /* Send */
    if(sendto(RADIUS_SOCK, pkt, idx, (uint8_t*)server_ip, RADIUS_PORT) <= 0) {
        free(pkt);
        close(RADIUS_SOCK);
        strncpy(result->status_str, "Send failed", sizeof(result->status_str));
        return false;
    }

    /* Wait for response */
    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < RADIUS_TIMEOUT_MS) {
        uint16_t rx_len = getSn_RX_RSR(RADIUS_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(RADIUS_SOCK, pkt, sizeof(pkt), from_ip, &from_port);
            if(recv_len >= 20) {
                result->code = pkt[0];
                result->identifier = pkt[1];
                result->length = ((uint16_t)pkt[2] << 8) | pkt[3];
                result->response_received = true;

                switch(result->code) {
                case RADIUS_ACCESS_ACCEPT:
                    strncpy(result->status_str, "Access-Accept", sizeof(result->status_str));
                    break;
                case RADIUS_ACCESS_REJECT:
                    strncpy(result->status_str, "Access-Reject", sizeof(result->status_str));
                    break;
                case RADIUS_ACCESS_CHALLENGE:
                    strncpy(result->status_str, "Access-Challenge", sizeof(result->status_str));
                    break;
                default:
                    snprintf(
                        result->status_str, sizeof(result->status_str), "Code %d", result->code);
                    break;
                }
                break;
            }
        }
        furi_delay_ms(10);
    }

    free(pkt);
    close(RADIUS_SOCK);

    if(!result->response_received) {
        strncpy(result->status_str, "No response (timeout)", sizeof(result->status_str));
    }

    result->valid = true;
    return result->response_received;
}
