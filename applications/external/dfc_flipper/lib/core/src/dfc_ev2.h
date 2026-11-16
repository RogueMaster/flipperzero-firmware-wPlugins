#pragma once

#include "dfc_emulator.h"

typedef enum {
    DfcEv2CommandPlain,
    DfcEv2CommandPlainCounted,
    DfcEv2CommandSecured,
    DfcEv2CommandFull,
    DfcEv2CommandInvalid,
} DfcEv2CommandSecurity;

DfcEv2CommandSecurity dfc_ev2_prepare_command(
    DfcEmulator* emulator,
    const uint8_t* command,
    size_t command_len,
    uint8_t* clear_command,
    size_t clear_capacity,
    size_t* clear_len);

// Protects a response directly in the caller's destination. `clear_response`
// may alias `secured_response`; partial overlap is supported. A cipher or MAC
// failure wipes any bytes staged in the destination, sets `secured_len` to
// zero, and terminates the EV2 session so the command cannot be replayed in it.
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
