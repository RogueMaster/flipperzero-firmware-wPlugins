#!/usr/bin/env python3
"""
CFS Spool Tool - Desktop companion utility for Creality K2/K1/CFS RFID tags.
Allows creating, decoding, and managing Flipper Zero (.nfc) spool files on PC/Mac.
Compatible with DnG-Crafts/K2-RFID.
"""

import sys
import os
import argparse
import random
from typing import Tuple

# AES implementation using standard library or tiny cipher if PyCryptodome not installed
try:
    from Crypto.Cipher import AES
    HAVE_CRYPTO = True
except ImportError:
    HAVE_CRYPTO = False

U_KEY = bytes([113, 51, 98, 117, 94, 116, 49, 110, 113, 102, 90, 40, 112, 102, 36, 49])
D_KEY = bytes([72, 64, 67, 70, 107, 82, 110, 122, 64, 75, 65, 116, 66, 74, 112, 50])

WEIGHT_MAP = {
    "1 KG": ("0330", 330),
    "750 G": ("0247", 247),
    "600 G": ("0198", 198),
    "500 G": ("0165", 165),
    "250 G": ("0082", 82),
}

MATERIALS = [
    ("01001", "Creality", "Hyper PLA", "PLA"),
    ("02001", "Creality", "Hyper PLA-CF", "PLA-CF"),
    ("06002", "Creality", "Hyper PETG", "PETG"),
    ("03001", "Creality", "Hyper ABS", "ABS"),
    ("09002", "Creality", "ENDER FAST PLA", "PLA"),
    ("04001", "Creality", "CR-PLA", "PLA"),
    ("05001", "Creality", "CR-Silk", "PLA"),
    ("06001", "Creality", "CR-PETG", "PETG"),
    ("07001", "Creality", "CR-ABS", "ABS"),
    ("00001", "Generic", "Generic PLA", "PLA"),
    ("00002", "Generic", "Generic PLA-Silk", "PLA"),
    ("00003", "Generic", "Generic PETG", "PETG"),
    ("00004", "Generic", "Generic ABS", "ABS"),
    ("00005", "Generic", "Generic TPU", "TPU"),
    ("00006", "Generic", "Generic PLA-CF", "PLA-CF"),
    ("00007", "Generic", "Generic ASA", "ASA"),
    ("08001", "Creality", "Ender-PLA", "PLA"),
    ("09001", "Creality", "EN-PLA+", "PLA"),
    ("10001", "Creality", "HP-TPU", "TPU"),
    ("11001", "Creality", "CR-Nylon", "PA"),
    ("13001", "Creality", "CR-PLA Carbon", "PLA-CF"),
    ("14001", "Creality", "CR-PLA Matte", "PLA"),
    ("15001", "Creality", "CR-PLA Fluo", "PLA"),
    ("16001", "Creality", "CR-TPU", "TPU"),
    ("17001", "Creality", "CR-Wood", "PLA"),
    ("18001", "Creality", "HP Ultra PLA", "PLA"),
    ("19001", "Creality", "HP-ASA", "ASA"),
    ("E1001", "eSUN", "PLA+", "PLA"),
    ("P1001", "Polymaker", "Panchroma PLA Satin", "PLA"),
    ("P1002", "Polymaker", "PolySonic PLA Pro", "PLA"),
    ("P1003", "Polymaker", "Panchroma PLA Matte", "PLA"),
]

def aes_ecb_encrypt(key: bytes, data: bytes) -> bytes:
    if HAVE_CRYPTO:
        cipher = AES.new(key, AES.MODE_ECB)
        return cipher.encrypt(data)
    else:
        import subprocess
        p = subprocess.run(
            ['openssl', 'enc', '-aes-128-ecb', '-K', key.hex(), '-nosalt', '-nopad'],
            input=data, stdout=subprocess.PIPE, check=True
        )
        return p.stdout

def aes_ecb_decrypt(key: bytes, data: bytes) -> bytes:
    if HAVE_CRYPTO:
        cipher = AES.new(key, AES.MODE_ECB)
        return cipher.decrypt(data)
    else:
        import subprocess
        p = subprocess.run(
            ['openssl', 'enc', '-d', '-aes-128-ecb', '-K', key.hex(), '-nosalt', '-nopad'],
            input=data, stdout=subprocess.PIPE, check=True
        )
        return p.stdout

