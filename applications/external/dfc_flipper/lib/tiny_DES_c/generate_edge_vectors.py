#!/usr/bin/env python3
"""Generate Wycheproof-style DES/3DES edge-case vectors.

Expected ciphertexts are checked against both PyCA cryptography and OpenSSL.
The resulting JSON is the reviewable corpus; edge_vectors.h is a generated
native-test representation of the same cases.
"""

import json
import os
import subprocess
from cryptography.hazmat.decrepit.ciphers.algorithms import TripleDES
from cryptography.hazmat.decrepit.ciphers import modes as decrepit_modes
from cryptography.hazmat.primitives.ciphers import Cipher, modes


ROOT = os.path.dirname(__file__)
JSON_PATH = os.path.join(ROOT, "test_vectors", "edge_cases.json")
HEADER_PATH = os.path.join(ROOT, "edge_vectors.h")


def expand_key(key):
    if len(key) == 8:
        return key * 3
    if len(key) == 16:
        return key + key[:8]
    return key


def pyca_encrypt(mode, key, iv, msg):
    key = expand_key(key)
    mode_impl = {
        "ECB": modes.ECB(),
        "CBC": modes.CBC(iv),
        "CFB64": decrepit_modes.CFB(iv),
        "CFB8": decrepit_modes.CFB8(iv),
        "OFB": decrepit_modes.OFB(iv),
    }[mode]
    return Cipher(TripleDES(key), mode_impl).encryptor().update(msg)


def openssl_encrypt(mode, key, iv, msg):
    cipher = {
        "ECB": "des-ede3-ecb",
        "CBC": "des-ede3-cbc",
        "CFB1": "des-ede3-cfb1",
        "CFB8": "des-ede3-cfb8",
        "CFB64": "des-ede3-cfb",
        "OFB": "des-ede3-ofb",
    }[mode]
    args = [
        "openssl",
        "enc",
        "-provider",
        "default",
        "-provider",
        "legacy",
        "-" + cipher,
        "-K",
        expand_key(key).hex(),
        "-nopad",
        "-nosalt",
    ]
    if mode != "ECB":
        args += ["-iv", iv.hex()]
    result = subprocess.run(
        args, input=msg, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True
    )
    return result.stdout


def cfb1_openssl(mode, key, iv, msg, bit_length):
    """Run OpenSSL on whole bytes and retain only the requested CFB1 bits."""
    padded_length = (bit_length + 7) // 8
    padded = msg[:padded_length]
    result = openssl_encrypt(mode, key, iv, padded)
    if bit_length % 8:
        keep = 0xFF & (0xFF << (8 - bit_length % 8))
        result = result[: padded_length - 1] + bytes([result[-1] & keep])
    return result


def case(tc_id, mode, key, iv, msg, comment, bit_length=None, flags=None):
    if mode == "CFB1":
        expected = cfb1_openssl(mode, key, iv, msg, bit_length)
        # The bit-oriented API leaves unprocessed low bits untouched.
        if bit_length % 8:
            last = (bit_length + 7) // 8 - 1
            mask = 0xFF >> (bit_length % 8)
            expected = expected[:last] + bytes(
                [(expected[last] & ~mask) | (msg[last] & mask)]
            )
        backend = "openssl"
    else:
        pyca = pyca_encrypt(mode, key, iv, msg)
        openssl = openssl_encrypt(mode, key, iv, msg)
        if pyca != openssl:
            raise RuntimeError(f"backend mismatch for test case {tc_id}")
        expected = pyca
        backend = "cryptography+openssl"
    item = {
        "tcId": tc_id,
        "mode": mode,
        "comment": comment,
        "key": key.hex(),
        "iv": iv.hex(),
        "msg": msg.hex(),
        "ct": expected.hex(),
        "result": "valid",
        "flags": flags or [],
        "generatedBy": backend,
    }
    if bit_length is not None:
        item["bitLength"] = bit_length
    return item


