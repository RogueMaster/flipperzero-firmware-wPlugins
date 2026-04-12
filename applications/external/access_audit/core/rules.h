#pragma once

#include <stdbool.h>
#include "observation.h"

/* ── High-risk rules ── */

/** Legacy credential family — EM4100-like or MIFARE Classic. */
bool rule_legacy_family(const AccessObservation* obs);

/**
 * Pure identifier pattern — UID present, no user memory, reads are
 * identical.  Card is likely used only as a static replay token.
 */
bool rule_identifier_only_pattern(const AccessObservation* obs);

/* ── Medium-risk rules ── */

/**
 * UID present but no evidence of user memory and reads have not been
 * confirmed identical.  Weaker form of identifier_only_pattern.
 */
bool rule_uid_no_memory(const AccessObservation* obs);

/* ── Low-risk / confidence rules ── */

/** Classification metadata is incomplete — treat result with caution. */
bool rule_incomplete_evidence(const AccessObservation* obs);

/** UID could not be extracted — observation cannot be fully assessed. */
bool rule_no_uid(const AccessObservation* obs);

/* ── Positive / mitigating rules ── */

/**
 * Card family uses modern cryptography (DESFire, MIFARE Plus, FeliCa).
 * Presence of this rule reduces the effective risk score.
 */
bool rule_modern_crypto(const AccessObservation* obs);

/**
 * MIFARE Classic card — Crypto1 cipher is broken but cracking keys still
 * requires active attack effort (dictionary/hardnested), unlike 125 kHz RFID
 * which is a passive replay with no cryptographic barrier at all.
 * Small score reduction relative to EM4100-class cards.
 */
bool rule_crypto1_breakable(const AccessObservation* obs);

/* ── Active findings ── */

/**
 * Sector 0 on a MIFARE Classic card was authenticated with a well-known
 * default key (FFFFFFFFFFFF or A0A1A2A3A4A5, key A or B).
 * Indicates the card has never had its keys changed.
 */
bool rule_default_keys(const AccessObservation* obs);
