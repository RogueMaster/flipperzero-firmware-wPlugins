#pragma once

#ifndef ASN_EMIT_DEBUG
#define ASN_EMIT_DEBUG 0
#endif

#include <nfc/helpers/iso13239_crc.h>
#include <optimized_ikeys.h>
#include <optimized_cipher.h>
#include <lib/nfc/nfc.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Seader Seader;
typedef struct SeaderPollerContainer SeaderPollerContainer;

#include <Payload.h>

#define ExternalApplicationA 0x44
#define NFCInterface         0x14
#define SAMInterface         0x0a

NfcCommand seader_worker_card_detect(
    Seader* seader,
    uint8_t sak,
    uint8_t* atqa,
    const uint8_t* uid,
    uint8_t uid_len,
    uint8_t* ats,
    uint8_t ats_len);

void seader_send_nfc_rx(Seader* seader, uint8_t* buffer, size_t len);
void seader_send_no_card_detected(Seader* seader);
bool seader_sam_can_accept_card(const Seader* seader);
bool seader_sam_has_active_card(const Seader* seader);
void seader_sam_force_idle_for_recovery(Seader* seader);

bool seader_process_success_response_i(
    Seader* seader,
    uint8_t* apdu,
    size_t len,
    bool online,
    SeaderPollerContainer* spc);

bool seader_worker_send_process_snmp_message(
    Seader* seader,
    const uint8_t* message,
    size_t message_len);
