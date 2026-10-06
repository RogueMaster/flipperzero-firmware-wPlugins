// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "flock_db.h"
#include <string.h>

/**
 * 30 OUI prefixes observed in fielded Flock Safety deployments.
 * Mostly @NitekryDPaul research; b4:1e:52 is Flock Safety's own
 * IEEE-registered OUI (GainSec).
 *
 * REMOVED 2026-10-05, both carried since v0.8 and both failing rules this file
 * already states:
 *   3c:71:bf  registered to ESPRESSIF. Every other Espressif prefix was
 *             rejected as a chip vendor that would detect our own companion
 *             board; this one predated that sweep and was never re-checked.
 *             Any ESP32 gadget on the block sending a wildcard probe scored
 *             Likely. Now in TOO_GENERIC.
 *   82:6b:f2  has the locally-administered bit set (0x82 & 0x02), so it is not
 *             a registered prefix at all: it is the first three bytes of an
 *             address some device invented. A camera that keeps a fixed
 *             randomised address is what the `macs` pin in signatures.json is
 *             for, on the WHOLE address. Now in RETRACTED.
 * These are generic vendor prefixes (Liteon, Espressif, etc.), hence OUI-only
 * matches are scored "possible", never "confirmed".
 *
 * ADDED 2026-09-07 after a sweep of the current community tables, each prefix
 * resolved against the IEEE MA-L registry before acceptance (the memory rule:
 * verify by REGISTRY ORGANISATION NAME, never by the list it came from):
 *   e0:0a:f6  Liteon -- the same vendor as 21 of the entries below, so it is
 *             consistent with the module Flock actually buys. From plume tier 2.
 *   38:5b:44  Silicon Laboratories -- accepted because it is corroborated by a
 *             FIELD-OBSERVED BLE NAME rather than by a list: "RWLS-38:5B:44:B3:
 *             0F:5A", a Flock unit that appends its own MAC to its GAP name. We
 *             already carry three SiLabs prefixes (58:8e:81, ec:1b:bd, 90:35:ea).
 *
 * ADDED 2026-09-10, during a sweep of community prefix lists:
 *   14:b5:cd  LITEON TECHNOLOGY CORPORATION, resolved against the IEEE MA-L
 *             registry before acceptance. Liteon is the vendor behind 21 of the
 *             entries already here, so this is the same module line Flock buys,
 *             not a new claim.
 * Two other prefixes circulating in those lists were re-checked and stay
 * REJECTED, for the reasons in the paragraph above: 48:27:ea is SAMSUNG and
 * a4:cf:12 is ESPRESSIF. TOO_GENERIC in tools/check_oui_parity.py is what keeps
 * a future sweep from quietly re-importing either one.
 *
 * REJECTED in the same sweep, and now ENFORCED by TOO_GENERIC in
 * tools/check_oui_parity.py so they cannot be quietly re-imported: 48:27:ea is
 * SAMSUNG (phones and hotspots -- literally the false-positive class a user
 * already reported), thirteen Espressif prefixes (chip vendor; they would make
 * this app detect its own companion board), f0:9f:c2 Ubiquiti, 8c:1f:64 and
 * 4c:6e:44 which belong to the IEEE Registration Authority itself (shared
 * MA-M/MA-S blocks -- a 3-byte match there names nobody), and d8:a0:d8 which is
 * not registered in MA-L at all. simeononsecurity/flock-finder ships 48:27:ea
 * and a4:cf:12; zmattmanz/plume ships all of the above. Widening recall from
 * those tables wholesale would cost precision, which the project rules forbid.
 *
 * SOURCE OF TRUTH is now nitekry/nite-oui-collection ->
 * groups/flockers/my_tested_flock.md, a per-prefix table with Confidence and
 * Status columns. It SUPERSEDES the flat, statusless
 * colonelpanichacks/flock-you -> datasets/NitekryDPaul_wifi_ouis.md list this
 * table was originally imported from -- the flat list cannot record that a
 * prefix was later doubted, so re-importing from it silently undoes retractions.
 *
 * RETRACTED UPSTREAM -- never re-add any of these: f8:a2:d6 ("low confidence;
 * hit on a Sony Media Player"), 6c:cd:d6 (Netgear), 94:2a:6f + f4:e2:c6
 * (Ubiquiti), cc:cc:cc (no hits), 00:0c:e7 (possible FP). The flat list still
 * carries some of them, which is exactly why re-importing from it is forbidden.
 *
 * f8:a2:d6 HAS BEEN REMOVED TWICE. Dropped 2026-07-27 for v0.44, then silently
 * re-added by 93beede (2026-08-05) -- a commit about TIGHTENING precision -- while
 * the table was reflowed, and it shipped in v0.67 through v0.71 scoring "Likely"
 * on any wildcard probe. Nothing caught it: the parity gate compared this table
 * against the sketch's and 93beede drifted BOTH sides identically, so 32-vs-32
 * passed while both count comments still said 31. That recurrence is why
 * tools/check_oui_parity.py now also checks the declared count and enforces a
 * retracted-prefix denylist, and why test_flock_db.c asserts each one is absent.
 * A comment is not a guard; treat the denylist as the real rule.
 *
 * This table is a claim of FIELD CORROBORATION. Uncorroborated candidates
 * belong in the user signature file, not here -- see docs/signatures.md.
 *
 * PROVENANCE GRADES. The rows below are NOT uniform evidence, and the ordering
 * is historical, so the grades are listed here rather than inline (the entries
 * would have to be reordered to comment them per row, and reordering both files
 * in lockstep is a worse risk than this list). Full table in docs/signatures.md.
 *   - Contract manufacturer (Liteon/USI), shared with unrelated consumer gear:
 *     f4:6a:dd, 00:f4:8d, d0:39:57, e8:d0:fc. WatchFlock files these separately
 *     from direct-Flock prefixes and warns a MAC match alone may be a FP.
 *   - Flat-list orphans, absent from the curated table in EVERY section (not
 *     Active, not testing, not Removed): 70:08:94, 58:00:e3, 5c:93:a2, 64:6e:69.
 *     Kept because absence is not retraction, but their status is unverifiable.
 *   - Weak upstream confidence: 08:3a:88 ("BLE Ring conflict - unsure"). Does
 *     NOT meet the field-corroboration bar this table's first line claims.
 * Nothing here changes scoring: an OUI-only match caps at "possible" regardless.
 *
 * DEMOTED to docs/signatures.seed.json (v0.73): 48:27:ea and a4:cf:12. Upstream
 * rates both "low confidence, WiGLE crowdsource" -- the weakest tier it has --
 * and the IEEE registry says 48:27:ea belongs to SAMSUNG ELECTRONICS and
 * a4:cf:12 to Espressif. Neither is Flock hardware; they are the chip vendors
 * inside a great many phones, tablets and hotspots.
 *
 * That mattered in the field, not just on paper. The companion scores
 * "Flock OUI + wildcard probe request" as LIKELY, and a wildcard probe is the
 * single most ordinary frame a Wi-Fi client emits -- it is what scanning for
 * networks looks like. So a Samsung-based T-Mobile hotspot doing nothing but
 * looking for a network was reported as a likely ALPR camera. Same failure as
 * the "tmobile-5416" gateway that killed bare-OUI-on-a-beacon scoring, one rung
 * up the ladder. A user reported it; that is what got these two demoted and the
 * probe-rate gate added on the companion side.
 *
 * Twenty-one of the prefixes below are registered to LITEON alone. Read the
 * table as "chip vendors Flock buys from", not "Flock devices" -- only
 * b4:1e:52 is registered to Flock Safety itself.
 *
 * DUPLICATED in esp32_companion/flock_companion/flock_companion.ino, which
 * scores ESP-side. No shared header is possible (that side is an Arduino
 * sketch), so change BOTH and keep the row layout identical -- EXACTLY four
 * entries per row -- so they can be diffed by eye. tools/check_oui_parity.py
 * enforces content parity as a required CI gate; the row layout is on you.
 */
