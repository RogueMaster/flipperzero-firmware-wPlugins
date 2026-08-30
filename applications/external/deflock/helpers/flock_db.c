// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "flock_db.h"
#include <string.h>

/**
 * 29 OUI prefixes observed in fielded Flock Safety deployments.
 * Mostly @NitekryDPaul research; 82:6b:f2 from DeFlockJoplin field testing;
 * the last entry b4:1e:52 is Flock Safety's own IEEE-registered OUI (GainSec).
 * These are generic vendor prefixes (Liteon, Espressif, etc.), hence OUI-only
 * matches are scored "possible", never "confirmed".
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
    {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x3c, 0x71, 0xbf}, {0x58, 0x00, 0xe3},
    {0x90, 0x35, 0xea}, {0x5c, 0x93, 0xa2}, {0x64, 0x6e, 0x69}, {0x82, 0x6b, 0xf2},
    {0xb4, 0x1e, 0x52},
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
 * VENDOR-EXCLUSIVE OUIs (12 across 5 vendors) -- competitor surveillance kit.
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
    // only about the vendor-exclusive competitor prefixes added in v0.77.
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
        return "Axon";
    case FlockClassGear:
        return "Gear";
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

FlockIeFp flock_ie_fp_match(uint32_t fp) {
    if(fp == 0) return FlockIeFpNone; // 0 = "no fingerprint", never a match
    // Built-ins first: a compiled-in (maintainer-verified) hit is the strongest.
    for(size_t i = 0; i < FLOCK_IE_FP_COUNT; i++) {
        if(flock_ie_fps[i] == 0) continue; // skip the sentinel / unseeded slots
        if(flock_ie_fps[i] == fp) return FlockIeFpBuiltin;
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
    case FlockMethodOui:
        return "OUI";
    case FlockMethodBle:
        return "BLE mfg ID";
    case FlockMethodUnknown:
    default:
        // Not "none": the companion DID score it, on probe behaviour we cannot
        // re-derive here. Saying "no indicator" would be the wrong claim.
        return "ESP probe rule";
    }
}
