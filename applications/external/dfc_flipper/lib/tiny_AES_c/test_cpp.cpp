/*
 * SPDX-FileCopyrightText: kokke
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 *
 * C++ unit test suite for tiny-AES-c (aes.hpp RAII wrapper)
 */

#include <iostream>
#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
#include <stdexcept>

#include "aes.hpp"
#include "test_vectors.h"

int main() {
    std::cout << "Testing tiny-AES-c C++ Wrapper (aes.hpp)..." << std::endl;

#if defined(AES_ENABLE_ECB) && AES_ENABLE_ECB == 1
    {
        std::array<uint8_t, 16> key;
        std::copy(aes128_key, aes128_key + 16, key.begin());
        tiny_aes::AES aes(key);

        uint8_t block[16];
        std::copy(nist_plaintext, nist_plaintext + 16, block);

        aes.encrypt_ecb(block);
        assert(std::equal(block, block + 16, aes128_ecb_ciphertext));

        aes.decrypt_ecb(block);
        assert(std::equal(block, block + 16, nist_plaintext));
        std::cout << "1. C++ AES-128 ECB: OK" << std::endl;
    }
#endif

#if defined(AES_ENABLE_CBC) && AES_ENABLE_CBC == 1
    {
        std::vector<uint8_t> key(aes128_key, aes128_key + 16);
        std::vector<uint8_t> iv(nist_iv, nist_iv + 16);

        tiny_aes::AES aes(key, iv);

        std::vector<uint8_t> data(nist_plaintext, nist_plaintext + 64);
        aes.encrypt_cbc(data);
        assert(data == std::vector<uint8_t>(aes128_cbc_ciphertext, aes128_cbc_ciphertext + 64));

        aes.set_iv(iv);
        aes.decrypt_cbc(data);
        assert(data == std::vector<uint8_t>(nist_plaintext, nist_plaintext + 64));
        std::cout << "2. C++ AES-128 CBC: OK" << std::endl;

        try {
            std::vector<uint8_t> bad(15, 0);
            aes.encrypt_cbc(bad);
            assert(false && "expected CBC misaligned length error");
        } catch (const std::invalid_argument&) {
            std::cout << "2b. C++ CBC misaligned length throws: OK" << std::endl;
        }
    }
#endif

#if defined(AES_ENABLE_CTR) && AES_ENABLE_CTR == 1
    {
        std::vector<uint8_t> key(aes128_key, aes128_key + 16);
        std::vector<uint8_t> iv(nist_ctr_iv, nist_ctr_iv + 16);
        tiny_aes::AES aes(key, iv);

        std::vector<uint8_t> data(nist_plaintext, nist_plaintext + 64);
        auto orig = data;
        aes.xcrypt_ctr(data);
        assert(data == std::vector<uint8_t>(aes128_ctr_ciphertext, aes128_ctr_ciphertext + 64));

        aes.set_iv(iv);
        aes.xcrypt_ctr(data);
        assert(data == orig);
        std::cout << "3. C++ AES-128 CTR round-trip: OK" << std::endl;
    }
#endif

#if defined(AES_ENABLE_OFB) && AES_ENABLE_OFB == 1
    {
        std::vector<uint8_t> key(aes128_key, aes128_key + 16);
        std::vector<uint8_t> iv(nist_iv, nist_iv + 16);
        tiny_aes::AES aes(key, iv);

        std::vector<uint8_t> data(nist_plaintext, nist_plaintext + 64);
        auto orig = data;
        aes.xcrypt_ofb(data);
        assert(data == std::vector<uint8_t>(aes128_ofb_ciphertext, aes128_ofb_ciphertext + 64));

        aes.set_iv(iv);
        aes.xcrypt_ofb(data);
        assert(data == orig);
        std::cout << "4. C++ AES-128 OFB round-trip: OK" << std::endl;
    }
#endif

#if defined(AES_ENABLE_GCM) && AES_ENABLE_GCM == 1
    {
        std::vector<uint8_t> key(aes128_key, aes128_key + 16);
        std::vector<uint8_t> iv(12, 0x01);
        tiny_aes::GCM gcm(key, iv, 16);
        std::vector<uint8_t> data = { 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!' };
        auto orig = data;

        gcm.encrypt_update(data);
        auto tag = gcm.encrypt_finish();
        assert(tag.size() == 16);

        tiny_aes::GCM gcm_dec(key, iv, 16);
        gcm_dec.decrypt_update(data);
        gcm_dec.decrypt_finish(tag);
        assert(data == orig);
        std::cout << "5. C++ AES-128 GCM streaming: OK" << std::endl;
    }
#endif

#if defined(AES_ENABLE_CMAC) && AES_ENABLE_CMAC == 1
    {
        std::vector<uint8_t> key(aes128_key, aes128_key + 16);
        std::vector<uint8_t> msg = { 'A', 'E', 'S', '-', 'C', 'M', 'A', 'C' };
        auto mac = tiny_aes::cmac(key, msg);
        assert(mac.size() == 16);
        std::cout << "6. C++ AES-128 CMAC: OK" << std::endl;
    }
#endif

    std::cout << "ALL C++ TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