static const uint8_t flock_ouis[][3] = {
    {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc}, {0x80, 0x30, 0x49},
    {0xb8, 0x35, 0x32}, {0x14, 0x5a, 0xfc}, {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88},
    {0x9c, 0x2f, 0x9d}, {0xc0, 0x35, 0x32}, {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    {0xf4, 0x6a, 0xdd}, {0x24, 0xb2, 0xb9}, {0x00, 0xf4, 0x8d}, {0xd0, 0x39, 0x57},
    {0xe8, 0xd0, 0xfc}, {0xe0, 0x4f, 0x43}, {0xb8, 0x1e, 0xa4}, {0x70, 0x08, 0x94},
    {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x58, 0x00, 0xe3}, {0x90, 0x35, 0xea},
    {0x5c, 0x93, 0xa2}, {0x64, 0x6e, 0x69}, {0xb4, 0x1e, 0x52}, {0xe0, 0x0a, 0xf6},
    {0x38, 0x5b, 0x44}, {0x14, 0xb5, 0xcd},
};

#define FLOCK_OUI_COUNT (sizeof(flock_ouis) / sizeof(flock_ouis[0]))

/**
 * SoundThinking (formerly ShotSpotter) acoustic gunshot sensors.
 *
 * A DIFFERENT DEVICE CLASS, not an ALPR: these listen, they do not read plates.
 * Kept in its own table so a hit can be reported as what it is. Folding it into
 * flock_ouis[] would have the app announce a camera it never saw, which is the
 * over-claiming the project rules forbid.
 *
 * d4:11:d6 via JakeSwiz/WatchFlock (esp32_marauder/WiFiScan.cpp,
 * fy_soundthinking_mac_prefixes[]). Like every OUI here it is a vendor prefix,
 * not proof: an OUI-only hit scores "possible", and there is no known SSID tell
 * for this hardware, so an acoustic detection can never reach "confirmed".
 *
 * DUPLICATED in esp32_companion/flock_companion/flock_companion.ino -- same
 * hand-sync rule as flock_ouis[] above, and covered by the same CI parity gate.
 */
static const uint8_t soundthinking_ouis[][3] = {
    {0xd4, 0x11, 0xd6},
};

#define SOUNDTHINKING_OUI_COUNT (sizeof(soundthinking_ouis) / sizeof(soundthinking_ouis[0]))

bool soundthinking_oui_match(const uint8_t* mac) {
    if(!mac) return false;
    for(size_t i = 0; i < SOUNDTHINKING_OUI_COUNT; i++) {
        if(mac[0] == soundthinking_ouis[i][0] && mac[1] == soundthinking_ouis[i][1] &&
           mac[2] == soundthinking_ouis[i][2]) {
            return true;
        }
    }
    // Deliberately NOT extended by signatures.json: the user schema has no class
    // field, so a user OUI is always read as ALPR. Adding acoustic prefixes needs
    // a schema change, not a silent reinterpretation of existing user files.
    return false;
}

/**
 * Axon Enterprise (formerly TASER International) police equipment.
 *
 * CORRECTED 2026-08-29 -- THIS TABLE USED TO MEAN "EQUIPMENT THAT MOVES".
 * Until v0.77 the comment here said Axon made body-worn and in-car kit and "not
 * fixed infrastructure at all", so a hit could never be a camera on a pole, and
 * the label said "body/in-car" to enforce that. In 2026 Axon launched Outpost
 * and Lightpost -- two FIXED, pole- and streetlight-mounted ALPR cameras, sold
 * into exactly the contracts cities were cancelling with Flock -- on this same
 * single OUI registration. The old claim is now wrong in the opposite direction.
 *
 * Nothing observable from a MAC separates an Axon Body camera from an Axon
 * Outpost, so the label no longer tries: it reads "Axon: body or fixed". Do not
 * "restore" the body/in-car wording; it is not a style choice.
 *
 * 00:25:df is Axon Enterprise's IEEE OUI registration -- the ONLY one they hold.
 * Verified directly against the IEEE registry, not taken from a list.
 *
 * DO NOT ADD PREFIXES BY SEARCHING A VENDOR DATABASE FOR "axon". That substring
 * also matches Axon NETWORKS Inc (00:58:28, 00:c0:d4, 84:70:03 -- an unrelated
 * networking company), Axona, Axonne, Interaxon, Maxon, Praxon, Paxonet and
 * Yaxon. Twelve unrelated registrants, none of them police equipment.
 *
 * NOR FROM A CURATED "LAW ENFORCEMENT" OUI LIST. One such list was checked
 * prefix-by-prefix against the IEEE registry and 11 of its 15 entries were wrong:
 * it attributed Apple prefixes to Digital Ally, Nintendo to WatchGuard, General
 * Motors and Samsung to Panasonic i-PRO, Xiaomi and Dell to Getac, and Axis
 * Communications to Flock Safety. Two of its three "Axon / TASER" prefixes are
 * really Honeywell Security and Nisca. Verify every prefix at the registry.
 *
 * FIELD STATUS: REGISTRY-VERIFIED, NEVER FIELD-OBSERVED. We have no capture of an
 * Axon device using this prefix on the air. Embedded products frequently expose
 * the Wi-Fi MODULE vendor's OUI instead of the brand owner's -- which is exactly
 * why most Flock hardware appears as Liteon or Espressif rather than b4:1e:52. So
 * this may match every Axon radio, or none of them. Scored accordingly: an
 * OUI-only hit caps at "possible", same as every other OUI in this file.
 *
 * DUPLICATED in esp32_companion/flock_companion/flock_companion.ino and covered
 * by the same tools/check_oui_parity.py gate as the other two tables.
 */
static const uint8_t axon_ouis[][3] = {
    {0x00, 0x25, 0xdf},
};

#define AXON_OUI_COUNT (sizeof(axon_ouis) / sizeof(axon_ouis[0]))

bool axon_oui_match(const uint8_t* mac) {
    if(!mac) return false;
    for(size_t i = 0; i < AXON_OUI_COUNT; i++) {
        if(mac[0] == axon_ouis[i][0] && mac[1] == axon_ouis[i][1] && mac[2] == axon_ouis[i][2]) {
            return true;
        }
    }
    // Deliberately NOT extended by signatures.json, for the same reason
    // soundthinking_oui_match() is not: the user schema has no class field, so a
    // user OUI is always read as ALPR. Letting one silently become a body-camera
    // detection would be a reinterpretation the file never asked for.
    return false;
}

/*
 * ===========================================================================
 * VENDOR-EXCLUSIVE OUIs (15 across 7 vendors) -- competitor surveillance kit.
 * ===========================================================================
 *
 * WHY THESE ARE A DIFFERENT KIND OF EVIDENCE FROM flock_ouis[].
 *
 * flock_ouis[] is 29 prefixes of which 21 are LITEON and only one (b4:1e:52) is
 * registered to Flock Safety itself. It is a list of "chip vendors Flock buys
 * from", so a bare OUI hit there describes an enormous number of ordinary
 * consumer devices -- which is why bare-OUI-on-a-beacon scoring was removed
 * after it called a T-Mobile gateway a possible ALPR camera.
 *
 * Every prefix below is instead registered to the SURVEILLANCE VENDOR ITSELF.
 * 94:7b:be is Ubicquia's own registration and Ubicquia makes streetlight nodes
 * and nothing else. That is a categorically stronger signal than "this device
 * contains a Liteon radio", and it is why these may score on a bare beacon
 * (possible) where a flock_ouis[] hit may not. It is NOT strong enough to name
 * a product: see the class note on each table.
 *
 * ALL REGISTRY-VERIFIED, NONE FIELD-OBSERVED. Every prefix was read directly out
 * of standards-oui.ieee.org/oui/oui.txt on 2026-08-29, one at a time, and the
 * organisation string is quoted verbatim on each table. We have no capture of
 * any of this hardware on the air. Embedded products routinely expose the Wi-Fi
 * MODULE's OUI rather than the brand owner's -- which is exactly why most Flock
 * gear appears as Liteon -- so these may match every unit of a product line, or
 * none of it. Scored accordingly: OUI-only never exceeds "possible".
 *
 * DO NOT ADD PREFIXES BY SUBSTRING-SEARCHING A VENDOR DATABASE. That rule is not
 * theoretical here; two of the vendors below are live traps:
 *   - "genetec" also matches GENETEC Corporation (00:0a:b1), an unrelated
 *     Japanese company, and Netgenetech (d8:c0:68). Neither is Genetec Inc.
 *   - "motorola" also matches Motorola Mobility LLC, a Lenovo Company
 *     (50:16:f4, c4:a0:52, c8:58:95 and more) -- consumer PHONES, a different
 *     company from Motorola Solutions. Including one would repeat the
 *     48:27:ea / a4:cf:12 failure on a far larger population.
 * Verify every prefix at the registry, by organisation name, before adding it.
 *
 * NOT ADDABLE AT ALL, so nobody re-researches this: Rekor, Vigilant, PlateSmart,
 * Altumint, LiveView, RedSpeed, Verra Mobility, Getac, Digital Ally and
 * Panasonic i-PRO hold NO IEEE registration -- they buy their hardware. Any list
 * claiming an OUI for them is fabricating it. (SoundThinking does hold one, but
 * under its old name ShotSpotter, and it is already in soundthinking_ouis[].)
 *
 * DUPLICATED in esp32_companion/flock_companion/flock_companion.ino, same
 * hand-sync rule and the same tools/check_oui_parity.py gate as the three tables
 * above -- EXACTLY four entries per row so the two files diff by eye.
 */

/**
 * Ubicquia LLC -- registry organisation "Ubicquia LLC", their ONLY registration.
 *
 * THE REASON THIS TABLE EXISTS AT ALL. Axon Lightpost -- one of the two fixed
 * ALPR cameras Axon launched in 2026 to take over contracts cities cancelled
 * with Flock -- is built with Ubicquia and mounts in the NEMA photocell socket
 * of an existing streetlight. The Ubicquia UbiHub it is based on is a triband
 * Wi-Fi 6 access point (the AP/AI variant adds dual 4K cameras and LPR), so
 * unlike a Flock camera -- which stopped beaconing around December 2025 and now
 * only probes -- this hardware is expected to BEACON CONTINUOUSLY. If that holds
 * in the field it is a considerably easier detection than Flock itself.
 *
 * CLASS IS Gear, NOT Alpr, and that is deliberate: a UbiHub AP6 is public Wi-Fi
 * with no camera at all, while an AP/AI carries the plate reader. Same OUI, and
 * nothing on the air separates them. Naming the vendor is honest; naming the
 * product would not be.
 */
static const uint8_t ubicquia_ouis[][3] = {
    {0x94, 0x7b, 0xbe},
};

#define UBICQUIA_OUI_COUNT (sizeof(ubicquia_ouis) / sizeof(ubicquia_ouis[0]))

/**
 * Motorola Solutions (7) -- four registered "Motorola Solutions Inc." and three
 * "MOTOROLA SOLUTIONS MALAYSIA SDN. BHD.".
 *
 * Motorola sells ALPR under the Vigilant, PIPS and Avigilon brands. The L6Q
 * quick-deploy plate reader is the interesting one: it ships built-in LTE, Wi-Fi
 * AND Bluetooth, and is commissioned from a phone over Bluetooth or Wi-Fi via
 * their "LPR Mobile Companion" app -- the same provisioning-radio pattern that
 * makes Flock's Falcon detectable in the first place.
 *
 * CLASS IS Gear AND MUST STAY Gear. These same seven prefixes also carry APX and
 * MOTOTRBO hand-held radios, worn by police, security guards and warehouse staff
 * alike. An OUI hit here means "Motorola Solutions equipment", full stop.
 * Mapping it to Alpr would announce a plate reader every time a radio walked
 * past, which is the precision failure this project refuses.
 *
 * NOT Motorola Mobility (Lenovo) -- see the trap list above.
 *
 * FIELD EVIDENCE AGAINST THIS TABLE, recorded 2026-09-07 and NOT yet acted on.
 * soyboi1312/all-cameras-are-beacons audited its own captures and found that ALL
 * 27 Motorola Wi-Fi OUI hits were confirmed NOT to be police equipment; it now
 * boots Motorola matching OFF by default and requires an explicit opt-in. Their
 * stated cause is that these blocks also carry Sierra Wireless AirLink and
 * Cradlepoint vehicle routers -- transit buses and fleet vehicles, not body cams.
 *
 * Left ON here for now, for one reason: this table has never claimed a product.
 * It maps to FlockClassGear ("vendor known, kind not determined") and its label
 * reads "Motorola Solutions", so a hit says exactly what the evidence supports
 * and no more -- which is the thing that made their version misleading and does
 * not apply to ours. But 27 out of 27 is not a rounding error, and if a future
 * capture reproduces it here, the honest move is to drop these prefixes rather
 * than to keep a rung that is almost always wrong. Do not treat this note as
 * settled; treat it as the next thing to measure.
 */
static const uint8_t motorola_ouis[][3] = {
    {0x00, 0x04, 0x7d},
    {0x00, 0x18, 0x85},
    {0x00, 0x1f, 0x92},
    {0x4c, 0xcc, 0x34},
    {0x10, 0x74, 0x6f},
    {0xb8, 0xe2, 0x8c},
    {0x9c, 0x86, 0x2b},
};

#define MOTOROLA_OUI_COUNT (sizeof(motorola_ouis) / sizeof(motorola_ouis[0]))

/**
 * Verkada Inc -- registry organisation "Verkada Inc", their only registration.
 *
 * Verkada cameras join Wi-Fi as CLIENTS, so they emit probe requests -- the
 * behaviour this app is already built to catch -- and the GW31E Wi-Fi Gateway is
 * commissioned over Bluetooth from the Command app while mounted on a pole.
 *
 * CLASS IS Gear. Verkada's line is mostly ordinary building-security cameras and
 * access control; LPR is one product among many. A hit says Verkada, not ALPR.
 */
static const uint8_t verkada_ouis[][3] = {
    {0xe0, 0xa7, 0x00},
};

#define VERKADA_OUI_COUNT (sizeof(verkada_ouis) / sizeof(verkada_ouis[0]))

/**
 * Genetec Inc (2) -- both registrations read "Genetec Inc." verbatim.
 *
 * WEAKEST TABLE HERE, AND KEPT DELIBERATELY. Genetec's AutoVu is the ALPR line
 * widely deployed in Canada and in parking enforcement, but the published SharpV
 * and SharpZ3 specifications list wired gigabit Ethernet ONLY -- no Wi-Fi, no
 * Bluetooth. So there is a real chance this table never fires, and that is fine:
 * it costs 6 bytes, it can only ever produce a correctly-named "possible", and
 * having the prefix present means a field capture that DOES hit it gets
 * attributed instead of landing as an unattributed mystery.
 *
 * NOT GENETEC Corporation (00:0a:b1) and NOT Netgenetech (d8:c0:68).
 */
static const uint8_t genetec_ouis[][3] = {
    {0x00, 0xbf, 0x15},
    {0x0c, 0xbf, 0x15},
};

#define GENETEC_OUI_COUNT (sizeof(genetec_ouis) / sizeof(genetec_ouis[0]))

/**
 * Avigilon Alta -- registry organisation "Avigilon Alta", the only Avigilon
 * registration in the file (there is no separate "Avigilon Corporation" MA-L).
 *
 * Motorola-owned, and one of the brands Motorola sells fixed ALPR under. Alta is
 * the former Openpath cloud access-control line, so like Verkada this is
 * building security as often as it is plate reading: CLASS IS Gear.
 */
static const uint8_t avigilon_ouis[][3] = {
    {0x70, 0x1a, 0xd5},
};

#define AVIGILON_OUI_COUNT (sizeof(avigilon_ouis) / sizeof(avigilon_ouis[0]))

/**
 * Utility, Inc -- "BodyWorn" body-worn cameras.
 *
 * Both prefixes are EXCLUSIVE MA-L blocks registered to "Utility, Inc" of
 * Decatur, GA (IEEE, checked 2026-09-07), so unlike the Flock silicon prefixes
 * a match here does name one company. It still does not name a PRODUCT: the
 * class stays "body-worn" because that is what this vendor ships, and a bare OUI
 * match still scores possible like every other.
 *
 * Corroborated by a BLE naming tell as well -- adverts containing
 * "BodyWorn Remote" (nite-oui-collection, 2025-08), which the companion matches
 * independently of the MAC and which therefore survives address randomisation.
 */
static const uint8_t utility_ouis[][3] = {
    {0x00, 0x09, 0xbc},
    {0x00, 0x16, 0xed},
};

#define UTILITY_OUI_COUNT (sizeof(utility_ouis) / sizeof(utility_ouis[0]))

/**
 * Digital Ally, Inc -- "FirstVU" body-worn and in-car cameras.
 *
 * Exclusive MA-L block registered to "Digital Ally, Inc." of Grain Valley, MO
 * (IEEE, checked 2026-09-07).
 *
 * NOT ADDED alongside these, and the reasons are the interesting part:
 *   fc:01:9e  VIEVU -- a real body-camera block, but Axon bought VIEVU in 2018
 *             and discontinued the line. Adding a prefix for hardware that is
 *             largely out of service buys recall we cannot demonstrate and adds
 *             a row nobody can verify.
 *   d4:2d:c5  i-PRO Co., Ltd -- a genuine surveillance vendor, but its range runs
 *             from body cameras to fixed network cameras to industrial sensors.
 *             That is the Motorola problem again: the vendor is knowable and the
 *             PRODUCT is not, so it could only ever be class Gear, and no field
 *             observation ties this block to police kit specifically.
 */
static const uint8_t digitalally_ouis[][3] = {
    {0x00, 0x23, 0xbd},
};

#define DIGITALALLY_OUI_COUNT (sizeof(digitalally_ouis) / sizeof(digitalally_ouis[0]))

/**
 * Drone manufacturers (24), as a FALLBACK to Remote ID -- never the main event.
 *
 * READ THE LIMITS BEFORE TRUSTING THIS TABLE.
 *
 * 1. IT CANNOT SEE THE FLEET THAT MATTERS. Resolved against the IEEE registry on
 *    2026-09-07: of the five vendors a US police department realistically buys
 *    from today -- Skydio, BRINC, Aerodome, Flock and Paladin -- only Skydio
 *    holds an IEEE block. BRINC, Aerodome and Paladin hold NOTHING, so their
 *    radios transmit under whatever module vendor they bought. Three of the five
 *    are structurally invisible to any prefix match, forever. That is why
 *    helpers/open_drone_id.c exists and why it is the primary path: Remote ID is
 *    a legal broadcast mandate and does not care who built the aircraft.
 *
 * 2. A HIT HERE IS NOT A POLICE DRONE. DJI's blocks are on more hobbyist
 *    quadcopters than anything else. The class says "unmanned aircraft" and the
 *    vendor says "Drone"; neither claims a police deployment, and nothing in a
 *    MAC could. Scored like every other bare-OUI match -- possible.
 *
 * 3. MA-L HOLDERS ONLY. Autel Robotics, Yuneec, Inspired Flight, Quantum
 *    Systems, ideaForge, ACSL, Cyon, UAV Navigation and Anduril all appear in
 *    community drone lists, and every one of them sits inside an IEEE
 *    Registration Authority MA-M/MA-S block (8c:1f:64, ec:5b:cd, e0:b6:f5,
 *    34:b5:f3, ac:86:d1, 24:a1:0d, b4:4d:43, e8:b4:70). Those blocks are shared
 *    by hundreds of unrelated companies, and this table is three bytes wide, so
 *    matching them would flag arbitrary hardware as an aircraft. They are in
 *    TOO_GENERIC in tools/check_oui_parity.py for that reason.
 *
 * DELIBERATELY EXCLUDED, though registered to DJI: f8:40:68 (DJI Ronin, camera
 * gimbals) and 20:1f:55 (DJI Osmo, handheld cameras). Both are DJI and neither
 * flies. Reporting a photographer's gimbal as an aircraft overhead is exactly the
 * over-claim FlockDevClass exists to prevent.
 *
 * ALSO EXCLUDED as look-alikes: 4c:48:da and 00:1f:64 are "Beijing Autelan
 * Technology", a NETWORKING company, not Autel Robotics -- the same
 * substring-of-a-vendor-name trap that put Axon Networks and Motorola Mobility on
 * the misattributed list.
 *
 * DUPLICATED in the ESP sketch under the same hand-sync rule as the tables above
 * and covered by the same CI parity gate.
 */
static const uint8_t drone_ouis[][3] = {
    // SZ DJI Technology
    {0x60, 0x60, 0x1f},
    {0x34, 0xd2, 0x62},
    {0x48, 0x1c, 0xb9},
    {0xe4, 0x7a, 0x2c},
    {0x58, 0xb8, 0x58},
    {0x04, 0xa8, 0x5a},
    {0x8c, 0x58, 0x23},
    {0x0c, 0x9a, 0xe6},
    {0x88, 0x29, 0x85},
    {0x4c, 0x43, 0xf6},
    // DJI Baiwang Technology
    {0x9c, 0x5a, 0x8a},
    {0xec, 0x72, 0xf7},
    {0x34, 0x91, 0xf0},
    // Skydio -- the ONLY one of the five US police-drone vendors with a block
    {0x38, 0x1d, 0x14},
    // Parrot SA
    {0x00, 0x12, 0x1c},
    {0x00, 0x26, 0x7e},
    {0x90, 0x03, 0xb7},
    {0x90, 0x3a, 0xe6},
    {0xa0, 0x14, 0x3d},
    // US defence / public-safety airframes
    {0xb0, 0x30, 0xc8}, // Teal Drones
    {0x00, 0x1a, 0xf9}, // AeroVironment
    {0x14, 0xdd, 0x48}, // Shield AI
    {0xec, 0x71, 0x5e}, // Freefly Systems
    {0x74, 0xb8, 0x0f}, // Zipline International
};

#define DRONE_OUI_COUNT (sizeof(drone_ouis) / sizeof(drone_ouis[0]))

/**
 * The one place a MAC becomes a (vendor, class) pair.
 *
 * A SINGLE TABLE ON PURPOSE. flock_class_from_mac() used to answer "what class"
 * with a chain of if-statements over three separate matchers; bolting a vendor
 * onto that shape would have meant two parallel chains that could disagree about
 * the same MAC. Here a prefix's vendor and its class are one row, so they cannot
 * drift apart, and adding a vendor is one table plus one row instead of an edit
 * in four places.
 *
 * ORDER: flock_ouis is scanned first so historical behaviour is preserved
 * bit-for-bit if a prefix ever appears in two tables. The vendor tables are
 * disjoint by construction -- each is exactly one registrant.
 */
typedef struct {
    const uint8_t (*ouis)[3];
    size_t count;
    FlockVendor vendor;
    FlockDevClass cls;
} FlockVendorTable;

static const FlockVendorTable flock_vendor_tables[] = {
    {flock_ouis, FLOCK_OUI_COUNT, FlockVendorFlock, FlockClassAlpr},
    {soundthinking_ouis, SOUNDTHINKING_OUI_COUNT, FlockVendorSoundThinking, FlockClassAcoustic},
    // Axon keeps FlockClassBodycam so stored records and the cls=x wire token
    // keep the meaning they shipped with. The LABEL is what was corrected --
    // see flock_device_long_str(): Axon now ships fixed ALPR on this same OUI.
    {axon_ouis, AXON_OUI_COUNT, FlockVendorAxon, FlockClassBodycam},
    {ubicquia_ouis, UBICQUIA_OUI_COUNT, FlockVendorUbicquia, FlockClassGear},
    {motorola_ouis, MOTOROLA_OUI_COUNT, FlockVendorMotorola, FlockClassGear},
    {verkada_ouis, VERKADA_OUI_COUNT, FlockVendorVerkada, FlockClassGear},
    {genetec_ouis, GENETEC_OUI_COUNT, FlockVendorGenetec, FlockClassGear},
    {avigilon_ouis, AVIGILON_OUI_COUNT, FlockVendorAvigilon, FlockClassGear},
    {utility_ouis, UTILITY_OUI_COUNT, FlockVendorUtility, FlockClassBodycam},
    {digitalally_ouis, DIGITALALLY_OUI_COUNT, FlockVendorDigitalAlly, FlockClassBodycam},
    {drone_ouis, DRONE_OUI_COUNT, FlockVendorDrone, FlockClassDrone},
};

#define FLOCK_VENDOR_TABLE_COUNT (sizeof(flock_vendor_tables) / sizeof(flock_vendor_tables[0]))

/** Row whose table contains `mac`'s OUI, or NULL. Built-ins only. */
static const FlockVendorTable* vendor_row_for_mac(const uint8_t* mac) {
    if(!mac) return NULL;
    for(size_t t = 0; t < FLOCK_VENDOR_TABLE_COUNT; t++) {
        const FlockVendorTable* vt = &flock_vendor_tables[t];
        for(size_t i = 0; i < vt->count; i++) {
            if(mac[0] == vt->ouis[i][0] && mac[1] == vt->ouis[i][1] && mac[2] == vt->ouis[i][2]) {
                return vt;
            }
        }
    }
    return NULL;
}

bool vendor_exclusive_oui_match(const uint8_t* mac) {
    const FlockVendorTable* vt = vendor_row_for_mac(mac);
    // Flock / SoundThinking / Axon are excluded on purpose: they have their own
    // matchers and their own (weaker, shared-silicon) evidence rules. This asks
    // about the vendor-exclusive competitor prefixes added in v0.77, and about
    // the drone manufacturers added in v0.88 -- both are blocks a single company
    // holds outright, which is what makes them worth a rung at all. It is the
    // shared silicon prefixes, not the exclusive ones, that need the weaker
    // treatment.
    return vt && vt->vendor != FlockVendorFlock && vt->vendor != FlockVendorSoundThinking &&
           vt->vendor != FlockVendorAxon;
}

FlockVendor flock_vendor_from_mac(const uint8_t* mac) {
    const FlockVendorTable* vt = vendor_row_for_mac(mac);
    // Deliberately does NOT consult g_extras. A user OUI from signatures.json
    // carries no vendor, and inventing one would attribute a detection to a
    // company that file never named.
    return vt ? vt->vendor : FlockVendorUnknown;
}

FlockVendor flock_vendor_of(const uint8_t* mac, const char* ssid) {
    // SSID first: the anchored "Flock-" + 6 hex provisioning name and the
    // test_flck CVE string are Flock's own, and they stay true when the MAC is
    // randomized or belongs to a module vendor we have never seen.
    if(flock_ssid_confidence(ssid) != FlockConfidenceNone) return FlockVendorFlock;
    return flock_vendor_from_mac(mac);
}

const char* flock_vendor_str(FlockVendor vendor) {
    switch(vendor) {
    case FlockVendorFlock:
        return "Flock";
    case FlockVendorSoundThinking:
        return "SoundThinking";
    case FlockVendorAxon:
        return "Axon";
    case FlockVendorUbicquia:
        return "Ubicquia";
    case FlockVendorMotorola:
        return "Motorola";
    case FlockVendorVerkada:
        return "Verkada";
    case FlockVendorGenetec:
        return "Genetec";
    case FlockVendorAvigilon:
        return "Avigilon";
    case FlockVendorUtility:
        return "Utility";
    case FlockVendorDigitalAlly:
        return "DigitalAlly";
    case FlockVendorDrone:
        // The manufacturer, when we can name one at all, comes from the OUI
        // vendor lookup and is shown separately -- this column only says the
        // class of thing. For an aircraft identified by Remote ID there may be
        // no manufacturer to name and no need for one: it broadcast its own
        // registration.
        return "Drone";
    case FlockVendorUnknown:
    default:
        // "-", never "Unknown": this lands in a narrow report column and a list
        // row, and a dash reads as "not attributed" without implying we looked
        // up a vendor and failed to recognise a known one.
        return "-";
    }
}

const char* flock_device_long_str(FlockVendor vendor, FlockDevClass cls) {
    // 20 CHARACTERS MAX -- the detail screen's 128 px row. Longer strings are cut
    // at draw time, and device identity must never be the field that gets cut.
    switch(vendor) {
    case FlockVendorFlock:
        // The only branch entitled to print the word "Flock".
        return (cls == FlockClassAcoustic) ? "Flock Raven acoustic" : "Flock / ALPR camera";
    case FlockVendorSoundThinking:
        return "SoundThinking sensor";
    case FlockVendorAxon:
        // WAS "Axon body/in-car kit", meaning "this moves with a person or a
        // vehicle" -- true while Axon made only body and fleet cameras. Axon
        // launched Outpost and Lightpost, both FIXED pole-mounted ALPR, on this
        // same single OUI in 2026, so that label is now wrong in the opposite
        // direction. Nothing in a MAC separates the two; say so.
        return "Axon: body or fixed";
    case FlockVendorUbicquia:
        // Not "camera": the AP6 variant has none. The streetlight node is the fact.
        return "Ubicquia streetlight";
    case FlockVendorMotorola:
        // Not "LPR": these prefixes carry hand-held radios too.
        return "Motorola Solutions";
    case FlockVendorVerkada:
        return "Verkada camera/AC";
    case FlockVendorGenetec:
        return "Genetec (AutoVu)";
    case FlockVendorAvigilon:
        return "Avigilon (Motorola)";
    case FlockVendorUtility:
        return "Utility BodyWorn"; // 16 chars
    case FlockVendorDigitalAlly:
        return "Digital Ally FirstVU"; // 20 chars, at the limit
    case FlockVendorDrone:
        return "Unmanned aircraft"; // 17 chars
    case FlockVendorUnknown:
    default:
        // THE FIX THE VENDOR FIELD EXISTS FOR. This case used to fall into
        // flock_class_long_str()'s "Flock / ALPR camera" and so named Flock
        // Safety on evidence that never mentioned them -- an ESP probe-behaviour
        // score against a MAC in no table at all. Name the class, name nobody.
        switch(cls) {
        case FlockClassAcoustic:
            return "Acoustic sensor";
        case FlockClassBodycam:
            return "Body/in-car kit";
        case FlockClassDrone:
            return "Unmanned aircraft";
        case FlockClassGear:
            return "Surveillance gear";
        case FlockClassAlpr:
        default:
            return "ALPR (unattributed)";
        }
    }
}

FlockDevClass flock_class_from_mac(const uint8_t* mac) {
    const FlockVendorTable* vt = vendor_row_for_mac(mac);
    return vt ? vt->cls : FlockClassAlpr;
}

const char* flock_class_str(FlockDevClass cls) {
    switch(cls) {
    case FlockClassAcoustic:
        return "Acoustic";
    case FlockClassBodycam:
        // NOT "Axon". This class covers Axon, Utility BodyWorn and Digital Ally,
        // so naming one vendor here printed "Utility / Axon" in a report's
        // Vendor+Class pair -- a wrong attribution in the one column whose job is
        // to say WHAT the thing is, while the vendor is already stated beside it.
        return "Body cam";
    case FlockClassGear:
        return "Gear";
    case FlockClassDrone:
        return "Drone";
    case FlockClassAlpr:
    default:
        return "ALPR";
    }
}

const char* flock_class_long_str(FlockDevClass cls) {
    // SUPERSEDED for anything an operator reads: this cannot see the vendor, so
    // its ALPR answer still says "Flock" for hardware that is not Flock's. Call
    // flock_device_long_str(vendor, cls) instead. Kept because it is the honest
    // answer when only a class is in hand, and it is still on the ABI.
    // "SoundThinking (acoustic sensor)" was 31 characters and overran the detail
    // screen's 128 px row. Shortened rather than truncated at draw time, so the
    // device class -- the thing that stops a gunshot sensor being read as a
    // camera -- is never the field that gets cut off.
    switch(cls) {
    case FlockClassAcoustic:
        return "SoundThinking sensor";
    case FlockClassBodycam:
        // WAS "Axon body/in-car kit", chosen so the label could not read like a
        // fixed pole -- correct while Axon made only equipment that moved with a
        // person or a vehicle. Axon put Outpost and Lightpost, both fixed ALPR,
        // on the same OUI in 2026, so the label no longer promises either form
        // factor. Kept in step with flock_device_long_str() deliberately: two
        // labels for one class that disagree is worse than either being vague.
        return "Axon: body or fixed";
    case FlockClassGear:
        // No vendor in scope here, so this cannot name one. Deliberately vaguer
        // than the vendor-aware label -- vagueness beats a wrong attribution.
        return "Surveillance gear";
    case FlockClassAlpr:
    default:
        return "Flock / ALPR camera";
    }
}

/**
 * OPTIONAL user-supplied extras, registered at runtime from the SD card by
 * sig_db.c and merged OVER the built-ins (extras can only ADD matches). These
 * default NULL/0 -- the fail-safe state in which only the built-ins above are
 * consulted -- and are CALLER-OWNED (flock_db.c just holds the pointers, so it
 * stays firmware-free / host-testable). User signatures are LOAD-ONLY and
 * UNVERIFIED; per precision-over-recall they never upgrade an OUI hit past
 * "possible".
 */
// Single caller-owned extras context (NULL = only the built-ins are consulted).
static const FlockDbExtras* g_extras = NULL;

void flock_db_set_extras(const FlockDbExtras* extras) {
    g_extras = extras; // atomic single-pointer swap; caller owns the struct + arrays
}

size_t flock_oui_count(void) {
    return FLOCK_OUI_COUNT;
}

const uint8_t* flock_oui_get(size_t index) {
    if(index >= FLOCK_OUI_COUNT) return NULL;
    return flock_ouis[index];
}

bool flock_oui_match(const uint8_t* mac) {
    if(!mac) return false;
    for(size_t i = 0; i < FLOCK_OUI_COUNT; i++) {
        if(mac[0] == flock_ouis[i][0] && mac[1] == flock_ouis[i][1] &&
           mac[2] == flock_ouis[i][2]) {
            return true;
        }
    }
    // Also scan the optional user-supplied extras (merged over the built-ins).
    if(g_extras) {
        for(size_t i = 0; i < g_extras->oui_count; i++) {
            if(mac[0] == g_extras->ouis[i][0] && mac[1] == g_extras->ouis[i][1] &&
               mac[2] == g_extras->ouis[i][2]) {
                return true;
            }
        }
    }
    return false;
}

/** ASCII lower-case (no locale, safe for embedded). */
static char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/** Case-insensitive substring search (needle assumed already lower-case). */
static bool ci_contains(const char* haystack, const char* needle_lower) {
    if(!haystack || !needle_lower) return false;
    size_t nlen = strlen(needle_lower);
    if(nlen == 0) return false;
    for(const char* h = haystack; *h; h++) {
        size_t k = 0;
        while(needle_lower[k] && ascii_lower(h[k]) == needle_lower[k]) {
            k++;
        }
        if(k == nlen) return true;
    }
    return false;
}

/** True if `ssid` is exactly "Flock-" + 6 hex digits (the provisioning-AP name). */
static bool is_flock_provisioning_ssid(const char* ssid) {
    const char* pfx = "flock-"; // case-insensitive prefix
    for(int i = 0; i < 6; i++) {
        if(ssid[i] == '\0' || ascii_lower(ssid[i]) != pfx[i]) return false;
    }
    for(int i = 6; i < 12; i++) {
        char c = ssid[i]; // '\0' (short SSID) is not hex -> correctly rejected
        bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if(!hex) return false;
    }
    return ssid[12] == '\0';
}

FlockConfidence flock_ssid_confidence(const char* ssid) {
    if(!ssid || ssid[0] == '\0') return FlockConfidenceNone;

    // Strong, near-unique naming -> confirmed. Anchor the provisioning-AP name
    // exactly ("Flock-" + 6 hex): an unanchored "flock-" substring wrongly
    // confirmed benign names like "Flock-Guest" or the Flock Freight / chat SSIDs.
    // Those still fall through to the "likely" contains-check below.
    //
    // "test_flck" is the hard-coded development SSID disclosed as CVE-2025-59409;
    // on the air it is close to self-identifying, hence Confirmed on substring.
    if(is_flock_provisioning_ssid(ssid) || ci_contains(ssid, "test_flck")) {
        return FlockConfidenceConfirmed;
    }

    // Optional user-supplied confirmed needles (already lower-case). Merged
    // over the built-ins: they can only ADD a confirmed match.
    if(g_extras) {
        for(size_t i = 0; i < g_extras->ssid_confirmed_count; i++) {
            if(ci_contains(ssid, g_extras->ssid_confirmed[i])) return FlockConfidenceConfirmed;
        }
    }

    // Weaker substrings -> likely (could be a coincidental network name).
    if(ci_contains(ssid, "flock") || ci_contains(ssid, "flck")) {
        return FlockConfidenceLikely;
    }

    // Optional user-supplied likely needles (already lower-case).
    if(g_extras) {
        for(size_t i = 0; i < g_extras->ssid_likely_count; i++) {
            if(ci_contains(ssid, g_extras->ssid_likely[i])) return FlockConfidenceLikely;
        }
    }

    return FlockConfidenceNone;
}

/**
 * B1: curated table of known-Flock probe IE-skeleton fingerprints (FNV-1a
 * uint32 of the tagged-IE skeleton, computed on the ESP companion).
 *
 * SHIPS EMPTY / INERT. We do NOT yet have confirmed-Flock IE-fp captures, so
 * this table is intentionally empty: nothing matches -> zero behaviour change ->
 * zero false positives, which is exactly right per precision-over-recall. The
 * full pipeline (hash on ESP -> transmit -> parse -> compare) ships and works;
 * it simply has no seeds to match until real captures are validated.
 *
 * TO SEED: add the FNV-1a hash(es) emitted in the companion's `,fp=` field for
 * a probe request from a *corroborated* Flock unit. Each entry is a
 * device-CLASS / firmware-stack signature, NOT a unique device ID -- only add a
 * hash once it is confirmed against a known deployment.
 *
 * Placeholder (compiled out -- NEEDS VALIDATION, do not enable):
 *   // 0x00000000u,  // <model> probe template -- NEEDS VALIDATION, unverified
 */
static const uint32_t flock_ie_fps[] = {
    0, // sentinel so the array is never zero-length; ignored by the matcher.
};

#define FLOCK_IE_FP_COUNT (sizeof(flock_ie_fps) / sizeof(flock_ie_fps[0]))

/**
 * CANDIDATE fingerprints: shipped, but SINGLE-SOURCE, so they corroborate (lift a
 * detection to Class?) and can NEVER auto-Confirm. A hash is promoted out of here
 * and into flock_ie_fps[] only once a SECOND independent capture confirms it.
 *
 * 0x42D75CD1 -- probe IE skeleton of a Flock camera on OUI 70:C9:4E, reported by
 * @h00die 2026-09-02 (false-positive report rows 5 & 6, two channels, one unit he
 * visually confirmed standing next to it). Points in its favour: it appeared on
 * that camera's OUI ONLY and did not smear across unrelated vendors the way the
 * generic 0x7C923B53 skeleton did. Still one operator, one camera, one drive --
 * hence candidate, not verified. Needs a second independent sighting to promote.
 *
 * 0xBA9FAFA0 -- WAS HERE, RETRACTED 2026-10-02 to flock_ie_fps_generic[] below.
 * It was argued onto this list on geography, and the reporter who supplied it
 * later observed the device following his car. Read that entry before adding
 * anything on a similar basis: the shape of the mistake was spreading one
 * CONSUMER HARDWARE CLASS across several kilometres of road and reading the
 * spread as several fixed installations. Geography separates "one device" from
 * "several devices". It cannot separate "several cameras" from "several phones",
 * and nothing in that argument ever did.
 *
 * 0xD0BBEC4C -- the same drive, and the one hash here with a VENDOR ANCHOR.
 * Three devices, all three on OUIs already in flock_ouis[] above, spanning TWO
 * different Flock prefixes (24:B2:B9 and 70:08:94). Zero randomised addresses
 * and zero other-vendor addresses carry it. Two of the three sit 14 m apart,
 * which reads as two poles at one intersection. This is the shape the
 * fingerprint table was designed around: the hash and the OUI agree without
 * anyone having to judge which row was the camera.
 *
 * Both stay CANDIDATES despite the strength, because both come from one
 * operator on one road. A candidate lifts a detection to Class? and can never
 * auto-Confirm, which is the honest ceiling for that. Note 0xD0BBEC4C would
 * Confirm on sight if promoted, since it rides on a Flock OUI -- which is the
 * exact reason it must not be promoted on a single reporter.
 */
static const uint32_t flock_ie_fps_candidate[] = {
    0x42D75CD1u,
    0xD0BBEC4Cu,
};

#define FLOCK_IE_FP_CANDIDATE_COUNT \
    (sizeof(flock_ie_fps_candidate) / sizeof(flock_ie_fps_candidate[0]))

/**
 * KNOWN-GENERIC probe skeletons. Never a signature, at any tier. See
 * flock_ie_fp_is_generic() in the header for the rationale.
 *
 * ENTRY REQUIREMENT: documented in-repo provenance, and evidence the hash
 * appears on hardware that is NOT surveillance gear. "It looked noisy" is not
 * enough -- the whole point is that these are hashes which look COMPELLING in a
 * field report and are still worthless.
 *
 * 0x96FCD1B2 -- a plain ESP32 running a stock wildcard scan. This project's own
 *   bench emitter (tools/flock_emitter) produces it, so its identity is not in
 *   doubt: we built the transmitter. It also surfaced on FOUR different
 *   randomised MACs around wiilover22's first camera in issue #25, which is
 *   exactly the "one device rotating its address" pattern that makes a
 *   fingerprint look like a find. It is an ordinary scan, and shipping it would
 *   match phones and ESP32 IoT devices on every street.
 *
 * 0x173D7A70 -- almost certainly a common phone WiFi stack. Seen on FOUR
 *   locally administered MACs across THREE channels in a single short capture on
 *   this project's own bench (CANDIDATES.md, 2026-09-08) with no camera anywhere
 *   near it. Same seductive shape as the above, same answer.
 *
 * 0x7C923B53 -- the generic skeleton that SMEARED ACROSS UNRELATED VENDORS in
 *   @h00die's 2026-09-02 false-positive report, which is what disqualified it
 *   there while 0x42D75CD1 from the same capture survived as a candidate. It was
 *   already named in the flock_ie_fps_candidate[] comment above as the
 *   counter-example; this makes that judgement enforceable instead of advisory.
 *
 * 0x89C3DEBF -- RETRACTED, and it was OURS. This is the hash this project told
 *   @wiilover22 to put in his signatures.json on 2026-09-08 as "the device I
 *   think is the camera ... matches nothing else". His 2026-09-11 drive settles
 *   it: TEN devices carrying it, spread from 0 to 7.9 km apart, at signals from
 *   -17 to -96 dBm and probe counts from 1 to 149. A fixed camera is not in ten
 *   places at once across eight kilometres. It had also already appeared on
 *   EIGHT randomised MACs on this project's own bench with no camera present
 *   (CANDIDATES.md, 2026-09-09), which is what first put it in doubt.
 *
 *   Denylisting is the ONLY thing that undoes the advice, because
 *   flock_ie_fp_match() consults this list before the user's own file: his card
 *   goes inert on upgrade without him editing anything. Ten of the nineteen
 *   rows in the hits.csv he sent were phones matched on this hash.
 *
 * 0xC59C341F -- 24 distinct locally administered MACs across two sessions, one
 *   probe each, best signal -78 dBm (same capture). Twenty-four addresses seen
 *   once apiece is a street full of phones randomising, not a device. Carried in
 *   CANDIDATES.md as a watch item since 2026-09-09; the MAC count settles it.
 *
 * 0xBA9FAFA0 -- RETRACTED 2026-10-02, and it was the strongest candidate this
 *   project ever shipped. It was promoted on geography: four devices 1.1 to
 *   6.1 km apart cannot be one unit rotating its address, and a mapped ALPR sat
 *   44 m from the closest of them. The comment that argued for it said in so
 *   many words that they "cannot be something riding in his car".
 *
 *   Three days later @wiilover22 reported the opposite from the same road: the
 *   address keeps changing and the device FOLLOWS HIM. Both observations fit one
 *   explanation the geography argument never considered -- a fingerprint shared
 *   by a class of consumer hardware, which is simultaneously in his car and in
 *   other cars spread along several kilometres of road.
 *
 *   The signature settles it independently of either reading. The device carries
 *   221:0050f208001400, a lone WMM vendor element with no Wi-Fi Alliance anchor
 *   and no HT or VHT capability elements. That is a bare client probe, not the
 *   ordered shape the probe-signature matcher looks for. In 45 KB of survey log
 *   off this project's own bench and drives, the production Flock signature
 *   matched zero times while this hash kept appearing, including at -63 dBm with
 *   115 sightings on a bench with no camera within range.
 *
 *   A map record is a coincidence detector, not a capture. One of eighteen
 *   detections landing 44 m from a mapped pole is the kind of result a street
 *   full of phones produces by chance, and that was the entire corroboration.
 */
static const uint32_t flock_ie_fps_generic[] = {
    0x96FCD1B2u,
    0x173D7A70u,
    0x7C923B53u,
    0x89C3DEBFu,
    0xC59C341Fu,
    0xBA9FAFA0u,
};

#define FLOCK_IE_FP_GENERIC_COUNT (sizeof(flock_ie_fps_generic) / sizeof(flock_ie_fps_generic[0]))

bool flock_ie_fp_is_generic(uint32_t fp) {
    if(fp == 0) return false; // 0 is "no fingerprint", not a generic one
    for(size_t i = 0; i < FLOCK_IE_FP_GENERIC_COUNT; i++) {
        if(flock_ie_fps_generic[i] == fp) return true;
    }
    return false;
}

FlockIeFp flock_ie_fp_match(uint32_t fp) {
    if(fp == 0) return FlockIeFpNone; // 0 = "no fingerprint", never a match
    // BEFORE any tier, including the user's own file. An operator who confirmed
    // the wrong row once must not carry a street-matching hash forever, and that
    // mistake is already on cards in the field -- so the guard lives here rather
    // than only at the point of learning.
    if(flock_ie_fp_is_generic(fp)) return FlockIeFpNone;
    // Strongest-first. Built-ins are maintainer-VERIFIED (>=2 corroborations) and
    // are the only tier that can auto-Confirm; it currently ships empty.
    for(size_t i = 0; i < FLOCK_IE_FP_COUNT; i++) {
        if(flock_ie_fps[i] == 0) continue; // skip the sentinel / unseeded slots
        if(flock_ie_fps[i] == fp) return FlockIeFpBuiltin;
    }
    // Candidate built-ins: single-source leads. Caller caps them at Class?.
    for(size_t i = 0; i < FLOCK_IE_FP_CANDIDATE_COUNT; i++) {
        if(flock_ie_fps_candidate[i] == fp) return FlockIeFpCandidate;
    }
    // Then the optional user-supplied extras (UNVERIFIED -> the caller caps these
    // at FlockConfidenceProbeFp; they can only ADD a candidate-class match).
    if(g_extras) {
        for(size_t i = 0; i < g_extras->ie_fp_count; i++) {
            if(g_extras->ie_fps[i] == fp) return FlockIeFpUser;
        }
    }
    return FlockIeFpNone;
}

bool flock_user_mac_match(const uint8_t* mac) {
    if(!mac || !g_extras || !g_extras->macs) return false;
    for(size_t i = 0; i < g_extras->mac_count; i++) {
        if(memcmp(g_extras->macs[i], mac, 6) == 0) return true;
    }
    return false;
}

FlockConfidence flock_mac_pin_confidence(const uint8_t* mac) {
    return flock_user_mac_match(mac) ? FlockConfidenceProbeFp : FlockConfidenceNone;
}

FlockConfidence flock_ie_fp_confidence(uint32_t fp, const uint8_t* mac) {
    switch(flock_ie_fp_match(fp)) {
    case FlockIeFpBuiltin:
        // Verified compiled-in class fingerprint. On a Flock OUI that is two
        // independent tells agreeing, which is the one path a fingerprint may
        // confirm on. The table ships empty, so this is currently unreachable.
        return (mac && flock_oui_match(mac)) ? FlockConfidenceConfirmed : FlockConfidenceProbeFp;
    case FlockIeFpCandidate:
    case FlockIeFpUser:
        // Single-source built-in, signatures.json, or something the operator
        // taught us. Capped at "Class?" even on a Flock OUI: the operator's eyes
        // are evidence about a camera and no evidence about which row in a list
        // emitted which packet.
        return FlockConfidenceProbeFp;
    case FlockIeFpNone:
    default:
        return FlockConfidenceNone;
    }
}

/*
 * flock_score() USED TO LIVE HERE and was deleted in v0.48.
 *
 * It read like the canonical scorer and had a full test suite, but it had ZERO
 * production callers -- nothing on the device ever ran it. The shipped
 * combination logic is parse_flock() in helpers/esp_parser.c (companion backend)
 * and the inline block in esp_parse_generic() (Marauder backend), and those are
 * different code. So the tests that "covered scoring" were guarding a function
 * the product did not use, while the code that WAS used had no such guard.
 *
 * That is not hypothetical: it is exactly how v0.46 shipped "Flock-Guest" as
 * CONFIRMED with every test green. Rather than leave a second, diverging ladder
 * around to be maintained and mistaken for the real one, the assertions moved to
 * esp_parse_companion_line() in test/test_esp_parser.c, where they exercise the
 * boundary the product actually uses.
 *
 * Do not reintroduce a standalone scorer here. If you need combination logic,
 * put it where the caller is, and test it through the wire protocol.
 */

const char* flock_confidence_str(FlockConfidence confidence) {
    switch(confidence) {
    case FlockConfidenceConfirmed:
        return "CONFIRMED";
    case FlockConfidenceProbeFp:
        return "Class?"; // candidate device-CLASS match, not a unique device
    case FlockConfidenceLikely:
        return "Likely";
    case FlockConfidencePossible:
        return "Possible";
    case FlockConfidenceNone:
    default:
        return "-";
    }
}

FlockMethod flock_method_of(const uint8_t* mac, const char* ssid, char ftype, uint32_t ie_fp) {
    // Strongest re-derivable indicator wins, mirroring the ladder's own ordering
    // so the label never claims more than the confidence rung does.
    if(flock_ssid_confidence(ssid) != FlockConfidenceNone) return FlockMethodSsid;
    if(flock_ie_fp_match(ie_fp) != FlockIeFpNone) return FlockMethodIeFp;
    // The operator pinned this exact address after looking at it. More specific
    // than any prefix, and the only thing that can name a camera whose invented
    // address is stable but belongs to no vendor.
    if(flock_user_mac_match(mac)) return FlockMethodPin;
    // ANY vendor table, not just Flock's: a SoundThinking, Axon, Ubicquia,
    // Motorola, Verkada, Genetec or Avigilon prefix is an OUI match too, just for
    // another vendor or device class. Reporting one as "unclassified" would hide
    // the one indicator we actually have for it. vendor_exclusive_oui_match()
    // covers the five competitor tables added in v0.77.
    if(flock_oui_match(mac) || soundthinking_oui_match(mac) || axon_oui_match(mac) ||
       vendor_exclusive_oui_match(mac)) {
        return FlockMethodOui;
    }
    // BLE is classified on the companion (mfg id 0x09C8 / Raven GATT) from advert
    // bytes that never reach this side, so name the source rather than guess.
    if(ftype == 'L') return FlockMethodBle;
    // Community PROBE SIGNATURE, flagged by the companion as sg=1 and carried
    // here as ftype 'S'. Last, because every test above re-derives something
    // from fields we hold and this one cannot: the signature is the IE layout of
    // a frame that never crosses the wire.
    //
    // It still has to be NAMED. This is the only indicator that fires on a
    // randomised address, so it is the one an operator is most likely to be
    // looking at and least able to explain -- on the bench it rendered as the
    // generic "ESP probe rule", which says nothing about why the row exists.
    if(ftype == 'S') return FlockMethodSig;
    return FlockMethodUnknown;
}

const char* flock_method_str(FlockMethod method) {
    // TERSE ON PURPOSE. These are composed into "Method: <this> + <frame>" on a
    // 128 px row that also carries a scrollbar, leaving ~26 characters. The
    // first draft ("OUI prefix", "IE fingerprint") pushed the longest
    // combination off the right edge on real hardware. "OUI" and "SSID" are also
    // the terms the reporter used, so nothing is lost by the shorter form.
    switch(method) {
    case FlockMethodSsid:
        return "SSID";
    case FlockMethodIeFp:
        return "IE fp";
    case FlockMethodPin:
        return "flagged MAC";
    case FlockMethodOui:
        return "OUI";
    case FlockMethodBle:
        return "BLE mfg ID";
    case FlockMethodSig:
        // "probe sig", not "community signature": composed into
        // "Method: <this> + <frame>" on a row with about 26 characters.
        return "probe sig";
    case FlockMethodUnknown:
    default:
        // Not "none": the companion DID score it, on probe behaviour we cannot
        // re-derive here. Saying "no indicator" would be the wrong claim.
        return "ESP probe rule";
    }
}
