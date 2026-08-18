#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// A 1-Wire ROM code is a 64-bit number burned in at the factory, and it is the
// only thing most 1-Wire parts will tell you about themselves. It can be
// replayed by any microcontroller, so finding one proves a device is present
// and says nothing about who made it. This module therefore never claims a
// 1-Wire part is genuine.
//
// What it can honestly do is catch the substitution that actually happens in
// the wild: the family code — the low byte of the ROM — is what determines the
// part's whole command set and register layout, so a DS18S20 or DS1822 sold as
// a DS18B20 is visible here as a different part, not a suspicion.

#define ONEWIRE_MAX_FOUND 8

typedef enum {
    OneWireRoleTemperature,
    OneWireRoleMemory,
    OneWireRoleSwitch,
    OneWireRoleOther,
} OneWireRole;

// One row of the family table: what a given family code means. The table is
// also what the app can honestly claim to recognise on this bus, so it is
// public — a count that leaves it out under-reports the app's own coverage.
typedef struct {
    uint8_t family;
    const char* name;
    const char* kind;
    OneWireRole role;
} OneWireFamily;

size_t onewire_family_count(void);
const OneWireFamily* onewire_family_get(size_t index); // NULL when out of range

typedef struct {
    uint8_t rom[8]; // [0] family code, [1..6] serial, [7] CRC8
    bool crc_ok; // ROM CRC8 checks out
    const char* name; // decoded part, NULL when the family is unknown
    const char* kind; // what that part does, in plain words
    OneWireRole role;

    // Filled in for temperature families only, by actually running a
    // conversion — presence is cheap to fake, a working measurement is not.
    bool measured;
    bool scratch_crc_ok;
    float temp_c;
} OneWireDevice;

typedef enum {
    OneWireBusEmpty, // reset pulse got no presence response
    OneWireBusShorted, // line held low: shorted to ground, or no pull-up
    OneWireBusOk,
} OneWireBusState;

typedef struct {
    OneWireBusState state;
    OneWireDevice found[ONEWIRE_MAX_FOUND];
    uint8_t count;
    bool overflow; // more devices on the bus than we can list
} OneWireScanResult;

// Runs a full ROM search and, for temperature parts, one conversion each.
// Blocking: a few milliseconds on an empty bus, up to ~800 ms per temperature
// sensor found. Must be called from a normal thread, never from a timer or
// inside a with_view_model block — the protocol is bit-banged with interrupts
// masked around each slot.
//
// abort may be NULL. When it is not, setting it true cuts the measurement pass
// short so leaving the screen does not have to wait out a conversion.
void onewire_worker_scan(OneWireScanResult* out, const volatile bool* abort);
