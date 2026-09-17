#!/usr/bin/env python3
"""Cross-check EAX' worked vectors with OpenSSL's AES-128 implementation."""

import json
import subprocess
from pathlib import Path


def aes(key, block):
    result = subprocess.run(
        ["openssl", "enc", "-aes-128-ecb", "-K", key.hex(), "-nopad", "-nosalt"],
        input=block,
        stdout=subprocess.PIPE,
        check=True,
    )
    return result.stdout


def double(value):
    carry = 0
    shifted = bytearray(16)
    for i in range(16):
        next_carry = value[i] >> 7
        shifted[i] = ((value[i] << 1) & 255) | carry
        carry = next_carry
    shifted[0] ^= 0x87 if carry else 0
    return bytes(shifted)


def cmac_prime(key, initial, data, complete, partial):
    mac = initial
    while len(data) > 16:
        mac = aes(key, bytes(a ^ b for a, b in zip(mac, data[:16])))
        data = data[16:]
    if len(data) == 16:
        block = bytes(a ^ b for a, b in zip(data, complete))
    else:
        block = data + b"\x80" + bytes(15 - len(data))
        block = bytes(a ^ b for a, b in zip(block, partial))
    return aes(key, bytes(a ^ b for a, b in zip(mac, block)))


def ctr_prime(key, initial, data):
    counter = bytearray(initial)
    counter[1] &= 0x7F
    counter[3] &= 0x7F
    output = bytearray()
    for offset in range(0, len(data), 16):
        stream = aes(key, bytes(counter))
        output.extend(a ^ b for a, b in zip(data[offset : offset + 16], stream))
        for i in range(15, -1, -1):
            counter[i] = (counter[i] + 1) & 0xFF
            if counter[i] != 0:
                break
    return bytes(output)


def verify(path):
    corpus = json.loads(path.read_text())
    checked = 0
    for group in corpus["testGroups"]:
        for vector in group["tests"]:
            key = bytes.fromhex(vector["key"])
            cleartext = bytes.fromhex(vector["cleartext"])
            plaintext = bytes.fromhex(vector["msg"])
            l = aes(key, bytes(16))
            d = double(l)
            q = double(d)
            nonce_mac = cmac_prime(key, d, cleartext, d, q)
            ciphertext = ctr_prime(key, nonce_mac, plaintext)
            message_mac = cmac_prime(key, q, ciphertext, d, q)
            tag = bytes(a ^ b for a, b in zip(nonce_mac, message_mac))[-4:][::-1]
            if ciphertext.hex() != vector["ct"] or tag.hex() != vector["tag"]:
                raise SystemExit("EAX' vector {} failed".format(vector["tcId"]))
            checked += 1
    print("verified {} EAX' vectors with OpenSSL AES-128".format(checked))


if __name__ == "__main__":
    verify(Path(__file__).with_name("eax_prime_worked.json"))
