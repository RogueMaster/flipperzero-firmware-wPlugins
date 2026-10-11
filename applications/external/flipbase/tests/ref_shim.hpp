// Stand-ins for the RPCS3 utilities the extracted reference code touches.
// Crypto goes through OpenSSL so the reference does not share code with ours.
#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <openssl/aes.h>
#include <openssl/sha.h>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s8 = std::int8_t;

using shared_mutex = std::mutex;

namespace fs {
enum seek_mode {
    seek_set = 0
};

class file {
    bool m_open = false;

public:
    file() = default;
    explicit file(bool open)
        : m_open(open) {
    }
    file(file&&) = default;
    file& operator=(file&&) = default;
    explicit operator bool() const {
        return m_open;
    }
    void seek(std::int64_t, int) {
    }
    std::size_t write(const void*, std::size_t size) {
        return size;
    }
    void close() {
        m_open = false;
    }
};
} // namespace fs

struct ref_logger {
    template <typename... Args>
    void error(const char*, Args&&...) {
    }
    template <typename... Args>
    void trace(const char*, Args&&...) {
    }
};
static ref_logger infinity_log;

struct sha1_context {
    SHA_CTX ctx;
};
inline void sha1_starts(sha1_context* c) {
    SHA1_Init(&c->ctx);
}
inline void sha1_update(sha1_context* c, const u8* data, std::size_t size) {
    SHA1_Update(&c->ctx, data, size);
}
inline void sha1_finish(sha1_context* c, u8* out) {
    SHA1_Final(out, &c->ctx);
}

#define AES_DECRYPT_MODE AES_DECRYPT
struct aes_context {
    AES_KEY key;
};
inline void aes_setkey_enc(aes_context* c, const u8* key, int bits) {
    AES_set_encrypt_key(key, bits, &c->key);
}
inline void aes_setkey_dec(aes_context* c, const u8* key, int bits) {
    AES_set_decrypt_key(key, bits, &c->key);
}
inline void aes_crypt_ecb(aes_context* c, int mode, const u8* in, u8* out) {
    AES_ecb_encrypt(in, out, &c->key, mode);
}

// The reference draws the tag UID from rand(); tests feed it known bytes.
static std::vector<int> g_ref_rand_values;
static std::size_t g_ref_rand_pos = 0;
static int ref_rand_next() {
    return g_ref_rand_values.at(g_ref_rand_pos++);
}
