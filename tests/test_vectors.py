#!/usr/bin/env python3
import hashlib
import hmac
import struct

MASTER = bytes.fromhex("9A759CF2C4F7CAFF222CB9769B41BC96")
INFO_A = b"RFID-A\0"
INFO_B = b"RFID-B\0"
UID = bytes.fromhex("EAFE5CFC")
EXPECTED_A = [
    "2FC5F17A68A4", "5E670EC6C2A6", "9105A7AB623E", "E77524418C47",
    "718A25970897", "99004583B9DF", "2F5E3BFEB550", "6902FB8D7076",
    "23FF67E42D97", "D4B4BC858145", "6DE0A4CB91B2", "AF82B7A14952",
    "AB37D9F8830F", "B0EAA7F8E938", "B09673D0FD9E", "C8F437569B17",
]
EXPECTED_B = [
    "759F4DF068B6", "19D28609FC31", "4EF8D5FFBA5D", "710137192496",
    "1BEB81D70A0D", "18EC397B596A", "F6E12F1BDBC5", "D4C9E58980B5",
    "1CEDA35704F2", "87A6CC9DDA26", "0ECC7C020F88", "2AAAB42ACBE4",
    "7CE0C9854C66", "2BE8E6594036", "5B817F5CD139", "E48AF7E59B22",
]


def hkdf(uid: bytes, info: bytes) -> bytes:
    prk = hmac.new(MASTER, uid, hashlib.sha256).digest()
    out = b""
    t = b""
    counter = 1
    while len(out) < 96:
        t = hmac.new(prk, t + info + bytes([counter]), hashlib.sha256).digest()
        out += t
        counter += 1
    return out[:96]


def main():
    derived_a = hkdf(UID, INFO_A)
    keys_a = [derived_a[i:i+6].hex().upper() for i in range(0, 96, 6)]
    assert keys_a == EXPECTED_A, (keys_a, EXPECTED_A)

    derived_b = hkdf(UID, INFO_B)
    keys_b = [derived_b[i:i+6].hex().upper() for i in range(0, 96, 6)]
    assert keys_b == EXPECTED_B, (keys_b, EXPECTED_B)

    key_file = derived_a + derived_b
    assert len(key_file) == 192
    assert key_file[:6].hex().upper() == "2FC5F17A68A4"
    assert key_file[96:102].hex().upper() == "759F4DF068B6"

    block5 = bytes.fromhex("87909AFFE80300000000E03F00000000")
    assert block5[:4].hex().upper() == "87909AFF"
    assert int.from_bytes(block5[4:6], "little") == 1000
    assert abs(struct.unpack("<f", block5[8:12])[0] - 1.75) < 0.0001

    block1 = bytes.fromhex("4230302D443100004746423030000000")
    assert block1[:8].rstrip(b"\0").decode() == "B00-D1"
    assert block1[8:16].rstrip(b"\0").decode() == "GFB00"

    block6 = bytes.fromhex("50000800000000000E01F00000000000")
    assert int.from_bytes(block6[0:2], "little") == 80
    assert int.from_bytes(block6[2:4], "little") == 8
    assert int.from_bytes(block6[8:10], "little") == 270
    assert int.from_bytes(block6[10:12], "little") == 240

    block8 = bytes.fromhex("D007D007E803E8036666663FCDCC4C3E")
    assert abs(struct.unpack("<f", block8[12:16])[0] - 0.2) < 0.0001

    block10 = bytes.fromhex("00000000E11900000000000000000000")
    assert int.from_bytes(block10[4:6], "little") / 100 == 66.25

    block12 = bytes.fromhex("323032345F30395F31375F30385F3537")
    assert block12.decode() == "2024_09_17_08_57"

    block14 = bytes.fromhex("000000008E0100000000000000000000")
    assert int.from_bytes(block14[4:6], "little") == 398

    print("Bambu RFID test vectors OK")


if __name__ == "__main__":
    main()
