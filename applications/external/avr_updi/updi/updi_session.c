#include "updi_session.h"

#include "updi_constants.h"
#include "updi_link.h"
#include "updi_nvm.h"
#include "updi_transport.h"

#include <furi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct UpdiSession {
    UpdiTransport* transport;
    UpdiLink link;
    char error[160];
    bool open;
    bool prog_mode;
    UpdiTargetInfo target;
    UpdiNvmFamily sib_family;
};

static const uint8_t updi_key_nvm[8] = {'N', 'V', 'M', 'P', 'r', 'o', 'g', ' '};
static const uint8_t updi_key_erase[8] = {'N', 'V', 'M', 'E', 'r', 'a', 's', 'e'};

static void updi_copy_sib(UpdiTargetInfo* info, const uint8_t sib[UPDI_SIB_SIZE]) {
    size_t end = 0;
    for(size_t i = 0; i < UPDI_SIB_SIZE; i++) {
        char c = (char)sib[i];
        if(c >= 32 && c < 127) {
            info->sib[i] = c;
            end = i + 1;
        } else {
            info->sib[i] = ' ';
        }
    }
    info->sib[end] = '\0';
    while(end > 0 && info->sib[end - 1] == ' ') {
        end--;
        info->sib[end] = '\0';
    }
}

static bool updi_session_need_prog(UpdiSession* session) {
    if(!session->prog_mode) {
        snprintf(session->error, sizeof(session->error), "Not in prog mode");
        return false;
    }
    if(session->target.profile == NULL) {
        snprintf(session->error, sizeof(session->error), "Unknown device profile");
        return false;
    }
    return true;
}

UpdiSession* updi_session_alloc(void) {
    UpdiSession* session = calloc(1, sizeof(UpdiSession));
    if(session == NULL) return NULL;
    session->transport = updi_transport_alloc();
    if(session->transport == NULL) {
        free(session);
        return NULL;
    }
    updi_link_init(&session->link, session->transport, session->error, sizeof(session->error));
    return session;
}

void updi_session_set_pins(UpdiSession* session, UpdiPins pins) {
    updi_transport_set_pins(session->transport, pins);
}

void updi_session_free(UpdiSession* session) {
    if(session == NULL) return;
    updi_session_disconnect(session);
    updi_transport_free(session->transport);
    free(session);
}

bool updi_session_enter_progmode(UpdiSession* session) {
    uint8_t sys = 0;
    if(!updi_link_ldcs(&session->link, UPDI_ASI_SYS_STATUS, &sys)) return false;
    if((sys & (1u << UPDI_ASI_SYS_STATUS_NVMPROG)) != 0) {
        session->prog_mode = true;
        session->target.locked = false;
        return true;
    }

    if(!updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, UPDI_RESET_REQ_VALUE)) return false;
    if(!updi_link_key(&session->link, updi_key_nvm)) return false;

    uint8_t key_status = 0;
    if(!updi_link_ldcs(&session->link, UPDI_ASI_KEY_STATUS, &key_status)) return false;
    if((key_status & (1u << UPDI_ASI_KEY_STATUS_NVMPROG)) == 0) {
        updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, 0x00);
        snprintf(session->error, sizeof(session->error), "NVMProg key rejected");
        return false;
    }

    if(!updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, UPDI_RESET_REQ_VALUE)) return false;
    if(!updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, 0x00)) return false;
    furi_delay_ms(10);

    uint32_t start = furi_get_tick();
    bool saw_lock = false;
    while((furi_get_tick() - start) < furi_ms_to_ticks(300)) {
        if(!updi_link_ldcs(&session->link, UPDI_ASI_SYS_STATUS, &sys)) return false;
        if((sys & (1u << UPDI_ASI_SYS_STATUS_LOCKSTATUS)) == 0) {
            if((sys & (1u << UPDI_ASI_SYS_STATUS_NVMPROG)) == 0) {
                snprintf(session->error, sizeof(session->error), "Did not enter prog mode");
                return false;
            }
            session->prog_mode = true;
            session->target.locked = false;
            return true;
        }
        saw_lock = true;
        furi_delay_ms(5);
    }

    session->target.locked = saw_lock;
    snprintf(
        session->error,
        sizeof(session->error),
        saw_lock ? "Device is locked" : "Did not enter prog mode");
    return false;
}

