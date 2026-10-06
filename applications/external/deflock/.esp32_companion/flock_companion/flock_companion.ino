// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/*
 * Flock Companion - universal ESP32 Wi-Fi sniffer for the Flipper Zero
 * "Recon Site Survey" app.
 *
 * Runs on ANY ESP32 board exposed to the Flipper UART (Marauder hardware,
 * ReksLab Tri-Board, bare WROVER/WROOM, Xiao ESP32-S3, DevKitC, ...).
 * Puts the radio in promiscuous monitor mode, hops channels 1-13 (plus the 28
 * 5 GHz channels on an ESP32-C5, which has a dual-band radio), and reports
 * frames that look like Flock Safety / ALPR surveillance gear (by OUI, by
 * phone-home probe behaviour, and by SSID naming) over the serial link in a
 * simple line protocol the Flipper parses.
 *
 * Detection method and OUI list are from the open-source counter-surveillance
 * projects (colonelpanichacks/flock-you, 0xXyc/flock-you-wifi-recon,
 * nitekry/nite-oui-collection) and the DeFlock community. Passive recon only --
 * no deauth, no injection.
 *
 * Build: Arduino IDE or arduino-cli with the esp32 core. Select your board,
 * set Serial baud to 115200. No extra libraries required.
 *
 * Line protocol (newline-terminated, ASCII), TX to Flipper:
 *   FLOCKCO,1,<build>                      protocol/build banner on boot and "ver"
 *   SIGREV,<revision>                      production signature-table revision
 *   SIGTEST,<signature>,<hash>,<0|1>       response to the "sigtest" command
 *   S,<frames>,<hits>,<ch>,<deauth_rate>   status, ~1 Hz (deauth/disassoc per interval)
 *   D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>][,cls=a|x][,hid=1] detection
 *       mac : aabbccddeeff (lower hex, no separators)
 *       rssi: signed dBm
 *       ch  : 1-13 (2.4 GHz), or 36-177 (5 GHz, ESP32-C5 only)
 *       type: P=probe-req  B=beacon  R=probe-resp  O=other
 *       conf: 1=possible 2=likely 3=confirmed (ESP-side score)
 *       ssid: raw SSID with ',' and control chars stripped (may be empty)
 *       fp  : FNV-1a uint32 (8 lower-hex) of the probe's IE skeleton (B1) --
 *             a MAC-independent device-CLASS fingerprint; trailing field,
 *             older parsers ignore it. Only emitted for probe requests.
 *       cls : device class. 'a' = SoundThinking acoustic sensor, 'x' = Axon
 *             police equipment, 'g' = vendor-exclusive competitor gear (vendor
 *             known, kind not). Absent means ALPR camera, so the common case
 *             adds no bytes. The VENDOR is never sent -- the Flipper re-derives
 *             it from the MAC using its own copy of these tables. Trailing.
 *       hid : the AP beaconed WITHOUT an SSID (zero-length or all-NUL IE).
 *             Beacons/probe-responses only. An observation the Flipper reports
 *             but does NOT score -- hiding an SSID is also ordinary consumer
 *             router behaviour. Trailing, only emitted when true.
 *
 *       All three trailing key=value fields are optional and order-independent.
 *       Add one and you must also grow the field array in esp_parser.c.
 *   BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1][,sep=1]   BLE device
 *       cat   : 0 unknown 1 Flock/Raven 2 AirTag 3 Tile 4 SmartTag 5 FMDN
 *               7 Axon (SIG company id 0x034D, TASER International)
 *       mfghex: raw mfg-data hex (Flock 0x09C8 only) for serial decode; pure
 *               hex, no '='. Trailing, older parsers ignore.
 *       rv=1  : the device exposed a Raven-specific GATT service (0x3100-
 *               0x3500) -> positive acoustic-sensor (Raven) identification.
 *               Trailing, contains '=' so it's distinguishable from mfghex;
 *               only emitted when matched, older parsers ignore.
 *       sep=1 : an Apple Find My tracker advertised its separated-state payload.
 *               This is a protocol state marker, not proof of ownership or
 *               stalking; older parsers ignore it.
 *   DA,<bssid>,<ch>                        deauth/disassoc attack target (attributed)
 *   ATK,<kind>,<value>                     active attack-tool signature
 *       kind : probeflood  (abnormal probe-request rate)
 *              beaconflood (many DISTINCT beaconing BSSIDs/s: Marauder/Pineapple)
 *              blespam     (Apple/Samsung/Google pairing-advert flood)
 *       value: the count/rate measured. New line; older app builds ignore it.
 *   LOC,<rssi>                             Locator: live RSSI of the active target
 *                                          (signed dBm), streamed while homing.
 *   ACT,<op>,<status>[,<rssi>]              explicit tracker action result
 *       op: PING (one-shot GATT reachability) or RING (non-owner sound request)
 *   BAND,<2g|5g|all>,<channels>            ACK for the `band` command: the band
 *                                          actually in force and how many
 *                                          channels the sweep now covers. On a
 *                                          2.4-only radio this always answers
 *                                          2g, whatever was asked -- claiming
 *                                          5 GHz coverage the chip cannot
 *                                          provide would be a lie on the wire.
 *
 * RX from Flipper (commands, newline-terminated):
 *   scan   start reporting        stop   pause reporting
 *   ver    re-send banner         ch <n> lock to channel n (0 = hop)
 *   bootloader  enter UART download mode (software; no BOOT button needed)
 *   band <2g|5g|all>         pick which band(s) the hopper sweeps (C5 only;
 *                            a 2.4-only radio always ends up on 2g)
 *   locate <w|b> <mac> [ch]  stream LOC for a target (w=Wi-Fi, b=BLE; mac is
 *                            aabbccddeeff). "locate off" ends Locator mode, as
 *                            does any command that re-tasks the radio. The
 *                            read-only queries "survey", "surveyclear" and
 *                            "ver" are exempt: the app polls the survey every
 *                            10 s from a tick that runs in every scene, so
 *                            cancelling on those ended every hunt within
 *                            seconds of it starting.
 *   ble_ping <mac>            one-shot active GATT reachability check for a
 *                            validated tracker; replies ACT,PING,...
 *   ble_ring <mac>            separated-state non-owner sound request for an
 *                            Apple/Find My tracker; replies ACT,RING,...
 */

#include <Arduino.h>
#include <stdarg.h> // buf_appendf()
#include "soc/soc_caps.h" // SOC_GPIO_PIN_COUNT / SOC_GPIO_VALID_GPIO_MASK
// RTC_CNTL_FORCE_DOWNLOAD_BOOT, for the hands-free "bootloader" command.
//
// __has_include, NOT a plain include: this header does not exist on every
// target. The ESP32-C5 has no soc/rtc_cntl_reg.h at all and the compile died
// with "No such file or directory" -- caught by the core-3.x/C5 compat job,
// which exists for exactly this. Where the header is missing the macros are
// undefined, so the command's #else branch reports "cannot" instead, which is
// the honest answer on a part with no software download-boot anyway.
#if defined(__has_include)
#if __has_include("soc/rtc_cntl_reg.h")
#include "soc/rtc_cntl_reg.h"
#endif
#endif
#include "soc/spi_pins.h" // SPI_IOMUX_PIN_NUM_* -- this chip's flash pins
#include "soc/uart_pins.h" // U0TXD_GPIO_NUM / U0RXD_GPIO_NUM -- the Flipper link
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

/* ---- Bluetooth capability -------------------------------------------------
 *
 * The ESP32-S2 has NO Bluetooth radio at all. The Arduino core therefore ships
 * no BLE library for it and every BLE symbol below is simply absent, so this
 * sketch did not compile for that target at all -- it died on BLEAddress. That
 * is not a build we forgot to publish; the silicon cannot do Bluetooth.
 *
 * On such a part this now builds as a WI-FI-ONLY companion: probe/OUI camera
 * detection works exactly as it does anywhere else, and the BLE half of Flock
 * detection is gone permanently. Worth saying out loud, because an operator on
 * a Wi-Fi-only board who finds nothing cannot otherwise tell "there was no
 * camera" from "this board cannot see half of them".
 *
 * CONFIG_BT_ENABLED / CONFIG_BLUEDROID_ENABLED come from sdkconfig.h, already
 * pulled in by Arduino.h above.
 */
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)
#define FLOCK_HAS_BLE 1
#else
#define FLOCK_HAS_BLE 0
#endif

#if FLOCK_HAS_BLE
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#endif

#include <string>

#include "tracker_rules.h"

/* ---- Arduino-ESP32 core 2.x / 3.x compatibility ---------------------------
 *
 * Core 3.x (IDF 5.x) changed the BLE API in ways that break compilation
 * outright, not subtly. Reported by @h00die (issue #4) on core 3.3.11, which is
 * what a fresh Arduino install gets TODAY -- so before this shim, anyone
 * following our own README hit a wall of errors. Our CI pinned 2.0.17, so it
 * never saw any of it. A pin is not portability; it just hides the question.
 *
 * The three breaks:
 *
 *   1. BLEScan::start(secs, bool) returns BLEScanResults* in 3.x, by value in 2.x.
 *   2. getManufacturerData() / getName() / BLEUUID::toString() /
 *      BLEAddress::toString() return Arduino String in 3.x, std::string in 2.x.
 *   3. BLEAddress::getNative() returns const uint8_t* in 3.x, but uint8_t(*)[6]
 *      in 2.x -- so the 2.x code deref'd it once and 3.x gave back a single byte.
 *
 * Normalised to std::string here because the detection logic below does
 * substring work (find/rfind) that reads clearly in std::string and would have
 * to be rewritten for String. Conversion is LENGTH-PRESERVING on purpose:
 * manufacturer data is binary and can contain NUL bytes, so it is rebuilt with
 * the (pointer, length) constructor rather than treated as a C string -- a
 * strlen-style copy would silently truncate an advert at its first zero byte and
 * lose the Flock 0x09C8 payload we decode serials from.
 *
 * ESP_ARDUINO_VERSION_MAJOR is absent on very old cores; treat absent as 2.x.
 */
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define FLOCK_ARDUINO3 1
#else
#define FLOCK_ARDUINO3 0
#endif

#if FLOCK_ARDUINO3
/** Arduino String -> std::string, preserving embedded NULs. */
static inline std::string fstr(const String& s) {
    return std::string(s.c_str(), s.length());
}
/** 3.x hands back a pointer to the scan's internal results. */
#define FLOCK_SCAN(scan, secs) (*(scan)->start((secs), false))
/** Same, but `cont` keeps results accumulated from earlier slices. */
#define FLOCK_SCAN_CONT(scan, secs, cont) (*(scan)->start((secs), (cont)))
/** 3.x: already a flat pointer to the 6 address bytes. */
#if FLOCK_HAS_BLE
static inline const uint8_t* fble_addr_bytes(BLEAddress& a) {
    return a.getNative();
}
#endif
#else
/** 2.x already returns std::string; pass through so call sites stay identical. */
static inline std::string fstr(const std::string& s) {
    return s;
}
/** 2.x returns by value. */
#define FLOCK_SCAN(scan, secs) ((scan)->start((secs), false))
/** Same, but `cont` keeps results accumulated from earlier slices. */
#define FLOCK_SCAN_CONT(scan, secs, cont) ((scan)->start((secs), (cont)))
/** 2.x: uint8_t(*)[6], so one deref yields the uint8_t*. */
#if FLOCK_HAS_BLE
static inline const uint8_t* fble_addr_bytes(BLEAddress& a) {
    return *a.getNative();
}
#endif
#endif