def derive_sector1_key(uid: bytes) -> bytes:
    uid16 = uid * 4
    enc = aes_ecb_encrypt(U_KEY, uid16)
    return enc[:6]

def build_spool_payload(material_id: str, color_hex: str, length_code: str,
                        serial: str = "000001", printer: str = "K2",
                        vendor: str = "0276", batch: str = "A2", date: str = "AB124") -> Tuple[bytes, bytes]:
    s1_str = f"{date[:5]:<5}{vendor[:4]:<4}{batch[:2]:<2}1{material_id[:5]:<5}0{color_hex[:6]:<6}{length_code[:4]:<4}{serial[:6]:<6}00000000000000"
    s1_bytes = s1_str.encode('ascii')[:48]
    s2_bytes = printer.encode('ascii').ljust(48, b' ')[:48]
    return s1_bytes, s2_bytes

def generate_nfc_file(output_path: str, material_id: str, color_hex: str,
                      length_code: str = "0330", printer: str = "K2",
                      serial: str = None, uid: bytes = None):
    if serial is None:
        serial = f"{random.randint(100000, 999999):06d}"
    if uid is None:
        uid = bytes([random.randint(1, 254) for _ in range(4)])
        if uid[0] == 0x88: uid = b'\x12' + uid[1:]

    key_a = derive_sector1_key(uid)
    s1_plain, s2_plain = build_spool_payload(material_id, color_hex, length_code, serial, printer)
    s1_enc = aes_ecb_encrypt(D_KEY, s1_plain)

    bcc = uid[0] ^ uid[1] ^ uid[2] ^ uid[3]
    b0 = uid + bytes([bcc, 0x08, 0x04, 0x00, 0xE1, 0x10, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00])

    blocks = {}
    blocks[0] = b0
    blocks[1] = bytes(16)
    blocks[2] = bytes(16)
    # Sector 0 trailer
    blocks[3] = bytes([0xFF]*6 + [0xFF, 0x07, 0x80, 0x69] + [0xFF]*6)

    # Sector 1 (blocks 4, 5, 6 encrypted data)
    blocks[4] = s1_enc[0:16]
    blocks[5] = s1_enc[16:32]
    blocks[6] = s1_enc[32:48]
    # Sector 1 trailer (derived keys)
    blocks[7] = key_a + bytes([0xFF, 0x07, 0x80, 0x69]) + key_a

    # Sector 2 (blocks 8, 9, 10 plaintext printer model)
    blocks[8] = s2_plain[0:16]
    blocks[9] = s2_plain[16:32]
    blocks[10] = s2_plain[32:48]
    # Sector 2 trailer
    blocks[11] = bytes([0xFF]*6 + [0xFF, 0x07, 0x80, 0x69] + [0xFF]*6)

    # Sectors 3 to 15 (blank)
    for s in range(3, 16):
        for b in range(3):
            blocks[s * 4 + b] = bytes(16)
        blocks[s * 4 + 3] = bytes([0xFF]*6 + [0xFF, 0x07, 0x80, 0x69] + [0xFF]*6)

    with open(output_path, "w") as f:
        f.write("Filetype: Flipper NFC device\n")
        f.write("Version: 4\n")
        f.write("# Device type can be ISO14443-3A, ISO14443-4A, etc\n")
        f.write("Device type: Mifare Classic\n")
        f.write(f"UID: {' '.join(f'{b:02X}' for b in uid)}\n")
        f.write("ATQA: 00 04\n")
        f.write("SAK: 08\n")
        f.write("Mifare Classic type: 1K\n")
        f.write("Data content:\n")
        for i in range(64):
            f.write(f"Block {i}: {' '.join(f'{b:02X}' for b in blocks[i])}\n")

    print(f"[+] Successfully generated Flipper NFC spool file: {output_path}")

