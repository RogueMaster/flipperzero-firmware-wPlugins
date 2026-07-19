#include "ctf_pack.h"
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------- hashing */
uint32_t ctf_hash(const char* s) {
    uint32_t h = 2166136261u; /* FNV-1a offset basis */
    for(; *s; s++) {
        h ^= (uint8_t)*s;
        h *= 16777619u; /* FNV prime */
    }
    return h;
}

uint32_t ctf_answer_hash(const char* s, bool exact) {
    char buf[96];
    size_t n = 0;
    while(*s == ' ' || *s == '\t') s++; /* trim leading */
    while(*s && n < sizeof(buf) - 1) {
        char c = *s++;
        if(!exact && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        buf[n++] = c;
    }
    while(n > 0 && (buf[n - 1] == ' ' || buf[n - 1] == '\t')) n--; /* trim trailing */
    buf[n] = '\0';
    return ctf_hash(buf);
}

bool ctf_check_answer(const CtfChallenge* c, const char* attempt) {
    return ctf_answer_hash(attempt, c->exact) == c->answer_hash;
}

/* --------------------------------------------------------------- helpers */
static void str_set(char* dst, size_t sz, const char* src) {
    size_t i = 0;
    for(; src[i] && i < sz - 1; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/* append `src` as a new line to dst (with a '\n' separator if non-empty) */
static void str_append_line(char* dst, size_t sz, const char* src) {
    size_t len = strlen(dst);
    if(len && len < sz - 1) dst[len++] = '\n';
    size_t i = 0;
    while(src[i] && len < sz - 1) dst[len++] = src[i++];
    dst[len] = '\0';
}

/* case-insensitive compare of a keyword (normalises BOTH sides) */
static bool key_is(const char* key, const char* want) {
    while(*key && *want) {
        char a = *key, b = *want;
        if(a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
        if(b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
        if(a != b) return false;
        key++;
        want++;
    }
    return *key == '\0' && *want == '\0';
}

/* ----------------------------------------------------------------- labels */
const char* ctf_category_label(CtfCategory c) {
    switch(c) {
    case CtfCatCipher: return "Cipher";
    case CtfCatRfid: return "RFID";
    case CtfCatInfrared: return "Infrared";
    case CtfCatRadio: return "Sub-GHz";
    default: return "Misc";
    }
}

const char* ctf_category_tag(CtfCategory c) {
    switch(c) {
    case CtfCatCipher: return "CIPHER";
    case CtfCatRfid: return "RFID";
    case CtfCatInfrared: return "IR";
    case CtfCatRadio: return "RF";
    default: return "MISC";
    }
}

CtfCategory ctf_category_from_str(const char* s) {
    if(key_is(s, "cipher") || key_is(s, "crypto")) return CtfCatCipher;
    if(key_is(s, "rfid") || key_is(s, "nfc")) return CtfCatRfid;
    if(key_is(s, "ir") || key_is(s, "infrared")) return CtfCatInfrared;
    if(key_is(s, "radio") || key_is(s, "subghz") || key_is(s, "rf")) return CtfCatRadio;
    return CtfCatMisc;
}

const char* ctf_difficulty_label(CtfDifficulty d) {
    switch(d) {
    case CtfDiffMedium: return "Medium";
    case CtfDiffHard: return "Hard";
    case CtfDiffInsane: return "Insane";
    default: return "Easy";
    }
}

CtfDifficulty ctf_difficulty_from_str(const char* s) {
    if(key_is(s, "medium") || key_is(s, "med")) return CtfDiffMedium;
    if(key_is(s, "hard")) return CtfDiffHard;
    if(key_is(s, "insane") || key_is(s, "expert")) return CtfDiffInsane;
    return CtfDiffEasy;
}

/* ----------------------------------------------------------------- parser */

/* Per-challenge parse scratch: the flag can arrive as plaintext (FLAG) or as a
 * precomputed hash (ANSWER), and EXACT may appear after FLAG, so we resolve the
 * hash only when the challenge is finalised. */
typedef struct {
    char flag_plain[80];
    bool has_flag_plain;
    uint32_t answer_hash;
    bool has_answer;
} FlagScratch;

static void finalize_challenge(CtfChallenge* c, FlagScratch* fs) {
    c->has_data = c->data[0] != '\0';
    c->has_hint = c->hint[0] != '\0';
    if(fs->has_answer)
        c->answer_hash = fs->answer_hash;
    else if(fs->has_flag_plain)
        c->answer_hash = ctf_answer_hash(fs->flag_plain, c->exact);
    else
        c->answer_hash = 0; /* no flag defined */
}

/* Copy one logical line out of `p` (up to newline), trimming a trailing '\r'
 * and trailing spaces. Advances *pp past the newline. Returns false at EOF. */
static bool next_line(const char** pp, char* line, size_t sz) {
    const char* p = *pp;
    if(*p == '\0') return false;
    size_t n = 0;
    while(*p && *p != '\n') {
        if(n < sz - 1) line[n++] = *p;
        p++;
    }
    if(*p == '\n') p++;
    while(n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) n--;
    line[n] = '\0';
    *pp = p;
    return true;
}

/* Split "KEY value" -> key ptr (in-place) and value ptr (leading ws trimmed). */
static void split_kv(char* line, char** key, char** val) {
    *key = line;
    char* sp = line;
    while(*sp && *sp != ' ' && *sp != '\t') sp++;
    if(*sp) {
        *sp = '\0';
        char* v = sp + 1;
        while(*v == ' ' || *v == '\t') v++;
        *val = v;
    } else {
        *val = sp; /* points at the NUL: empty value */
    }
}

bool ctf_pack_parse(const char* text, CtfPack* out) {
    memset(out, 0, sizeof(*out));
    str_set(out->title, sizeof(out->title), "Untitled Pack");

    CtfChallenge* cur = NULL;
    FlagScratch fs;
    memset(&fs, 0, sizeof(fs));

    const char* p = text;
    char line[256];

    while(next_line(&p, line, sizeof(line))) {
        if(line[0] == '\0' || line[0] == '#') continue;
        char *key, *val;
        split_kv(line, &key, &val);

        if(key_is(key, "PACK")) {
            str_set(out->title, sizeof(out->title), val);
        } else if(key_is(key, "AUTHOR")) {
            str_set(out->author, sizeof(out->author), val);
        } else if(key_is(key, "DESC") || key_is(key, "INTRO")) {
            str_set(out->desc, sizeof(out->desc), val);
        } else if(key_is(key, "CHALLENGE")) {
            if(cur) finalize_challenge(cur, &fs);
            if(out->count >= CTF_MAX_CHALLENGES) {
                cur = NULL; /* ignore overflow challenges */
                continue;
            }
            cur = &out->challenges[out->count++];
            memset(cur, 0, sizeof(*cur));
            memset(&fs, 0, sizeof(fs));
            cur->points = 100;
            cur->category = CtfCatMisc;
            cur->difficulty = CtfDiffEasy;
            str_set(cur->title, sizeof(cur->title), val);
        } else if(cur) {
            if(key_is(key, "CATEGORY")) {
                cur->category = ctf_category_from_str(val);
            } else if(key_is(key, "POINTS")) {
                int v = atoi(val);
                if(v < 0) v = 0;
                if(v > 9999) v = 9999;
                cur->points = (uint16_t)v;
            } else if(key_is(key, "DIFFICULTY") || key_is(key, "DIFF")) {
                cur->difficulty = ctf_difficulty_from_str(val);
            } else if(key_is(key, "PROMPT")) {
                str_append_line(cur->prompt, sizeof(cur->prompt), val);
            } else if(key_is(key, "DATA")) {
                str_append_line(cur->data, sizeof(cur->data), val);
            } else if(key_is(key, "HINT")) {
                str_append_line(cur->hint, sizeof(cur->hint), val);
            } else if(key_is(key, "FLAG")) {
                str_set(fs.flag_plain, sizeof(fs.flag_plain), val);
                fs.has_flag_plain = true;
            } else if(key_is(key, "ANSWER")) {
                fs.answer_hash = (uint32_t)strtoul(val, NULL, 16);
                fs.has_answer = true;
            } else if(key_is(key, "EXACT")) {
                cur->exact = true;
            }
        }
    }
    if(cur) finalize_challenge(cur, &fs);

    /* stable per-challenge id = hash(pack_title "/" challenge_title) */
    for(uint8_t i = 0; i < out->count; i++) {
        char idbuf[CTF_PACK_TITLE_LEN + CTF_TITLE_LEN + 2];
        str_set(idbuf, sizeof(idbuf), out->title);
        size_t l = strlen(idbuf);
        if(l < sizeof(idbuf) - 1) idbuf[l++] = '/';
        idbuf[l] = '\0';
        size_t j = 0;
        while(out->challenges[i].title[j] && l < sizeof(idbuf) - 1)
            idbuf[l++] = out->challenges[i].title[j++];
        idbuf[l] = '\0';
        out->challenges[i].id = ctf_hash(idbuf);
    }

    return out->count > 0;
}

bool ctf_pack_scan(const char* text, CtfPackInfo* out) {
    out->title[0] = '\0';
    out->count = 0;
    const char* p = text;
    char line[256];
    while(next_line(&p, line, sizeof(line))) {
        if(line[0] == '\0' || line[0] == '#') continue;
        char *key, *val;
        split_kv(line, &key, &val);
        if(key_is(key, "PACK")) {
            str_set(out->title, sizeof(out->title), val);
        } else if(key_is(key, "CHALLENGE")) {
            if(out->count < 255) out->count++;
        }
    }
    if(out->title[0] == '\0') str_set(out->title, sizeof(out->title), "Untitled Pack");
    return out->count > 0;
}
