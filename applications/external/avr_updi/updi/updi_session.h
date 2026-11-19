#pragma once

#include "updi_device.h"
#include "updi_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UpdiConnectFail,
    UpdiConnectLocked,
    UpdiConnectOk,
} UpdiConnectStatus;

typedef struct UpdiSession UpdiSession;

UpdiSession* updi_session_alloc(void);
void updi_session_free(UpdiSession* session);
void updi_session_set_pins(UpdiSession* session, UpdiPins pins);

UpdiConnectStatus updi_session_connect(UpdiSession* session);
bool updi_session_unlock(UpdiSession* session);
bool updi_session_enter_progmode(UpdiSession* session);
bool updi_session_read_signature(UpdiSession* session);

bool updi_session_chip_erase(UpdiSession* session);
bool updi_session_write_page(UpdiSession* session, uint32_t byte_address, const uint8_t* page);
bool updi_session_read_flash(
    UpdiSession* session,
    uint32_t byte_address,
    uint8_t* data,
    size_t size);
bool updi_session_read_eeprom(
    UpdiSession* session,
    uint32_t byte_address,
    uint8_t* data,
    size_t size);
bool updi_session_write_eeprom(
    UpdiSession* session,
    uint32_t byte_address,
    const uint8_t* data,
    size_t size);
bool updi_session_read_fuses(
    UpdiSession* session,
    uint8_t* fuses,
    size_t max_count,
    size_t* out_count);
bool updi_session_write_fuse(UpdiSession* session, uint8_t fuse_index, uint8_t value);

void updi_session_disconnect(UpdiSession* session);

const char* updi_session_error(const UpdiSession* session);
const UpdiTargetInfo* updi_session_target(const UpdiSession* session);
uint32_t updi_session_baud(const UpdiSession* session);
bool updi_session_in_progmode(const UpdiSession* session);
