#include "report.h"
#include "rules.h"
#include "scoring.h"
#include <furi.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <stdlib.h>
#include <string.h>

#define TAG "AccessAudit"

#define REPORT_DIR "/ext/apps_data/access_audit"
#define REPORT_APP_VERSION "1.0"

/* Write a plain C string to the file. */
static void fw(File* f, const char* s) {
    storage_file_write(f, s, strlen(s));
}

/** Known user-memory capacity for card types we can determine statically. */
static const char* memory_capacity(CardType type) {
    switch(type) {
    case CardTypeMifareClassic1K:    return "1 KB (16 sectors)";
    case CardTypeMifareClassic4K:    return "4 KB (40 sectors)";
    case CardTypeMifareClassicMini:  return "320 B (5 sectors)";
    case CardTypeMifareUltralight:   return "64 B";
    case CardTypeMifareUltralightC:  return "192 B";
    case CardTypeNtag203:            return "144 B";
    case CardTypeNtag213:            return "144 B";
    case CardTypeNtag215:            return "540 B";
    case CardTypeNtag216:            return "888 B";
    default:                         return NULL;
    }
}

/**
 * Manufacturer hint from the first byte of a 7-byte ISO14443-A UID.
 * Per ISO/IEC 7816-6 manufacturer codes.
 */
static const char* nfc_manufacturer(const AccessObservation* obs) {
    if(obs->tech != TechTypeNfc13Mhz) return NULL;
    if(!obs->uid_present || obs->uid_len < 7) return NULL;
    switch(obs->uid[0]) {
    case 0x02: return "STMicroelectronics";
    case 0x04: return "NXP Semiconductors";
    case 0x05: return "Infineon Technologies";
    case 0x07: return "Texas Instruments";
    case 0x16: return "Atmel";
    default:   return NULL;
    }
}

/** Count distinct UIDs across the session (cards scanned more than once appear once). */
static size_t count_unique_uids(const ScanSession* session) {
    size_t unique = 0;
    for(size_t i = 0; i < session->count; i++) {
        const AccessObservation* a = &session->entries[i].obs;
        if(!a->uid_present || a->uid_len == 0) { unique++; continue; }
        bool dup = false;
        for(size_t j = 0; j < i; j++) {
            const AccessObservation* b = &session->entries[j].obs;
            if(b->uid_present && b->uid_len == a->uid_len &&
               memcmp(a->uid, b->uid, a->uid_len) == 0) {
                dup = true;
                break;
            }
        }
        if(!dup) unique++;
    }
    return unique;
}

/** True when the session contains both NFC and RFID cards. */
static bool session_has_mixed_tech(const ScanSession* session) {
    bool has_nfc = false, has_rfid = false;
    for(size_t i = 0; i < session->count; i++) {
        if(session->entries[i].obs.tech == TechTypeNfc13Mhz) has_nfc = true;
        if(session->entries[i].obs.tech == TechTypeRfid125)  has_rfid = true;
    }
    return has_nfc && has_rfid;
}

static const char* report_advice(CardType type) {
    switch(type) {
    case CardTypeEm4100Like:
    case CardTypeHidProxLike:
    case CardTypeHidGeneric:
    case CardTypeIndala:
    case CardTypeRfid125:
        return "No crypto. Replace with an ISO14443 card using AES auth.";
    case CardTypeMifareClassic:
    case CardTypeMifareClassic1K:
    case CardTypeMifareClassic4K:
    case CardTypeMifareClassicMini:
        return "Crypto1 is broken. Replace with DESFire EV2+ or Plus SL3.";
    case CardTypeMifarePlusSL1:
        return "SL1 = Classic compat. Upgrade to SL3 or replace with DESFire.";
    case CardTypeMifarePlusSL2:
        return "SL2 has AES but Classic frames. Consider upgrading to SL3.";
    case CardTypeMifarePlus:
    case CardTypeMifarePlusSL3:
        return "Verify key diversification and mutual auth are configured.";
    case CardTypeMifareUltralight:
    case CardTypeMifareUltralightC:
    case CardTypeNtag203:
    case CardTypeNtag213:
    case CardTypeNtag215:
    case CardTypeNtag216:
    case CardTypeNtagI2C:
        return "No mutual auth. Avoid for access control; use DESFire EV2+.";
    case CardTypeMifareDesfireEV1:
        return "EV1 uses 3DES. Upgrade to EV2/EV3 for AES crypto.";
    case CardTypeMifareDesfire:
    case CardTypeMifareDesfireEV2:
    case CardTypeMifareDesfireEV3:
    case CardTypeMifareDesfireLight:
        return "Verify key diversification and mutual auth are configured.";
    case CardTypeHidIclass:
    case CardTypeHidIclassLegacy:
    case CardTypeHidIclassLegacy2k:
    case CardTypeHidIclassLegacy16k:
    case CardTypeHidIclassLegacy32k:
        return "iCLASS DES/3DES master key is publicly known. Upgrade to iCLASS SE/Seos.";
    case CardTypeFelica:
        return "Verify FeliCa application crypto is properly configured.";
    case CardTypeFeliCaLite:
        return "FeliCa Lite has no mutual auth. Avoid for access control; use standard FeliCa or DESFire EV2+.";
    default:
        return NULL;
    }
}

