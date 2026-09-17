#pragma once

#include "dfc.h"
#include "dfc_emulator.h"

#define DFC_EMULATOR_TAG         "DfcEmulator"
#define DFC_ISO14443_4A_CID_MASK 0x08
#define DFC_ISO14443_4A_NAD_MASK 0x04

DfcApplication* dfc_emulator_current_app(DfcEmulator* emulator);
bool dfc_emulator_accepts_auth_cipher(DfcEmulator* emulator, uint8_t cipher);
size_t dfc_emulator_key_len(DfcEmulator* emulator);
size_t dfc_emulator_num_keys(DfcEmulator* emulator);
uint8_t* dfc_emulator_key(DfcEmulator* emulator, uint8_t key_no);
uint8_t* dfc_emulator_key_version(DfcEmulator* emulator, uint8_t key_no);

uint32_t dfc_emulator_read_uint24_le(const uint8_t* data);
uint16_t dfc_emulator_crc16_iso14443(const uint8_t* data, size_t len);
uint32_t dfc_emulator_crc32(const uint8_t* data, size_t len);
void dfc_emulator_d40_receive_plain(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* encrypted,
    size_t encrypted_len,
    uint8_t* plain);
uint8_t dfc_emulator_des_key_version(const uint8_t* key);


bool dfc_emulator_handle_iso7816_select(
    DfcEmulator* emulator,
    const uint8_t* apdu,
    size_t apdu_len,
    DfcByteBuf* tx_buffer,
    size_t prefix_len,
    bool* app_selected);

const char* dfc_desfire_command_name(uint8_t cmd, const DfcEmulator* emulator);
const char* dfc_iso_dep_control_frame_name(uint8_t pcb);
