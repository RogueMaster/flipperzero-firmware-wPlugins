#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/**
 * File that handles the tag objects for the YRM100
 * @author frux-c
 * @author modified by haffnerriley
*/

#define EPC_MAX_BANK_SIZE      32 // 96-bit EPC = 12 B; 256-bit EPC = 32 B (very generous)
#define TID_MAX_BANK_SIZE      32 // standard TID 8–16 B; extended up to 32 B
#define USER_MAX_BANK_SIZE     64 // Gen2 standard user memory ≤ 64 B
#define RESERVED_MAX_BANK_SIZE 32 // kill+access is 8 B, but some tags expose more readable words

#define MONZA4QT_CANONICAL_TID_SIZE 12U
#define MONZA4QT_EPC_PUBLIC_SIZE    12U
#define MONZA4QT_TID_WINDOW_SIZE    24U

// storage enum
typedef enum {
    ReservedBank,
    EPCBank,
    TIDBank,
    UserBank,
    KillPwd,
    AccessPwd,
    FileZero
} BankType;

typedef enum {
    PermaLock,
    Lock,
    Unlock,
    PermaUnlock
} LockType;

typedef enum {
    UHFTagBankUnknown = 0,
    UHFTagBankReadable,
    UHFTagBankEmpty,
    UHFTagBankLocked,
    UHFTagBankWrongPassword,
    UHFTagBankReadError,
    UHFTagBankAborted,
} UHFTagBankStatus;

static inline const char* uhf_tag_bank_status_name(UHFTagBankStatus status) {
    switch(status) {
    case UHFTagBankReadable:
        return "OK";
    case UHFTagBankEmpty:
        return "EMPTY";
    case UHFTagBankLocked:
        return "LOCKED";
    case UHFTagBankWrongPassword:
        return "WRONG_AP";
    case UHFTagBankReadError:
        return "READ_ERROR";
    case UHFTagBankAborted:
        return "ABORTED";
    case UHFTagBankUnknown:
    default:
        return "UNKNOWN";
    }
}

// Reserved Memory Bank ***Revised by Wiliam Riley Haffner***
typedef struct {
    uint8_t kill_password[4]; // 4 bytes (32 bits) for kill password
    uint8_t access_password[4]; // 4 bytes (32 bits) for access password
    size_t size; // length of the raw reserved bank as read
    uint8_t data[RESERVED_MAX_BANK_SIZE]; // full reserved bank contents exactly as returned
} ReservedMemoryBank;

// EPC Memory Bank
typedef struct {
    size_t size; // Size of EPC memory data
    uint8_t data[EPC_MAX_BANK_SIZE]; // 2 bytes for CRC16, 2 bytes for PC, and max 14 bytes for EPC
    uint16_t pc;
    uint16_t crc;
    int8_t rssi; // Last RSSI reading from the poll frame (signed dBm)
} EPCMemoryBank;

// TID Memory Bank
typedef struct {
    size_t size; // Size of TID memory data
    uint8_t data[TID_MAX_BANK_SIZE]; // 4 bytes for Class ID
} TIDMemoryBank;

// User Memory Bank
typedef struct {
    size_t size; // Size of user memory data
    uint8_t data[USER_MAX_BANK_SIZE]; // Assuming max 512 bits (64 bytes) for User Memory
} UserMemoryBank;

// EPC Gen 2 Tag containing all memory banks
typedef struct {
    ReservedMemoryBank* reserved;
    EPCMemoryBank* epc;
    TIDMemoryBank* tid;
    UserMemoryBank* user;
    // True only when all accessible banks were read without access/read errors.
    bool full_dump_complete;

    // Terminal acquisition state. FULL and PARTIAL are both terminal and must
    // never be retried forever in Read Full(Multi).
    bool dump_finalized;

    UHFTagBankStatus tid_status;
    UHFTagBankStatus user_status;
    UHFTagBankStatus reserved_status;

    // Whole-dump attempts used to bound transient RF retry loops.
    uint8_t dump_attempts;

    // Runtime-only flag: the .uhf file + Saved entry were created for this capture.
    bool auto_dump_saved;

    /*
     * Manufacturer-specific acquisition metadata.
     *
     * The canonical tag->tid remains the actual serialized TID used by UI,
     * Saved_EPCs, Write and Clone.
     */
    uint8_t read_profile;
    bool profile_match;

    // Impinj Monza 4QT: bytes immediately following the 96-bit serialized TID
    // when the 24-byte TID-bank window is readable (EPC_Public in the documented
    // Private-profile memory map).
    uint8_t monza4qt_epc_public[MONZA4QT_EPC_PUBLIC_SIZE];
    size_t monza4qt_epc_public_size;
    UHFTagBankStatus monza4qt_epc_public_status;
} UHFTag;

#define UHF_TAG_WRAPPER_MAX_TAGS 50

typedef struct UHFTagWrapper {
    UHFTag* uhf_tag;
    UHFTag* tags[UHF_TAG_WRAPPER_MAX_TAGS];
    size_t tag_count;
} UHFTagWrapper;

UHFTagWrapper* uhf_tag_wrapper_alloc();
void uhf_tag_wrapper_set_tag(UHFTagWrapper* uhf_tag_wrapper, UHFTag* uhf_tag);
void uhf_tag_wrapper_free(UHFTagWrapper* uhf_tag_wrapper);
bool uhf_tag_wrapper_add_tag(UHFTagWrapper* uhf_tag_wrapper, UHFTag* uhf_tag);
void uhf_tag_wrapper_reset_list(UHFTagWrapper* uhf_tag_wrapper);

UHFTag* uhf_tag_alloc();
void uhf_tag_reset(UHFTag* uhf_tag);
void uhf_tag_reset_dump_banks(UHFTag* uhf_tag);
void uhf_tag_free(UHFTag* uhf_tag);

void uhf_tag_set_kill_pwd(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_access_pwd(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_epc_pc(UHFTag* uhf_tag, uint16_t pc);
void uhf_tag_set_epc_crc(UHFTag* uhf_tag, uint16_t crc);
void uhf_tag_set_epc(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_epc_size(UHFTag* uhf_tag, size_t size);
void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_tid_size(UHFTag* uhf_tag, size_t size);
void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_user_size(UHFTag* uhf_tag, size_t size);
void uhf_tag_set_reserved(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
uint8_t* uhf_tag_get_reserved(UHFTag* uhf_tag);
size_t uhf_tag_get_reserved_size(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_kill_pwd(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_access_pwd(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_epc(UHFTag* uhf_tag);
uint16_t uhf_tag_get_epc_pc(UHFTag* uhf_tag);
uint16_t uhf_tag_get_epc_crc(UHFTag* uhf_tag);
size_t uhf_tag_get_epc_size(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_tid(UHFTag* uhf_tag);
size_t uhf_tag_get_tid_size(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_user(UHFTag* uhf_tag);
size_t uhf_tag_get_user_size(UHFTag* uhf_tag);

// debug
char* uhf_tag_get_cstr(UHFTag* uhf_tag);
