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


#define MAX_BANK_SIZE 200
// storage enum
typedef enum { ReservedBank, EPCBank, TIDBank, UserBank, KillPwd, AccessPwd, FileZero} BankType;

typedef enum { PermaLock, Lock, Unlock, PermaUnlock} LockType;

// Reserved Memory Bank ***Revised by Wiliam Riley Haffner***
typedef struct {
    uint8_t kill_password[4]; // 4 bytes (32 bits) for kill password 
    uint8_t access_password[4]; // 4 bytes (32 bits) for access password 
} ReservedMemoryBank;

// EPC Memory Bank
typedef struct {
    size_t size; // Size of EPC memory data
    uint8_t data[MAX_BANK_SIZE]; // 2 bytes for CRC16, 2 bytes for PC, and max 14 bytes for EPC
    uint16_t pc;
    uint16_t crc;
} EPCMemoryBank;

// TID Memory Bank
typedef struct {
    size_t size; // Size of TID memory data
    uint8_t data[MAX_BANK_SIZE]; // 4 bytes for Class ID
} TIDMemoryBank;

// User Memory Bank
typedef struct {
    size_t size; // Size of user memory data
    uint8_t data[MAX_BANK_SIZE]; // Assuming max 512 bits (64 bytes) for User Memory
} UserMemoryBank;

// EPC Gen 2 Tag containing all memory banks
typedef struct {
    ReservedMemoryBank* reserved;
    EPCMemoryBank* epc;
    TIDMemoryBank* tid;
    UserMemoryBank* user;
} UHFTag;

typedef struct UHFTagWrapper {
    UHFTag* uhf_tag;
} UHFTagWrapper;

UHFTagWrapper* uhf_tag_wrapper_alloc();
void uhf_tag_wrapper_set_tag(UHFTagWrapper* uhf_tag_wrapper, UHFTag* uhf_tag);
void uhf_tag_wrapper_free(UHFTagWrapper* uhf_tag_wrapper);

UHFTag* uhf_tag_alloc();
void uhf_tag_reset(UHFTag* uhf_tag);
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