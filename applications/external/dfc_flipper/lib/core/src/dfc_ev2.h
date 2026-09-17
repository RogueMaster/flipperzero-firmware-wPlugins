#pragma once

#include "dfc_emulator.h"

typedef enum {
    DfcEv2CommandPlain,
    DfcEv2CommandSecured,
    DfcEv2CommandInvalid,
} DfcEv2CommandSecurity;

DfcEv2CommandSecurity dfc_ev2_prepare_command(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len,
    uint8_t* clear_command,
    size_t clear_capacity,
    size_t* clear_len);

bool dfc_ev2_protect_response(
    DfcEmulator* emulator,
    const uint8_t* clear_command,
    size_t clear_command_len,
    const uint8_t* clear_response,
    size_t clear_response_len,
    uint8_t* secured_response,
    size_t secured_capacity,
    size_t* secured_len);

bool dfc_ev2_verify_chained_command_mac(
    const DfcEmulator* emulator,
    uint8_t instruction,
    const uint8_t* data,
    size_t data_len,
    const uint8_t mac[DFC_WIRE_MAC_LENGTH]);
