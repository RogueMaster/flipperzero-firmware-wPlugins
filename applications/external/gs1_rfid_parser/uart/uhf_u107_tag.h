#pragma once

#define MAX_UHF_BANK_SIZE 256

typedef enum {
    ReservedBank,
    EPCBank,
    TIDBank,
    UserBank
} BankType;

typedef struct {
    bool is_valid;
    uint16_t pc;
    uint16_t crc;
    uint8_t rssi;
    size_t epc_size;
    uint8_t epc_data[MAX_UHF_BANK_SIZE];
} UhfEpcTagData;

typedef struct {
    bool is_valid;
    size_t user_mem_size;
    uint8_t user_mem_data[MAX_UHF_BANK_SIZE];
} UhfUserMemoryData;