bool updi_session_unlock(UpdiSession* session) {
    if(!updi_link_key(&session->link, updi_key_erase)) return false;

    uint8_t key_status = 0;
    if(!updi_link_ldcs(&session->link, UPDI_ASI_KEY_STATUS, &key_status)) return false;
    if((key_status & (1u << UPDI_ASI_KEY_STATUS_CHIPERASE)) == 0) {
        snprintf(session->error, sizeof(session->error), "Erase key rejected");
        return false;
    }

    if(!updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, UPDI_RESET_REQ_VALUE)) return false;
    if(!updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, 0x00)) return false;
    furi_delay_ms(20);

    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < furi_ms_to_ticks(1000)) {
        uint8_t sys = 0;
        if(!updi_link_ldcs(&session->link, UPDI_ASI_SYS_STATUS, &sys)) return false;
        if((sys & (1u << UPDI_ASI_SYS_STATUS_LOCKSTATUS)) == 0) {
            session->target.locked = false;
            return true;
        }
        furi_delay_ms(10);
    }

    snprintf(session->error, sizeof(session->error), "Chip erase key timed out");
    return false;
}

bool updi_session_read_signature(UpdiSession* session) {
    uint32_t base = 0x1100u;
    if(session->target.profile != NULL) base = session->target.profile->sigrow_base;
    uint8_t signature[3];
    if(!updi_link_read(&session->link, base, signature, sizeof(signature))) return false;
    updi_target_set_signature(&session->target, signature);
    if(session->target.profile != NULL) {
        updi_link_set_addr_bytes(&session->link, session->target.profile->addr_bytes);
    }
    return true;
}

UpdiConnectStatus updi_session_connect(UpdiSession* session) {
    session->error[0] = '\0';
    memset(&session->target, 0, sizeof(session->target));
    session->prog_mode = false;
    session->sib_family = UpdiNvmNone;
    updi_link_set_addr_bytes(&session->link, 3);

    if(!updi_transport_open(session->transport)) {
        snprintf(
            session->error, sizeof(session->error), "%s", updi_transport_error(session->transport));
        return UpdiConnectFail;
    }
    session->open = true;

    if(!updi_link_startup(&session->link)) return UpdiConnectFail;

    uint8_t sib[UPDI_SIB_SIZE];
    if(!updi_link_read_sib(&session->link, sib)) return UpdiConnectFail;
    updi_copy_sib(&session->target, sib);

    session->sib_family = updi_nvm_family_from_sib(sib);
    snprintf(
        session->target.nvm,
        sizeof(session->target.nvm),
        "P:%c",
        session->sib_family != UpdiNvmNone ? (char)session->sib_family : '?');

    if(!updi_nvm_family_supported(session->sib_family)) {
        snprintf(
            session->error, sizeof(session->error), "Unsupported NVM %s", session->target.nvm);
        return UpdiConnectFail;
    }

    /* P:0 uses 16-bit addressing before signature lookup. */
    updi_link_set_addr_bytes(&session->link, session->sib_family == UpdiNvmP0 ? 2 : 3);

    if(!updi_link_try_fast_baud(&session->link)) return UpdiConnectFail;
    if(!updi_session_enter_progmode(session)) {
        if(session->target.locked) return UpdiConnectLocked;
        return UpdiConnectFail;
    }
    if(!updi_session_read_signature(session)) return UpdiConnectFail;

    if(session->target.known && session->target.profile->nvm_family != session->sib_family) {
        snprintf(
            session->error,
            sizeof(session->error),
            "SIB NVM %s mismatches device table",
            session->target.nvm);
        return UpdiConnectFail;
    }

    FURI_LOG_I(
        "Updi",
        "Connected %s %02X%02X%02X at %lu",
        session->target.name,
        session->target.signature[0],
        session->target.signature[1],
        session->target.signature[2],
        (unsigned long)updi_transport_baud(session->transport));
    return UpdiConnectOk;
}