// ---- Flock-associated OUI prefixes (30) ----------------------------------
// MUST stay byte-identical to flock_ouis[] in helpers/flock_db.c. There is no
// shared header (an Arduino sketch cannot include the app's), so editing one
// side alone would silently desync ESP-side `conf` scoring from the Flipper's.
// tools/check_oui_parity.py is a REQUIRED CI gate that catches exactly that.
// See flock_db.c for the provenance notes and the per-grade breakdown (contract
// manufacturer / flat-list orphan / weak upstream confidence).
//
// DEMOTED in v0.73: 48:27:ea (SAMSUNG) and a4:cf:12 (Espressif), both rated
// "low confidence, WiGLE crowdsource" upstream. They made a Samsung-based
// T-Mobile hotspot score LIKELY just for sending the wildcard probe every
// Wi-Fi client sends while scanning. They now live in docs/signatures.seed.json.
//
// RETRACTED UPSTREAM -- never re-add: f8:a2:d6 (hit on a Sony Media Player),
// 6c:cd:d6, 94:2a:6f, f4:e2:c6, cc:cc:cc, 00:0c:e7. f8:a2:d6 in particular has
// been removed TWICE: dropped for v0.44, silently re-added here by 93beede
// (2026-08-05) during a reflow, and shipped in v0.67-v0.71 scoring `conf=2` on
// any wildcard probe (see the ladder below). The parity gate did not catch it
// because that commit drifted this table and flock_db.c identically. The gate
// now also enforces the declared count and a retracted-prefix denylist.
//
// REMOVED 2026-10-05: 3c:71:bf (registered to Espressif, a chip vendor -- it
// would score our own class of board) and 82:6b:f2 (locally-administered bit
// set, so not a registered prefix at all). See flock_db.c.
//
// b4:1e:52 is Flock Safety's own registered OUI (GainSec).
// Row layout matches flock_db.c line-for-line -- EXACTLY four entries per row --
// so the two can be diffed by eye. The 5-on-one-row drift is what hid 93beede.
static const uint8_t FLOCK_OUIS[][3] = {
    {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc}, {0x80, 0x30, 0x49},
    {0xb8, 0x35, 0x32}, {0x14, 0x5a, 0xfc}, {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88},
    {0x9c, 0x2f, 0x9d}, {0xc0, 0x35, 0x32}, {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    {0xf4, 0x6a, 0xdd}, {0x24, 0xb2, 0xb9}, {0x00, 0xf4, 0x8d}, {0xd0, 0x39, 0x57},
    {0xe8, 0xd0, 0xfc}, {0xe0, 0x4f, 0x43}, {0xb8, 0x1e, 0xa4}, {0x70, 0x08, 0x94},
    {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x58, 0x00, 0xe3}, {0x90, 0x35, 0xea},
    {0x5c, 0x93, 0xa2}, {0x64, 0x6e, 0x69}, {0xb4, 0x1e, 0x52}, {0xe0, 0x0a, 0xf6},
    {0x38, 0x5b, 0x44}, {0x14, 0xb5, 0xcd},
};
static const size_t FLOCK_OUI_COUNT = sizeof(FLOCK_OUIS) / sizeof(FLOCK_OUIS[0]);

// ---- SoundThinking / ShotSpotter acoustic sensors (1) --------------------
// A DIFFERENT DEVICE CLASS from the ALPRs above: these listen, they do not read
// plates. Matches are tagged `cls=a` on the wire so the Flipper can say which it
// found. MUST stay byte-identical to soundthinking_ouis[] in helpers/flock_db.c.
static const uint8_t SOUNDTHINKING_OUIS[][3] = {
    {0xd4, 0x11, 0xd6},
};
static const size_t SOUNDTHINKING_OUI_COUNT =
    sizeof(SOUNDTHINKING_OUIS) / sizeof(SOUNDTHINKING_OUIS[0]);

// ---- Axon Enterprise body-worn / in-car police equipment (1) -------------
// A THIRD DEVICE CLASS, and not fixed infrastructure: these MOVE with a person
// or a vehicle. Tagged `cls=x` on the wire. MUST stay byte-identical to
// axon_ouis[] in helpers/flock_db.c -- same CI parity gate as the other two.
//
// 00:25:df is Axon Enterprise's only IEEE registration, verified at the registry.
// Do NOT add prefixes by searching a vendor database for "axon" (that also hits
// Axon NETWORKS, Axona, Axonne, Interaxon, Maxon, Praxon, Paxonet, Yaxon -- all
// unrelated), nor from a curated "law enforcement" OUI list: one such list was
// checked entry by entry and 11 of 15 were wrong, including Apple prefixes filed
// as Digital Ally and Axis Communications filed as Flock Safety.
//
// REGISTRY-VERIFIED, NEVER FIELD-OBSERVED -- see flock_db.c for why that matters.
static const uint8_t AXON_OUIS[][3] = {
    {0x00, 0x25, 0xdf},
};
static const size_t AXON_OUI_COUNT = sizeof(AXON_OUIS) / sizeof(AXON_OUIS[0]);

// ---- Vendor-exclusive competitor OUIs (15 across 7 vendors) --------------
// Ubicquia (1), Motorola Solutions (7), Verkada (1), Genetec (2), Avigilon (1).
// MUST stay byte-identical to ubicquia_ouis[] / motorola_ouis[] / verkada_ouis[]
// / genetec_ouis[] / avigilon_ouis[] in helpers/flock_db.c -- same hand-sync
// rule and the same tools/check_oui_parity.py gate as the three tables above.
// Full provenance, the registry organisation strings, and the two live
// substring traps (GENETEC Corporation is NOT Genetec Inc; Motorola Mobility is
// NOT Motorola Solutions) are documented in flock_db.c. Read that before adding.
//
// A DIFFERENT EVIDENCE CLASS FROM FLOCK_OUIS, WHICH IS WHY THEY SCORE
// DIFFERENTLY BELOW. FLOCK_OUIS is mostly Liteon and Espressif -- chip vendors
// Flock buys from -- so a bare OUI hit there describes millions of consumer
// devices and was deliberately dropped from scoring after it reported a
// T-Mobile gateway. Every prefix here is registered to the surveillance vendor
// ITSELF, so a bare beacon match is a real, attributable observation and earns
// conf=1 ("possible"). It earns nothing more: registry-verified is not
// field-observed, and none of this hardware has been captured on the air yet.
//
// Tagged `cls=g` on the wire (gear -- vendor known, KIND not determined). An
// older Flipper build ignores the token and falls back to its MAC-derived class.
static const uint8_t UBICQUIA_OUIS[][3] = {
    {0x94, 0x7b, 0xbe},
};
static const size_t UBICQUIA_OUI_COUNT = sizeof(UBICQUIA_OUIS) / sizeof(UBICQUIA_OUIS[0]);

static const uint8_t MOTOROLA_OUIS[][3] = {
    {0x00, 0x04, 0x7d}, {0x00, 0x18, 0x85}, {0x00, 0x1f, 0x92}, {0x4c, 0xcc, 0x34},
    {0x10, 0x74, 0x6f}, {0xb8, 0xe2, 0x8c}, {0x9c, 0x86, 0x2b},
};
static const size_t MOTOROLA_OUI_COUNT = sizeof(MOTOROLA_OUIS) / sizeof(MOTOROLA_OUIS[0]);

static const uint8_t VERKADA_OUIS[][3] = {
    {0xe0, 0xa7, 0x00},
};
static const size_t VERKADA_OUI_COUNT = sizeof(VERKADA_OUIS) / sizeof(VERKADA_OUIS[0]);

static const uint8_t GENETEC_OUIS[][3] = {
    {0x00, 0xbf, 0x15}, {0x0c, 0xbf, 0x15},
};
static const size_t GENETEC_OUI_COUNT = sizeof(GENETEC_OUIS) / sizeof(GENETEC_OUIS[0]);

static const uint8_t AVIGILON_OUIS[][3] = {
    {0x70, 0x1a, 0xd5},
};
static const size_t AVIGILON_OUI_COUNT = sizeof(AVIGILON_OUIS) / sizeof(AVIGILON_OUIS[0]);

// Utility, Inc "BodyWorn" body cameras. Exclusive MA-L blocks (IEEE 2026-09-07).
// MUST stay byte-identical to utility_ouis[] in helpers/flock_db.c.
static const uint8_t UTILITY_OUIS[][3] = {
    {0x00, 0x09, 0xbc}, {0x00, 0x16, 0xed},
};
static const size_t UTILITY_OUI_COUNT = sizeof(UTILITY_OUIS) / sizeof(UTILITY_OUIS[0]);

// Digital Ally "FirstVU" body/in-car cameras. Exclusive MA-L block.
// MUST stay byte-identical to digitalally_ouis[] in helpers/flock_db.c.
static const uint8_t DIGITALALLY_OUIS[][3] = {
    {0x00, 0x23, 0xbd},
};
static const size_t DIGITALALLY_OUI_COUNT =
    sizeof(DIGITALALLY_OUIS) / sizeof(DIGITALALLY_OUIS[0]);


// ---- Drone manufacturers (24) --------------------------------------------
//
// A FALLBACK to Remote ID, never the main path. Of the five drone vendors a US
// police department realistically buys from -- Skydio, BRINC, Aerodome, Flock,
// Paladin -- only Skydio holds an IEEE block at all, so three of the five cannot
// be matched by any prefix table, ever. The aircraft that matters is found by its
// ASTM F3411 Remote ID broadcast (decoded app-side), which is a
// legal mandate and vendor-independent.
//
// A hit here is NOT a police drone: DJI's blocks are on far more hobbyist
// quadcopters than anything else. It scores like any other bare OUI.
//
// MA-L HOLDERS ONLY. Autel Robotics, Yuneec, Inspired Flight, ideaForge and the
// rest sit inside shared IEEE Registration Authority MA-M/MA-S blocks, and this
// table is three bytes wide, so matching them would flag unrelated hardware as an
// aircraft. Excluded on purpose despite being DJI: f8:40:68 (Ronin gimbals) and
// 20:1f:55 (Osmo handhelds) -- neither flies.
//
// MUST stay byte-identical to drone_ouis[] in helpers/flock_db.c; the CI parity
// gate enforces it. EXACTLY four entries per row.
static const uint8_t DRONE_OUIS[][3] = {
    {0x60, 0x60, 0x1f}, {0x34, 0xd2, 0x62}, {0x48, 0x1c, 0xb9}, {0xe4, 0x7a, 0x2c},
    {0x58, 0xb8, 0x58}, {0x04, 0xa8, 0x5a}, {0x8c, 0x58, 0x23}, {0x0c, 0x9a, 0xe6},
    {0x88, 0x29, 0x85}, {0x4c, 0x43, 0xf6}, {0x9c, 0x5a, 0x8a}, {0xec, 0x72, 0xf7},
    {0x34, 0x91, 0xf0}, {0x38, 0x1d, 0x14}, {0x00, 0x12, 0x1c}, {0x00, 0x26, 0x7e},
    {0x90, 0x03, 0xb7}, {0x90, 0x3a, 0xe6}, {0xa0, 0x14, 0x3d}, {0xb0, 0x30, 0xc8},
    {0x00, 0x1a, 0xf9}, {0x14, 0xdd, 0x48}, {0xec, 0x71, 0x5e}, {0x74, 0xb8, 0x0f},
};
static const size_t DRONE_OUI_COUNT = sizeof(DRONE_OUIS) / sizeof(DRONE_OUIS[0]);

// One row per vendor table, so the index builder and the matcher below cannot
// disagree about which tables exist -- adding a vendor means adding one row here
// and nowhere else. Missing a table in the index builder would make
// oui_first_possible() reject every frame from that vendor before the matcher
// ever ran, and the table would look perfectly correct while detecting nothing.
struct VendorOuiTable {
    const uint8_t (*ouis)[3];
    size_t count;
};
static const VendorOuiTable VENDOR_OUI_TABLES[] = {
    {UBICQUIA_OUIS, UBICQUIA_OUI_COUNT},
    {MOTOROLA_OUIS, MOTOROLA_OUI_COUNT},
    {VERKADA_OUIS, VERKADA_OUI_COUNT},
    {GENETEC_OUIS, GENETEC_OUI_COUNT},
    {AVIGILON_OUIS, AVIGILON_OUI_COUNT},
    {UTILITY_OUIS, UTILITY_OUI_COUNT},
    {DIGITALALLY_OUIS, DIGITALALLY_OUI_COUNT},
    {DRONE_OUIS, DRONE_OUI_COUNT},
};
static const size_t VENDOR_OUI_TABLE_COUNT =
    sizeof(VENDOR_OUI_TABLES) / sizeof(VENDOR_OUI_TABLES[0]);


// ---- State ---------------------------------------------------------------
//
// THREADING. promisc_cb() runs in the WiFi driver task (usually core 0) and
// loop()/handle_command() run in the Arduino task (core 1), so everything they
// share needs `volatile` at minimum, and a critical section wherever a read and
// a write must agree with each other.
//
// g_mux guards the two places where a torn read is not merely inaccurate but
// unsafe or wrong: the beacon ring (whose count BOUNDS an array write) and the
// Locator target (a 6-byte MAC that must be swapped atomically or promisc_cb
// homes on a half-old, half-new address for a few frames).
//
// The frame counters below are deliberately NOT protected. `volatile` does not
// make `++` atomic, so they can lose the odd increment under contention -- but
// they are display-only rate indicators reset every interval, and taking a lock
// per frame inside the WiFi callback would cost more than the drift.
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile bool g_scanning = true;
static volatile uint32_t g_frames = 0;
static volatile uint32_t g_hits = 0;
static volatile uint32_t g_deauths = 0; // deauth + disassoc frames seen (attack indicator)
static volatile uint8_t g_channel = 1;
static uint8_t g_lock_channel = 0; // 0 = hop
/** Highest 2.4 GHz channel the hopper visits. See the hop block in loop(). */
#define MAX_HOP_CHANNEL 13

/* ---- Dual-band (5 GHz) support -------------------------------------------
 *
 * A 2.4-only companion CANNOT SEE a Flock uplink on 5 GHz -- not "sees it
 * weakly", cannot see it at all. The ESP32-C5 is the first Espressif part with a
 * 5 GHz radio, so on that chip we hop both bands.
 *
 * Gated on the SoC capability, not on a board name: SOC_WIFI_SUPPORT_5G comes
 * from the IDF's own soc_caps.h, so a classic ESP32/S3/C3 compiles exactly as
 * before and pays nothing (the 5 GHz table is not even emitted). Requires
 * Arduino core 3.x, which is where the C5 exists at all.
 *
 * Channel list is the 28 20 MHz channels the IDF enumerates for this radio
 * (esp_wifi_types_generic.h). Band switching is done purely by setting the
 * channel: the IDF docs say to prefer esp_wifi_set_channel() over
 * esp_wifi_set_band(), and it moves bands on its own once band mode is AUTO.
 *
 * COST, stated plainly: a full sweep goes from 13 channels to 41. At the same
 * 300 ms dwell that is ~12.3 s per sweep instead of ~3.9 s, so a given camera is
 * revisited a third as often. That is the honest price of covering a band we
 * currently cannot see, and `band 2g` returns the fast sweep for anyone who
 * wants it.
 */
#if defined(SOC_WIFI_SUPPORT_5G) && SOC_WIFI_SUPPORT_5G
#define FLOCK_HAS_5GHZ 1
#else
#define FLOCK_HAS_5GHZ 0
#endif

#if FLOCK_HAS_5GHZ
/** 5 GHz 20 MHz channels, incl. DFS (52-144) -- we only ever listen. */
static const uint8_t CHANNELS_5G[] = {36,  40,  44,  48,  52,  56,  60,  64,  100, 104,
                                      108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
                                      149, 153, 157, 161, 165, 169, 173, 177};
#define CHANNELS_5G_COUNT (sizeof(CHANNELS_5G) / sizeof(CHANNELS_5G[0]))
#endif

/** Which band(s) the hopper sweeps. `band 2g|5g|all` selects at runtime. */
typedef enum {
    FlockBand2G = 0,
    FlockBand5G = 1,
    FlockBandAll = 2,
} FlockBandSel;

/* Default: sweep everything the radio can reach. On a 2.4-only part this is
 * identical to the old behaviour, because the 5 GHz list does not exist. */
#if FLOCK_HAS_5GHZ
static FlockBandSel g_band = FlockBandAll;
#else
static FlockBandSel g_band = FlockBand2G;
#endif

/** Hop cursor: index into the logical (2.4 then 5) channel sequence. */
static uint16_t g_hop_i = 0;

/** True if `ch` is a 5 GHz channel number (2.4 GHz tops out at 14). */
static inline bool is_5ghz_channel(uint8_t ch) {
    return ch >= 36;
}

/** Number of channels in the current sweep. */
static uint16_t hop_count() {
    uint16_t n = 0;
    if(g_band == FlockBand2G || g_band == FlockBandAll) n += MAX_HOP_CHANNEL;
#if FLOCK_HAS_5GHZ
    if(g_band == FlockBand5G || g_band == FlockBandAll) n += CHANNELS_5G_COUNT;
#endif
    return n ? n : MAX_HOP_CHANNEL; // never zero: degrade to 2.4 rather than stall
}

/** i-th channel of the current sweep (2.4 GHz first, then 5 GHz). */
static uint8_t hop_channel(uint16_t i) {
    bool do_24 = (g_band == FlockBand2G || g_band == FlockBandAll);
#if FLOCK_HAS_5GHZ
    bool do_5 = (g_band == FlockBand5G || g_band == FlockBandAll);
#else
    bool do_5 = false;
#endif
    if(do_24) {
        if(i < MAX_HOP_CHANNEL) return (uint8_t)(i + 1);
        i -= MAX_HOP_CHANNEL;
    }
#if FLOCK_HAS_5GHZ
    if(do_5 && i < CHANNELS_5G_COUNT) return CHANNELS_5G[i];
#else
    (void)do_5;
#endif
    return 1;
}
static uint32_t g_last_status = 0;
static uint32_t g_last_hop = 0;
static uint32_t g_deauths_last = 0; // for per-interval deauth rate
static uint32_t g_last_da = 0; // rate-limit DA attribution lines

// ---- Active attack-tool detection (emitted as ATK lines, ~1 Hz) -----------
// These are RATE signals reset every status interval, so the alert clears when
// the attack stops. Thresholds are deliberately conservative to avoid false
// positives in dense-but-benign RF; tune for your environment.
//   probeflood : abnormal probe-request rate (KARMA / mass-probe tools)
//   beaconflood: many DISTINCT beaconing BSSIDs/s (Marauder/Pineapple SSID spam)
//   blespam    : a flood of impersonation BLE adverts (Flipper/ESP BLE spam)
#define PROBE_FLOOD_MIN  80 // probe requests in one ~1 s interval
#define BEACON_FLOOD_MIN 40 // distinct beaconing BSSIDs in one ~1 s interval
#define BLE_SPAM_MIN     12 // impersonation-class adverts in one BLE scan
static volatile uint32_t g_probe_reqs = 0; // probe requests this interval (reset ~1 Hz)
#define BEACON_RING 64
static uint32_t g_beacon_ring[BEACON_RING]; // recent beacon-BSSID hashes this interval
static uint8_t g_beacon_ring_n = 0;
static uint32_t g_beacon_distinct = 0; // distinct beaconing BSSIDs this interval

// Note a beacon's source BSSID; counts it once per interval. Approximate by
// design (a small ring + best-effort across the WiFi-callback / loop tasks) --
// it only needs to tell "a handful of real APs" from "a spam flood."
static void note_beacon_bssid(const uint8_t* bssid) {
    uint32_t h = 2166136261u; // FNV-1a over the 6 BSSID bytes (inlined: no fwd dep)
    for(int i = 0; i < 6; i++) {
        h ^= bssid[i];
        h *= 16777619u;
    }
    // The scan and the append must see the SAME g_beacon_ring_n: it is both the
    // dedup bound and the write index, and loop() zeroes it from the other core
    // every status interval. Hash outside the lock, hold it only for the ring.
    portENTER_CRITICAL(&g_mux);
    for(uint8_t i = 0; i < g_beacon_ring_n; i++) {
        if(g_beacon_ring[i] == h) { // already counted this interval
            portEXIT_CRITICAL(&g_mux);
            return;
        }
    }
    if(g_beacon_ring_n < BEACON_RING) {
        g_beacon_ring[g_beacon_ring_n++] = h;
        g_beacon_distinct++;
    }
    portEXIT_CRITICAL(&g_mux);
}

// ---- Locator: stream live RSSI for one target so the app can home in on it ---
// 'w' Wi-Fi (match the MAC in promiscuous frames, channel-locked) or 'b' BLE
// (match the addr in a repeating scan). MAC kept BOTH as bytes (Wi-Fi, compared
// to raw frame bytes) and as the lowercase hex string (BLE, compared to the
// same toString() form the BLE line is built from -- avoids byte-order traps).
// volatile: promisc_cb (WiFi task) reads g_locate_kind as the fast gate before
// touching the target. The target bytes themselves are swapped under g_mux.
static volatile char g_locate_kind = 0; // 0 none / 'w' / 'b'
static uint8_t g_locate_mac[6];
static char g_locate_macs[13]; // lowercase hex, no separators
static uint8_t g_locate_ch = 0;
static int g_locate_best = -127; // strongest RSSI since the last LOC emit (Wi-Fi)
static uint32_t g_last_loc = 0; // LOC emit throttle

static int hexv(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool parse_hexmac(const char* s, uint8_t out[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = hexv(s[i * 2]), lo = hexv(s[i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Dual-band (WiFi + BLE) Flock detection. BLE is initialised once and kept
// resident (avoids the Bluedroid init/deinit heap leak); the radio is shared by
// toggling promiscuous off during a BLE scan, then back on. flockcombo
// interleaves a WiFi-promiscuous phase with a periodic BLE scan phase.
#if FLOCK_HAS_BLE
static bool g_ble_inited = false;
static BLEScan* g_ble = nullptr;
#endif
static bool g_combo = false;
static uint32_t g_phase_start = 0;
#define COMBO_WIFI_MS 9000 // ~3 channel sweeps before a BLE scan (WiFi-biased)
#define COMBO_BLE_SEC 3 // BLE scan seconds (BLE adverts repeat fast)

/**
 * First-byte rejection bitmap for the OUI tables.
 *
 * promisc_cb() tests TWO addresses on EVERY management frame, and each test used
 * to walk all 32 Flock prefixes plus the SoundThinking one -- up to 64 three-byte
 * comparisons per frame, inside the WiFi driver callback, before any filtering.
 * The overwhelming majority of frames match nothing.
 *
 * 256 bits (32 bytes) say whether ANY table entry starts with a given byte, so
 * the common no-match case costs one array index and one bit test. Built once at
 * boot by oui_index_init(); the tables are const, so it can never go stale.
 */
// Built during C++ static initialisation, i.e. before setup() and before any
// frame can arrive. Deliberately NOT an init function called from setup():
// forgetting that call would make every OUI test return false and silently kill
// all detection, which is the worst possible failure mode for this app. Deriving
// the index from the tables in a constructor makes that unrepresentable.
static const struct OuiFirstIndex {
    uint8_t bits[32]; // bit b set => some prefix starts with byte b
    OuiFirstIndex() : bits{} {
        for(size_t i = 0; i < FLOCK_OUI_COUNT; i++) {
            uint8_t b = FLOCK_OUIS[i][0];
            bits[b >> 3] |= (uint8_t)(1u << (b & 7));
        }
        for(size_t i = 0; i < SOUNDTHINKING_OUI_COUNT; i++) {
            uint8_t b = SOUNDTHINKING_OUIS[i][0];
            bits[b >> 3] |= (uint8_t)(1u << (b & 7));
        }
        // Axon too. Miss this and oui_first_possible() rejects every Axon frame
        // before ax_oui_match() is ever reached -- detection would be silently
        // dead while the table looked perfectly correct.
        for(size_t i = 0; i < AXON_OUI_COUNT; i++) {
            uint8_t b = AXON_OUIS[i][0];
            bits[b >> 3] |= (uint8_t)(1u << (b & 7));
        }
        // And every vendor-exclusive competitor table, via the one list of them.
        // Same trap as Axon above: a table left out here is silently undetectable.
        for(size_t t = 0; t < VENDOR_OUI_TABLE_COUNT; t++) {
            for(size_t i = 0; i < VENDOR_OUI_TABLES[t].count; i++) {
                uint8_t b = VENDOR_OUI_TABLES[t].ouis[i][0];
                bits[b >> 3] |= (uint8_t)(1u << (b & 7));
            }
        }
    }
} g_oui_index;

static inline bool oui_first_possible(uint8_t b) {
    return (g_oui_index.bits[b >> 3] >> (b & 7)) & 1;
}

static bool flock_oui_match(const uint8_t* mac) {
    if(!oui_first_possible(mac[0])) return false; // fast reject, no table walk
    for(size_t i = 0; i < FLOCK_OUI_COUNT; i++) {
        if(mac[0] == FLOCK_OUIS[i][0] && mac[1] == FLOCK_OUIS[i][1] &&
           mac[2] == FLOCK_OUIS[i][2])
            return true;
    }
    return false;
}

static bool st_oui_match(const uint8_t* mac) {
    if(!oui_first_possible(mac[0])) return false; // fast reject, no table walk
    for(size_t i = 0; i < SOUNDTHINKING_OUI_COUNT; i++) {
        if(mac[0] == SOUNDTHINKING_OUIS[i][0] && mac[1] == SOUNDTHINKING_OUIS[i][1] &&
           mac[2] == SOUNDTHINKING_OUIS[i][2])
            return true;
    }
    return false;
}

static bool ax_oui_match(const uint8_t* mac) {
    if(!oui_first_possible(mac[0])) return false; // fast reject, no table walk
    for(size_t i = 0; i < AXON_OUI_COUNT; i++) {
        if(mac[0] == AXON_OUIS[i][0] && mac[1] == AXON_OUIS[i][1] &&
           mac[2] == AXON_OUIS[i][2])
            return true;
    }
    return false;
}

// Any known surveillance-vendor prefix, any class. Scoring is class-agnostic;
// the class itself rides along in the `cls=` field.
//
// DELIBERATELY EXCLUDES the vendor-exclusive competitor tables. This predicate
// feeds the conf=2 ("likely") probe-request rungs below, and those rungs were
// calibrated against upstream's Flock field test -- 11/12 cameras, 2 false
// positives. Extending them to five vendors whose hardware has never been
// captured on the air would be asserting a measurement nobody has taken.
// vendor_oui_match() below gets its own, lower rung instead.
static bool oui_match(const uint8_t* mac) {
    return flock_oui_match(mac) || st_oui_match(mac) || ax_oui_match(mac);
}

// Vendor-exclusive competitor prefixes -> conf=1 only. See the table comment.
static bool vendor_oui_match(const uint8_t* mac) {
    if(!oui_first_possible(mac[0])) return false; // fast reject, no table walk
    for(size_t t = 0; t < VENDOR_OUI_TABLE_COUNT; t++) {
        const VendorOuiTable& vt = VENDOR_OUI_TABLES[t];
        for(size_t i = 0; i < vt.count; i++) {
            if(mac[0] == vt.ouis[i][0] && mac[1] == vt.ouis[i][1] && mac[2] == vt.ouis[i][2])
                return true;
        }
    }
    return false;
}

static char lc(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/**
 * True if `s` is EXACTLY "flock-" + 6 hex digits: the provisioning-AP name.
 *
 * Mirrors is_flock_provisioning_ssid() in helpers/flock_db.c -- keep the two in
 * step, same hand-sync rule as the OUI tables above.
 *
 * ANCHORED on purpose. This used to be a bare strstr(buf, "flock-"), which
 * confirmed every benign name that merely contained the substring:
 * "Flock-Guest", "Flock-Safety-Corp", "Flock-12345". The Flipper takes the
 * companion's conf verbatim on this path, so that went straight to the screen
 * as CONFIRMED. Those now fall through to the "likely" check below.
 *
 * `s` is already lower-cased by the caller, so only a-f need testing.
 */
static bool is_flock_provisioning_ssid(const char* s) {
    if(strncmp(s, "flock-", 6) != 0) return false;
    for(int i = 6; i < 12; i++) {
        char c = s[i]; // '\0' on a short SSID is not hex -> correctly rejected
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if(!hex) return false;
    }
    return s[12] == '\0'; // nothing may follow the 6 hex digits
}

// Returns: 0 none, 2 likely (flock/flck substring), 3 confirmed
// (^flock-[0-9a-f]{6}$ or the test_flck dev SSID, CVE-2025-59409)
static int ssid_score(const char* s, int len) {
    if(len <= 0) return 0;
    char buf[64];
    int n = len < 63 ? len : 63;
    for(int i = 0; i < n; i++) buf[i] = lc(s[i]);
    buf[n] = 0;
    if(is_flock_provisioning_ssid(buf) || strstr(buf, "test_flck")) return 3;
    if(strstr(buf, "flock") || strstr(buf, "flck")) return 2;
    return 0;
}

// Append up to `max` bytes of `s` into buf[*pos], stripping ',', CR, LF and control chars
// to '.' so the payload can't break the line protocol. Bounds-checked; advances *pos.
// Callers assemble a whole line in one buffer and emit it with a SINGLE Serial.write, so a
// line built in promisc_cb (WiFi task) can't interleave on the UART with loop()'s status
// lines -- a single HardwareSerial::write() is atomic w.r.t. the other task's writes.
static void buf_append_escaped(char* buf, size_t bufsz, size_t* pos, const char* s, int len, int max) {
    for(int i = 0; i < len && i < max && *pos + 1 < bufsz; i++) {
        char c = s[i];
        if(c == ',' || c == '\r' || c == '\n' || (uint8_t)c < 0x20) c = '.';
        buf[(*pos)++] = c;
    }
}

/**
 * Append a printf-formatted field at buf[*pos], clamping to the buffer.
 *
 * snprintf() returns what it WOULD have written, so the natural-looking
 * `pos += snprintf(buf + pos, sizeof(buf) - pos, ...)` overshoots `pos` past the
 * buffer on truncation. The NEXT call then computes `sizeof(buf) - pos` as a
 * size_t UNDERFLOW -- a huge length against an out-of-bounds pointer. Chained
 * appends must never accumulate the raw return value; this clamps instead.
 */
static void buf_appendf(char* buf, size_t bufsz, size_t* pos, const char* fmt, ...) {
    if(*pos + 1 >= bufsz) return; // no room for even one byte + NUL
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *pos, bufsz - *pos, fmt, ap);
    va_end(ap);
    if(n < 0) return; // encoding error
    size_t avail = bufsz - *pos - 1;
    *pos += ((size_t)n > avail) ? avail : (size_t)n;
}

// ---- Sustained-probe-rate gate -------------------------------------------
//
// WHY. "Flock OUI + wildcard probe request -> LIKELY" is too generous on its own,
// because a wildcard probe is the most ordinary frame a Wi-Fi client emits: it is
// literally what scanning for a network looks like. And FLOCK_OUIS is mostly chip
// vendors, not Flock -- 21 of its entries are Liteon. So an ordinary consumer
// device built on the same silicon, doing nothing but looking for a network, was
// scoring LIKELY. A user reported exactly that for a T-Mobile hotspot.
//
// THE GATE THIS BUILT WAS REMOVED. Twice it was set to a value that could not be
// demonstrated to pass a camera, and a filter on the primary detection path that
// cannot be shown to pass a true positive is worse than the false positive it
// prevents. What remains is the COUNTER, reported as `pr=<n>`, so a future
// threshold can be derived from measurement instead of from reasoning.
//
// The reasoning was: a fielded camera sprays wildcard probes roughly every 125 ms
// while a phone emits a burst and goes quiet, so rate should separate them. The
// first attempt asked for 3 probes in 2000 ms, which the radio's own duty cycle
// makes impossible -- it watches one channel for 300 ms out of every 3900 ms, so
// a camera can only ever put 2 into one window. The second widened the window to
// span sweeps; on the bench that still produced zero probe-sourced detections
// while beacons from the same board detected fine, and whether the cause was the
// gate or the rig's MAC spoofing was never established.
//
// The reported false positive is addressed where it actually came from: 48:27:ea
// (Samsung) and a4:cf:12 (Espressif) are out of the built-in table.
//
// So: require several probes from the same transmitter inside a short window
// before OUI+probe may reach conf=2. Because the radio only watches any one
// channel for 300 ms at a time, a camera does NOT clear this inside a single
// dwell -- it clears it by still probing on the next sweep, which a phone that
// has finished scanning does not do. See the arithmetic on PROBE_WINDOW_MS.
//
// THRESHOLDS ARE NOT FIELD-TUNED. 125 ms is upstream's figure, but the client-side
// distribution has never been measured here, so these are deliberately loose --
// they are set to catch the reported false positive without risking a real
// camera, not to be optimal. The observed count is reported on the wire as
// `pr=<n>` precisely so it can be tuned from real captures instead of guessed at
// again. Widen the window or lower the threshold only with data.
#define PROBE_TRACK_N   24 // transmitters tracked (LRU); urban scans are busy
// THE WINDOW MUST SPAN SEVERAL CHANNEL SWEEPS. This is not a tuning preference,
// it is arithmetic, and getting it wrong once already shipped a gate that could
// never open:
//
//   channel dwell            300 ms   (the hop below)
//   2.4 GHz sweep     13 x 300 = 3900 ms
//   so any one channel is watched for 300 ms out of every 3900 ms
//   a fielded camera probes every ~125 ms -> 300/125 = 2 probes seen per dwell
//
// v0.74 asked for 3 probes inside 2000 ms. A camera can only ever put 2 into one
// dwell, and consecutive dwells are 3900 ms apart, so two dwells never share a
// 2000 ms window. The threshold was unreachable BY CONSTRUCTION -- not strict,
// impossible -- and it silently disabled the OUI+probe path that finds most
// fielded cameras. Caught by tools/flock_emitter on the bench, which is exactly
// why that rig exists.
//
// What actually separates a camera from a phone at this dwell is PERSISTENCE
// ACROSS SWEEPS, not burst rate inside one. A camera in station mode is probing
// on every single visit, forever. A phone scanning emits a burst and then goes
// quiet for tens of seconds. So the window spans ~2 sweeps and the threshold is
// what a camera accumulates over them:
//
//   camera  ~2 per dwell x 2 dwells in 8000 ms = ~4  -> passes
//   phone    1-2 probes total in one burst          -> blocked
//
// STILL NOT VALIDATED AGAINST A REAL CAMERA. 125 ms is upstream's figure and the
// arithmetic above follows from it; nobody has held this against fielded
// hardware. The observed count rides the wire as `pr=<n>` so it can be measured
// rather than reasoned about a third time.
#define PROBE_WINDOW_MS 8000 // ~2 full channel sweeps, so persistence accumulates
// No PROBE_BURST_MIN any more: nothing GATES on this count. It is reported as
// `pr=<n>` and weighed by a human, which is the only honest use for it until
// someone measures a real camera against a real phone.
//
// VENDOR_PROBE_SUSTAINED is NOT that gate coming back, and the difference is
// the direction it fails in. The old one REJECTED: set too high, it threw away
// real cameras, which is how it came to be removed. This one PROMOTES a
// vendor-exclusive hit from "possible" to "likely" and nothing else. Set too
// high it simply never fires and the behaviour is exactly what it is today;
// nothing is lost, only not gained.
//
// It also only has to fire ONCE per device. Confidence is max-held per entry on
// the Flipper, so a camera that crosses the line on any single frame stays
// promoted, and the many frames where it lands low in the window do not undo it.
// That is what makes a conservative value safe rather than useless.
//
// 4 comes from the arithmetic above (a camera accumulates ~4 across two sweeps,
// a phone burst is 1-2) and matches what this bench shows: the sustained prober
// here reaches 6, 7 and 12 while a randomised-MAC phone sat at 1. STILL NOT
// VALIDATED AGAINST A FIELDED CAMERA -- pr= remains on the wire precisely so the
// number can be corrected from a real capture instead of reasoned about again.
#define VENDOR_PROBE_SUSTAINED 4

struct ProbeTrack {
    uint8_t mac[6];
    uint8_t count;
    uint32_t first_ms;
    uint32_t last_ms;
};
static ProbeTrack g_probe_track[PROBE_TRACK_N];

/**
 * Record one probe request from `mac` and return how many it has sent inside the
 * current window (>= 1). Called on EVERY candidate probe, before the sequence-run
 * coalescer -- the coalescer deliberately suppresses repeats, so counting after it
 * would always see 1 and the gate would reject everything including real cameras.
 */
static uint8_t probe_rate_bump(const uint8_t* mac) {
    uint32_t now = millis();
    int slot = -1, oldest = 0;
    for(int i = 0; i < PROBE_TRACK_N; i++) {
        if(memcmp(g_probe_track[i].mac, mac, 6) == 0 && g_probe_track[i].count) {
            slot = i;
            break;
        }
        // millis() wraps every ~49 days; the subtraction is unsigned so the
        // comparison stays correct across the wrap.
        if((uint32_t)(now - g_probe_track[i].last_ms) >
           (uint32_t)(now - g_probe_track[oldest].last_ms)) {
            oldest = i;
        }
    }
    if(slot < 0) { // evict the least recently used entry
        slot = oldest;
        memcpy(g_probe_track[slot].mac, mac, 6);
        g_probe_track[slot].count = 0;
    }
    ProbeTrack* e = &g_probe_track[slot];
    if(e->count == 0 || (uint32_t)(now - e->first_ms) > PROBE_WINDOW_MS) {
        e->count = 1; // window expired -> start a fresh one
        e->first_ms = now;
    } else if(e->count < 255) {
        e->count++;
    }
    e->last_ms = now;
    return e->count;
}

// ---- B1: probe IE-fingerprint + sequence-number coalescer ----------------
//
// The whole OUI/SSID ladder collapses the day Flock randomizes the probe MAC.
// The probe *body* is MAC-independent: the ordered set of tagged Information
// Elements (supported rates, HT/VHT/HE caps, vendor-specific 0xDD OUI+type) is
// baked into the WiFi SoC driver and can't be scrambled without breaking
// 802.11. We hash that skeleton into a uint32 the Flipper compares against a
// curated table. This is a device-CLASS / firmware-stack match, NOT a unique
// device ID -- the Flipper reports it as a candidate class match only.
//
// We hash only the *skeleton* (tag id + length, plus the first OUI+type bytes
// of vendor-specific IEs), never per-frame variable contents, so the same probe
// template hashes identically regardless of the (possibly randomized) MAC.

#define FNV1A_OFFSET 0x811c9dc5u
#define FNV1A_PRIME 0x01000193u

static inline uint32_t fnv1a_u8(uint32_t h, uint8_t b) {
    return (h ^ b) * FNV1A_PRIME;
}

// FNV-1a over the IE skeleton of a probe request. `p` points at the frame body,
// `len` is the body length (FCS already removed). Tagged params start at byte
// 24 (probe request has no fixed params). For each IE we fold in (tag_id,
// length); for vendor-specific (0xDD) we also fold in up to the first 5 bytes
// (3-byte OUI + 1-2 type/subtype) -- enough to distinguish vendor IEs without
// shipping their variable payloads. Returns 0 if there are no parseable IEs.
static uint32_t ie_skeleton_hash(const uint8_t* p, int len) {
    uint32_t h = FNV1A_OFFSET;
    int off = 24; // tagged parameters begin here for a probe request
    bool any = false;
    while(off + 2 <= len) {
        uint8_t tag = p[off];
        uint8_t tlen = p[off + 1];
        if(off + 2 + tlen > len) break; // truncated IE -> stop
        h = fnv1a_u8(h, tag);
        h = fnv1a_u8(h, tlen);
        if(tag == 0xDD) { // vendor-specific: fold OUI + type (first 5 bytes)
            int n = tlen < 5 ? tlen : 5;
            for(int i = 0; i < n; i++) h = fnv1a_u8(h, p[off + 2 + i]);
        }
        any = true;
        off += 2 + tlen;
    }
    return any ? h : 0;
}

// ---- IE CONTENT hash + printable signature -------------------------------
//
// WHY THE SKELETON HASH ABOVE IS NOT ENOUGH. It folds in tag id and length and
// then throws the CONTENTS away, so every IE that actually describes the radio
// -- supported rates, HT/VHT/HE capabilities, extended capabilities -- counts
// for nothing. Two unrelated chipsets that happen to lay their probe out the
// same way hash identically.
//
// That is not theoretical. Across the 120 devices in @wiilover22's 2026-09-11
// capture the skeleton produced only 49 distinct values and 74% of devices
// landed in a collision, one hash covering 24 separate devices. A signature
// that coarse cannot identify anything, which is the whole reason a camera
// standing in front of an operator stayed invisible.
//
// WHAT IS SAFE TO HASH. Capability IEs describe the HARDWARE and are identical
// in every probe a device sends, so they survive MAC randomisation exactly as
// the skeleton does. What must stay out is anything that varies per frame --
// above all DS Parameter Set (tag 3), which carries the channel: fold that in
// and a device gets a different fingerprint on every channel it sweeps.
static inline bool ie_content_is_stable(uint8_t tag) {
    switch(tag) {
    case 1: // Supported Rates
    case 45: // HT Capabilities
    case 50: // Extended Supported Rates
    case 127: // Extended Capabilities
    case 191: // VHT Capabilities
    case 255: // Element ID Extension (HE capabilities and friends)
        return true;
    default:
        return false; // 0 SSID, 3 DS Param (channel!), anything per-frame
    }
}

// FNV-1a over tag + length for EVERY IE, plus the full contents of the stable
// capability IEs and up to 7 bytes (OUI + type + 3 payload) of each vendor IE.
// Strictly more discriminating than ie_skeleton_hash(); reported alongside it
// rather than replacing it, so existing signatures.json files keep working.
static uint32_t ie_content_hash(const uint8_t* p, int len) {
    uint32_t h = FNV1A_OFFSET;
    int off = 24;
    bool any = false;
    while(off + 2 <= len) {
        uint8_t tag = p[off];
        uint8_t tlen = p[off + 1];
        if(off + 2 + tlen > len) break;
        h = fnv1a_u8(h, tag);
        h = fnv1a_u8(h, tlen);
        if(ie_content_is_stable(tag)) {
            for(int i = 0; i < tlen; i++) h = fnv1a_u8(h, p[off + 2 + i]);
        } else if(tag == 0xDD) {
            int n = tlen < 7 ? tlen : 7;
            for(int i = 0; i < n; i++) h = fnv1a_u8(h, p[off + 2 + i]);
        }
        any = true;
        off += 2 + tlen;
    }
    return any ? h : 0;
}

// PRINTABLE signature: an ordered IE tag list.
//
// The SSID is skipped and vendor IEs are expanded to
// `221:<hex of OUI+type+3 payload bytes>`; everything else is its decimal tag
// id. The format is deliberately plain so a signature can be read off a CSV,
// compared by eye against another capture, and quoted in a field report.
//
// A hash cannot be read, compared by eye, partially matched, or published in a
// form anyone else can use. This can, which is why it goes in the survey CSV
// next to the hashes rather than instead of them.
static void ie_sig_string(const uint8_t* p, int len, char* out, size_t cap) {
    if(!out || cap == 0) return;
    out[0] = '\0';
    size_t pos = 0;
    int off = 24;
    static const char hexd[] = "0123456789abcdef";
    while(off + 2 <= len) {
        uint8_t tag = p[off];
        uint8_t tlen = p[off + 1];
        if(off + 2 + tlen > len) break;
        if(tag == 0) { // SSID: wildcard or not, never part of the shape
            off += 2 + tlen;
            continue;
        }
        // Worst case appended below is ",221:" + 14 hex = 19 chars, + NUL.
        if(pos + 21 >= cap) break;
        if(pos) out[pos++] = ',';
        if(tag == 0xDD) {
            pos += (size_t)snprintf(out + pos, cap - pos, "221:");
            int n = tlen < 7 ? tlen : 7;
            for(int i = 0; i < n; i++) {
                uint8_t b = p[off + 2 + i];
                out[pos++] = hexd[b >> 4];
                out[pos++] = hexd[b & 0x0f];
            }
        } else {
            pos += (size_t)snprintf(out + pos, cap - pos, "%u", (unsigned)tag);
        }
        out[pos] = '\0';
        off += 2 + tlen;
    }
    out[cap - 1] = '\0';
}

// Known Flock probe signatures, matched as a SUBSTRING -- see why below.
//
// WHAT THE ENTRY IS. A Wi-Fi Alliance vendor IE (50:6f:9a type 0x16, MBO) with
// the exact payload 03 01 03, then HT capabilities, then VHT capabilities, then
// a second vendor IE (00:50:f2 type 0x08) with payload 00 00 00. Two vendor
// elements with fixed payloads bracketing a specific capability pair is a far
// tighter claim than any one anchor alone.
//
// WHY A SUBSTRING AND NOT THE WHOLE TAG LIST. The elements BEFORE the first
// vendor IE are the ones a capture path is most likely to mangle -- a driver
// that mis-starts its IE walk, or trims a frame, loses the leading tags first
// and the tail last. Anchoring on the run that ends the probe means a signature
// still matches when the front of the list is damaged, and it costs nothing in
// precision because the discriminating content is all in that run.
//
// THE ANCHOR ALONE WOULD FALSE-POSITIVE, measured rather than assumed. Two
// ordinary devices on this project's bench carry the same Wi-Fi Alliance MBO
// element, one of them with the identical 03 01 03 payload:
//   1,50,3,45,191,221:0050f208002600,255,127,255,221:506f9a16030103
//   1,50,3,45,127,191,221:0050f208002a00,255,255,221:506f9a16030102
// Neither matches, because in both the MBO element sits at the END of the tag
// list while the signature requires it FOLLOWED BY 45,191 and the second vendor
// element. Matching the ordered run rather than the anchor is what keeps those
// two out, and it is why this is a substring of a sequence and not a keyword.
//
// SINGLE-SOURCE, so it is capped at Class? on the Flipper and can never
// auto-Confirm. It came from one contributor's drive, where it matched 11 of 12
// cameras with 2 false positives. That is good evidence and it is not proof.
//
// This is MAC-INDEPENDENT, which is the entire point: it matches a camera whose
// address is randomised and therefore invisible to every OUI table we ship.
static const char* const FLOCK_SIG_TABLE[] = {
    "221:506f9a16030103,45,191,221:0050f208000000",
#ifdef FLOCK_SIG_BENCH_TEST
    // BENCH ONLY -- NEVER IN A SHIPPED BUILD, and structurally unable to be in
    // one: no build sets this macro, so the committed source already has the
    // shipping table. Enable it deliberately for one flash with
    //   arduino-cli compile --build-property build.extra_flags=-DFLOCK_SIG_BENCH_TEST
    //
    // WHY IT EXISTS. The signature path's last unexercised link is the pair of
    // lines that turn a match into `,sg=1` and let a conf==0 frame past the
    // drop gate. It cannot be reached with the real Flock signature, because
    // the ESP-IDF will not inject a probe request with a foreign address and no
    // camera is on this bench. This entry is the commodity skeleton that
    // several randomised-MAC devices here emit continuously -- no OUI behind
    // any of them, so conf is 0 and only the signature can produce a detection.
    // That is exactly the field case, with a string we can actually generate.
    "1,50,3,45,127,255",
#endif
};
#define FLOCK_SIG_COUNT (sizeof(FLOCK_SIG_TABLE) / sizeof(FLOCK_SIG_TABLE[0]))

static bool flock_sig_match(const char* sig) {
    if(!sig || !sig[0]) return false;
    for(size_t i = 0; i < FLOCK_SIG_COUNT; i++) {
        if(strstr(sig, FLOCK_SIG_TABLE[i]) != NULL) return true;
    }
    return false;
}

// ---- Remote ID over WI-FI (ASTM F3411) -----------------------------------
//
// The BLE path in ble_do_scan() is not the whole story. ASTM F3411 defines FOUR
// broadcast transports -- BLE legacy, BLE extended, Wi-Fi NAN and Wi-Fi Beacon --
// and an aircraft is only required to implement one. DJI in particular favours
// the beacon form, so a BLE-only receiver silently misses whole fleets while
// looking like it is working.
//
// The beacon form is a vendor-specific IE (tag 0xDD) whose OUI is FA:0B:BC with
// vendor type 0x0D, followed by a one-byte counter and then a MESSAGE PACK.
// odid_parse_messages() app-side already handles packs, so once the IE is
// located this is the same decode as BLE.
//
// Walks the IEs itself rather than reusing ie_skeleton_hash(): that one hashes
// and discards, and its start offset is fixed at 24 for probe requests, whereas a
// beacon's tagged parameters begin at 36 after the fixed body.
#define ODID_WIFI_OUI_0 0xFA
#define ODID_WIFI_OUI_1 0x0B
#define ODID_WIFI_OUI_2 0xBC
#define ODID_WIFI_TYPE  0x0D

static const uint8_t* odid_find_wifi_ie(const uint8_t* p, int len, int tag_off, int* out_len) {
    if(!p || !out_len || tag_off < 0) return NULL;
    int off = tag_off;
    while(off + 2 <= len) {
        uint8_t tag = p[off];
        uint8_t tlen = p[off + 1];
        if(off + 2 + tlen > len) break; // truncated IE -> stop, trust nothing
        if(tag == 0xDD && tlen >= 5 && p[off + 2] == ODID_WIFI_OUI_0 &&
           p[off + 3] == ODID_WIFI_OUI_1 && p[off + 4] == ODID_WIFI_OUI_2 &&
           p[off + 5] == ODID_WIFI_TYPE) {
            // Skip the 3-byte OUI, the vendor type and the message counter; what
            // is left is the message pack the app decodes.
            int body = (int)tlen - 5;
            if(body <= 0) return NULL;
            *out_len = body;
            return &p[off + 7];
        }
        off += 2 + tlen;
    }
    return NULL;
}

// ---- PROBE SURVEY --------------------------------------------------------
//
// WHY THIS EXISTS. Field reports (issue #25, and a drive of the maintainer's own)
// show the same picture from different hardware: tens of thousands of management
// frames captured, and ZERO candidates. 83,916 frames over 31 minutes past real
// ALPR cameras, nothing scoring. The radio is fine; nothing in the air matched
// any table we ship.
//
// The detector cannot explain that, because it only ever reports what it already
// recognises. A camera on an OUI we do not know, or one that has moved to MAC
// randomisation -- which this file's own comments warn "collapses the whole
// OUI/SSID ladder" -- is indistinguishable from an empty street.
//
// So this records what is ACTUALLY there: every distinct transmitter sending
// WILDCARD probe requests, matched or not, with its IE-skeleton fingerprint. Park
// next to a camera you can see, and the row with a large count and a stable
// fingerprint is the camera -- whatever its OUI turns out to be. That is how the
// OUI table gets extended and how flock_ie_fps[] finally gets populated, which is
// the designed answer to randomised MACs and currently ships empty.
//
// Wildcard probes only: a directed probe names a network the device already
// knows, which is ordinary client behaviour and would bury the table in phones.
// Phones still appear -- they scan too -- but a phone emits a burst and stops
// while a camera probes every ~125 ms forever, so COUNT is the discriminator.
//
// Costs nothing when unused: rows are kept in RAM and only leave the board when
// the app asks with `survey`. No extra UART traffic during normal detection.
#define SURVEY_MAX 32
// Printable IE signature kept per row, for the CSV a field report is built
// from. A real camera's full tag list runs to about 51 characters, so 72 holds
// it with room for a couple more elements; longer ones truncate rather than
// drop, because even a cut signature shows its leading tag order. 32 rows x
// 72 B = 2.3 KB on the ESP, which has headroom the Flipper does not.
#define SURVEY_SIG_LEN 72
// Working buffer for BUILDING and MATCHING a signature, before it is truncated
// into a survey row. Deliberately larger than SURVEY_SIG_LEN: matching has to
// see the whole string or it can miss the very thing it is looking for.
#define IE_SIG_MAX     96
typedef struct {
    uint8_t mac[6];
    uint32_t fp; /**< IE-skeleton hash; survives MAC randomisation */
    uint32_t fp2; /**< IE-CONTENT hash -- see ie_content_hash(). The skeleton
                    *  collided across 74% of devices in a real 120-device
                    *  capture; this one folds in the capability IEs. */
    int8_t rssi; /**< strongest seen -- closest approach */
    uint8_t ch;
    uint16_t count;
    bool used;
    char sig[SURVEY_SIG_LEN]; /**< printable IE signature; see ie_sig_string() */
} SurveyRow;
static SurveyRow g_survey[SURVEY_MAX];

static void survey_note(
    const uint8_t* mac, uint32_t fp, int8_t rssi, uint8_t ch, uint32_t fp2, const char* sig) {
    int free_slot = -1, weakest = -1;
    for(int i = 0; i < SURVEY_MAX; i++) {
        if(!g_survey[i].used) {
            if(free_slot < 0) free_slot = i;
            continue;
        }
        if(memcmp(g_survey[i].mac, mac, 6) == 0) {
            if(g_survey[i].count < 0xFFFF) g_survey[i].count++;
            // THE CHANNEL BELONGS TO THE CLOSEST APPROACH, NOT THE LAST FRAME.
            //
            // rssi already kept the best sighting while ch took the most recent
            // one, so the row described two different moments -- and the app
            // hands this channel to the Locator, which then parks the radio on
            // it. 2.4 GHz channels overlap 20 MHz on 5 MHz spacing, so a camera
            // on channel 6 is genuinely received on 2 and 10 as well; measured
            // on the bench at 30 cm, a beacon-only emitter pinned to 6 was heard
            // on 2/5/6/7/8/10/12, peaking at -20 on 6 and down at -57 on 2 and
            // 10. Whichever of those arrived last became the stored channel.
            //
            // Tying both fields to the same sighting makes the pair coherent and
            // picks the transmitter's real channel for free: the strongest
            // capture is the one where the receiver was tuned to it.
            if(rssi > g_survey[i].rssi) {
                g_survey[i].rssi = rssi; // closest approach
                g_survey[i].ch = ch;
            }
            if(fp) g_survey[i].fp = fp;
            if(fp2) g_survey[i].fp2 = fp2;
            // Keep the first non-empty signature. It is a property of the
            // device, not of the sighting, so re-copying it every probe would
            // burn cycles in the WiFi task for an identical string.
            if(sig && sig[0] && !g_survey[i].sig[0]) {
                strncpy(g_survey[i].sig, sig, SURVEY_SIG_LEN - 1);
                g_survey[i].sig[SURVEY_SIG_LEN - 1] = '\0';
            }
            return;
        }
        // Evict the LEAST seen, never the most: a persistent emitter is the one
        // worth keeping, and it is the one a camera produces.
        if(weakest < 0 || g_survey[i].count < g_survey[weakest].count) weakest = i;
    }
    int slot = (free_slot >= 0) ? free_slot : weakest;
    if(slot < 0) return;
    memset(&g_survey[slot], 0, sizeof(SurveyRow));
    memcpy(g_survey[slot].mac, mac, 6);
    g_survey[slot].fp = fp;
    g_survey[slot].fp2 = fp2;
    g_survey[slot].rssi = rssi;
    g_survey[slot].ch = ch;
    g_survey[slot].count = 1;
    g_survey[slot].used = true;
    if(sig && sig[0]) {
        strncpy(g_survey[slot].sig, sig, SURVEY_SIG_LEN - 1);
        g_survey[slot].sig[SURVEY_SIG_LEN - 1] = '\0';
    }
}

/** Stream the survey to the app. One line per transmitter, strongest first. */
static void survey_dump() {
    Serial.print("SVBEGIN\n");
    int n = 0;
    for(int i = 0; i < SURVEY_MAX; i++) {
        if(!g_survey[i].used) continue;
        Serial.printf(
            // fp2 and the printable signature are APPENDED, so a Flipper build
            // that predates them still reads the first six fields exactly as
            // before. The signature goes LAST because it contains commas: the
            // reader takes the whole remainder as the signature rather than
            // splitting it further.
            "SV,%02x%02x%02x%02x%02x%02x,%d,%u,%08lx,%u,%08lx,%s\n",
            g_survey[i].mac[0],
            g_survey[i].mac[1],
            g_survey[i].mac[2],
            g_survey[i].mac[3],
            g_survey[i].mac[4],
            g_survey[i].mac[5],
            g_survey[i].rssi,
            g_survey[i].ch,
            (unsigned long)g_survey[i].fp,
            (unsigned)g_survey[i].count,
            (unsigned long)g_survey[i].fp2,
            g_survey[i].sig);
        n++;
    }
    Serial.printf("SVEND,%d\n", n);
}

// Sequence-number-run coalescer. A MAC-cycling Flock burst sprays many probes
// from different (randomized) MACs but with a *contiguous* 802.11 sequence
// number run -- the SoC's seq counter increments across the burst regardless of
// the source address. We treat such a run as ONE logical sighting and suppress
// the duplicates on the ESP side so they never flood the Flipper's 64-entry
// table. Keyed on the IE-skeleton hash so unrelated traffic with nearby seq
// numbers isn't merged.
#define SEQ_RUN_GAP 4 // max seq-num step to still count as the same burst
#define SEQ_RUN_MS 1500 // a run older than this is stale; start fresh
static uint32_t g_seq_fp = 0; // IE hash of the current run (0 = none)
static uint16_t g_seq_last = 0; // last 802.11 sequence number in the run
static uint32_t g_seq_t = 0; // millis() of the last frame in the run

// Returns true if this frame should be SUPPRESSED as a duplicate within an
// in-progress MAC-cycling burst (same IE fingerprint, monotonic seq-num run).
static bool seq_run_duplicate(uint32_t fp, uint16_t seq) {
    if(fp == 0) return false; // no fingerprint -> can't coalesce
    uint32_t now = millis();
    bool fresh = (now - g_seq_t) <= SEQ_RUN_MS;
    if(fresh && fp == g_seq_fp) {
        uint16_t step = (uint16_t)(seq - g_seq_last); // wraps mod 4096 naturally
        if(step != 0 && step <= SEQ_RUN_GAP) {
            g_seq_last = seq; // extend the run, suppress this frame
            g_seq_t = now;
            return true;
        }
    }
    // New run (or a gap too large / stale): this frame is the run's first
    // sighting -> report it and start tracking from here.
    g_seq_fp = fp;
    g_seq_last = seq;
    g_seq_t = now;
    return false;
}

static void promisc_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if(type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
    const uint8_t* p = pkt->payload;
    // sig_len includes the 4-byte FCS; drop it so SSID bounds checks stay inside
    // the actual frame body.
    int len = pkt->rx_ctrl.sig_len;
    if(len < 28) return;
    len -= 4;

    // THE CHANNEL COMES FROM THE FRAME, NOT FROM THE HOPPER VARIABLE.
    //
    // This used to read g_channel -- where the sweep had got to by the time the
    // callback ran, which is not where the frame was received. The promiscuous
    // queue drains behind the hop, and every BLE scan window stalls it for a
    // second or more, so frames surfaced several hops late and got stamped with
    // a channel the transmitter had never used. On the bench, an emitter pinned
    // to channel 6 was logged on 7, 10 and 12.
    //
    // That is not cosmetic: the Flipper stores this channel and `locate` parks
    // the radio on it, so the Locator sat on "acquiring signal..." indefinitely
    // for a WiFi target 30 cm away. rx_ctrl.channel is the primary channel the
    // packet was actually received on, and it cannot race.
    uint8_t frame_channel = pkt->rx_ctrl.channel;
    if(frame_channel < 1 || frame_channel > 14) frame_channel = g_channel;

    g_frames++;

    uint8_t subtype = (p[0] >> 4) & 0x0F;

    // Active-attack rate sampling (counted on EVERY frame, before any Flock
    // candidacy filtering): probe-request floods and beacon-spam (many distinct
    // beaconing BSSIDs). Flushed/evaluated in the ~1 Hz status block.
    if(subtype == 0x04) {
        g_probe_reqs++; // probe request
    } else if(subtype == 0x08) {
        note_beacon_bssid(p + 16); // beacon: addr3 = BSSID
    }

    // Locator (Wi-Fi): track the strongest RSSI of any frame to/from the target
    // MAC (addr1/2/3). loop() emits it as a throttled LOC line.
    if(g_locate_kind == 'w') {
        int r = pkt->rx_ctrl.rssi;
        if(memcmp(p + 4, g_locate_mac, 6) == 0 || memcmp(p + 10, g_locate_mac, 6) == 0 ||
           memcmp(p + 16, g_locate_mac, 6) == 0) {
            if(r > g_locate_best) g_locate_best = r;
        }
    }

    // Deauthentication (0x0C) / disassociation (0x0A) frames: a flood of these
    // is the signature of a deauth attack or an evil-twin kicking clients off.
    if(subtype == 0x0C || subtype == 0x0A) {
        g_deauths++;
        // Attribution: report the targeted BSSID (addr3) + channel, rate-limited
        // so a heavy flood can't saturate the UART.
        uint32_t now_da = millis();
        if(now_da - g_last_da >= 250) {
            g_last_da = now_da;
            const uint8_t* b = p + 16; // addr3 = BSSID
            Serial.printf(
                "DA,%02x%02x%02x%02x%02x%02x,%u\n",
                b[0], b[1], b[2], b[3], b[4], b[5], frame_channel);
        }
    }

    char ftype = 'O';
    const char* ssid = NULL;
    int ssid_len = 0;

    // Locate SSID element (tag 0) within tagged parameters.
    int tag_off = -1;
    if(subtype == 0x04) { // probe request
        ftype = 'P';
        tag_off = 24;
    } else if(subtype == 0x08) { // beacon
        ftype = 'B';
        tag_off = 36;
    } else if(subtype == 0x05) { // probe response
        ftype = 'R';
        tag_off = 36;
    }
    bool ssid_ie_found = false;
    if(tag_off >= 0 && tag_off + 2 <= len && p[tag_off] == 0x00) {
        ssid_ie_found = true;
        ssid_len = p[tag_off + 1];
        if(tag_off + 2 + ssid_len <= len) {
            ssid = (const char*)(p + tag_off + 2);
        } else {
            // The IE claims more bytes than the frame holds -- a truncated or
            // malformed capture. Retract the whole finding, don't just zero the
            // length: leaving ssid_ie_found set made the all-NUL scan below pass
            // vacuously (zero bytes to disagree with it) and every truncated
            // frame got reported as a hidden network. A parse miss is not
            // evidence of concealment.
            ssid_ie_found = false;
            ssid_len = 0;
        }
    }

    // Hidden-SSID beaconing: an AP that advertises but withholds its name. Two
    // encodings are legal and both appear in the wild -- a zero-length SSID IE,
    // and a length-N IE of all NULs -- so test for both.
    //
    // Only meaningful for beacons and probe RESPONSES: those identify an AP. A
    // probe REQUEST with no SSID is an ordinary wildcard scan from a client and
    // says nothing about hiding. And we only claim "hidden" when the SSID IE was
    // actually located: a parse miss is not evidence of concealment.
    bool hidden = false;
    if(ssid_ie_found && (subtype == 0x08 || subtype == 0x05)) {
        hidden = true;
        for(int i = 0; i < ssid_len; i++) {
            if(ssid[i] != '\0') {
                hidden = false;
                break;
            }
        }
    }

    // REMOTE ID OVER WI-FI. Checked on beacons and probe responses, before the
    // Flock scoring ladder and independently of it: an aircraft is not a camera
    // and must not be scored as one. Same RID line as the BLE path, so the app
    // decodes both with the one host-tested decoder.
    if(subtype == 0x08 || subtype == 0x05) {
        int rid_len = 0;
        const uint8_t* rid = odid_find_wifi_ie(p, len, tag_off, &rid_len);
        if(rid) {
            if(rid_len > 64) rid_len = 64;
            char rl[176];
            size_t rp = snprintf(
                rl,
                sizeof(rl),
                "RID,%02x%02x%02x%02x%02x%02x,%d,0d00",
                p[10],
                p[11],
                p[12],
                p[13],
                p[14],
                p[15],
                pkt->rx_ctrl.rssi);
            // The app's parser expects the BLE framing (application code then a
            // counter) before the messages, so the two transports converge on one
            // wire format and one decoder. Synthesised here rather than teaching
            // the app a second shape.
            for(int j = 0; j < rid_len && rp + 2 < sizeof(rl); j++) {
                buf_appendf(rl, sizeof(rl), &rp, "%02x", rid[j]);
            }
            if(rp > sizeof(rl) - 1) rp = sizeof(rl) - 1;
            rl[rp++] = '\n';
            Serial.write((const uint8_t*)rl, rp);
        }
    }

    int s_score = ssid ? ssid_score(ssid, ssid_len) : 0;
    bool oui_tx = oui_match(p + 10); // addr2 = transmitter
    bool oui_rx = oui_match(p + 4); // addr1 = receiver (silent station)
    // Vendor-exclusive competitor prefixes, tracked separately because they earn
    // a different (lower) rung than the Flock tables -- see the ladder below.
    bool ven_tx = vendor_oui_match(p + 10);
    bool ven_rx = vendor_oui_match(p + 4);
    bool is_probe = (ftype == 'P');
    bool wildcard = is_probe && (ssid_len == 0); // broadcast/wildcard probe

    // THE SSID BELONGS TO THE TRANSMITTER. THE MAC WE REPORT MAY NOT.
    //
    // On a receive-side match we deliberately report addr1, the silent device
    // the frame was addressed TO -- that is the whole point of the rx rungs and
    // it is how a dormant camera gets caught. But the SSID in the body was put
    // there by whoever SENT the frame. Pairing them names one device with
    // another device's network.
    //
    // Seen on the bench: a Motorola-OUI station receiving probe responses was
    // stored as `00:04:7D:00:00:0C,WiFi` -- the address is the Motorola device,
    // the name is a neighbour's access point. Two devices in one row.
    //
    // s_score IS ZEROED TOO, and that is the half that matters. The name also
    // SCORES: s_score == 3 sets conf = 3 outright, which the Flipper maps to
    // CONFIRMED. So an access point named like a Flock unit, answering a probe
    // from any device with a tracked OUI, would have confirmed that device on a
    // name belonging to something else. Dropping the SSID from the wire alone
    // would have made it worse, not better -- the Flipper only re-derives a
    // claimed CONFIRMED when it HAS an SSID to re-derive it from, so an empty
    // one would have sailed straight through.
    //
    // Nothing is lost: on the rx side we genuinely know the address and not the
    // name, and saying so is the honest answer.
    bool rx_side = !(oui_tx || ven_tx) && (oui_rx || ven_rx);
    if(rx_side) {
        ssid = NULL;
        ssid_len = 0;
        s_score = 0;
    }


    // Count this probe against its transmitter BEFORE the coalescer downstream
    // suppresses repeats. Only the transmitter is rate-tracked: on an oui_rx hit
    // the frame was sent TO the Flock-OUI device by someone else, so the cadence
    // belongs to that someone else and says nothing about the receiver.
    // Counted for REPORTING ONLY -- see the note on probe_rate_bump(). This used
    // to gate the OUI+probe rungs below; it does not any more.
    // SURVEY: record every wildcard-probe emitter, matched or not. Deliberately
    // BEFORE the scoring ladder, because the whole point is to see the devices
    // the ladder rejects -- a camera on an OUI we do not carry, or one using a
    // randomised MAC, is invisible everywhere else.
    //
    // The content hash and the printable signature are computed ONCE here and
    // reused by the ladder and the D-line below, so the IEs are not walked three
    // times per frame inside the WiFi task.
    uint32_t ie_fp2 = 0;
    bool sig_hit = false;
    // MATCHED AT FULL LENGTH, STORED TRUNCATED. The known signature is 44
    // characters and sits at the END of a probe's tag list, so matching against
    // a SURVEY_SIG_LEN-sized copy would compare against a string whose tail had
    // already been cut off -- the match would fail on exactly the frames it
    // exists to catch. survey_note() does the truncation for storage.
    char ie_sig[IE_SIG_MAX];
    ie_sig[0] = '\0';
    if(is_probe && wildcard) {
        ie_fp2 = ie_content_hash(p, len);
        ie_sig_string(p, len, ie_sig, sizeof(ie_sig));
        sig_hit = flock_sig_match(ie_sig);
        survey_note(
            p + 10, ie_skeleton_hash(p, len), pkt->rx_ctrl.rssi, frame_channel, ie_fp2, ie_sig);
    }

    uint8_t probe_rate = 0;
    if(is_probe) probe_rate = probe_rate_bump(p + 10); // addr2 = transmitter

    // A NAME IN A PROBE REQUEST BELONGS TO THE NETWORK SOUGHT, NOT THE SENDER.
    // Beacons and probe responses carry the transmitter's own SSID. A probe
    // request carries the one the sender is LOOKING FOR, so a phone that once
    // joined a camera's provisioning AP probes for "Flock-A1B2C3" from its own
    // address and would have gone out as conf=3 under the phone's MAC. Same
    // attribution error as the rx_side block above, from the other direction.
    // The Flipper applies the same cap at its trust boundary; this keeps the
    // wire honest for anything else that reads it.
    if(is_probe && s_score == 3) s_score = 2;

    int conf = 0;
    if(s_score == 3)
        conf = 3; // confirmed Flock SSID name (beacon / probe response only)
    else if(oui_tx && wildcard)
        conf = 2; // OUI + wildcard probe -> "likely". FLOCK_OUIS is mostly shared
                  // silicon-vendor ranges, so reserve conf=3 for an SSID-name or
                  // IE-fp match. This is what upstream runs, and what its field
                  // test measured at 11/12 cameras with 2 false positives.
    else if(oui_tx && is_probe)
        conf = 2; // same for a directed probe from the OUI device itself
    else if(oui_rx && (is_probe || ftype == 'R'))
        conf = 2; // OUI on the SILENT RECEIVER: a frame addressed to a Flock-OUI
                  // device by some other station. Deliberately NOT rate-gated --
                  // the rate would be the sender's, not this device's, and this is
                  // upstream's key technique for catching a dormant camera that is
                  // not transmitting at all. Narrower and rarer than the tx paths.
                  //
                  // 'R' (probe RESPONSE) added alongside the request case. An AP
                  // only answers a station that probed it moments ago, so a
                  // response addressed to a Flock-OUI address is a station that
                  // is present and transmitting right now -- we simply missed
                  // its own probe, typically because the hopper was elsewhere.
                  // That is at least as strong as a directed request addressed
                  // to the OUI, which a client can send for a BSSID it merely
                  // remembers. Same rung, no new class of evidence.
    else if(s_score == 2)
        conf = 2;
    else if(ven_tx && wildcard && probe_rate >= VENDOR_PROBE_SUSTAINED)
        conf = 2; // VENDOR-EXCLUSIVE OUI + SUSTAINED WILDCARD PROBING -> "likely".
                  //
                  // WHAT THIS RUNG SEPARATES, and it is not what it looks like.
                  // It does NOT distinguish an ALPR from other fixed gear; the
                  // class stays Gear and the vendor is still all we know. What
                  // it distinguishes is FIXED INFRASTRUCTURE from a HANDHELD, on
                  // a prefix that covers both.
                  //
                  // Motorola Solutions is the case that prompted it: they sell
                  // ALPR poles and hand-portable radios on one OUI, so the
                  // vendor alone genuinely cannot say which is in front of you,
                  // and until now a beacon and 8 Hz wildcard probing both came
                  // out "possible". They are not the same observation. A battery
                  // handheld cannot emit wildcard probes every ~125 ms for hours
                  // -- and a radio provisioning over WiFi is looking for a KNOWN
                  // network, which is a directed probe, not a wildcard one. A
                  // mains or PoE powered pole phoning home does exactly this,
                  // forever. Same reasoning the Flock rungs above already use.
                  //
                  // ven_tx ONLY, never ven_rx: on a receive-side match the frame
                  // was sent TO the vendor device by somebody else, so the
                  // cadence belongs to that somebody else and says nothing about
                  // the device we are scoring.
    else if(ven_tx || ven_rx)
        conf = 1; // VENDOR-EXCLUSIVE OUI, any frame type -> "possible".
                  //
                  // THE ONE BARE-OUI RUNG THAT SURVIVES, AND ONLY FOR THESE
                  // TABLES. Read the next comment block first: bare-OUI scoring
                  // was removed because FLOCK_OUIS is mostly Liteon/Espressif,
                  // so "beacons, and has one of these OUIs" described a huge
                  // population of ordinary consumer devices and reported a
                  // T-Mobile gateway as a possible camera.
                  //
                  // That reasoning does not transfer. 94:7b:be is registered to
                  // Ubicquia, who make streetlight nodes and nothing else;
                  // e0:a7:00 is Verkada's own. There is no consumer population
                  // hiding behind these prefixes to generate the false positives
                  // that killed the Flock version of this rung.
                  //
                  // Nor is a beacon disqualifying here the way it is for Flock.
                  // A Flock camera is a station that does not beacon, so an OUI
                  // hit on a beacon is by construction not one. A Ubicquia UbiHub
                  // IS an access point -- beaconing is its normal behaviour --
                  // so requiring probe-request behaviour would reject the very
                  // frames this table exists to catch.
                  //
                  // Capped at 1 and never promoted: registry-verified is not
                  // field-observed, and no unit of this hardware has been seen on
                  // the air. The Flipper names the VENDOR from the same tables,
                  // so the operator gets "Ubicquia streetlight / Possible"
                  // rather than a mystery -- an attributable lead to go verify by
                  // eye, which is all a "possible" was ever meant to be.
    // NO conf=1 FOR A BARE OUI MATCH ANY MORE -- for FLOCK_OUIS, that is.
    //
    // A Flock camera is not an access point. Flock's management AP was
    // deactivated around December 2025 and the cameras moved to station mode --
    // they now emit wildcard PROBE REQUESTS roughly every 125 ms and do not
    // beacon (see the upstream flock-you research this OUI list comes from).
    // So an OUI hit on a BEACON is, by construction, not a camera. It is some
    // other product built on the same silicon.
    //
    // And this list is mostly shared silicon-vendor ranges -- Espressif, Liteon
    // and friends -- so "beacons, and has one of these OUIs" describes an
    // enormous number of ordinary consumer devices. It reported a T-Mobile
    // gateway (SSID "tmobile-5416") as a possible ALPR camera, which is exactly
    // the failure this project says it will not accept: a false positive is
    // worse than a missed detection.
    //
    // Nothing real is lost. Anything that IS a camera still reaches conf=2 via
    // the probe-request branches above, and conf=3 via an SSID name or IE
    // fingerprint. The only sightings dropped are OUI-only ones with no probe
    // behaviour and no name -- which is precisely the set that cannot be
    // distinguished from an unrelated device sharing a chip vendor.

    // A KNOWN COMMUNITY SIGNATURE IS A DETECTION IN ITS OWN RIGHT.
    //
    // Every rung above needs an OUI match or a Flock SSID, so a camera on a
    // RANDOMISED address scored 0 and was dropped right here -- before the
    // fingerprint was even computed. That is the structural reason a camera an
    // operator was parked in front of stayed invisible through issue #25, and no
    // amount of fingerprint curation fixes it while this return sits in front of
    // the fingerprint.
    //
    // The score is NOT raised here. conf stays whatever the ladder decided,
    // possibly 0, and the match travels as `sg=1` for the Flipper to weigh:
    // conf=3 on this wire means CONFIRMED, and a single-source community
    // signature must never auto-confirm. helpers/esp_parser.c caps it at
    // "Class?", exactly as it caps a candidate fingerprint.
    if(conf == 0 && !sig_hit) return; // not a candidate; drop to keep UART quiet

    // B1: fingerprint the probe body (MAC-independent device-class signature)
    // and coalesce MAC-cycling bursts via the 802.11 sequence-number run, so a
    // randomized-MAC spray collapses to one logical sighting before it can flood
    // the Flipper's 64-entry table. Only probe requests carry a meaningful IE
    // skeleton. The coalescer runs only on candidate frames so unrelated noise
    // can't capture the run slot and suppress a real detection.
    uint32_t ie_fp = 0;
    if(is_probe) {
        ie_fp = ie_skeleton_hash(p, len);
        // 802.11 sequence control: bytes 22-23, seq number is the top 12 bits.
        uint16_t seq = ((uint16_t)p[23] << 8 | p[22]) >> 4;
        if(seq_run_duplicate(ie_fp, seq)) return; // duplicate in an active burst
    }

    g_hits++;
    if(!g_scanning) return;

    // Report the Flock device's MAC: the transmitter if it matched, else the
    // silent receiver (addr1).
    const uint8_t* mac = (oui_tx || ven_tx) ? (p + 10) : ((oui_rx || ven_rx) ? (p + 4) : (p + 10));

    char macstr[13];
    snprintf(
        macstr,
        sizeof(macstr),
        "%02x%02x%02x%02x%02x%02x",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);

    // Build the whole D-line in one buffer + single write: promisc_cb runs in the WiFi
    // task, so a multi-call line could be split on the UART by loop()'s status lines.
    char line[160];
    size_t pos =
        snprintf(line, sizeof(line), "D,%s,%d,%u,%c,%d,", macstr, pkt->rx_ctrl.rssi, frame_channel, ftype, conf);
    if(ssid && ssid_len > 0) buf_append_escaped(line, sizeof(line), &pos, ssid, ssid_len, 48);
    // B1: trailing IE-fingerprint field (probe requests only). Older parsers
    // ignore it; the Flipper matches it against a curated Flock IE-fp table.
    if(ie_fp != 0) buf_appendf(line, sizeof(line), &pos, ",fp=%08x", ie_fp);
    // fp2 is NOT sent here. The content hash is a COLLECTION field: it goes out
    // on the SV line, where it reaches survey.csv and can be analysed. Repeating
    // it on every detection would add wire traffic for a value the Flipper has
    // nothing to match it against yet, since the fp2 signature table is empty
    // until field captures populate it.
    //
    // Matched a known community probe signature. MAC-independent, which is the
    // whole point -- this is the one tell that fires on a randomised address.
    if(sig_hit) buf_appendf(line, sizeof(line), &pos, ",sg=1");
    // Device class. Only emitted for the non-default (acoustic) case: absent
    // means ALPR, so the wire stays unchanged for every existing detection and
    // an older Flipper build just ignores the token.
    if(probe_rate) buf_appendf(line, sizeof(line), &pos, ",pr=%u", (unsigned)probe_rate);
    if(st_oui_match(mac)) buf_appendf(line, sizeof(line), &pos, ",cls=a");
    else if(ax_oui_match(mac)) buf_appendf(line, sizeof(line), &pos, ",cls=x");
    // cls=g: vendor-exclusive competitor gear. Vendor known, KIND not -- these
    // OUIs carry plate readers, building cameras and hand-held radios alike, so
    // claiming "ALPR" would invent a detection. The Flipper names the vendor
    // from its own copy of these tables.
    else if(vendor_oui_match(mac)) buf_appendf(line, sizeof(line), &pos, ",cls=g");
    // Hidden-SSID attribute. Rides on a line we were already sending, so it adds
    // no UART traffic and needs no per-BSSID dedup of its own. Reported, NOT
    // scored: see the note in helpers/esp_parser.c.
    if(hidden) buf_appendf(line, sizeof(line), &pos, ",hid=1");
    if(pos > sizeof(line) - 1) pos = sizeof(line) - 1;
    line[pos++] = '\n';
    Serial.write((const uint8_t*)line, pos);
}

static void set_channel(uint8_t ch) {
    g_channel = ch;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

static void start_promisc() {
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&promisc_cb);
    esp_wifi_set_promiscuous(true);
    set_channel(g_channel);
}

/**
 * The companion's own BUILD version, distinct from the wire-protocol version.
 *
 * WHY BOTH. "FLOCKCO,1" is the PROTOCOL version -- it answers "can these two
 * talk". It does not answer "which firmware is on this board", and until now
 * nothing did: the only label a flashed companion had was the filename of the
 * .bin somebody picked on the SD card, which cannot be verified after the fact
 * and is routinely wrong. One card here held companion_forensic.bin,
 * companion_gatefix.bin, companion_survey.bin and companion_ungated.bin -- none
 * of which say which build they are -- alongside companion_v073/v077/v087, whose
 * labels nobody can check.
 *
 * That is not cosmetic. A whole hardware validation session was run against a
 * companion nobody could identify, and the app's CLAUDE.md carries a HARD RULE
 * about the same failure on the Flipper side (redeploy after a version bump,
 * because the .fap reports the old version while running the new code). This is
 * that rule's missing half.
 *
 * MUST equal FAP_VERSION in application.fam: the two halves are built, flashed
 * and tested as a pair, and a companion left over from a different release is
 * precisely what this exists to expose. tools/check_oui_parity.py fails CI if
 * they drift.
 */
#define FLOCK_COMPANION_VERSION "0.99"
// First eight hex digits of SHA-256 over the newline-joined PRODUCTION entries
// in FLOCK_SIG_TABLE. tools/check_oui_parity.py derives and enforces it, and the
// Flipper compares this advertised value with FDF_SIGNATURE_REVISION.
#define FLOCK_SIGNATURE_REVISION "96274812"

static void banner() {
    // Third field is the BUILD version. Appending is wire-safe: an older app
    // splits this line with max=2, so esp_split_fields() glues "1,0.88" into one
    // field and atoi() still reads 1 as the protocol version. It sees no change.
    Serial.print("FLOCKCO,1," FLOCK_COMPANION_VERSION "\n");
    // Separate record for backward compatibility: older apps ignore it, while
    // appending it to FLOCKCO would be folded into the build field by parsers
    // that intentionally split that banner into at most three fields.
    Serial.print("SIGREV," FLOCK_SIGNATURE_REVISION "\n");
    // What this chip actually is, so the app stops offering a classic ESP32's
    // pinout on every board. Sent as its own line rather than appended to the
    // banner: an older app ignores lines it does not know, but a changed banner
    // would trip its wire-protocol version check.
    //   CHIP,<target>,<gpio_count>,<usable_gps_pin_mask_hi>,<lo>,<has5g>
    uint64_t m = gps_usable_mask();
    Serial.printf(
        "CHIP,%s,%d,%08lx,%08lx,%d\n",
        CONFIG_IDF_TARGET,
        (int)SOC_GPIO_PIN_COUNT,
        (unsigned long)(m >> 32),
        (unsigned long)(m & 0xFFFFFFFFULL),
        FLOCK_HAS_5GHZ);
}

// One-shot WiFi security scan for the FlipDeFlock audit. Switches out of
// promiscuous Flock mode, runs an active esp_wifi_scan (which yields the auth
// mode + ciphers + WPS that Marauder never emits over serial), streams one
// "W," line per AP, then restores Flock promiscuous mode.
//   W,<bssid>,<rssi>,<ch>,<authmode>,<pairwise>,<group>,<wps>,<ssid>
static void wifi_security_scan() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    wifi_scan_config_t sc = {};
    sc.show_hidden = true;
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    Serial.print("WBEGIN\n");
    uint16_t num = 0;
    if(esp_wifi_scan_start(&sc, true) == ESP_OK) {
        esp_wifi_scan_get_ap_num(&num);
        if(num > 64) num = 64;
        wifi_ap_record_t* recs =
            (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * (num ? num : 1));
        if(recs) {
            uint16_t got = num;
            if(esp_wifi_scan_get_ap_records(&got, recs) == ESP_OK) {
                for(uint16_t i = 0; i < got; i++) {
                    wifi_ap_record_t* r = &recs[i];
                    char bss[13];
                    snprintf(
                        bss,
                        sizeof(bss),
                        "%02x%02x%02x%02x%02x%02x",
                        r->bssid[0], r->bssid[1], r->bssid[2],
                        r->bssid[3], r->bssid[4], r->bssid[5]);
                    char line[160];
                    size_t pos = snprintf(
                        line, sizeof(line), "W,%s,%d,%u,%d,%d,%d,%d,",
                        bss, r->rssi, r->primary, (int)r->authmode,
                        (int)r->pairwise_cipher, (int)r->group_cipher, r->wps ? 1 : 0);
                    const char* s = (const char*)r->ssid;
                    int sl = 0;
                    while(sl < 32 && s[sl]) sl++; // r->ssid is NUL-terminated
                    buf_append_escaped(line, sizeof(line), &pos, s, sl, 32);
                    if(pos > sizeof(line) - 1) pos = sizeof(line) - 1;
                    line[pos++] = '\n';
                    Serial.write((const uint8_t*)line, pos);
                }
            }
            free(recs);
        }
    }
    Serial.printf("WEND,%u\n", num);

    // Back to Flock detection.
    esp_wifi_set_mode(WIFI_MODE_NULL);
    start_promisc();
}

// One-shot BLE scan for the anti-tracker / BLE-Flock feature. Stops WiFi to free
// the radio, active-scans a few seconds, classifies each device, then restores
// WiFi/Flock mode.
//   BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1][,sep=1]
//   cat: 0 unknown  1 Flock/Raven  2 AirTag/FindMy  3 Tile  4 SmartTag  7 Axon
//   mfghex: raw manufacturer-specific data as hex (Flock 0x09C8 only), so the
//   Flipper can decode the device serial; trailing field, older parsers ignore.
//   rv=1: device exposed a Raven-specific GATT service (0x3100-0x3500) -> a
//   positive Raven (acoustic sensor) ID. Emitted AFTER mfghex when both apply;
//   contains '=' so the Flipper tells it apart from mfghex. Older parsers ignore.
//   sep=1: Apple Find My tracker was in separated-state payload form. This is
//   an indicator only; the app still requires repeated sightings over distance.
static void ble_action_emit(const char* op, const char* status, int rssi, bool have_rssi) {
    if(have_rssi)
        Serial.printf("ACT,%s,%s,%d\n", op, status, rssi);
    else
        Serial.printf("ACT,%s,%s\n", op, status);
}

#if FLOCK_HAS_BLE

static void ble_ensure_init() {
    if(g_ble_inited) return;
    BLEDevice::init("");
    g_ble = BLEDevice::getScan();
    g_ble->setActiveScan(true);
    g_ble->setInterval(80); // interval > window so BLE doesn't hog the radio
    g_ble->setWindow(60);
    g_ble_inited = true;
}

// Serialised BLE scan: toggles WiFi promiscuous OFF for the scan, then back ON
// (BLE stays resident). Classifies Flock/Raven by mfg id 0x09C8, device name
// (Penguin* / FS Ext Battery), Raven custom service UUIDs (0x3100-0x3500), or a
// Flock OUI on the BLE address; plus validated AirTag/Tile/SmartTag. Emits
// BBEGIN/BLE/BEND. Weak tracker adverts below BLE_TRACKER_MIN_RSSI are omitted:
// they are not useful evidence that a tag is travelling with the operator.
// Defined further down with the BLE action helpers, but needed by the Raven GATT
// check below: it iterates EVERY advertised service UUID, which is the whole
// point -- the singular accessor missed Ravens that list 0x3100 second.
static bool ble_action_has_service(BLEAdvertisedDevice& d, const char* token);

// "FS-" + EXACTLY six hex digits and nothing else -- Flock's post-"Penguin" unit
// id. Mirrors is_fs_unit_name() in helpers/flock_ble.c byte for byte.
//
// Shaped rather than a bare "FS-" prefix on purpose: two letters and a dash is
// not evidence, and this classification reaches the app as cat=1, which its BLE
// path can only score Confirmed or Possible -- there is no Likely rung to demote
// into. Same reasoning that keeps a loose "flock" substring off this path.
static bool ble_name_is_fs_unit(const std::string& nm) {
    if(nm.size() != 9) return false;
    if(!((nm[0] == 'F' || nm[0] == 'f') && (nm[1] == 'S' || nm[1] == 's') && nm[2] == '-')) {
        return false;
    }
    for(size_t i = 3; i < 9; i++) {
        if(!isxdigit((unsigned char)nm[i])) return false;
    }
    return true;
}


/**
 * True if the raw advertising payload contains the ASCII tag "BWCDEVICE".
 *
 * Axon body-worn cameras carry this in their BLE service data. It is a
 * MAC-INDEPENDENT, positive identification of a body camera specifically, rather
 * than "some device on Axon's OUI" -- which is all a prefix match can ever say,
 * and which is worth much less now that address randomisation is routine.
 *
 * Searched across the WHOLE payload rather than inside a parsed service-data
 * element on purpose. The tag is a fixed nine-byte ASCII string with no ordinary
 * meaning, so a substring hit is already specific; parsing the exact element
 * would add a second thing to get wrong for no gain in precision. It is scored
 * app-side, where it can be weighed against the OUI rather than replacing it.
 *
 * Corroboration: field-validated 2026-07-19 by soyboi1312/all-cameras-are-beacons,
 * which scores it 90 against 75 for the bare Axon OUI. We hold 00:25:df already;
 * this is the tell that says WHAT the device is.
 */
static bool ble_str_has_bwc(const std::string& s) {
    return s.find("BWCDEVICE") != std::string::npos;
}

static void ble_do_scan(int seconds) {
    ble_ensure_init();
    esp_wifi_set_promiscuous(false);

    Serial.print("BBEGIN\n");
    if(seconds < 1) seconds = 1;

    // Run the scan as 1-second slices, draining the GPS between each.
    //
    // BLEScan::start() blocks, so one 3 s call also stops loop() for 3 s -- and
    // loop() is what empties Serial1. At 9600 baud that is ~2.9 KB of NMEA
    // arriving against the RX buffer, so sentences were dropped outright; even
    // the survivors left the fix up to ~4 s old. At 50 km/h a 4 s old fix
    // geotags a camera ~55 m from where it really is, which is worse than
    // useless on a DeFlock submission.
    //
    // is_continue = true keeps the library's accumulated results, so total
    // dwell, dedup and the reported device list are preserved. Not quite free:
    // each slice restarts GAP scanning, so a few milliseconds of advert time is
    // lost per boundary (two boundaries at the default 3 s). BLE advertisers
    // repeat every 20-100 ms, so that is far below the noise floor of whether a
    // given device is seen at all -- and a fix that is 1 s old instead of 4 s is
    // worth much more than those milliseconds.
    //
    // 1 s is the floor because start() takes whole seconds.
    for(int i = 0; i < seconds - 1; i++) {
        FLOCK_SCAN_CONT(g_ble, 1, i > 0);
        gps_poll();
    }
    // Last slice returns the accumulated results. is_continue only when earlier
    // slices actually ran, so a 1-second scan still starts from a clean list.
    BLEScanResults found = FLOCK_SCAN_CONT(g_ble, 1, seconds > 1);
    gps_poll();
    int count = found.getCount();
    if(count > 80) count = 80;
    int spam = 0; // impersonation/pairing adverts -> BLE-spam flood indicator
    for(int i = 0; i < count; i++) {
        BLEAdvertisedDevice d = found.getDevice(i);

        int company = -1;
        int cat = 0;
        int rssi = d.getRSSI();
        bool tracker_separated = false;
        // raven: set when this device exposes a Raven-specific GATT service
        // (0x3100-0x3500). Tracked separately from cat because cat=1 also covers
        // the shared battery / Penguin / OUI cases -- only the GATT match is a
        // positive Raven (acoustic) ID, so we surface it as its own rv=1 field.
        bool raven = false;
        if(d.haveManufacturerData()) {
            std::string md = fstr(d.getManufacturerData());
            if(md.length() >= 2) company = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
            if(company == 0x09C8) {
                cat = 1; // Flock Safety / Raven
            } else if(company == 0x034D) {
                // Axon Enterprise, filed with the SIG under their former name
                // TASER International. Its own category, NOT Flock's: the two are
                // unrelated vendors and merging them would have the app report a
                // body camera as Flock hardware.
                cat = 7;
            } else if(company == APPLE_FIND_MY_COMPANY_ID) {
                AppleFindMyAdvert advert;
                if(apple_find_my_decode(
                       (const uint8_t*)md.data(), md.size(), &advert) &&
                   apple_find_my_is_tracker(&advert)) {
                    cat = BLE_TRACKER_CAT_AIRTAG; // AirTag / licensed Find My accessory
                    tracker_separated = advert.separated;
                }
            }
        }
        if(cat != 1 && d.haveName()) {
            std::string nm = fstr(d.getName());
            // MUST stay in step with flock_ble_name_is_flock() in
            // helpers/flock_ble.c, which is where the app re-derives confidence.
            // Anything this side misses reaches the Flipper as cat=0 and is never
            // reconsidered, so a name only the app knows about is a name that
            // finds nothing.
            // "Penguin-" WITH the dash: unit names are "Penguin-<digits>", and
            // the bare word is a product name other people use. Mirrors
            // helpers/flock_ble.c, which stakes CONFIRMED on this.
            if(nm.rfind("Penguin-", 0) == 0 || nm.find("FS Ext") != std::string::npos ||
               nm.rfind("Pigvision", 0) == 0 || nm.rfind("FlockCam", 0) == 0 ||
               nm.rfind("RWLS-", 0) == 0 || ble_name_is_fs_unit(nm))
                cat = 1; // Flock Penguin battery / FS external battery / field-observed names
        }
        if(d.haveServiceUUID()) {
            std::string u = fstr(d.getServiceUUID().toString());
            // RAVEN GATT: check EVERY advertised service UUID, not just the first.
            //
            // This used to read only getServiceUUID() -- the singular accessor,
            // index 0 -- so a Raven advertising 0x3100 anywhere but first was
            // missed outright: no cat=1, no rv=1, no detection, no alert. A
            // silent recall hole against real hardware, and the one bug in this
            // sweep that costs whole devices rather than a rung.
            //
            // ble_action_has_service() below already iterates the full list with
            // the same substring semantics, and is already compiling on both core
            // 2.0.x and 3.x, so the multi-UUID API is proven available. Substring
            // matching is kept exactly as it was: anchoring would be a recall
            // change, which is out of scope here.
            if(ble_action_has_service(d, "00003100") ||
               ble_action_has_service(d, "00003200") ||
               ble_action_has_service(d, "00003300") ||
               ble_action_has_service(d, "00003400") ||
               ble_action_has_service(d, "00003500")) {
                cat = 1; // Raven custom GATT services
                raven = true; // Raven-specific GATT -> positive acoustic-sensor ID
            }
            else if(cat == 0 && (u.find("feed") != std::string::npos || u.find("feec") != std::string::npos))
                cat = 3; // Tile
            else if(cat == 0 && u.find("fd5a") != std::string::npos)
                cat = 4; // Samsung SmartTag
            else if(cat == 0 && u.find("feaa") != std::string::npos)
                cat = 5; // Google Find My Device network (Pebblebee/Chipolo/Moto/Eufy)
            else if(cat == 0 &&
                    (u.find("fd44") != std::string::npos || u.find("fcb2") != std::string::npos))
                cat = BLE_TRACKER_CAT_AIRTAG; // Apple/DULT Find My accessory service
        }
        // AXON BODY-WORN CAMERA, by its own service-data tag rather than by a
        // MAC prefix. Checked before the Flock-OUI fallback below so a body cam
        // is never mislabelled as a camera: cat=7 is Axon, cat=1 is Flock, and
        // announcing one as the other is the over-claim the class enum exists to
        // prevent. Emitted as bwc=1 so the app can tell "Axon OUI" from "an Axon
        // body camera said so".
        // Same dangling-getPayload() trap as the Remote ID block below: search
        // the COPIED service-data and manufacturer-data strings instead.
        bool bwc = false;
        {
            int nsd = d.getServiceDataCount();
            for(int si = 0; si < nsd && !bwc; si++) {
                std::string sd = fstr(d.getServiceData(si));
                if(ble_str_has_bwc(sd)) bwc = true;
            }
            if(!bwc && d.haveManufacturerData()) {
                bwc = ble_str_has_bwc(fstr(d.getManufacturerData()));
            }
        }
        if(bwc) cat = 7;
        // Utility, Inc "BodyWorn" cameras name themselves in the advert. A NAME
        // tell, so it survives MAC randomisation the way the OUI table cannot.
        // Lands in the same body-worn class; the VENDOR is re-derived app-side
        // from the OUI, so a Utility unit on a Utility block reads "Utility
        // BodyWorn" and one on a randomised address reads the honest generic
        // "Body/in-car kit" rather than claiming Axon.
        if(cat == 0 && d.haveName()) {
            std::string nm = fstr(d.getName());
            if(nm.find("BodyWorn Remote") != std::string::npos) cat = 7;
        }

        if(cat == 0 && d.getAddressType() == BLE_ADDR_TYPE_PUBLIC) {
            // PUBLIC addresses only. A random (static, RPA or NRPA) address has
            // no registered OUI -- its first three bytes are whatever the stack
            // rolled -- so a 3-byte match there is a coincidence, not a vendor.
            // Phones rotate RPAs constantly, which made this fallback a steady
            // source of Possible rows that named nothing.
            BLEAddress ba = d.getAddress();
            const uint8_t* nat = fble_addr_bytes(ba); // shape differs 2.x vs 3.x
            if(nat && oui_match(nat)) cat = 1; // Flock OUI on the BLE address
        }

        // Apple/Tile/Samsung/Google pairing adverts are what BLE-spam tools
        // (Flipper "BLE spam", ESP32 sour-apple, etc.) impersonate in bulk. A few
        // are normal; a flood of them in one scan is the spam signature.
        if(ble_tracker_category_is_known((uint8_t)cat)) spam++;

        // A distant tracker is still a BLE observation, but it cannot support
        // the anti-stalking inference. Do not send it across the wire where it
        // would consume a table slot or help a device clear the following gate.
        if(ble_tracker_category_is_known((uint8_t)cat) &&
           !ble_tracker_rssi_is_usable((int8_t)rssi)) {
            continue;
        }

        std::string a = fstr(d.getAddress().toString());
        char addr[13];
        int k = 0;
        for(size_t j = 0; j < a.size() && k < 12; j++) {
            if(a[j] != ':') addr[k++] = a[j];
        }
        addr[k] = 0;

        // REMOTE ID (ASTM F3411). Emitted as its own line, BEFORE the BLE line
        // and independently of `cat`, because an aircraft is not a Flock device
        // and must not be filed as one. The bytes go across verbatim as hex; all
        // decoding is app-side in helpers/open_drone_id.c.
        //
        // This is the only detection path that reaches the drones a US police
        // department actually flies. Checked against the IEEE registry
        // 2026-09-07: of Skydio, BRINC, Aerodome, Flock and Paladin, only Skydio
        // holds a block at all -- so for three of the five there is no MAC prefix
        // to match, ever. Remote ID is a legal broadcast mandate, so it works
        // regardless of vendor, and it carries the OPERATOR's position.
        //
        // USES THE PARSED SERVICE-DATA ACCESSORS, NOT getPayload().
        //
        // getPayload() looks like the obvious way to do this and is a TRAP:
        // BLEAdvertisedDevice::parseAdvertisement() stores `m_payload = payload`,
        // a bare POINTER into the ESP-IDF GAP event buffer, and never copies it.
        // We iterate results AFTER the scan has finished, by which time that
        // buffer is long gone -- so getPayload() is a dangling pointer and the
        // walk reads whatever now occupies that memory. Names and manufacturer
        // data survive only because those ARE copied into std::string members.
        // getServiceData(i) is likewise a real copy, so it stays valid here.
        //
        // Cost of learning that: the emitter transmitted a byte-perfect Remote ID
        // advert, the decoder parsed those exact captured bytes correctly in a
        // host test, and the drone still never appeared on the device.
        {
            int nsd = d.getServiceDataCount();
            for(int si = 0; si < nsd; si++) {
                std::string u = fstr(d.getServiceDataUUID(si).toString());
                // 16-bit 0xFFFA renders inside the full 128-bit form.
                if(u.find("fffa") == std::string::npos && u.find("FFFA") == std::string::npos) {
                    continue;
                }
                // The library strips the 2-byte UUID, so this already begins at
                // the ODID application code -- exactly what the app's decoder
                // expects to be handed.
                std::string sd = fstr(d.getServiceData(si));
                if(sd.size() < 2 || (uint8_t)sd[0] != 0x0D) continue;
                size_t sd_len = sd.size() > 64 ? 64 : sd.size();
                char rid[176];
                size_t rp = snprintf(rid, sizeof(rid), "RID,%s,%d,", addr, rssi);
                for(size_t k = 0; k < sd_len && rp + 2 < sizeof(rid); k++) {
                    buf_appendf(rid, sizeof(rid), &rp, "%02x", (uint8_t)sd[k]);
                }
                if(rp > sizeof(rid) - 1) rp = sizeof(rid) - 1;
                rid[rp++] = '\n';
                Serial.write((const uint8_t*)rid, rp);
                break;
            }
        }

        // One buffer + single write (same rationale as the D-line) so the multi-field
        // BLE line is emitted atomically.
        char line[176];
        size_t pos =
            snprintf(line, sizeof(line), "BLE,%s,%d,%d,%d,", addr, rssi, cat, company);
        if(d.haveName()) {
            std::string nm = fstr(d.getName());
            buf_append_escaped(line, sizeof(line), &pos, nm.c_str(), (int)nm.size(), 32);
        }
        // Trailing field: raw mfg-data hex for Flock (0x09C8) only, so the
        // Flipper can decode the device serial. Capped so the line stays well
        // under the Flipper's RX line limit; only Flock units carry it.
        // Widened from `company == 0x09C8` to ANY cat=1 device carrying
        // manufacturer data. A unit that reached cat=1 by Penguin naming, Raven
        // GATT or a Flock OUI while advertising under some other id previously
        // sent ZERO mfg bytes, so its serial could never be decoded and the app
        // had nothing to show.
        //
        // LINE BUDGET (char line[176], now the longest line the protocol emits):
        //   "BLE," 4 + addr 12 + "," + rssi <=4 + "," + cat 1 + "," + company <=6
        //   + ","                                              ~= 27
        //   escaped name, capped at 32 by buf_append_escaped, <=64 worst case
        //   "," + 62 hex chars (31-byte cap)                    = 63
        //   ",rv=1" 5 + ",sep=1" 6 + "\n" 1                     = 12
        //   -> 166 of 176. Fits, and buf_appendf clamps regardless.
        //
        // Wire shape is unchanged, so an older app still parses it: esp_parser.c
        // splits trailers on the presence of '=', and a foreign payload simply
        // lands in mfg[] and yields no serial. No ESP_PROTO_VERSION bump.
        if(cat == 1 && d.haveManufacturerData()) {
            std::string md = fstr(d.getManufacturerData());
            if(pos + 1 < sizeof(line)) line[pos++] = ',';
            for(size_t j = 0; j < md.length() && j < 31 && pos + 2 < sizeof(line); j++) {
                buf_appendf(line, sizeof(line), &pos, "%02x", (uint8_t)md[j]);
            }
        }
        // Raven GATT flag, emitted LAST so it follows the optional mfghex field.
        // The '=' lets the Flipper distinguish it from the pure-hex mfghex token.
        if(raven && pos + 5 < sizeof(line)) {
            memcpy(line + pos, ",rv=1", 5);
            pos += 5;
        }
        if(tracker_separated && pos + 6 < sizeof(line)) {
            memcpy(line + pos, ",sep=1", 6);
            pos += 6;
        }
        // Axon BWCDEVICE tag. Same '=' trailer convention as rv=/sep=, so an
        // older app ignores it rather than mis-parsing the line.
        if(bwc && pos + 6 < sizeof(line)) {
            memcpy(line + pos, ",bwc=1", 6);
            pos += 6;
        }
        if(pos > sizeof(line) - 1) pos = sizeof(line) - 1;
        line[pos++] = '\n';
        Serial.write((const uint8_t*)line, pos);
    }
    Serial.printf("BEND,%d\n", count);

    // A bulk of impersonation/pairing adverts in a single scan = a BLE-spam
    // flood (independent BLE radio class for the app's fused score).
    if(spam >= BLE_SPAM_MIN) Serial.printf("ATK,blespam,%d\n", spam);

    g_ble->clearResults();
    esp_wifi_set_promiscuous(true);
    set_channel(g_channel);
}

// Locator (BLE): one short scan; emit the target's RSSI if seen. Builds the same
// lowercased, colon-stripped toString() form the BLE line uses, so the comparison
// is byte-order-safe against the addr the app originally parsed.
static void ble_locate_scan() {
    ble_ensure_init();
    esp_wifi_set_promiscuous(false);
    BLEScanResults res = FLOCK_SCAN(g_ble, 1);
    int best = -127;
    int n = res.getCount();
    for(int i = 0; i < n; i++) {
        BLEAdvertisedDevice d = res.getDevice(i);
        std::string a = fstr(d.getAddress().toString());
        char addr[13];
        int k = 0;
        for(size_t j = 0; j < a.size() && k < 12; j++) {
            char c = a[j];
            if(c == ':') continue;
            addr[k++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        addr[k] = 0;
        if(strcmp(addr, g_locate_macs) == 0) {
            int r = d.getRSSI();
            if(r > best) best = r;
        }
    }
    g_ble->clearResults();
    if(best > -127) Serial.printf("LOC,%d\n", best);
}

// ---- explicit tracker actions -------------------------------------------
//
// These are deliberately separate from the passive scan path. The Flipper can
// request them only from the selected BLE detail view, and the companion
// re-validates the fresh advertisement before it connects. Ring is limited to
// an Apple/Find My tracker advertising the separated payload: that is the
// non-owner anti-stalking path, not the owner-only Find My control path.
#define DULT_NON_OWNER_SERVICE "15190001-12f4-c226-88ed-2ac5579f2a85"
#define DULT_NON_OWNER_CHAR    "8e0c0001-1d68-fb92-bf61-48377421680e"
#define FIND_MY_SERVICE        "fd44"
#define FIND_MY_NON_OWNER_CHAR "4f860003-943b-49ef-bed4-2f730304427a"
#define AIRTAG_SOUND_SERVICE   "7dfc9000-7d1c-4951-86aa-8d9728f8d66c"
#define AIRTAG_SOUND_CHAR      "7dfc9001-7d1c-4951-86aa-8d9728f8d66c"

static volatile bool g_ring_response_ready = false;
static volatile uint16_t g_ring_response_status = 0xFFFF;

static void ble_action_restore_radio() {
    g_ble->clearResults();
    esp_wifi_set_promiscuous(true);
    set_channel(g_channel);
}

static void ble_action_compact_address(BLEAdvertisedDevice& d, char out[13]) {
    std::string a = fstr(d.getAddress().toString());
    int k = 0;
    for(size_t i = 0; i < a.size() && k < 12; i++) {
        char c = a[i];
        if(c == ':') continue;
        out[k++] = (c >= 'A' && c <= 'F') ? (char)(c + 32) : c;
    }
    out[k] = 0;
}

static bool ble_action_has_service(BLEAdvertisedDevice& d, const char* token) {
    for(int i = 0; i < d.getServiceUUIDCount(); i++) {
        std::string u = fstr(d.getServiceUUID(i).toString());
        if(u.find(token) != std::string::npos) return true;
    }
    return false;
}

/** Classify only the tracker families for which an explicit action is allowed. */
static int ble_action_tracker_category(BLEAdvertisedDevice& d, bool* separated) {
    if(separated) *separated = false;
    bool apple_payload_seen = false;

    if(d.haveManufacturerData()) {
        std::string md = fstr(d.getManufacturerData());
        int company = md.length() >= 2 ? ((uint8_t)md[0] | ((uint8_t)md[1] << 8)) : -1;
        if(company == APPLE_FIND_MY_COMPANY_ID) {
            apple_payload_seen = true;
            AppleFindMyAdvert advert;
            if(apple_find_my_decode(
                   (const uint8_t*)md.data(), md.size(), &advert) &&
               apple_find_my_is_tracker(&advert)) {
                if(separated) *separated = advert.separated;
                return BLE_TRACKER_CAT_AIRTAG;
            }
        }
    }

    // A valid Apple manufacturer block that decoded as a phone, Mac, or
    // AirPods must not be rescued by a broad service-UUID fallback.
    if(apple_payload_seen) return 0;

    if(d.haveServiceUUID()) {
        if(ble_action_has_service(d, "fd44") || ble_action_has_service(d, "fcb2") ||
           ble_action_has_service(d, "15190001"))
            return BLE_TRACKER_CAT_AIRTAG;
        if(ble_action_has_service(d, "feed") || ble_action_has_service(d, "feec"))
            return BLE_TRACKER_CAT_TILE;
        if(ble_action_has_service(d, "fd5a")) return BLE_TRACKER_CAT_SMARTTAG;
        if(ble_action_has_service(d, "feaa")) return BLE_TRACKER_CAT_FIND_MY_DEV;
    }
    return 0;
}

/** Scan once and keep the exact advertisement object so random-address type is retained. */
static bool ble_action_find_target(
    const char* wanted,
    BLEAdvertisedDevice* target,
    int* category,
    bool* separated,
    int* rssi) {
    BLEScanResults found = FLOCK_SCAN(g_ble, 1);
    bool have = false;
    int best = -127;
    int n = found.getCount();
    for(int i = 0; i < n; i++) {
        BLEAdvertisedDevice d = found.getDevice(i);
        char addr[13];
        ble_action_compact_address(d, addr);
        if(strcmp(addr, wanted) != 0) continue;
        int signal = d.getRSSI();
        if(!have || signal > best) {
            *target = d;
            *category = ble_action_tracker_category(d, separated);
            best = signal;
            have = true;
        }
    }
    if(rssi) *rssi = best;
    return have;
}

static BLEClient* ble_action_connect(BLEAdvertisedDevice* target) {
    BLEClient* client = BLEDevice::createClient();
    if(!client || !client->connect(target)) {
        if(client) delete client;
        return nullptr;
    }
    return client;
}

static void ble_ring_notify(
    BLERemoteCharacteristic* /*characteristic*/,
    uint8_t* data,
    size_t length,
    bool /*isNotify*/) {
    // Command_Response: opcode 0x0302, then the command opcode and a uint16
    // response status, all little-endian per the non-owner protocol.
    if(length < 6) return;
    uint16_t response_opcode = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    uint16_t command_opcode = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
    if(response_opcode == 0x0302 && command_opcode == 0x0300) {
        g_ring_response_status = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
        g_ring_response_ready = true;
    }
}

static bool ble_ring_write_airtag(BLEClient* client, char status[16]) {
    BLERemoteService* service = client->getService(AIRTAG_SOUND_SERVICE);
    if(!service) return false;
    BLERemoteCharacteristic* characteristic = service->getCharacteristic(AIRTAG_SOUND_CHAR);
    if(!characteristic || (!characteristic->canWrite() && !characteristic->canWriteNoResponse()))
        return false;
    uint8_t command = 0xAF;
    characteristic->writeValue(&command, 1, true);
    snprintf(status, 16, "sent");
    return true;
}

static bool ble_ring_write_dult(BLERemoteService* service, char status[16]) {
    if(!service) return false;
    BLERemoteCharacteristic* characteristic = service->getCharacteristic(DULT_NON_OWNER_CHAR);
    if(!characteristic || (!characteristic->canWrite() && !characteristic->canWriteNoResponse()))
        return false;

    g_ring_response_ready = false;
    g_ring_response_status = 0xFFFF;
    if(characteristic->canIndicate() || characteristic->canNotify()) {
        // The DULT control point uses indications. The Arduino BLE API's first
        // boolean selects notifications, so false selects the indication CCCD.
        characteristic->registerForNotify(ble_ring_notify, !characteristic->canIndicate());
    }
    uint8_t command[] = {0x00, 0x03}; // Sound_Start 0x0300, little-endian
    characteristic->writeValue(command, sizeof(command), true);

    uint32_t deadline = millis() + 500;
    while(!g_ring_response_ready && (int32_t)(millis() - deadline) < 0) delay(10);
    if(!g_ring_response_ready) {
        snprintf(status, 16, "sent");
    } else if(g_ring_response_status == 0x0000) {
        snprintf(status, 16, "ok");
    } else if(g_ring_response_status == 0x0001) {
        snprintf(status, 16, "busy");
    } else {
        // 0xFFFF is the documented invalid-command response, which also covers
        // the owner-nearby case described by the Find My behavior.
        snprintf(status, 16, "rejected");
    }
    return true;
}

static bool ble_ring_write_legacy(BLEClient* client, char status[16]) {
    BLERemoteService* service = client->getService(FIND_MY_SERVICE);
    if(!service) return false;
    BLERemoteCharacteristic* characteristic = service->getCharacteristic(FIND_MY_NON_OWNER_CHAR);
    if(!characteristic || (!characteristic->canWrite() && !characteristic->canWriteNoResponse()))
        return false;
    uint8_t command[] = {0x01, 0x00, 0x03};
    characteristic->writeValue(command, sizeof(command), true);
    snprintf(status, 16, "sent");
    return true;
}

static void ble_action_ping(const char* wanted) {
    ble_ensure_init();
    esp_wifi_set_promiscuous(false);

    BLEAdvertisedDevice target;
    int category = 0;
    bool separated = false;
    int rssi = -127;
    if(!ble_action_find_target(wanted, &target, &category, &separated, &rssi)) {
        ble_action_emit("PING", "not_found", rssi, false);
        ble_action_restore_radio();
        return;
    }
    if(!ble_tracker_category_is_known((uint8_t)category)) {
        ble_action_emit("PING", "not_tracker", rssi, true);
        ble_action_restore_radio();
        return;
    }

    BLEClient* client = ble_action_connect(&target);
    if(!client) {
        ble_action_emit("PING", "connect_fail", rssi, true);
        ble_action_restore_radio();
        return;
    }
    client->disconnect();
    delete client;
    ble_action_emit("PING", "ok", rssi, true);
    ble_action_restore_radio();
}

static void ble_action_ring(const char* wanted) {
    ble_ensure_init();
    esp_wifi_set_promiscuous(false);

    BLEAdvertisedDevice target;
    int category = 0;
    bool separated = false;
    int rssi = -127;
    if(!ble_action_find_target(wanted, &target, &category, &separated, &rssi)) {
        ble_action_emit("RING", "not_found", rssi, false);
        ble_action_restore_radio();
        return;
    }
    if(category != BLE_TRACKER_CAT_AIRTAG) {
        ble_action_emit("RING", "unsupported", rssi, true);
        ble_action_restore_radio();
        return;
    }
    if(!separated) {
        ble_action_emit("RING", "not_separated", rssi, true);
        ble_action_restore_radio();
        return;
    }

    BLEClient* client = ble_action_connect(&target);
    if(!client) {
        ble_action_emit("RING", "connect_fail", rssi, true);
        ble_action_restore_radio();
        return;
    }

    char status[16] = "unsupported";
    bool wrote = ble_ring_write_airtag(client, status);
    if(!wrote) wrote = ble_ring_write_dult(client->getService(DULT_NON_OWNER_SERVICE), status);
    if(!wrote) {
        // Some Find My accessories expose the older FD44 control point rather
        // than the DULT UUID. It is still a non-owner command, never owner auth.
        wrote = ble_ring_write_legacy(client, status);
    }
    client->disconnect();
    delete client;
    ble_action_emit("RING", wrote ? status : "service_missing", rssi, true);
    ble_action_restore_radio();
}

#else // FLOCK_HAS_BLE == 0

// Wi-Fi-only target (ESP32-S2 and friends). No Bluetooth radio exists, so these
// stand in as no-ops rather than missing symbols, which keeps loop() and the
// command dispatcher byte-identical across targets instead of threading #ifs
// through both. The ACT replies still ANSWER: a Flipper left waiting on a reply
// that can never arrive is worse than one told plainly the board cannot do it.
static void ble_do_scan(int seconds) {
    (void)seconds;
}
static void ble_locate_scan() {
}
static void ble_action_ping(const char* wanted) {
    (void)wanted;
    ble_action_emit("PING", "noble", -127, false);
}
static void ble_action_ring(const char* wanted) {
    (void)wanted;
    ble_action_emit("RING", "noble", -127, false);
}

#endif // FLOCK_HAS_BLE

// ---- optional GPS relay (FlipDeFlock issue #5) ---------------------------
//
// Some carrier boards wire a GPS module to the ESP32 instead of to the Flipper's
// header. The Flipper then cannot see it on ANY pin setting, because the NMEA
// never reaches its GPIO. When switched on, read the module here and relay the
// sentences the Flipper's parser understands as `G,<sentence>` lines.
//
// OFF unless the app asks for it (`gps <rx> [baud]`): Serial1's pins differ per
// board and per chip, so a wrong guess would just spray a dead pin's noise onto
// a link that carries detections.
//
// Deliberately NOT parsed here. The Flipper already has a host-tested NMEA
// parser used by its own UART path; relaying raw sentences means one parser, one
// set of lock-loss semantics, and no duplicated coordinate maths on the ESP.
#define GPS_LINE_MAX 100 // NMEA caps a sentence at 82 incl. CRLF; headroom
// Sized for the worst configured baud, not the common one. The app offers up to
// 115200, and a 10 Hz receiver at that rate emits on the order of 6 KB/s. loop()
// now drains between 1-second BLE slices rather than being stalled for a whole
// 3-second scan, so one slice is the window this has to cover: 8 KB gives
// headroom over that with room for a scheduling hiccup. It is ~3% of the free
// heap the sketch reports, which is a cheap way to make dropped sentences a
// non-event instead of a silent position error.
#define GPS_RX_BUF   8192

/* ---- which pins can carry a GPS on THIS chip -------------------------------
 *
 * Every bound below comes from the IDF's own per-target headers. Nothing here is
 * a hardcoded pin number, deliberately: the previous guard was
 * `rx > 0 && rx != 1 && rx != 3 && rx < 48`, which is the classic ESP32's
 * pinout written as if it were universal. On an ESP32-C5 that is wrong three
 * separate ways, and the failure modes get worse as they go:
 *
 *   - GPIO32..35 were offered by the app and do not exist (C5 stops at 28).
 *   - GPIO16..22 are the flash/PSRAM bus. A C5-WROOM-1-MDN8R8 has 8 MB of each,
 *     so they are genuinely occupied. This is what a user was told to use.
 *   - UART0 is GPIO11/12 on a C5, NOT 1/3. So the one thing the guard existed to
 *     prevent -- taking the link to the Flipper and cutting the board off, which
 *     needs a recovery flash to undo -- was exactly what it failed to prevent.
 *
 * Deriving the bounds from the SOC_, SPI_IOMUX_ and U0xxD_GPIO_NUM macros means
 * this is automatically correct on parts nobody here has ever held.
 */
// The contiguous span the flash bus occupies. Folding min..max over the six
// IOMUX pins rather than testing each one also covers the PSRAM lines, which
// share this bus on parts that have both and are not exposed as their own
// macros. All six exist on every target (verified against esp32/s2/s3/c3), and
// the span is contiguous on each: esp32 6-11, s2/s3 27-32, c3 12-17.
//
// constexpr, not nested ternary macros: the first attempt at this folded only
// five of the six and silently stopped the span at GPIO10, leaving the flash CS
// pin offered as a valid GPS input. The static_asserts below caught it.
static constexpr int flock_min_i(int a, int b) {
    return a < b ? a : b;
}
static constexpr int flock_max_i(int a, int b) {
    return a > b ? a : b;
}
// The flash-bus pin macros were RENAMED between IDF 4.x and 5.x: core 2.x calls
// them SPI_IOMUX_PIN_NUM_*, core 3.x calls the memory-SPI bus MSPI_IOMUX_PIN_NUM_*
// and reuses the bare SPI_ prefix for the general-purpose controllers. Building
// against only one spelling compiles on one core and fails on the other, which is
// exactly what the core-3.x compat job exists to catch -- and did.
#if defined(MSPI_IOMUX_PIN_NUM_CLK)
#define FLOCK_F_CLK  MSPI_IOMUX_PIN_NUM_CLK
#define FLOCK_F_MISO MSPI_IOMUX_PIN_NUM_MISO
#define FLOCK_F_MOSI MSPI_IOMUX_PIN_NUM_MOSI
#define FLOCK_F_HD   MSPI_IOMUX_PIN_NUM_HD
#define FLOCK_F_WP   MSPI_IOMUX_PIN_NUM_WP
// ...and the chip-select is CS0 on some core-3.x targets, bare CS on others.
#if defined(MSPI_IOMUX_PIN_NUM_CS0)
#define FLOCK_F_CS MSPI_IOMUX_PIN_NUM_CS0
#else
#define FLOCK_F_CS MSPI_IOMUX_PIN_NUM_CS
#endif
#elif defined(SPI_IOMUX_PIN_NUM_CLK)
#define FLOCK_F_CLK  SPI_IOMUX_PIN_NUM_CLK
#define FLOCK_F_MISO SPI_IOMUX_PIN_NUM_MISO
#define FLOCK_F_MOSI SPI_IOMUX_PIN_NUM_MOSI
#define FLOCK_F_HD   SPI_IOMUX_PIN_NUM_HD
#define FLOCK_F_WP   SPI_IOMUX_PIN_NUM_WP
#define FLOCK_F_CS   SPI_IOMUX_PIN_NUM_CS
#else
#error "No flash IOMUX pin macros for this IDF -- the GPS pin guard cannot be derived"
#endif

static constexpr int FLOCK_FLASH_LO = flock_min_i(
    flock_min_i(flock_min_i(FLOCK_F_CLK, FLOCK_F_MISO), FLOCK_F_MOSI),
    flock_min_i(flock_min_i(FLOCK_F_HD, FLOCK_F_WP), FLOCK_F_CS));
static constexpr int FLOCK_FLASH_HI = flock_max_i(
    flock_max_i(flock_max_i(FLOCK_F_CLK, FLOCK_F_MISO), FLOCK_F_MOSI),
    flock_max_i(flock_max_i(FLOCK_F_HD, FLOCK_F_WP), FLOCK_F_CS));

/* These pin down the two things above that are easy to get quietly wrong: that
 * the MIN5/MAX5 folding actually yields the flash span, and that the per-target
 * headers hold the values the datasheets say they do.
 *
 * The C5 block matters most, because NOBODY ON THIS PROJECT HAS A C5. Its
 * numbers come from Espressif's docs (GPIO0-28; UART0 on GPIO11/12; GPIO16-22
 * the flash/PSRAM bus) and are asserted here so CI, which does build the C5,
 * fails loudly if the research was wrong -- rather than shipping a guard that
 * refuses the wrong pins to the one person actually testing on that chip.
 */
#if defined(CONFIG_IDF_TARGET_ESP32)
static_assert(SOC_GPIO_PIN_COUNT == 40, "classic ESP32 has GPIO0-39");
static_assert(U0TXD_GPIO_NUM == 1 && U0RXD_GPIO_NUM == 3, "classic ESP32 UART0 is GPIO1/3");
static_assert(FLOCK_FLASH_LO == 6 && FLOCK_FLASH_HI == 11, "classic ESP32 flash is GPIO6-11");
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
static_assert(SOC_GPIO_PIN_COUNT == 29, "ESP32-C5 has GPIO0-28");
static_assert(U0TXD_GPIO_NUM == 11 && U0RXD_GPIO_NUM == 12, "ESP32-C5 UART0 is GPIO11/12");
static_assert(FLOCK_FLASH_LO >= 16 && FLOCK_FLASH_HI <= 22, "ESP32-C5 flash/PSRAM is GPIO16-22");
#endif

/**
 * Why this pin cannot carry a GPS, or NULL if it can.
 * The string is echoed to the operator, because "refused" without "why" is what
 * made the last round of this take four attempts to diagnose.
 */
static const char* gps_pin_reject(int rx) {
    if(rx < 0 || rx >= SOC_GPIO_PIN_COUNT) return "no such pin on this chip";
    if(!((1ULL << rx) & SOC_GPIO_VALID_GPIO_MASK)) return "not a usable GPIO";
    if(rx == U0TXD_GPIO_NUM || rx == U0RXD_GPIO_NUM) return "carries the Flipper link";
    if(rx >= FLOCK_FLASH_LO && rx <= FLOCK_FLASH_HI) return "flash/PSRAM bus";
    return NULL;
}

/** Bitmask of pins gps_pin_reject() accepts. Sent to the app so its picker can
 *  offer this chip's real pins instead of a hardcoded classic-ESP32 list. */
static uint64_t gps_usable_mask() {
    uint64_t m = 0;
    for(int i = 0; i < SOC_GPIO_PIN_COUNT && i < 64; i++) {
        if(!gps_pin_reject(i)) m |= (1ULL << i);
    }
    return m;
}

static bool g_gps_on = false;
static int g_gps_rx = -1;
static uint32_t g_gps_baud = 9600;
static char g_gps_line[GPS_LINE_MAX];
static size_t g_gps_len = 0;

// Only the three sentence types the Flipper decodes (RMC / GGA / GLL). Filtering
// on this side keeps GSV/GSA/VTG chatter off a UART shared with detection lines
// -- a talkative receiver emits well over a dozen sentences per fix.
static bool gps_wanted(const char* s, size_t n) {
    if(n < 6 || s[0] != '$') return false;
    const char* t = s + 3; // '$' + 2-char talker (GP / GN / GL / GA / BD ...)
    return strncmp(t, "RMC", 3) == 0 || strncmp(t, "GGA", 3) == 0 || strncmp(t, "GLL", 3) == 0;
}

static void gps_relay_line() {
    if(!gps_wanted(g_gps_line, g_gps_len)) return;
    char out[GPS_LINE_MAX + 4];
    size_t pos = 0;
    out[pos++] = 'G';
    out[pos++] = ',';
    // Copy printable ASCII only. Commas and '$'/'*' MUST survive (they are the
    // sentence), so buf_append_escaped() is wrong here -- it maps ',' to '.'.
    // Anything outside printable ASCII is dropped rather than substituted: a
    // stray CR/LF would split the line and desync the Flipper's framing.
    for(size_t i = 0; i < g_gps_len && pos + 2 < sizeof(out); i++) {
        uint8_t c = (uint8_t)g_gps_line[i];
        if(c < 0x20 || c > 0x7E) continue;
        out[pos++] = (char)c;
    }
    out[pos++] = '\n';
    Serial.write((const uint8_t*)out, pos); // single write: atomic vs the WiFi task
}

static void gps_poll() {
    if(!g_gps_on) return;
    // Drain everything buffered, but emit at most ONE sentence of each type per
    // pass -- the newest.
    //
    // Only the current position matters, and the Flipper's parser ends up in the
    // same state either way: it applies sentences in order, so the last one wins.
    // Relaying a whole backlog instead would burn the Flipper's UART on
    // already-superseded fixes, competing with detection lines for the same link
    // at the exact moment a scan phase just ended and hits are being reported.
    //
    // Keeps the lock-loss semantics intact: "newest wins" is what the direct UART
    // path effectively does too, so a valid -> invalid transition still clears the
    // fix rather than being coalesced away.
    char last[3][GPS_LINE_MAX];
    size_t last_len[3] = {0, 0, 0};
    int budget = 4096; // generous: this runs once per pass, not per byte of link
    while(g_gps_on && Serial1.available() && budget-- > 0) {
        char c = (char)Serial1.read();
        if(c == '\n' || c == '\r') {
            if(g_gps_len && gps_wanted(g_gps_line, g_gps_len)) {
                // Bucket by sentence type so an RMC cannot displace a GGA: the
                // two carry different fields (course/validity vs satellites).
                const char* t = g_gps_line + 3;
                int slot = (strncmp(t, "RMC", 3) == 0) ? 0 : (strncmp(t, "GGA", 3) == 0) ? 1 : 2;
                memcpy(last[slot], g_gps_line, g_gps_len);
                last_len[slot] = g_gps_len;
            }
            g_gps_len = 0;
        } else if(g_gps_len + 1 < sizeof(g_gps_line)) {
            g_gps_line[g_gps_len++] = c;
        } else {
            // Overlong: drop the whole thing. Emitting a truncated sentence
            // would fail the Flipper's checksum check anyway, and a sentence
            // without its '*hh' could be parsed as a WRONG fix.
            g_gps_len = 0;
        }
    }
    // GGA first so the satellite count is in place before RMC's position/course.
    static const int order[3] = {1, 0, 2};
    for(int i = 0; i < 3; i++) {
        int slot = order[i];
        if(!last_len[slot]) continue;
        memcpy(g_gps_line, last[slot], last_len[slot]);
        g_gps_len = last_len[slot];
        gps_relay_line();
        g_gps_len = 0;
    }
}

// `gps off` | `gps <rx_pin> [baud]`. Echoes GPSCFG either way so a user hunting
// for their board's pin can confirm from a plain serial terminal. The Flipper
// ignores unknown lines, so the echo is safe on a live link.
static void gps_configure(int rx, uint32_t baud) {
    if(g_gps_on) {
        Serial1.end();
        g_gps_on = false;
    }
    g_gps_len = 0;
    if(rx >= 0) {
        g_gps_rx = rx;
        g_gps_baud = baud;
        Serial1.setRxBufferSize(GPS_RX_BUF);
        // RX only: we never talk to the receiver, so TX stays unassigned rather
        // than claiming a second pin the board may be using for something else.
        Serial1.begin(g_gps_baud, SERIAL_8N1, g_gps_rx, -1);
        g_gps_on = true;
    }
    Serial.printf("GPSCFG,%d,%d,%lu\n", g_gps_on ? 1 : 0, g_gps_rx, (unsigned long)g_gps_baud);
}

void setup() {
    Serial.begin(115200);
    // Short RX timeout so loop()'s readStringUntil('\n') can't stall channel-hop /
    // heartbeat for the default 1 s when a command arrives without a trailing newline.
    Serial.setTimeout(20);
    delay(200);

    nvs_flash_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    // WIFI_MODE_NULL is the classic-ESP32 idiom for a promiscuous sniffer and is
    // left exactly as it was on that target -- it is field-proven there and this
    // is not the place to experiment.
    //
    // CANDIDATE FIX FOR THE ESP32-S2, UNVERIFIED ON HARDWARE. Reported in issue
    // #25: an S2 (the official Flipper Wi-Fi Devboard v1) running the Wi-Fi-only
    // build comes up looking perfectly healthy -- banner fine, channel counting,
    // no errors -- and detects nothing at all across ten-plus minutes past
    // ten-plus cameras, on a board proven good because Marauder scans and logs on
    // it. A promiscuous RX callback that never fires produces exactly that
    // picture, and NULL-mode promiscuous is not guaranteed to deliver packets on
    // the S2 the way it does on the classic part. STA mode is the portable form.
    //
    // Gated to the S2 alone so the classic image is byte-identical and cannot
    // regress for anyone currently working. Nobody on this project has an S2, so
    // this ships as a nightly for the reporter to test, not as a release, and it
    // is a hypothesis until his diag.csv shows esp_frames climbing.
#if defined(CONFIG_IDF_TARGET_ESP32S2)
    esp_wifi_set_mode(WIFI_MODE_STA);
#else
    esp_wifi_set_mode(WIFI_MODE_NULL);
#endif
    esp_wifi_start();
#if FLOCK_HAS_5GHZ
    // AUTO = 2.4 + 5. Must be set before hopping: with the default 2.4-only mode
    // a 5 GHz set_channel() is rejected and the sweep silently covers half of
    // what it reports. Non-fatal if it fails -- hop_channel() still yields valid
    // 2.4 GHz channels, so the companion degrades to the classic behaviour
    // instead of scanning nothing.
    if(esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO) != ESP_OK) {
        g_band = FlockBand2G;
    }
#endif
    start_promisc();

    banner();
}

static void handle_command(String cmd) {
    cmd.trim();
    // Any command that RE-TASKS THE RADIO ends Locator mode: unlock the Wi-Fi
    // channel it pinned, and restore promiscuous if BLE-locate had turned it off.
    //
    // READ-ONLY QUERIES ARE EXEMPT, and that exemption is the whole point of
    // this list. The rule used to be "anything that is not `locate`", which
    // included `survey` -- a pure table dump that touches no radio state. The
    // app polls for the survey every ten seconds from a tick that runs in every
    // scene, the Locator included, so a hunt was cancelled about ten seconds
    // after it began, or immediately when the Locator was opened on an already
    // live link. Nothing was reported on either side: the board just stopped
    // streaming LOC and the meter sat on "acquiring signal..." forever.
    //
    // Keep this list to commands that genuinely cannot coexist with a locked
    // channel or a dedicated BLE scan. When in doubt, exempt -- a stale locate
    // is recoverable with one Back press; a Locator that never works is not.
    bool radio_retask = !(
        cmd.startsWith("locate") || cmd.startsWith("survey") || cmd == "ver" ||
        cmd == "sigtest"); // pure computation on a static buffer, touches no radio
    if(radio_retask) {
        if(g_locate_kind == 'w') g_lock_channel = 0;
        if(g_locate_kind == 'b') {
            esp_wifi_set_promiscuous(true);
            set_channel(g_channel);
        }
        g_locate_kind = 0;
    }
    if(cmd.startsWith("gps")) {
        // gps            -> report current state
        // gps off        -> stop relaying, release Serial1
        // gps <rx> [baud]-> relay NMEA from that RX pin (default 9600)
        String a = cmd.substring(3);
        a.trim();
        if(a.length() == 0) {
            Serial.printf(
                "GPSCFG,%d,%d,%lu\n", g_gps_on ? 1 : 0, g_gps_rx, (unsigned long)g_gps_baud);
        } else if(a == "off") {
            gps_configure(-1, g_gps_baud);
        } else {
            int sp = a.indexOf(' ');
            int rx = (sp < 0 ? a : a.substring(0, sp)).toInt();
            uint32_t baud = 9600;
            if(sp >= 0) {
                long b = a.substring(sp + 1).toInt();
                if(b >= 1200 && b <= 921600) baud = (uint32_t)b;
            }
            // Ask the chip, don't assume the pinout. gps_pin_reject() is derived
            // entirely from this target's own IDF headers, so the pins it refuses
            // are this board's real flash bus and this board's real UART0 -- not
            // the classic ESP32's, which is what the old literal test encoded.
            const char* why = gps_pin_reject(rx);
            if(!why) {
                gps_configure(rx, baud);
            } else {
                Serial.printf("GPSCFG,0,%d,%lu\n", rx, (unsigned long)baud);
                Serial.printf("GPSERR,%d,%s\n", rx, why);
            }
        }
        return; // not a scan-mode command
    }
    if(cmd == "scan") {
        g_scanning = true;
        g_combo = false; // pure WiFi Flock
    } else if(cmd == "stop") {
        g_scanning = false;
        g_combo = false; // also leave dual-band mode so the board goes idle
    } else if(cmd == "ver") {
        banner();
    } else if(cmd == "wifiscan") {
        wifi_security_scan();
    } else if(cmd == "blescan") {
        ble_do_scan(6);
    } else if(cmd.startsWith("ble_ping ")) {
        String a = cmd.substring(9);
        a.trim();
        uint8_t mac[6];
        if(a.length() == 12 && parse_hexmac(a.c_str(), mac)) {
            ble_action_ping(a.c_str());
        } else {
            ble_action_emit("PING", "invalid", -127, false);
        }
    } else if(cmd.startsWith("ble_ring ")) {
        String a = cmd.substring(9);
        a.trim();
        uint8_t mac[6];
        if(a.length() == 12 && parse_hexmac(a.c_str(), mac)) {
            ble_action_ring(a.c_str());
        } else {
            ble_action_emit("RING", "invalid", -127, false);
        }
    } else if(cmd == "flockcombo") {
        g_scanning = true;
        g_combo = true; // interleaved WiFi + BLE Flock detection
        g_phase_start = millis();
    } else if(cmd == "survey") {
        // Dump on request only, so a survey costs nothing until someone asks.
        survey_dump();
    } else if(cmd == "sigtest") {
        // SELF-TEST FOR THE SIGNATURE PATH, because the radio cannot reach it.
        //
        // The IE signature is the only tell that fires on a randomised MAC, so
        // it must not ship unexercised -- but there is no way to put a synthetic
        // Flock probe on the air from this hardware: the ESP-IDF will not inject
        // a probe request with a foreign source address (the emitter sketch's
        // own header says so, and three flashes confirmed it the hard way).
        //
        // What the radio DOES prove is the capture half: real probes arrive and
        // come out of ie_sig_string() with correct, distinct signatures every
        // scan. What it cannot prove is the match, because no camera is here.
        // So this feeds a frame built to the known Flock IE layout through the
        // SAME production functions the promiscuous callback uses -- not a copy
        // of them -- and prints what they return.
        //
        // Kept permanently. It costs a few hundred bytes and makes the one
        // detection path that matters checkable on any bench, forever, without
        // a camera in front of it.
        static const uint8_t probe[] = {
            // 24-byte management header; contents are irrelevant to the IE walk,
            // which starts at offset 24 for a probe request.
            0x40, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02, 0x11,
            0x22, 0x33, 0x44, 0x55, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
            0x00, 0x00, // SSID, wildcard
            0xDD, 0x07, 0x50, 0x6F, 0x9A, 0x16, 0x03, 0x01, 0x03, // WFA MBO
            0x2D, 0x04, 0x00, 0x00, 0x00, 0x00, // HT capabilities (45)
            0xBF, 0x04, 0x00, 0x00, 0x00, 0x00, // VHT capabilities (191)
            0xDD, 0x07, 0x00, 0x50, 0xF2, 0x08, 0x00, 0x00, 0x00, // 00:50:f2 / 8
        };
        char sig[IE_SIG_MAX];
        ie_sig_string(probe, (int)sizeof(probe), sig, sizeof(sig));
        Serial.printf(
            "SIGTEST,%s,%08lx,%d\n",
            sig,
            (unsigned long)ie_content_hash(probe, (int)sizeof(probe)),
            flock_sig_match(sig) ? 1 : 0);
    } else if(cmd == "surveyclear") {
        memset(g_survey, 0, sizeof(g_survey));
        Serial.print("SVEND,0\n");
    } else if(cmd == "bootloader") {
        // ENTER UART DOWNLOAD MODE IN SOFTWARE, so reflashing needs no hands.
        //
        // WHY. On a board with no USB port and no auto-reset circuit -- the
        // ReksLab Tri-Board, the CaracalDB multi-boards -- IO0 and EN are on
        // buttons wired to nothing the Flipper can drive, so every reflash needs
        // a human to hold BOOT and tap RESET at the right moment. Confirmed on
        // the bench 2026-09-07: the flasher reports "no sync" on all five
        // attempts without it.
        //
        // WORKS ON S2 / S3 / C3 ONLY, AND THAT IS A HARDWARE FACT. Those ROMs
        // read RTC_CNTL_OPTION1_REG's FORCE_DOWNLOAD_BOOT bit, which survives a
        // software reset and selects download mode regardless of the strapping
        // pins. THE CLASSIC ESP32 HAS NO SUCH BIT -- checked against the 2.0.17
        // SDK headers, where RTC_CNTL_OPTION1_REG does not exist for esp32 at
        // all, only for esp32s2/s3/c3. Its ROM decides boot mode purely from GPIO0
        // latched at reset, so on a classic part there is no software route and
        // the manual hold is the only way. Say so instead of pretending.
        //
        // ONE-WAY ON PURPOSE where it does work: the bit is cleared by a power
        // cycle, so a board that lands here by accident is recovered by
        // unplugging it. There is no way back in software -- once the ROM loader
        // owns the UART this firmware is no longer running.
#if defined(RTC_CNTL_OPTION1_REG) && defined(RTC_CNTL_FORCE_DOWNLOAD_BOOT)
        Serial.print("ACT,BOOTLOADER,1\n");
        Serial.flush();
        delay(50); // let the ack reach the Flipper before the UART goes away
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart();
#else
        // Classic ESP32. Reported as an explicit "cannot", so the app can tell
        // "this chip has no software path" from "the command was ignored by old
        // firmware" -- two situations that look identical from the other end.
        Serial.print("ACT,BOOTLOADER,0\n");
#endif
    } else if(cmd == "flockwifi") {
        g_combo = false;
    } else if(cmd.startsWith("ch ")) {
        int n = cmd.substring(3).toInt();
        // 1-14 are 2.4 GHz; 36-177 are the 5 GHz channels (C5 only). Anything
        // else, including 0, means "resume hopping".
        bool ok_24 = (n >= 1 && n <= 14);
        bool ok_5 = false;
#if FLOCK_HAS_5GHZ
        for(size_t i = 0; i < CHANNELS_5G_COUNT; i++) {
            if(n == CHANNELS_5G[i]) {
                ok_5 = true;
                break;
            }
        }
#endif
        if(ok_24 || ok_5) {
            g_lock_channel = (uint8_t)n;
            set_channel((uint8_t)n);
        } else {
            g_lock_channel = 0;
        }
    } else if(cmd.startsWith("band")) {
        // band 2g|5g|all -- pick which band(s) the hopper sweeps.
        // Always ACKs with the band actually in force, which on a 2.4-only chip
        // is 2g whatever was asked: silently accepting "5g" on a radio that has
        // no 5 GHz would report coverage that does not exist.
        String a = cmd.substring(4);
        a.trim();
#if FLOCK_HAS_5GHZ
        if(a == "5g")
            g_band = FlockBand5G;
        else if(a == "all")
            g_band = FlockBandAll;
        else if(a == "2g")
            g_band = FlockBand2G;
#else
        g_band = FlockBand2G;
#endif
        g_hop_i = 0;
        g_lock_channel = 0;
        set_channel(hop_channel(0));
        Serial.printf(
            "BAND,%s,%u\n",
            (g_band == FlockBand5G) ? "5g" : ((g_band == FlockBandAll) ? "all" : "2g"),
            (unsigned)hop_count());
    } else if(cmd.startsWith("locate")) {
        // locate <w|b> <hexmac> [ch]   -> stream LOC,<rssi> for that target
        // locate off                   -> stop
        if(cmd.indexOf("off") > 0) {
            if(g_locate_kind == 'b') {
                esp_wifi_set_promiscuous(true);
                set_channel(g_channel);
            }
            g_locate_kind = 0;
            g_lock_channel = 0;
        } else {
            int s1 = cmd.indexOf(' ');
            int s2 = (s1 > 0) ? cmd.indexOf(' ', s1 + 1) : -1;
            int s3 = (s2 > 0) ? cmd.indexOf(' ', s2 + 1) : -1;
            if(s1 > 0 && s2 > s1) {
                char kind = cmd.charAt(s1 + 1);
                String macs = (s3 > s2) ? cmd.substring(s2 + 1, s3) : cmd.substring(s2 + 1);
                macs.toLowerCase();
                uint8_t mac[6];
                if(macs.length() >= 12 && parse_hexmac(macs.c_str(), mac)) {
                    // promisc_cb() memcmp's g_locate_mac from the WiFi task, so a
                    // plain memcpy here can be observed half-applied and the
                    // Locator homes on a spliced old/new address. Swap the target
                    // and arm g_locate_kind together, under the lock -- kind is
                    // what gates the compare, so publishing it last inside the
                    // same section makes the whole target visible atomically.
                    portENTER_CRITICAL(&g_mux);
                    memcpy(g_locate_mac, mac, 6);
                    strncpy(g_locate_macs, macs.c_str(), 12);
                    g_locate_macs[12] = 0;
                    g_locate_kind = (kind == 'b') ? 'b' : 'w';
                    portEXIT_CRITICAL(&g_mux);
                    g_locate_ch = (s3 > s2) ? (uint8_t)cmd.substring(s3 + 1).toInt() : 0;
                    g_locate_best = -127;
                    g_scanning = false; // Locator dedicates the radio to one target
                    g_combo = false;
                    if(g_locate_kind == 'w') {
                        esp_wifi_set_promiscuous(true);
                        if(g_locate_ch >= 1 && g_locate_ch <= 14) {
                            g_lock_channel = g_locate_ch;
                            set_channel(g_locate_ch);
                        } else {
                            g_lock_channel = 0;
                        }
                    }
                }
            }
        }
    }
}

void loop() {
    if(Serial.available()) {
        handle_command(Serial.readStringUntil('\n'));
    }

    // Before every early return below: a fix must keep flowing in Locator mode
    // and while idle, or detections geotag with a stale position (or none).
    // The blocking BLE scans drain the GPS between their 1-second slices too
    // (see ble_do_scan), so no phase of the rotation stalls this for longer than
    // about a second.
    gps_poll();

    uint32_t now = millis();

    // Locator mode owns the radio: stream the target's live RSSI as LOC lines.
    if(g_locate_kind == 'w') {
        // 400 ms, not 120.
        //
        // The main target is a Flock camera, and those are station-mode devices
        // that sweep the channels with probe requests. We sit on ONE channel, so
        // the target lands on us roughly once every 1.6 s. Against that, a 120 ms
        // window that hard-resets its peak spends about 92% of its life expiring
        // empty, and the readings that do survive are single raw frames.
        //
        // A longer window does not invent data -- the same frames arrive either
        // way. It stops discarding the peak between them, so a reading that was
        // captured actually reaches the operator instead of being thrown away
        // 120 ms later. Still silent when genuinely nothing was heard, which is
        // what makes "out of range" mean something.
        if(now - g_last_loc >= 400) {
            g_last_loc = now;
            if(g_locate_best > -127) {
                Serial.printf("LOC,%d\n", g_locate_best);
                g_locate_best = -127; // reset window; quiet interval = no LOC (out of range)
            }
        }
        return;
    }
    if(g_locate_kind == 'b') {
        ble_locate_scan(); // ~1 s blocking scan; emits LOC if the target is seen
        return;
    }

    // Dual-band: after a WiFi-promiscuous phase, run a BLE scan phase, then
    // resume. The BLE scan blocks for a few seconds and restores promiscuous.
    if(g_combo && now - g_phase_start >= COMBO_WIFI_MS) {
        ble_do_scan(COMBO_BLE_SEC);
        g_phase_start = millis();
        return;
    }

    // When not scanning, stay idle (no channel hopping, no status TX) so the
    // board isn't "in use" after the app stops/exits.
    if(!g_scanning) return;

    // Channel hop every 300 ms unless locked.
    //
    // 1-13, not 1-11. 12 and 13 are unusable for APs in the US, so the old bound
    // cost nothing there -- but they are ordinary channels across most of the
    // rest of the world, and a probe REQUEST is not bound by the same rule
    // anywhere. The price is ~18% less dwell per channel.
    //
    // 14 stays out: it is Japan-only, DSSS-only, and would burn dwell almost
    // everywhere to cover almost nothing.
    //
    // On a 5 GHz-capable radio the sweep also walks the 28 5 GHz channels -- see
    // the dual-band block near the top. The cursor is an index rather than
    // "current + 1" because the 5 GHz channel numbers are not contiguous.
    if(g_lock_channel == 0 && now - g_last_hop >= 300) {
        g_last_hop = now;
        uint16_t n = hop_count();
        g_hop_i = (uint16_t)((g_hop_i + 1) % n);
        set_channel(hop_channel(g_hop_i));
    }

    // Status heartbeat ~1 Hz. 4th field = deauth/disassoc frames in the LAST
    // interval (a rate, not a lifetime total) so the alert clears when a flood
    // stops. Older parsers ignore the extra field.
    if(now - g_last_status >= 1000) {
        g_last_status = now;
        uint32_t deauth_rate = g_deauths - g_deauths_last;
        g_deauths_last = g_deauths;
        Serial.printf(
            "S,%lu,%lu,%u,%lu\n",
            (unsigned long)g_frames,
            (unsigned long)g_hits,
            (unsigned)g_channel,
            (unsigned long)deauth_rate);

        // Active attack-tool signatures for this interval, then reset the windows.
        // Snapshot and reset the beacon ring under the lock: note_beacon_bssid()
        // appends from the WiFi task using g_beacon_ring_n as its write bound, so
        // zeroing it outside the lock can race a half-finished append.
        portENTER_CRITICAL(&g_mux);
        uint32_t beacon_distinct = g_beacon_distinct;
        g_beacon_distinct = 0;
        g_beacon_ring_n = 0;
        portEXIT_CRITICAL(&g_mux);

        if(g_probe_reqs >= PROBE_FLOOD_MIN)
            Serial.printf("ATK,probeflood,%lu\n", (unsigned long)g_probe_reqs);
        if(beacon_distinct >= BEACON_FLOOD_MIN)
            Serial.printf("ATK,beaconflood,%lu\n", (unsigned long)beacon_distinct);
        g_probe_reqs = 0;
    }
}
