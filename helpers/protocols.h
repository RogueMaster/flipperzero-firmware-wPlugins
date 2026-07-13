#pragma once

#include <stdint.h>

/* The three protocols Rosetta explains. The order here is the order they appear
 * in the main menu and is used to index title/step tables. */
typedef enum {
    ProtocolMifare, // Mifare Classic authentication (13.56 MHz NFC)
    ProtocolModulation, // OOK & PSK modulation (Sub-GHz)
    ProtocolOneWire, // 1-Wire / iButton (Dallas)
    ProtocolCount,
} RosettaProtocol;

/* Short menu/title name, e.g. "Mifare Auth". */
const char* protocol_name(RosettaProtocol p);

/* One-line subtitle for the protocol menu header. */
const char* protocol_tagline(RosettaProtocol p);

/* Number of animated steps in a protocol's walkthrough. */
uint8_t protocol_step_count(RosettaProtocol p);

/* Title of a given walkthrough step (chapter heading). */
const char* protocol_step_title(RosettaProtocol p, uint8_t step);

/* Caption shown under the animation for a given step. */
const char* protocol_step_caption(RosettaProtocol p, uint8_t step);
