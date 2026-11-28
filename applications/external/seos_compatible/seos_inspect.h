#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <furi.h>
#include <lib/toolbox/bit_buffer.h>

#include "seos_iso14443_4.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inspection walks a reader through the NfcPassThrough applet, polling its IPC
// for version, memory and credential-table data. These are the ordered steps.
typedef enum {
    SeosInspectIdle,
    SeosInspectOpSelected,
    SeosInspectSelected,
    SeosInspectSamVer,
    SeosInspectCoreVer,
    SeosInspectPersistMem,
    SeosInspectCredConfig,
    SeosInspectSuspendEnd,
    SeosInspectDone,
} SeosInspectState;

#define SEOS_INSPECT_CRED_COUNT     8
// A table entry: a four-byte hivecode, then its card type in the eighth byte.
#define SEOS_INSPECT_CRED_ENTRY_LEN 9

typedef struct {
    SeosInspectState state;
    bool cmd_sent;
    uint8_t poll_count;

    uint16_t sam_ver;
    uint16_t core_ver;
    uint32_t persist_mem;
    uint8_t segment_count;
    uint8_t assembly_count;

    bool cred_enabled[SEOS_INSPECT_CRED_COUNT];
    bool seos_enabled;
    bool cred_parsed;

    // The credential table arrives split across PUT DATA fragments and is read
    // as it comes rather than kept: a group header or one entry is the most
    // that is ever held, so the table can be any length.
    bool cred_started; // The first fragment, which declares the length, was seen.
    size_t cred_declared; // Length the table declares.
    size_t cred_received; // How much of that has arrived.
    uint8_t cred_proto; // Protocol of the group being read.
    uint8_t cred_remaining; // Entries of that group still to come.
    uint8_t cred_part[SEOS_INSPECT_CRED_ENTRY_LEN]; // Header or entry in progress.
    uint8_t cred_part_len;

    // The block protocol under the commands. We run it ourselves on a raw 3A
    // listener because the firmware 4A path mishandles the reader's chained
    // credential-table response and makes it drop the field.
    SeosIso14443_4Card card;
} SeosInspectData;

// What the caller should surface after handling a frame.
typedef enum {
    SeosInspectEventNone,
    SeosInspectEventAidSelected,
    SeosInspectEventDone,
} SeosInspectEvent;

// Display name of credential item i (0..SEOS_INSPECT_CRED_COUNT-1).
const char* seos_inspect_cred_name(size_t i);

void seos_inspect_reset(SeosInspectData* inspect);

// The field went away: a command that was only partly received is forgotten.
void seos_inspect_field_off(SeosInspectData* inspect);

// Handle one raw 3A frame (PCB + INF, no CRC): run the T=CL block protocol and,
// for a complete APDU, the inspection step. Resets tx and writes the wire
// response (PCB + INF, no CRC) into it, leaving it empty when nothing should be
// sent. The caller appends the CRC.
void seos_inspect_handle_frame(
    SeosInspectData* inspect,
    const uint8_t* frame,
    size_t frame_len,
    BitBuffer* tx,
    SeosInspectEvent* event);

#ifdef __cplusplus
}
#endif
