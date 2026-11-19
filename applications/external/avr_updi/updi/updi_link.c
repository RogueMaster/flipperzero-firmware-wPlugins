#include "updi_link.h"

#include "updi_constants.h"

#include <furi.h>

#include <stdio.h>
#include <string.h>

static void updi_link_error(UpdiLink* link, const char* message) {
    if(link->error != NULL && link->error_size > 0) {
        snprintf(link->error, link->error_size, "%s", message);
    }
}

static uint8_t updi_link_addr_mode(const UpdiLink* link) {
    return link->addr_bytes == 2 ? UPDI_ADDRESS_16 : UPDI_ADDRESS_24;
}

static bool updi_link_phy(UpdiLink* link, const uint8_t* data, size_t size) {
    if(!updi_transport_send(link->transport, data, size)) {
        updi_link_error(link, updi_transport_error(link->transport));
        return false;
    }
    return true;
}

static bool updi_link_phy_recv(UpdiLink* link, uint8_t* data, size_t size, uint32_t timeout_ms) {
    if(!updi_transport_recv(link->transport, data, size, timeout_ms)) {
        updi_link_error(link, updi_transport_error(link->transport));
        return false;
    }
    return true;
}

void updi_link_init(UpdiLink* link, UpdiTransport* transport, char* error, size_t error_size) {
    link->transport = transport;
    link->error = error;
    link->error_size = error_size;
    link->addr_bytes = 3;
}

void updi_link_set_addr_bytes(UpdiLink* link, uint8_t addr_bytes) {
    link->addr_bytes = addr_bytes == 2 ? 2 : 3;
}

bool updi_link_ldcs(UpdiLink* link, uint8_t address, uint8_t* value) {
    uint8_t frame[2] = {UPDI_PHY_SYNC, (uint8_t)(UPDI_LDCS | (address & 0x0Fu))};
    if(!updi_link_phy(link, frame, sizeof(frame))) return false;
    return updi_link_phy_recv(link, value, 1, 200);
}

bool updi_link_stcs(UpdiLink* link, uint8_t address, uint8_t value) {
    uint8_t frame[3] = {UPDI_PHY_SYNC, (uint8_t)(UPDI_STCS | (address & 0x0Fu)), value};
    return updi_link_phy(link, frame, sizeof(frame));
}

static bool updi_link_check(UpdiLink* link) {
    if(!updi_link_stcs(link, UPDI_CS_CTRLB, UPDI_CTRLB_CCDETDIS)) return false;
    if(!updi_link_stcs(link, UPDI_CS_CTRLA, UPDI_CTRLA_IBDLY)) return false;
    uint8_t status = 0;
    if(!updi_link_ldcs(link, UPDI_CS_STATUSA, &status)) return false;
    if(status == 0) {
        updi_link_error(link, "UPDI did not answer");
        return false;
    }
    return true;
}

bool updi_link_startup(UpdiLink* link) {
    if(!updi_transport_double_break(link->transport)) {
        updi_link_error(link, updi_transport_error(link->transport));
        return false;
    }
    if(updi_link_check(link)) return true;

    FURI_LOG_I("Updi", "Datalink retry after double break");
    if(!updi_transport_double_break(link->transport)) {
        updi_link_error(link, updi_transport_error(link->transport));
        return false;
    }
    if(updi_link_check(link)) return true;
    if(link->error[0] == '\0') updi_link_error(link, "UPDI initialisation failed");
    return false;
}

bool updi_link_read_sib(UpdiLink* link, uint8_t sib[32]) {
    uint8_t frame[2] = {UPDI_PHY_SYNC, (uint8_t)(UPDI_KEY | UPDI_KEY_SIB | UPDI_SIB_32)};
    if(!updi_link_phy(link, frame, sizeof(frame))) return false;
    return updi_link_phy_recv(link, sib, UPDI_SIB_SIZE, 500);
}

bool updi_link_try_fast_baud(UpdiLink* link) {
    /* The reply to this store can already be clocked at the new rate, so a
     * missing echo is not fatal. The next SYNCH retimes the link. */
    uint8_t frame[3] = {
        UPDI_PHY_SYNC,
        (uint8_t)(UPDI_STCS | UPDI_ASI_CTRLA),
        UPDI_ASI_CTRLA_CLK_8MHZ,
    };
    if(!updi_transport_send(link->transport, frame, sizeof(frame))) {
        updi_transport_flush_rx(link->transport);
    }

    if(!updi_transport_set_baud(link->transport, UPDI_BAUD_FAST)) {
        updi_link_error(link, updi_transport_error(link->transport));
        return false;
    }

    uint8_t status = 0;
    if(!updi_link_ldcs(link, UPDI_CS_STATUSA, &status) || status == 0) {
        FURI_LOG_I("Updi", "230400 failed, staying at 115200");
        if(!updi_transport_double_break(link->transport)) {
            updi_link_error(link, updi_transport_error(link->transport));
            return false;
        }
        if(!updi_link_check(link)) return false;
        return true;
    }
    return true;
}

bool updi_link_key(UpdiLink* link, const uint8_t key[8]) {
    uint8_t header[2] = {UPDI_PHY_SYNC, UPDI_KEY};
    uint8_t reversed[8];
    for(size_t i = 0; i < 8; i++)
        reversed[i] = key[7 - i];
    if(!updi_link_phy(link, header, sizeof(header))) return false;
    return updi_link_phy(link, reversed, sizeof(reversed));
}

static bool updi_link_ack(UpdiLink* link) {
    uint8_t ack = 0;
    if(!updi_link_phy_recv(link, &ack, 1, 200)) return false;
    if(ack != UPDI_PHY_ACK) {
        updi_link_error(link, "UPDI ACK missing");
        return false;
    }
    return true;
}

