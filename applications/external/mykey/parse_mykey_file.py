#!/usr/bin/env python3
"""
COGES MyKey File Parser
Parses .myk files created by the COGES MyKai Flipper Zero application.
 - a luhf shitscript
"""

import sys
from datetime import datetime

def bswap32(val):
    """Byte swap 32-bit value"""
    return ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF)

def encode_decode_block(block):
    """libmikai encode_decode function"""
    block ^= ((block & 0x00C00000) << 6 | (block & 0x0000C000) << 12 | (block & 0x000000C0) << 18 |
              (block & 0x000C0000) >> 6 | (block & 0x00030000) >> 12 | (block & 0x00000300) >> 6)
    block ^= ((block & 0x30000000) >> 6 | (block & 0x0C000000) >> 12 | (block & 0x03000000) >> 18 |
              (block & 0x00003000) << 6 | (block & 0x00000030) << 12 | (block & 0x0000000C) << 6)
    block ^= ((block & 0x00C00000) << 6 | (block & 0x0000C000) << 12 | (block & 0x000000C0) << 18 |
              (block & 0x000C0000) >> 6 | (block & 0x00030000) >> 12 | (block & 0x00000300) >> 6)
    return block & 0xFFFFFFFF

def parse_mykey_file(filename):
    """Parse a .myk file and display its contents"""
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        return False
    except Exception as e:
        print(f"Error reading file: {e}")
        return False

    # Verify header
    if not lines[0].startswith("COGES_MYKEY_V1"):
        print("Error: Invalid file format (missing header)")
        return False

    # Parse UID
    uid_line = lines[1].strip()
    if not uid_line.startswith("UID:"):
        print("Error: Invalid file format (missing UID)")
        return False
    uid = int(uid_line.split(":")[1].strip(), 16)

    # Parse encryption key
    key_line = lines[2].strip()
    if not key_line.startswith("ENCRYPTION_KEY:"):
        print("Error: Invalid file format (missing encryption key)")
        return False
    encryption_key = int(key_line.split(":")[1].strip(), 16)

    # Parse blocks
    blocks = {}
    for line in lines[3:]:
        line = line.strip()
        if line.startswith("BLOCK_"):
            parts = line.split(":")
            block_num = int(parts[0].split("_")[1])
            block_val = int(parts[1].strip(), 16)
            blocks[block_num] = block_val

    if len(blocks) != 128:
        print(f"Warning: Expected 128 blocks, found {len(blocks)}")

    # Display information
    print("=" * 60)
    print(" COGES MyKey Card Information")
    print("=" * 60)
    print(f"\nUID: 0x{uid:016X}")
    print(f"Encryption Key: 0x{encryption_key:08X}")

    # Serial number (block 0x07 in BCD format)
    serial = blocks.get(0x07, 0)
    print(f"Serial Number: {serial:08X}")

    # Decode credit from block 0x21
    block_21 = blocks.get(0x21, 0)
    current_credit_raw = block_21 ^ encryption_key
    current_credit_decoded = encode_decode_block(current_credit_raw)
    credit_value = current_credit_decoded & 0xFFFF
    print(f"\nCurrent Credit: {credit_value} cents ({credit_value/100:.2f} EUR)")

    # Operation counter (block 0x12, lower 24 bits)
    block_12 = blocks.get(0x12, 0)
    op_count = block_12 & 0x00FFFFFF
    print(f"Operations: {op_count}")

    # Check if reset
    block_18 = blocks.get(0x18, 0)
    block_19 = blocks.get(0x19, 0)
    is_reset = (block_18 == 0x8FCD0F48 and block_19 == 0xC0820007)
    print(f"Status: {'Reset' if is_reset else 'Active'}")

    # Parse transaction history
    block_3C = blocks.get(0x3C, 0xFFFFFFFF)
    block_07 = blocks.get(0x07, 0)

    if block_3C != 0xFFFFFFFF:
        block_3C_decrypted = block_3C ^ block_07
        starting_offset = ((block_3C_decrypted & 0x30000000) >> 28) | \
                         ((block_3C_decrypted & 0x00100000) >> 18)

        if starting_offset < 8:
            # Count transactions
            transactions = []
            for i in range(8):
                txn_block = blocks.get(0x34 + ((starting_offset + i) % 8), 0xFFFFFFFF)
                if txn_block == 0xFFFFFFFF:
                    break

                day = txn_block >> 27
                month = (txn_block >> 23) & 0xF
                year = 2000 + ((txn_block >> 16) & 0x7F)
                credit = txn_block & 0xFFFF

                transactions.append({
                    'date': f"{day:02d}/{month:02d}/{year}",
                    'credit': credit
                })

            if transactions:
                print("\n" + "=" * 60)
                print(" Transaction History (Newest First)")
                print("=" * 60)
                for i, txn in enumerate(reversed(transactions), 1):
                    print(f"{i}. {txn['date']} - {txn['credit']} cents ({txn['credit']/100:.2f} EUR)")

    # Display interesting blocks
    print("\n" + "=" * 60)
    print(" Key Blocks")
    print("=" * 60)
    interesting_blocks = [0x06, 0x07, 0x12, 0x18, 0x19, 0x21, 0x23, 0x25, 0x27, 0x3C]
    for block_num in interesting_blocks:
        if block_num in blocks:
            print(f"Block 0x{block_num:02X}: 0x{blocks[block_num]:08X}")

    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 parse_mykey_file.py <file.myk>")
        print("\nThis script parses COGES MyKey files created by the Flipper Zero app")
        print("and displays card information in a human-readable format.")
        sys.exit(1)

    filename = sys.argv[1]
    if parse_mykey_file(filename):
        print("\n" + "=" * 60)
        print(" Parsing completed successfully")
        print("=" * 60)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()
#culo