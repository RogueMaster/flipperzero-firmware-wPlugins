#include "yrm100x_tag.h"
#include "uhf_reader_settings.h"
#include "debug_log.h"
#include <stdlib.h>
#include <string.h>

/**
 * File that handles the tag objects for the YRM100
 * @author frux-c
 * @author modified by haffnerriley
*/

UHFTagWrapper* uhf_tag_wrapper_alloc() {
    UHFTagWrapper* uhf_tag_wrapper = (UHFTagWrapper*)calloc(1, sizeof(UHFTagWrapper));
    if(!uhf_tag_wrapper) return NULL;

    uhf_tag_wrapper->uhf_tag = NULL;
    uhf_tag_wrapper->tag_count = 0;
    for(size_t i = 0; i < UHF_TAG_WRAPPER_MAX_TAGS; i++) {
        uhf_tag_wrapper->tags[i] = NULL;
    }
    return uhf_tag_wrapper;
}

void uhf_tag_wrapper_set_tag(UHFTagWrapper* uhf_tag_wrapper, UHFTag* uhf_tag) {
    if(uhf_tag_wrapper->uhf_tag != NULL) {
        uhf_tag_free(uhf_tag_wrapper->uhf_tag);
    }
    uhf_tag_wrapper->uhf_tag = uhf_tag;
}

void uhf_tag_wrapper_free(UHFTagWrapper* uhf_tag_wrapper) {
    if(!uhf_tag_wrapper) return;

    uhf_tag_free(uhf_tag_wrapper->uhf_tag);
    for(size_t i = 0; i < uhf_tag_wrapper->tag_count; i++) {
        uhf_tag_free(uhf_tag_wrapper->tags[i]);
        uhf_tag_wrapper->tags[i] = NULL;
    }
    uhf_tag_wrapper->tag_count = 0;
    free(uhf_tag_wrapper);
}

bool uhf_tag_wrapper_add_tag(UHFTagWrapper* uhf_tag_wrapper, UHFTag* uhf_tag) {
    if(uhf_tag_wrapper->tag_count >= UHF_TAG_WRAPPER_MAX_TAGS) return false;
    uhf_tag_wrapper->tags[uhf_tag_wrapper->tag_count] = uhf_tag;
    uhf_tag_wrapper->tag_count++;
    return true;
}

void uhf_tag_wrapper_reset_list(UHFTagWrapper* uhf_tag_wrapper) {
    if(!uhf_tag_wrapper) return;

    const size_t heap_before_reset =
        uhf_debug_heap(UHFDebugTrace, "tag_wrapper", "reset_begin", 0U);
    const size_t previous_count = uhf_tag_wrapper->tag_count;
    for(size_t i = 0; i < uhf_tag_wrapper->tag_count; i++) {
        uhf_tag_free(uhf_tag_wrapper->tags[i]);
        uhf_tag_wrapper->tags[i] = NULL;
    }
    uhf_tag_wrapper->tag_count = 0;
    const size_t heap_after_reset =
        uhf_debug_heap(UHFDebugTrace, "tag_wrapper", "reset_end", heap_before_reset);
    UHF_T(
        "MEM",
        "WRAPPER RESET count=%lu before=%lu after=%lu recovered=%ld",
        (unsigned long)previous_count,
        (unsigned long)heap_before_reset,
        (unsigned long)heap_after_reset,
        (long)heap_after_reset - (long)heap_before_reset);
}

UHFTag* uhf_tag_alloc() {
    UHFTag* uhf_tag = (UHFTag*)calloc(1, sizeof(UHFTag));
    if(!uhf_tag) return NULL;

    uhf_tag->reserved = (ReservedMemoryBank*)calloc(1, sizeof(ReservedMemoryBank));
    uhf_tag->epc = (EPCMemoryBank*)calloc(1, sizeof(EPCMemoryBank));
    uhf_tag->tid = (TIDMemoryBank*)calloc(1, sizeof(TIDMemoryBank));
    uhf_tag->user = (UserMemoryBank*)calloc(1, sizeof(UserMemoryBank));

    if(!uhf_tag->reserved || !uhf_tag->epc || !uhf_tag->tid || !uhf_tag->user) {
        uhf_tag_free(uhf_tag);
        return NULL;
    }

    return uhf_tag;
}

void uhf_tag_reset(UHFTag* uhf_tag) {
    uhf_tag->full_dump_complete = false;
    uhf_tag->dump_finalized = false;
    uhf_tag->auto_dump_saved = false;
    uhf_tag->tid_status = UHFTagBankUnknown;
    uhf_tag->user_status = UHFTagBankUnknown;
    uhf_tag->reserved_status = UHFTagBankUnknown;
    uhf_tag->dump_attempts = 0;
    uhf_tag->read_profile = UHF_READER_TAG_PROFILE_GENERIC;
    uhf_tag->profile_match = false;
    uhf_tag->monza4qt_epc_public_size = 0;
    uhf_tag->monza4qt_epc_public_status = UHFTagBankUnknown;
    memset(uhf_tag->monza4qt_epc_public, 0, sizeof(uhf_tag->monza4qt_epc_public));
    uhf_tag->epc->crc = 0;
    uhf_tag->epc->pc = 0;
    uhf_tag->epc->size = 0;
    uhf_tag->tid->size = 0;
    uhf_tag->user->size = 0;
    memset(uhf_tag->epc->data, 0, EPC_MAX_BANK_SIZE);
    memset(uhf_tag->tid->data, 0, TID_MAX_BANK_SIZE);
    memset(uhf_tag->user->data, 0, USER_MAX_BANK_SIZE);
    memset(uhf_tag->reserved->kill_password, 0, 4);
    memset(uhf_tag->reserved->access_password, 0, 4);
    uhf_tag->reserved->size = 0;
    memset(uhf_tag->reserved->data, 0, RESERVED_MAX_BANK_SIZE);
}

