#pragma once

// SAM Reader emulation: presents the attached HID SAM to a USB host as a
// CCID contact smart-card reader (ACR39U-style), relaying host APDUs to the
// SAM over Seader's existing T=1 / CCID-XfrBlock path.
//
// Topology:
//   USB host (PC/SC, e.g. PM3_SAM_App / Bridge tools)
//        | USB CCID (this module presents the reader)
//   Flipper (Seader)
//        | UART -> reader chip / raw ISO7816
//   HID SAM  (appears as the card in slot 0)

#include <furi.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Seader Seader;

// Buffers sized to Seader's existing SAM frame ceiling.
#define SEADER_READER_MAX_APDU   272
#define SEADER_READER_TIMEOUT_MS 4000U
#define SEADER_READER_NAME_MAX   32

// Default identity presented on the USB bus. Edit these to change how the
// Flipper advertises itself. The product string is what surfaces (with a slot
// suffix) as the PC/SC reader name that host tools match against.
//
// Neutral VID/PID: keep these OFF a real vendor's IDs so Windows binds the
// inbox Microsoft CCID class driver (on USB\Class_0B) instead of a vendor
// driver. The inbox driver then uses the iProduct string below as the PC/SC
// reader name, which is what name-matching host tools key on.
//
// Do NOT use ACS's real VID (0x072F): Windows loads the ACS driver, renames
// the device generically ("ACS CCID USB Reader"), and refuses to initialize a
// non-genuine ACS device -- host apps then cannot connect.
//
// The manufacturer + product strings are chosen so the PC/SC reader name
// matches a genuine ACR39U ("ACS ACR39U ICC Reader 0"). If your host tool
// matches a different exact name, edit the product string to suit.
#define SEADER_READER_DEFAULT_VID     0x1209U // pid.codes generic
#define SEADER_READER_DEFAULT_PID     0x5346U
#define SEADER_READER_DEFAULT_MANUF   "ACS"
#define SEADER_READER_DEFAULT_PRODUCT "ACR39U ICC Reader"

typedef struct SeaderReader SeaderReader;

// Runtime-configurable identity.
typedef struct {
    uint16_t vid;
    uint16_t pid;
    char manufacturer[SEADER_READER_NAME_MAX];
    char product[SEADER_READER_NAME_MAX];
} SeaderReaderConfig;

// Fill cfg with the compiled-in defaults.
void seader_reader_config_default(SeaderReaderConfig* cfg);

// Load/save the persisted, user-editable reader identity (product name + PID)
// into seader->reader_product / seader->reader_pid. Load applies compiled-in
// defaults when no config file exists.
void seader_reader_settings_load(Seader* seader);
void seader_reader_settings_save(Seader* seader);

// Worker-thread entry: brings up the USB CCID reader, then blocks relaying
// APDUs until the worker leaves SeaderWorkerStateReaderEmulation. Tears the
// USB reader down and restores the previous USB config on exit.
void seader_reader_run(Seader* seader);

// Called from the UART-RX path (seader_worker_process_sam_message) when a SAM
// response arrives while in reader-emulation mode. Hands the response back to
// the waiting USB callback. Returns true if consumed.
bool seader_reader_sam_response(Seader* seader, uint8_t* apdu, uint32_t len);

// Number of APDUs relayed since the current session started (for the UI).
uint32_t seader_reader_apdu_count(Seader* seader);

// Product/reader name currently advertised (for the UI).
const char* seader_reader_active_name(Seader* seader);

// Diagnostics for the UI: total CCID commands seen + last command name.
uint32_t seader_reader_ccid_count(void);
const char* seader_reader_ccid_last_name(void);
void seader_reader_ccid_debug(uint16_t* atr_len, int32_t* tx_last);
