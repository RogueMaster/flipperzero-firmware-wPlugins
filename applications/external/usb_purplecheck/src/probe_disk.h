// SPDX-License-Identifier: GPL-3.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PC_BLOCK_SIZE    512u
#define PC_BLOCKS        2048u
#define PC_ROOT_START    13u
#define PC_DATA_START    15u
#define PC_TOKEN_SIZE    19u
#define PC_PAYLOAD_BYTES 4096u
#define PC_SEND_START    16u
#define PC_SEND_BLOCKS   8u
typedef bool (*PcSectorRead)(void* context, uint32_t lba, uint8_t out[PC_BLOCK_SIZE]);
typedef enum {
    PcReturnNone,
    PcReturnMatch,
    PcReturnChanged,
    PcReturnIoError
} PcReturnResult;
void pc_disk_sector(uint32_t lba, const char* token, uint8_t out[PC_BLOCK_SIZE]);
void pc_disk_sector_config(uint32_t lba, const char* token, bool files, uint8_t out[PC_BLOCK_SIZE]);
void pc_payload_sector(const char* token, uint32_t index, uint8_t out[PC_BLOCK_SIZE]);
PcReturnResult
    pc_verify_return(PcSectorRead read, void* context, const char* token, uint32_t* crc);
bool pc_token_valid(const char* token);
