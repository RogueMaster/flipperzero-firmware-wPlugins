/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The list of host test suites. Adding one means adding a line here, a
 * matching test_suite_<name>() in tests/host/test_<name>.c, and the file in
 * the Makefile's TEST_SRC.
 */
#pragma once

#define EMRTD_TEST_SUITES(X)                     \
    X(mac, "mac and padding")                    \
    X(kdf, "key derivation")                     \
    X(ec, "elliptic curves")                     \
    X(tlv, "BER-TLV")                            \
    X(apdu, "APDU encoding")                     \
    X(isodep, "ISO-DEP block transmission")      \
    X(files, "file catalogue")                   \
    X(mrz, "machine readable zone")              \
    X(lds, "logical data structure")             \
    X(security_info, "security infos")           \
    X(sm, "secure messaging")                    \
    X(bac, "basic access control")               \
    X(pace, "password authenticated connection") \
    X(access, "access driver selection")         \
    X(session, "a full read against a simulated chip")