def decode_nfc_file(file_path: str):
    if not os.path.isfile(file_path):
        print(f"[-] File not found: {file_path}")
        return

    blocks = {}
    uid = None
    with open(file_path, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("UID:"):
                uid = bytes.fromhex(line.split("UID:")[1].strip())
            elif line.startswith("Block ") and ":" in line:
                parts = line.split(":")
                b_num = int(parts[0].replace("Block", "").strip())
                b_bytes = bytes.fromhex(parts[1].strip())
                blocks[b_num] = b_bytes

    if 4 not in blocks or 5 not in blocks or 6 not in blocks:
        print("[-] Missing Sector 1 blocks in NFC file.")
        return

    cipher = blocks[4] + blocks[5] + blocks[6]
    plain_s1 = aes_ecb_decrypt(D_KEY, cipher)
    plain_s2 = (blocks.get(8, b'') + blocks.get(9, b'') + blocks.get(10, b''))

    try:
        s1_str = plain_s1.decode('ascii', errors='replace')
        s2_str = plain_s2.decode('ascii', errors='replace').strip()
    except Exception:
        print("[-] Failed to decode ASCII string.")
        return

    date = s1_str[0:5]
    vendor = s1_str[5:9]
    batch = s1_str[9:11]
    mat_id = s1_str[12:17]
    color = s1_str[18:24]
    length = s1_str[24:28]
    serial = s1_str[28:34]

    mat_match = next((m for m in MATERIALS if m[0] == mat_id), None)
    mat_name = f"{mat_match[1]} {mat_match[2]} ({mat_match[3]})" if mat_match else f"ID {mat_id}"

    weight_match = next((k for k, v in WEIGHT_MAP.items() if v[0] == length), f"{length}m")

    print("==========================================")
    print(" Creality CFS RFID Spool Tag Decoded")
    print("==========================================")
    if uid:
        print(f" UID:            {' '.join(f'{b:02X}' for b in uid)}")
        print(f" Derived Key A:  {derive_sector1_key(uid).hex().upper()}")
    print(f" Material:       {mat_name} [ID: {mat_id}]")
    print(f" Color:          #{color}")
    print(f" Spool Size:     {weight_match}")
    print(f" Serial:         {serial}")
    print(f" Batch / Date:   {batch} / {date}")
    print(f" Vendor ID:      {vendor}")
    print(f" Printer Model:  {s2_str}")
    print("==========================================")

def main():
    parser = argparse.ArgumentParser(description="Creality CFS RFID Spool Tool for Flipper Zero")
    subparsers = parser.add_subparsers(dest="cmd")

    gen_p = subparsers.add_parser("generate", help="Generate a Flipper Zero .nfc spool file")
    gen_p.add_argument("-m", "--material", default="01001", help="Material ID (e.g. 01001 for Hyper PLA)")
    gen_p.add_argument("-c", "--color", default="FFFFFF", help="RGB hex color (e.g. FFFFFF)")
    gen_p.add_argument("-w", "--weight", default="1 KG", choices=list(WEIGHT_MAP.keys()), help="Spool size")
    gen_p.add_argument("-p", "--printer", default="K2", choices=["K2", "K1", "HI"], help="Printer model")
    gen_p.add_argument("-s", "--serial", default=None, help="6-digit serial number")
    gen_p.add_argument("-o", "--output", default="spool.nfc", help="Output .nfc file path")

    dec_p = subparsers.add_parser("decode", help="Decode a Flipper Zero .nfc spool file")
    dec_p.add_argument("file", help="Path to .nfc file")

    subparsers.add_parser("list", help="List supported materials")

    args = parser.parse_args()

    if args.cmd == "generate":
        len_code = WEIGHT_MAP.get(args.weight, ("0330", 330))[0]
        generate_nfc_file(args.output, args.material, args.color, len_code, args.printer, args.serial)
    elif args.cmd == "decode":
        decode_nfc_file(args.file)
    elif args.cmd == "list":
        print(f"{'ID':<8} {'Brand':<12} {'Name':<24} {'Type':<10}")
        print("-" * 56)
        for m in MATERIALS:
            print(f"{m[0]:<8} {m[1]:<12} {m[2]:<24} {m[3]:<10}")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