void uhf_tag_reset_dump_banks(UHFTag* uhf_tag) {
    if(!uhf_tag) return;

    if(uhf_tag->reserved) memset(uhf_tag->reserved, 0, sizeof(ReservedMemoryBank));
    if(uhf_tag->tid) memset(uhf_tag->tid, 0, sizeof(TIDMemoryBank));
    if(uhf_tag->user) memset(uhf_tag->user, 0, sizeof(UserMemoryBank));

    uhf_tag->full_dump_complete = false;
    uhf_tag->dump_finalized = false;
    uhf_tag->tid_status = UHFTagBankUnknown;
    uhf_tag->user_status = UHFTagBankUnknown;
    uhf_tag->reserved_status = UHFTagBankUnknown;
    uhf_tag->profile_match = false;
    uhf_tag->monza4qt_epc_public_size = 0;
    uhf_tag->monza4qt_epc_public_status = UHFTagBankUnknown;
    memset(uhf_tag->monza4qt_epc_public, 0, sizeof(uhf_tag->monza4qt_epc_public));
}

void uhf_tag_free(UHFTag* uhf_tag) {
    if(uhf_tag == NULL) return;
    free(uhf_tag->reserved);
    free(uhf_tag->epc);
    free(uhf_tag->tid);
    free(uhf_tag->user);
    free(uhf_tag);
}

void uhf_tag_set_epc_pc(UHFTag* uhf_tag, uint16_t pc) {
    uhf_tag->epc->pc = pc;
}

void uhf_tag_set_kill_pwd(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size >= 4) {
        memcpy(uhf_tag->reserved->kill_password, data_in, 4);
    }
}

void uhf_tag_set_access_pwd(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size >= 8) {
        // Access password is always Reserved words 2-3 (bytes 4-7), regardless of how
        // many extra words the bank read returned. Anchoring to the tail broke once
        // the reserved read grew past 8 bytes.
        memcpy(uhf_tag->reserved->access_password, data_in + 4, 4);
    } else if(size >= 4) {
        memcpy(uhf_tag->reserved->access_password, data_in, 4);
    }
}

void uhf_tag_set_epc_crc(UHFTag* uhf_tag, uint16_t crc) {
    uhf_tag->epc->crc = crc;
}

void uhf_tag_set_epc(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > EPC_MAX_BANK_SIZE) size = EPC_MAX_BANK_SIZE;
    memcpy(uhf_tag->epc->data, data_in, size);
    uhf_tag->epc->size = size;
}

void uhf_tag_set_epc_size(UHFTag* uhf_tag, size_t size) {
    uhf_tag->epc->size = size;
}

void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > TID_MAX_BANK_SIZE) size = TID_MAX_BANK_SIZE;
    memcpy(uhf_tag->tid->data, data_in, size);
    uhf_tag->tid->size = size;
}

void uhf_tag_set_tid_size(UHFTag* uhf_tag, size_t size) {
    uhf_tag->tid->size = size;
}

void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > USER_MAX_BANK_SIZE) size = USER_MAX_BANK_SIZE;
    memcpy(uhf_tag->user->data, data_in, size);
    uhf_tag->user->size = size;
}

void uhf_tag_set_user_size(UHFTag* uhf_tag, size_t size) {
    uhf_tag->user->size = size;
}

// getters

uint8_t* uhf_tag_get_epc(UHFTag* uhf_tag) {
    return uhf_tag->epc->data;
}

size_t uhf_tag_get_epc_size(UHFTag* uhf_tag) {
    return uhf_tag->epc->size;
}

uint16_t uhf_tag_get_epc_pc(UHFTag* uhf_tag) {
    return uhf_tag->epc->pc;
}

uint16_t uhf_tag_get_epc_crc(UHFTag* uhf_tag) {
    return uhf_tag->epc->crc;
}

uint8_t* uhf_tag_get_tid(UHFTag* uhf_tag) {
    return uhf_tag->tid->data;
}

size_t uhf_tag_get_tid_size(UHFTag* uhf_tag) {
    return uhf_tag->tid->size;
}

uint8_t* uhf_tag_get_user(UHFTag* uhf_tag) {
    return uhf_tag->user->data;
}

size_t uhf_tag_get_user_size(UHFTag* uhf_tag) {
    return uhf_tag->user->size;
}

uint8_t* uhf_tag_get_access_pwd(UHFTag* uhf_tag) {
    return uhf_tag->reserved->access_password;
}
uint8_t* uhf_tag_get_kill_pwd(UHFTag* uhf_tag) {
    return uhf_tag->reserved->kill_password;
}

void uhf_tag_set_reserved(UHFTag* uhf_tag, uint8_t* data_in, size_t size) {
    if(size > RESERVED_MAX_BANK_SIZE) size = RESERVED_MAX_BANK_SIZE;
    memcpy(uhf_tag->reserved->data, data_in, size);
    uhf_tag->reserved->size = size;
}

uint8_t* uhf_tag_get_reserved(UHFTag* uhf_tag) {
    return uhf_tag->reserved->data;
}

size_t uhf_tag_get_reserved_size(UHFTag* uhf_tag) {
    return uhf_tag->reserved->size;
}