static const char* report_risk_label(Severity severity) {
    switch(severity) {
    case SeverityHigh:
        return "HIGH RISK";
    case SeverityMedium:
        return "MODERATE";
    case SeverityLow:
        return "LOW RISK";
    default:
        return "SECURE";
    }
}

static void write_card_entry(
    File* f,
    size_t index,
    size_t total,
    const SessionEntry* entry) {
    char buf[64];

    snprintf(buf, sizeof(buf), "Card %u/%u\n", (unsigned)(index + 1), (unsigned)total);
    fw(f, buf);

    fw(f,
       entry->obs.tech == TechTypeRfid125 ? "  Radio:  RFID 125kHz\n" :
                                            "  Radio:  NFC 13.56MHz\n");

    snprintf(buf, sizeof(buf), "  Type:   %s\n", card_type_to_string(entry->obs.card_type));
    fw(f, buf);

    /* UID with byte count */
    if(entry->obs.uid_present && entry->obs.uid_len > 0) {
        snprintf(buf, sizeof(buf), "  UID:    (%u-byte) ", (unsigned)entry->obs.uid_len);
        fw(f, buf);
        for(size_t i = 0; i < entry->obs.uid_len; i++) {
            if(i > 0) fw(f, " ");
            snprintf(buf, sizeof(buf), "%02X", entry->obs.uid[i]);
            fw(f, buf);
        }
        fw(f, "\n");
    } else {
        fw(f, "  UID:    unavailable\n");
    }

    /* SAK / ATQA (ISO14443-3A only — useful for unclassified generic types) */
    if(entry->obs.sak_atqa_present) {
        snprintf(
            buf,
            sizeof(buf),
            "  ATQA:   %02X %02X  SAK: %02X\n",
            entry->obs.atqa[0],
            entry->obs.atqa[1],
            entry->obs.sak);
        fw(f, buf);
    }

    /* Manufacturer (NFC 7-byte UIDs only) */
    const char* mfr = nfc_manufacturer(&entry->obs);
    if(mfr) {
        snprintf(buf, sizeof(buf), "  Mfr:    %s\n", mfr);
        fw(f, buf);
    }

    /* Memory capacity (known types only) */
    const char* mem = memory_capacity(entry->obs.card_type);
    if(mem) {
        snprintf(buf, sizeof(buf), "  Memory: %s\n", mem);
        fw(f, buf);
    }

    snprintf(
        buf, sizeof(buf), "  Risk:   %s\n", report_risk_label(entry->score.max_severity));
    fw(f, buf);

    snprintf(buf, sizeof(buf), "  Score:  %u/100  Confidence: %u%%\n",
        entry->score.score, entry->score.confidence);
    fw(f, buf);

    /* Rules triggered */
    struct {
        const char* name;
        bool hit;
    } checks[] = {
        {"legacy_family", rule_legacy_family(&entry->obs)},
        {"identifier_only", rule_identifier_only_pattern(&entry->obs)},
        {"uid_no_memory", rule_uid_no_memory(&entry->obs)},
        {"modern_crypto", rule_modern_crypto(&entry->obs)},
        {"incomplete_evidence", rule_incomplete_evidence(&entry->obs)},
        {"no_uid", rule_no_uid(&entry->obs)},
        {"default_keys", rule_default_keys(&entry->obs)},
    };

    fw(f, "  Rules:  ");
    bool any = false;
    for(size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        if(checks[i].hit) {
            if(any) fw(f, ", ");
            fw(f, checks[i].name);
            any = true;
        }
    }
    if(!any) fw(f, "none");
    fw(f, "\n");

    /* Advice */
    const char* advice = report_advice(entry->obs.card_type);
    if(advice) {
        fw(f, "  Advice: ");
        fw(f, advice);
        fw(f, "\n");
    }

    /* Default key finding - explicit note when confirmed via active scan */
    if(rule_default_keys(&entry->obs)) {
        fw(f, "  Note:   Sector 0 readable with a default key (active scan). "
              "Keys have never been changed.\n");
    }

    fw(f, "\n");
}