bool updi_session_chip_erase(UpdiSession* session) {
    if(!updi_session_need_prog(session)) return false;
    return updi_nvm_chip_erase(&session->link, session->target.profile);
}

bool updi_session_write_page(UpdiSession* session, uint32_t byte_address, const uint8_t* page) {
    if(!updi_session_need_prog(session)) return false;
    return updi_nvm_write_flash_page(&session->link, session->target.profile, byte_address, page);
}

bool updi_session_read_flash(
    UpdiSession* session,
    uint32_t byte_address,
    uint8_t* data,
    size_t size) {
    if(!updi_session_need_prog(session)) return false;
    return updi_nvm_read_memory(
        &session->link, session->target.profile->flash_base + byte_address, data, size);
}

bool updi_session_read_eeprom(
    UpdiSession* session,
    uint32_t byte_address,
    uint8_t* data,
    size_t size) {
    if(!updi_session_need_prog(session)) return false;
    if(byte_address + size > session->target.profile->eeprom_size) {
        snprintf(session->error, sizeof(session->error), "EEPROM past end");
        return false;
    }
    return updi_nvm_read_memory(
        &session->link, session->target.profile->eeprom_base + byte_address, data, size);
}

bool updi_session_write_eeprom(
    UpdiSession* session,
    uint32_t byte_address,
    const uint8_t* data,
    size_t size) {
    if(!updi_session_need_prog(session)) return false;
    const UpdiDeviceProfile* profile = session->target.profile;
    while(size > 0) {
        uint32_t page_mask = (uint32_t)profile->eeprom_page - 1u;
        uint32_t page_off = byte_address & page_mask;
        size_t chunk = profile->eeprom_page - page_off;
        if(chunk > size) chunk = size;
        if(!updi_nvm_write_eeprom_page(&session->link, profile, byte_address, data, chunk)) {
            return false;
        }
        byte_address += (uint32_t)chunk;
        data += chunk;
        size -= chunk;
    }
    return true;
}

bool updi_session_read_fuses(
    UpdiSession* session,
    uint8_t* fuses,
    size_t max_count,
    size_t* out_count) {
    if(!updi_session_need_prog(session)) return false;
    size_t count = session->target.profile->fuse_count;
    if(count > max_count) count = max_count;
    if(!updi_nvm_read_memory(&session->link, session->target.profile->fuse_base, fuses, count)) {
        return false;
    }
    if(out_count) *out_count = count;
    return true;
}

bool updi_session_write_fuse(UpdiSession* session, uint8_t fuse_index, uint8_t value) {
    if(!updi_session_need_prog(session)) return false;
    return updi_nvm_write_fuse(&session->link, session->target.profile, fuse_index, value);
}

void updi_session_disconnect(UpdiSession* session) {
    if(session == NULL || !session->open) return;

    char saved[sizeof(session->error)];
    snprintf(saved, sizeof(saved), "%s", session->error);

    if(session->prog_mode) {
        updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, UPDI_RESET_REQ_VALUE);
        updi_link_stcs(&session->link, UPDI_ASI_RESET_REQ, 0x00);
        session->prog_mode = false;
    }
    updi_link_stcs(
        &session->link, UPDI_CS_CTRLB, (uint8_t)(UPDI_CTRLB_UPDIDIS | UPDI_CTRLB_CCDETDIS));

    updi_transport_close(session->transport);
    session->open = false;
    snprintf(session->error, sizeof(session->error), "%s", saved);
}

const char* updi_session_error(const UpdiSession* session) {
    return session->error;
}

const UpdiTargetInfo* updi_session_target(const UpdiSession* session) {
    return &session->target;
}

uint32_t updi_session_baud(const UpdiSession* session) {
    return updi_transport_baud(session->transport);
}

bool updi_session_in_progmode(const UpdiSession* session) {
    return session->prog_mode;
}
