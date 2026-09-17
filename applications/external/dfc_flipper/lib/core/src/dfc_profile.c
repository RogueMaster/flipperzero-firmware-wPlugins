#include "dfc_profile.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void say(char* reason, size_t cap, const char* fmt, ...) {
    if(!reason || cap == 0) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(reason, cap, fmt, ap);
    va_end(ap);
}

void dfc_profile_default(DfcProfile* profile) {
    if(!profile) return;
    memset(profile, 0, sizeof(*profile));
    profile->max_apps = DFC_MAX_APPS;
    profile->max_files = DFC_MAX_FILES;
    profile->max_keys_per_app = DFC_MAX_KEYS;
    profile->max_storage = DFC_ENABLE_STORAGE_8K ? DfcStorage8KByteCount :
                           DFC_ENABLE_STORAGE_4K ? DfcStorage4KByteCount :
                                                  DfcStorage2KByteCount;
    profile->supports_picc_files = false;
    profile->supported_generations =
        (DFC_ENABLE_GENERATION_EV1 ? DFC_PROFILE_GEN_EV1 : 0) |
        (DFC_ENABLE_GENERATION_EV2 ? DFC_PROFILE_GEN_EV2 : 0) |
        (DFC_ENABLE_GENERATION_EV3 ? DFC_PROFILE_GEN_EV3 : 0);
}

static const char* generation_name(DfcGeneration g) {
    switch(g) {
    case DfcGenerationEv1:
        return "EV1";
    case DfcGenerationEv2:
        return "EV2";
    case DfcGenerationEv3:
        return "EV3";
    }
    return "unknown";
}

DfcDerStatus dfc_profile_check(
    const DfcProfile* profile,
    const DfcCredential* credential,
    char* reason,
    size_t reason_cap) {
    if(reason && reason_cap) reason[0] = '\0';
    if(!profile || !credential) {
        say(reason, reason_cap, "no profile or credential");
        return DfcDerMalformed;
    }

    size_t max_apps = profile->max_apps ? profile->max_apps : DFC_MAX_APPS;
    size_t max_files = profile->max_files ? profile->max_files : DFC_MAX_FILES;
    size_t max_keys = profile->max_keys_per_app ? profile->max_keys_per_app : DFC_MAX_KEYS;

    // Generation decides which feature set applies to the remaining checks.
    uint8_t bit = DFC_PROFILE_GEN_BIT(credential->card.generation);
    if((profile->supported_generations & bit) == 0) {
        say(reason, reason_cap, "generation %s not emulated",
            generation_name(credential->card.generation));
        return DfcDerUnsupported;
    }

    // Refuse an ATS whose length octet disagrees with its encoded length.
    if(!dfc_credential_picc_ats_is_consistent(credential)) {
        say(reason, reason_cap, "user ATS length octet %02X for %zu octets",
            credential->picc_ats[0], credential->picc_ats_len);
        return DfcDerUnsupported;
    }

    if(!profile->supports_picc_files) {
        for(size_t i = 0; i < credential->num_files; i++) {
            if(credential->files[i].app_index == DFC_FILE_OWNER_PICC) {
                say(reason, reason_cap, "file %02X at PICC level",
                    credential->files[i].number);
                return DfcDerUnsupported;
            }
        }
    }

    if((credential->card.storage == DfcStorage2KByteCount && !DFC_ENABLE_STORAGE_2K) ||
       (credential->card.storage == DfcStorage4KByteCount && !DFC_ENABLE_STORAGE_4K) ||
       (credential->card.storage == DfcStorage8KByteCount && !DFC_ENABLE_STORAGE_8K)) {
        say(reason, reason_cap, "storage %u not enabled", credential->card.storage);
        return DfcDerUnsupported;
    }

    if(credential->num_apps > max_apps) {
        say(reason, reason_cap, "applications %zu over limit %zu",
            credential->num_apps, max_apps);
        return DfcDerCapacity;
    }
    if(credential->num_files > max_files) {
        say(reason, reason_cap, "files %zu over limit %zu", credential->num_files, max_files);
        return DfcDerCapacity;
    }
    if(credential->picc_num_keys > max_keys) {
        say(reason, reason_cap, "PICC keys %zu over limit %zu",
            credential->picc_num_keys, max_keys);
        return DfcDerCapacity;
    }
    for(size_t i = 0; i < credential->num_apps; i++) {
        if(credential->apps[i].num_keys > max_keys) {
            say(reason, reason_cap, "application %zu keys %zu over limit %zu", i,
                credential->apps[i].num_keys, max_keys);
            return DfcDerCapacity;
        }
    }

    // Files per application, which is a per-container limit rather than a total.
    for(size_t i = 0; i < credential->num_apps; i++) {
        size_t count = dfc_credential_count_files_in_app(credential, i);
        if(count > DFC_EV1_MAX_FILES_PER_APP) {
            say(reason, reason_cap, "application %zu files %zu over limit %d", i, count,
                DFC_EV1_MAX_FILES_PER_APP);
            return DfcDerCapacity;
        }
    }

    // The credential's own declared storage, and the space its contents need.
    if(profile->max_storage && credential->card.storage > profile->max_storage) {
        say(reason, reason_cap, "storage %u over limit %u", credential->card.storage,
            profile->max_storage);
        return DfcDerCapacity;
    }
    if(profile->max_storage) {
        uint64_t declared = 0;
        for(size_t i = 0; i < credential->num_files; i++) {
            const DfcFile* f = &credential->files[i];
            if(f->type == 0x00 || f->type == 0x01) {
                declared += f->declared_size;
            } else if(f->type == 0x03 || f->type == 0x04) {
                declared += (uint64_t)f->record_size * f->max_records;
            }
        }
        if(declared > profile->max_storage) {
            say(reason, reason_cap, "declared file space %llu over limit %u",
                (unsigned long long)declared, profile->max_storage);
            return DfcDerCapacity;
        }
    }

    return DfcDerOk;
}
