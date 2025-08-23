# Flipper Share - direct file transfer between flippers

## Overview

Flipper Share is a file sharing app for Flipper Zero.

It allows to send any file over a Sub-GHz directly from one Flipper Zero to another without any additional hardware, cables, smartphones, computers, internet connection and magic needed.

Features:

- Works from out of the box on any Flipper Zero, the simplest possible way to transfer files directly
- Multiple receivers supported simultaneously and works just fine (broadcast)
- Continuation of download / auto retries in case of packet loss is guaranteed at the protocol level
- File size tested is up to 1 MB transfered successfully, without corruption
- Actual speed is around 800 bytes/sec, that allows to send average `.fap` file less than in 1 minute
- No pairing or session establishment needed
- No encryption, anyone nearby can receive the file, please don't send sensitive data

## Notes

Please feel free to open an issues and PRs if you have any ideas or found bugs.

Source code: https://github.com/lomalkin/flipper-zero-apps/tree/dev/flipper_share


# Flipper Share protocol

## Packet structure

Packet is 60 bytes long, due to Flipper CC1101 limitations.

- `version`: 1 byte
- `tx_id`: 1 byte
- `packet_type`: 1 byte
- `payload`: [56] bytes
- `crc`: 1 byte

Payloads for different types of packets:

- 0x01: `announce`
    - `file_name`         # 36 bytes, zero-padded string
    - `file_size` [4]     # uint32_t
    - `file_hash` [16]    # md5

- 0x02: `request range`
    - `start` [4]         # uint32_t
    - `end` [4]           # uint32_t
    - zero padding [48]

- 0x03: `data`
    - `block_num` [4]    # uint32_t
    - `block_data` [52]  # 48 bytes of data

## Example session

**Sender:**
- User selects a file to share.
- The sender prepares the `announce` packet with file metadata and random `tx_id`.
- If there are no other communications:
    - The sender sends the `announce` packet periodically (every 3 sec)
- When received a `request range` packet, the sender:
    - Checks if the `tx_id` matches the expected one.
    - Checks if the `start` and `end` are within the file size.
    - Sends `data` packets for each block in the requested range.
        - Each `data` packet contains a `block_num` and `block_data`.
        - The sender continues sending data packets until all requested blocks are sent.

**Receiver:**
- The receiver listens for `announce` packets.
- When received an `announce` packet, the receiver displays the file information.
    - Receiver locks to first valid received announce's `tx_id`.
    - Saves `file_name`, `file_size`, and `file_hash` to internal state.
    - Allocates file `file_name` in output directory.
    - Creates map of received blocks (count calculated from `file_size` and actual block size).
- If there are no other communications
    - The receiver sends a `request range` packet to the sender.
        - Default range is from 0  to `file_size` bytes (full file).
- When received a `data` packet, the receiver:
    - Checks if the `tx_id` matches the expected one.
    - Checks if the `block_num` is in range.
    - Saves the block data to the file.