static size_t updi_link_pack_address(UpdiLink* link, uint8_t* frame, uint32_t address) {
    frame[0] = (uint8_t)address;
    frame[1] = (uint8_t)(address >> 8);
    if(link->addr_bytes == 2) return 2;
    frame[2] = (uint8_t)(address >> 16);
    return 3;
}

bool updi_link_st8(UpdiLink* link, uint32_t address, uint8_t value) {
    uint8_t frame[6];
    frame[0] = UPDI_PHY_SYNC;
    frame[1] = (uint8_t)(UPDI_STS | updi_link_addr_mode(link) | UPDI_DATA_8);
    size_t n = 2 + updi_link_pack_address(link, &frame[2], address);
    if(!updi_link_phy(link, frame, n)) return false;
    if(!updi_link_ack(link)) return false;
    if(!updi_link_phy(link, &value, 1)) return false;
    return updi_link_ack(link);
}

bool updi_link_ld8(UpdiLink* link, uint32_t address, uint8_t* value) {
    uint8_t frame[6];
    frame[0] = UPDI_PHY_SYNC;
    frame[1] = (uint8_t)(UPDI_LDS | updi_link_addr_mode(link) | UPDI_DATA_8);
    size_t n = 2 + updi_link_pack_address(link, &frame[2], address);
    if(!updi_link_phy(link, frame, n)) return false;
    return updi_link_phy_recv(link, value, 1, 200);
}

static bool updi_link_st_ptr(UpdiLink* link, uint32_t address) {
    uint8_t frame[6];
    frame[0] = UPDI_PHY_SYNC;
    uint8_t data_sz = link->addr_bytes == 2 ? UPDI_DATA_16 : UPDI_DATA_24;
    frame[1] = (uint8_t)(UPDI_ST | UPDI_PTR_ADDRESS | data_sz);
    size_t n = 2 + updi_link_pack_address(link, &frame[2], address);
    if(!updi_link_phy(link, frame, n)) return false;
    return updi_link_ack(link);
}

static bool updi_link_repeat(UpdiLink* link, size_t repeats) {
    if(repeats < 1 || repeats > UPDI_MAX_REPEAT) {
        updi_link_error(link, "Bad UPDI repeat count");
        return false;
    }
    uint8_t frame[3] = {
        UPDI_PHY_SYNC,
        UPDI_REPEAT,
        (uint8_t)(repeats - 1u),
    };
    return updi_link_phy(link, frame, sizeof(frame));
}

bool updi_link_read(UpdiLink* link, uint32_t address, uint8_t* data, size_t size) {
    if(size == 0 || size > UPDI_MAX_REPEAT) {
        updi_link_error(link, "Bad UPDI read size");
        return false;
    }
    if(!updi_link_st_ptr(link, address)) return false;
    if(size > 1 && !updi_link_repeat(link, size)) return false;
    uint8_t frame[2] = {UPDI_PHY_SYNC, (uint8_t)(UPDI_LD | UPDI_PTR_INC | UPDI_DATA_8)};
    if(!updi_link_phy(link, frame, sizeof(frame))) return false;
    return updi_link_phy_recv(link, data, size, 1000);
}

static bool updi_link_set_ack(UpdiLink* link, bool enabled) {
    uint8_t value = enabled ? UPDI_CTRLA_IBDLY : (uint8_t)(UPDI_CTRLA_IBDLY | UPDI_CTRLA_RSD);
    return updi_link_stcs(link, UPDI_CS_CTRLA, value);
}

bool updi_link_write_words(UpdiLink* link, uint32_t address, const uint8_t* data, size_t size) {
    if(size == 0 || (size % 2u) != 0 || (address % 2u) != 0 || size > 64u) {
        updi_link_error(link, "Bad flash write chunk");
        return false;
    }

    if(size == 2) {
        uint8_t frame[6];
        frame[0] = UPDI_PHY_SYNC;
        frame[1] = (uint8_t)(UPDI_STS | updi_link_addr_mode(link) | UPDI_DATA_16);
        size_t n = 2 + updi_link_pack_address(link, &frame[2], address);
        if(!updi_link_phy(link, frame, n)) return false;
        if(!updi_link_ack(link)) return false;
        if(!updi_link_phy(link, data, 2)) return false;
        return updi_link_ack(link);
    }

    if(!updi_link_st_ptr(link, address)) return false;
    if(!updi_link_repeat(link, size / 2u)) return false;
    if(!updi_link_set_ack(link, false)) return false;

    uint8_t header[2] = {UPDI_PHY_SYNC, (uint8_t)(UPDI_ST | UPDI_PTR_INC | UPDI_DATA_16)};
    bool ok = updi_link_phy(link, header, sizeof(header));
    if(ok) ok = updi_link_phy(link, data, size);
    if(!updi_link_set_ack(link, true)) return false;
    return ok;
}

bool updi_link_write_bytes(UpdiLink* link, uint32_t address, const uint8_t* data, size_t size) {
    if(size == 0 || size > 64u) {
        updi_link_error(link, "Bad byte write chunk");
        return false;
    }

    if(size == 1) {
        return updi_link_st8(link, address, data[0]);
    }

    if(!updi_link_st_ptr(link, address)) return false;
    if(!updi_link_repeat(link, size)) return false;
    if(!updi_link_set_ack(link, false)) return false;

    uint8_t header[2] = {UPDI_PHY_SYNC, (uint8_t)(UPDI_ST | UPDI_PTR_INC | UPDI_DATA_8)};
    bool ok = updi_link_phy(link, header, sizeof(header));
    if(ok) ok = updi_link_phy(link, data, size);
    if(!updi_link_set_ack(link, true)) return false;
    return ok;
}