bool report_save_session(const ScanSession* session) {
    if(!session || session->count == 0) return false;

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    /* Ensure the full directory path exists — creates all intermediate dirs. */
    bool mkdir_ok = storage_simply_mkdir(storage, REPORT_DIR);
    if(!mkdir_ok) {
        FURI_LOG_E(TAG, "storage_simply_mkdir failed for: %s", REPORT_DIR);
    }

    char path[72];
    snprintf(
        path,
        sizeof(path),
        REPORT_DIR "/report_%04u%02u%02u_%02u%02u%02u.txt",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    File* file = storage_file_alloc(storage);
    bool open_ok = storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!open_ok) {
        FURI_LOG_E(TAG, "open failed: %s err=%u", path,
            (unsigned)storage_file_get_error(file));
    }

    if(open_ok) {
        char buf[64];
        SessionSummary sum = session_summarise(session);

        fw(file, "Access Audit Report\n");
        fw(file, "Generated by Access Audit v" REPORT_APP_VERSION "\n");
        if(session->name[0] != '\0') {
            snprintf(buf, sizeof(buf), "Session: %s\n", session->name);
            fw(file, buf);
        }
        snprintf(
            buf,
            sizeof(buf),
            "%04u-%02u-%02u %02u:%02u:%02u\n",
            dt.year,
            dt.month,
            dt.day,
            dt.hour,
            dt.minute,
            dt.second);
        fw(file, buf);
        snprintf(buf, sizeof(buf), "Cards scanned: %u\n", (unsigned)session->count);
        fw(file, buf);

        size_t unique = count_unique_uids(session);
        if(unique != session->count) {
            snprintf(buf, sizeof(buf), "Unique cards:  %u\n", (unsigned)unique);
            fw(file, buf);
        }

        if(session_has_mixed_tech(session)) {
            fw(file, "Tech: NFC + RFID (mixed)\n");
        }

        /* Summary block */
        snprintf(
            buf,
            sizeof(buf),
            "High: %u  Medium: %u  Low: %u  Secure: %u\n",
            (unsigned)sum.high,
            (unsigned)sum.medium,
            (unsigned)sum.low,
            (unsigned)sum.secure);
        fw(file, buf);
        if(sum.most_common_type != CardTypeUnknown) {
            snprintf(
                buf,
                sizeof(buf),
                "Most common: %s\n",
                card_type_to_string(sum.most_common_type));
            fw(file, buf);
        }

        fw(file, "----------------------------------------\n\n");

        /* Write only unique entries. Deduplicate by UID so that cards scanned
         * more than once (on old builds without session-level dedup) appear
         * only once in the report body. UID-less entries are always written. */
        size_t written = 0;
        for(size_t i = 0; i < session->count; i++) {
            const AccessObservation* obs = &session->entries[i].obs;
            if(obs->uid_present && obs->uid_len > 0) {
                bool dup = false;
                for(size_t j = 0; j < i; j++) {
                    const AccessObservation* prev = &session->entries[j].obs;
                    if(prev->uid_present && prev->uid_len == obs->uid_len &&
                       memcmp(prev->uid, obs->uid, obs->uid_len) == 0) {
                        dup = true;
                        break;
                    }
                }
                if(dup) continue;
            }
            write_card_entry(file, written, unique, &session->entries[i]);
            written++;
        }

        /* Session-level advisory */
        if(sum.high > 0 || sum.medium > 0) {
            fw(file, "========================================\n");
            if(sum.high > 0) {
                snprintf(buf, sizeof(buf),
                    "ACTION REQUIRED: %u high-risk card(s) detected.\n",
                    (unsigned)sum.high);
                fw(file, buf);
                fw(file, "Replace or upgrade legacy credentials immediately.\n");
            } else {
                fw(file, "REVIEW RECOMMENDED: moderate-risk card(s) detected.\n");
            }
            fw(file, "========================================\n");
        }
    }

    /* storage_file_close MUST be called even when open failed (SDK requirement).
     * Its return value indicates whether data was fully flushed to the SD card —
     * this is the authoritative success signal, not storage_file_open. */
    bool close_ok = storage_file_close(file);
    bool ok = open_ok && close_ok;

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* ── Report listing / loading ──────────────────────────────────────────── */

size_t report_list(char names[REPORT_LIST_MAX][REPORT_NAME_LEN]) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    size_t count = 0;

    if(storage_dir_open(dir, REPORT_DIR)) {
        char fname[64];
        while(count < REPORT_LIST_MAX &&
              storage_dir_read(dir, NULL, fname, sizeof(fname))) {
            /* Expect "report_YYYYMMDD_HHMMSS.txt" — 26 chars */
            size_t len = strlen(fname);
            if(len >= 26 && strncmp(fname, "report_", 7) == 0) {
                strncpy(names[count], fname + 7, 15);
                names[count][15] = '\0';
                count++;
            }
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    /* Sort newest first (YYYYMMDD_HHMMSS sorts lexicographically = chronologically). */
    for(size_t i = 0; i < count; i++) {
        for(size_t j = i + 1; j < count; j++) {
            if(strcmp(names[i], names[j]) < 0) {
                char tmp[REPORT_NAME_LEN];
                memcpy(tmp, names[i], REPORT_NAME_LEN);
                memcpy(names[i], names[j], REPORT_NAME_LEN);
                memcpy(names[j], tmp, REPORT_NAME_LEN);
            }
        }
    }

    return count;
}

bool report_load(const char* name, ReportContent* out) {
    if(!name || !out) return false;

    char path[72];
    snprintf(path, sizeof(path), REPORT_DIR "/report_%s.txt", name);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t file_size = storage_file_size(file);
        /* Cap at 8 KB — no legitimate report will exceed this. */
        if(file_size > 0 && file_size <= 8192) {
            char* buf = malloc(file_size + 1);
            if(buf) {
                size_t read = storage_file_read(file, buf, (size_t)file_size);
                buf[read] = '\0';

                /* Count lines to size the pointer array. */
                size_t line_count = 0;
                for(size_t i = 0; i < read; i++) {
                    if(buf[i] == '\n') line_count++;
                }
                /* Account for a final line without trailing newline. */
                if(read > 0 && buf[read - 1] != '\n') line_count++;

                char** lines = malloc(line_count * sizeof(char*));
                if(lines) {
                    size_t idx = 0;
                    lines[idx++] = buf;
                    for(size_t i = 0; i < read; i++) {
                        if(buf[i] == '\n') {
                            buf[i] = '\0';
                            if(idx < line_count && i + 1 < read) {
                                lines[idx++] = buf + i + 1;
                            }
                        }
                    }
                    out->buf = buf;
                    out->lines = lines;
                    out->count = idx;
                    ok = true;
                } else {
                    free(buf);
                }
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void report_content_free(ReportContent* content) {
    if(!content) return;
    free(content->lines);
    free(content->buf);
    content->lines = NULL;
    content->buf = NULL;
    content->count = 0;
}

ReportSummary report_read_summary(const char* name) {
    ReportSummary sum = {0};
    if(!name) return sum;

    char path[72];
    snprintf(path, sizeof(path), REPORT_DIR "/report_%s.txt", name);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* Read up to 512 bytes — the summary line always appears in the header */
        char buf[512];
        size_t read = storage_file_read(file, buf, sizeof(buf) - 1);
        buf[read] = '\0';
        storage_file_close(file);

        /* Scan line by line for "High: N  Medium: N  Low: N  Secure: N" */
        char* line = buf;
        for(int l = 0; l < 20 && line && *line; l++) {
            char* newline = strchr(line, '\n');
            if(newline) *newline = '\0';

            unsigned h = 0, m = 0, lo = 0, s = 0;
            if(sscanf(line, "High: %u  Medium: %u  Low: %u  Secure: %u",
                      &h, &m, &lo, &s) == 4) {
                sum.high   = (uint8_t)(h  > 255 ? 255 : h);
                sum.medium = (uint8_t)(m  > 255 ? 255 : m);
                sum.low    = (uint8_t)(lo > 255 ? 255 : lo);
                sum.secure = (uint8_t)(s  > 255 ? 255 : s);
                sum.valid  = true;
                break;
            }

            line = newline ? newline + 1 : NULL;
        }
    } else {
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return sum;
}

bool report_delete(const char* name) {
    if(!name) return false;

    char path[72];
    snprintf(path, sizeof(path), REPORT_DIR "/report_%s.txt", name);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error err = storage_common_remove(storage, path);
    furi_record_close(RECORD_STORAGE);

    return err == FSE_OK;
}
