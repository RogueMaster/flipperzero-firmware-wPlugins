#include "protocols.h"

typedef struct {
    const char* title;
    const char* caption;
} Step;

/* --- Mifare Classic authentication (13.56 MHz) --- */
static const Step mifare_steps[] = {
    {"RF Field & ATQA", "Field powers the card. It wakes: ATQA."},
    {"Anticollision", "UID cascade picks one. SAK = Classic 1K."},
    {"Auth Request", "Reader: AUTH block 7 with Key A."},
    {"3-Pass Crypto1", "Card nT, reader {nR,aR}, card aT."},
    {"Broken Since 2008", "Weak nonce breaks Crypto1: keys in seconds."},
};

/* --- OOK & PSK modulation (Sub-GHz) --- */
static const Step modulation_steps[] = {
    {"The Carrier", "A pure carrier sine. No data yet."},
    {"OOK Keying", "Bit 1 = carrier ON, bit 0 = OFF."},
    {"PSK Shifting", "Each bit can flip the phase 180 deg."},
    {"Recovering Bits", "Slice envelope or track phase -> bits."},
};

/* --- 1-Wire / iButton (Dallas) --- */
static const Step onewire_steps[] = {
    {"One Wire", "One line, idle-high. Slave steals power."},
    {"Reset & Presence", "Master low 480us; slave pulses back."},
    {"Write Slots", "Long-low = 0; short-low + release = 1."},
    {"Read Slots", "Master blips; samples low=0 / high=1."},
    {"ROM: 64 Bits", "8b family + 48b serial + 8b CRC."},
};

const char* protocol_name(RosettaProtocol p) {
    switch(p) {
    case ProtocolMifare:
        return "Mifare Auth";
    case ProtocolModulation:
        return "OOK & PSK";
    case ProtocolOneWire:
        return "1-Wire";
    default:
        return "?";
    }
}

const char* protocol_tagline(RosettaProtocol p) {
    switch(p) {
    case ProtocolMifare:
        return "Crypto1 handshake";
    case ProtocolModulation:
        return "Sub-GHz on the wire";
    case ProtocolOneWire:
        return "Dallas / iButton";
    default:
        return "";
    }
}

uint8_t protocol_step_count(RosettaProtocol p) {
    switch(p) {
    case ProtocolMifare:
        return sizeof(mifare_steps) / sizeof(mifare_steps[0]);
    case ProtocolModulation:
        return sizeof(modulation_steps) / sizeof(modulation_steps[0]);
    case ProtocolOneWire:
        return sizeof(onewire_steps) / sizeof(onewire_steps[0]);
    default:
        return 0;
    }
}

static const Step* step_of(RosettaProtocol p, uint8_t step) {
    const Step* table;
    uint8_t n = protocol_step_count(p);
    switch(p) {
    case ProtocolMifare:
        table = mifare_steps;
        break;
    case ProtocolModulation:
        table = modulation_steps;
        break;
    case ProtocolOneWire:
        table = onewire_steps;
        break;
    default:
        return 0;
    }
    if(step >= n) step = n ? n - 1 : 0;
    return &table[step];
}

const char* protocol_step_title(RosettaProtocol p, uint8_t step) {
    const Step* s = step_of(p, step);
    return s ? s->title : "";
}

const char* protocol_step_caption(RosettaProtocol p, uint8_t step) {
    const Step* s = step_of(p, step);
    return s ? s->caption : "";
}
