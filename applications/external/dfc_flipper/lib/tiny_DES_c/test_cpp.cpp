/*
 * SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: Unlicense
 */

#include <iostream>
#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
#include "des.hpp"
#include "test_vectors.h"

int main() {
    std::cout << "Testing tiny-DES-c C++ Wrapper (des.hpp)..." << std::endl;

#if DES_ENABLE_ECB
    {
        std::array<uint8_t, 8> key;
        std::copy(des_test_key, des_test_key + 8, key.begin());
        tiny_des::DES des(key);

        uint8_t block[8];
        std::copy(des_test_pt, des_test_pt + 8, block);

        des.encrypt_ecb(block);
        assert(std::equal(block, block + 8, des_test_ct));

        des.decrypt_ecb(block);
        assert(std::equal(block, block + 8, des_test_pt));
        std::cout << "1. C++ Single DES ECB: OK" << std::endl;
    }
#endif

#if DES_ENABLE_TDES && DES_ENABLE_CBC && DES_NEEDS_IV
    {
        std::vector<uint8_t> key(tdes2_key, tdes2_key + 16);
        std::array<uint8_t, 8> iv;
        std::copy(des_cbc_iv, des_cbc_iv + 8, iv.begin());

        tiny_des::DES3 des3(key, iv);

        std::vector<uint8_t> data(tdes2_pt, tdes2_pt + 16);
        des3.encrypt_cbc(data);
        assert(data == std::vector<uint8_t>(tdes2_cbc_ct, tdes2_cbc_ct + 16));

        des3.set_iv(iv);
        des3.decrypt_cbc(data);
        assert(data == std::vector<uint8_t>(tdes2_pt, tdes2_pt + 16));
        std::cout << "2. C++ 2-Key 3DES CBC: OK" << std::endl;

        try {
            std::vector<uint8_t> bad(7, 0);
            des3.encrypt_cbc(bad);
            assert(false && "expected CBC length error");
        } catch (const std::invalid_argument&) {
            std::cout << "2b. C++ CBC misaligned length throws: OK" << std::endl;
        }
    }
#endif

#if DES_ENABLE_CMAC
    {
        std::vector<uint8_t> key(tdes3_key, tdes3_key + 24);
        std::vector<uint8_t> msg = {'D', 'E', 'S', 'F', 'i', 'r', 'e'};
        auto mac = tiny_des::cmac(key, msg);
        assert(mac.size() == 8);
        std::cout << "3. C++ 3-Key 3DES CMAC: OK" << std::endl;
    }
#endif

#if DES_ENABLE_TDES && DES_ENABLE_CTR && DES_NEEDS_IV
    {
        std::vector<uint8_t> key(tdes3_key, tdes3_key + 24);
        std::array<uint8_t, 8> iv;
        std::copy(des_ctr_iv, des_ctr_iv + 8, iv.begin());
        tiny_des::DES3 des3(key, iv);
        std::vector<uint8_t> data = {1, 2, 3, 4, 5};
        auto orig = data;
        des3.xcrypt_ctr(data);
        des3.set_iv(iv);
        des3.xcrypt_ctr(data);
        assert(data == orig);
        std::cout << "4. C++ 3DES CTR round-trip: OK" << std::endl;
    }
#endif

    std::cout << "ALL C++ TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
