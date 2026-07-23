/**
 * Gauntlet - challenge pack model + parser.
 *
 * A "pack" is a plain-text .pack file on the SD card describing a set of CTF
 * challenges. This module holds the in-memory model and a pure line-based
 * parser (no storage, no UI). Flags are never stored in the clear: an author's
 * plaintext FLAG is folded into a 32-bit FNV-1a hash at load time, and a solved
 * attempt is checked by hashing the same way. See docs in the README.
 */
#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTF_MAX_CHALLENGES 24
#define CTF_TITLE_LEN      48
#define CTF_PROMPT_LEN     220
#define CTF_DATA_LEN       220
#define CTF_HINT_LEN       140
#define CTF_PACK_TITLE_LEN 40
#define CTF_AUTHOR_LEN     28
#define CTF_DESC_LEN       120
#define CTF_FILENAME_LEN   64

typedef enum {
    CtfCatMisc = 0,
    CtfCatCipher,
    CtfCatRfid,
    CtfCatInfrared,
    CtfCatRadio,
    CtfCatCount,
} CtfCategory;

typedef enum {
    CtfDiffEasy = 0,
    CtfDiffMedium,
    CtfDiffHard,
    CtfDiffInsane,
    CtfDiffCount,
} CtfDifficulty;

typedef struct {
    char title[CTF_TITLE_LEN];
    char prompt[CTF_PROMPT_LEN];
    char data[CTF_DATA_LEN]; /* the ciphertext / clue payload; may be empty */
    char hint[CTF_HINT_LEN];
    CtfCategory category;
    CtfDifficulty difficulty;
    uint16_t points;
    uint32_t answer_hash; /* FNV-1a of the normalised answer   */
    uint32_t id; /* stable key = hash(pack "/" title)  */
    bool exact; /* case-sensitive flag compare        */
    bool has_hint;
    bool has_data;
} CtfChallenge;

typedef struct {
    char title[CTF_PACK_TITLE_LEN];
    char author[CTF_AUTHOR_LEN];
    char desc[CTF_DESC_LEN];
    CtfChallenge challenges[CTF_MAX_CHALLENGES];
    uint8_t count;
} CtfPack;

/** Lightweight index entry, built without keeping the whole pack in memory. */
typedef struct {
    char filename[CTF_FILENAME_LEN]; /* base name inside the packs dir  */
    char title[CTF_PACK_TITLE_LEN];
    uint8_t count; /* number of challenges            */
} CtfPackInfo;

/* --- hashing ------------------------------------------------------------- */
/** FNV-1a 32-bit over a NUL-terminated string. */
uint32_t ctf_hash(const char* s);
/** Hash of the normalised answer: trim outer whitespace, lower-case unless
 *  `exact`, then FNV-1a. This is the exact rule the pack generator mirrors. */
uint32_t ctf_answer_hash(const char* s, bool exact);
/** True if `attempt` matches the challenge's stored flag. */
bool ctf_check_answer(const CtfChallenge* c, const char* attempt);

/* --- parsing ------------------------------------------------------------- */
/** Full parse of a NUL-terminated pack buffer into *out. Returns true if at
 *  least one challenge was found. */
bool ctf_pack_parse(const char* text, CtfPack* out);
/** Light scan: fills title + challenge count only (for the pack list). */
bool ctf_pack_scan(const char* text, CtfPackInfo* out);

/* --- labels -------------------------------------------------------------- */
const char* ctf_category_label(CtfCategory c); /* "Cipher", "RFID", ...     */
const char* ctf_category_tag(CtfCategory c); /* short "CIPHER", "RFID"    */
CtfCategory ctf_category_from_str(const char* s);
const char* ctf_difficulty_label(CtfDifficulty d);
CtfDifficulty ctf_difficulty_from_str(const char* s);

#ifdef __cplusplus
}
#endif