def build_cases():
    zero_iv = bytes(8)
    ff_iv = bytes([0xFF] * 8)
    cases = []
    tc = 1
    definitions = [
        (
            "ECB",
            bytes.fromhex("0101010101010101"),
            zero_iv,
            bytes.fromhex("0000000000000000"),
            "DES weak all-zero block",
            ["weak-key"],
        ),
        (
            "ECB",
            bytes.fromhex("0000000000000000"),
            zero_iv,
            bytes.fromhex("FFFFFFFFFFFFFFFF"),
            "DES zero key with parity cleared",
            ["parity"],
        ),
        (
            "ECB",
            bytes.fromhex("0123456789ABCDEE"),
            zero_iv,
            bytes.fromhex("4E6F772069732074"),
            "DES key with altered parity",
            ["parity"],
        ),
        (
            "CBC",
            bytes.fromhex("0123456789ABCDEF"),
            ff_iv,
            bytes.fromhex("000102030405060708090A0B0C0D0E0F"),
            "DES CBC multi-block with all-ones IV",
            [],
        ),
        (
            "CFB64",
            bytes.fromhex("0123456789ABCDEF"),
            zero_iv,
            bytes.fromhex("000102030405060708090A0B0C"),
            "DES CFB64 non-block-aligned stream",
            [],
        ),
        (
            "CFB8",
            bytes.fromhex("0123456789ABCDEF"),
            ff_iv,
            bytes.fromhex("FF00A55A1234567890"),
            "DES CFB8 chaining across varied bytes",
            [],
        ),
        (
            "CFB1",
            bytes.fromhex("0123456789ABCDEF"),
            zero_iv,
            bytes.fromhex("A5C3"),
            "DES CFB1 one bit",
            1,
            [],
        ),
        (
            "CFB1",
            bytes.fromhex("0123456789ABCDEF"),
            zero_iv,
            bytes.fromhex("A5C3"),
            "DES CFB1 seven bits",
            7,
            [],
        ),
        (
            "CFB1",
            bytes.fromhex("0123456789ABCDEF"),
            ff_iv,
            bytes.fromhex("A5C35A"),
            "DES CFB1 nine bits",
            9,
            [],
        ),
        (
            "OFB",
            bytes.fromhex("0123456789ABCDEF"),
            ff_iv,
            bytes.fromhex("000102030405060708090A"),
            "DES OFB non-block-aligned stream",
            [],
        ),
        (
            "ECB",
            bytes.fromhex("0123456789ABCDEF23456789ABCDEF01"),
            zero_iv,
            bytes.fromhex("0001020304050607"),
            "2-key TDES ECB",
            [],
        ),
        (
            "CBC",
            bytes.fromhex("0123456789ABCDEF23456789ABCDEF01"),
            zero_iv,
            bytes.fromhex("000102030405060708090A0B0C0D0E0F"),
            "2-key TDES CBC",
            [],
        ),
        (
            "CFB64",
            bytes.fromhex("0123456789ABCDEF23456789ABCDEF01"),
            ff_iv,
            bytes.fromhex("00112233445566778899AABBCCDDEE"),
            "2-key TDES CFB64 stream",
            [],
        ),
        (
            "ECB",
            bytes.fromhex("0123456789ABCDEF" * 3),
            zero_iv,
            bytes.fromhex("FEDCBA9876543210"),
            "3-key TDES with repeated key components",
            ["degenerate-3key"],
        ),
        (
            "ECB",
            bytes.fromhex("0123456789ABCDEF23456789ABCDEF01456789ABCDEF0123"),
            zero_iv,
            bytes.fromhex("FEDCBA9876543210"),
            "3-key TDES ECB",
            [],
        ),
        (
            "OFB",
            bytes.fromhex("0123456789ABCDEF23456789ABCDEF01456789ABCDEF0123"),
            zero_iv,
            bytes.fromhex("00112233445566778899AABBCCDDEEFF00"),
            "3-key TDES OFB stream",
            [],
        ),
    ]
    for definition in definitions:
        mode, key, iv, msg, comment, *rest = definition
        bit_length = rest[0] if mode == "CFB1" else None
        flags = rest[1] if mode == "CFB1" else rest[0]
        cases.append(case(tc, mode, key, iv, msg, comment, bit_length, flags))
        tc += 1
    return cases


def hex_array(value):
    return ", ".join(f"0x{byte:02x}" for byte in bytes.fromhex(value))


def write_header(cases):
    lines = [
        "/* Auto-generated by generate_edge_vectors.py; do not edit. */",
        "#ifndef EDGE_VECTORS_H",
        "#define EDGE_VECTORS_H",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "struct edge_vector {",
        "  const char* mode;",
        "  const uint8_t* key; size_t key_len;",
        "  const uint8_t* iv;",
        "  const uint8_t* msg; const uint8_t* ct; size_t len;",
        "  size_t bit_length;",
        "};",
        "",
    ]
    for index, item in enumerate(cases):
        for field in ("key", "iv", "msg", "ct"):
            value = item[field]
            name = f"edge_{index}_{field}"
            lines.append(f"static const uint8_t {name}[] = {{ {hex_array(value)} }};")
        lines.append("")
    lines.append("static const struct edge_vector edge_vectors[] = {")
    for index, item in enumerate(cases):
        lines.append(
            f'  {{ "{item["mode"]}", edge_{index}_key, sizeof(edge_{index}_key), '
            f"edge_{index}_iv, edge_{index}_msg, edge_{index}_ct, "
            f'sizeof(edge_{index}_msg), {item.get("bitLength", 0)} }},'
        )
    lines += [
        "};",
        "#define EDGE_VECTOR_COUNT (sizeof(edge_vectors) / sizeof(edge_vectors[0]))",
        "#endif /* EDGE_VECTORS_H */",
        "",
    ]
    with open(HEADER_PATH, "w", encoding="utf-8") as output:
        output.write("\n".join(lines))


def main():
    cases = build_cases()
    document = {
        "algorithm": "DES/3DES",
        "generatorVersion": "tiny-DES-c edge vectors 1",
        "notes": [
            "All cases are valid inputs; weak and parity-variant keys are intentional.",
            "CFB1 bitLength is MSB-first and unused low bits are preserved.",
        ],
        "numberOfTests": len(cases),
        "testGroups": [{"type": "encryption", "tests": cases}],
    }
    os.makedirs(os.path.dirname(JSON_PATH), exist_ok=True)
    with open(JSON_PATH, "w", encoding="utf-8") as output:
        json.dump(document, output, indent=2)
        output.write("\n")
    write_header(cases)
    print(f"Generated {JSON_PATH} and {HEADER_PATH} ({len(cases)} vectors)")


if __name__ == "__main__":
    main()
