/*
 * Portions of this file are derived from the EtherCard library
 * (https://github.com/njh/EtherCard), originally authored by Guido Socher
 * and Jean-Claude Wippler, itself based on AVRlib by Pascal Stang.
 * EtherCard is licensed under GPL-2.0-or-later. See LICENSES/EtherCard.LICENSE.
 *
 * Modifications by Electronic Cats, 2024-2025: ported to Flipper Zero
 * (Furi HAL SPI), C-only, struct-based instance API.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "enc28j60.h"

/**
 * To debug the message it lands
 */

// F0.6 — flipped off for release. Was forced on in tree, printf'ing the
// hex dump of every received frame to UART (cost: throughput on busy LANs).
// Re-enable from a local checkout when debugging the chip driver.
#define DEBUG_MESSAGE false

void show_message(uint8_t* buffer, uint16_t len) {
    UNUSED(buffer);
    UNUSED(len);

#if DEBUG_MESSAGE
    printf("Message: %i bytes --------------------------------------", len);

    for(uint16_t i = 0; i < len; i++) {
        printf("%u\t", buffer[i]);
    }

    printf("\n");
#endif
}

// F0.6 — flipped off for release. Same rationale as DEBUG_MESSAGE.
#define SHOW_PACKETS_RECEIVED 0

#if SHOW_PACKETS_RECEIVED
#include "../protocol_tools/debug_packets.h"
#endif

/**
 * Command, registers and mask for the ENC28J60
 */

// Masks for the registers and bank
#define ADDR_MASK 0x1F
#define BANK_MASK 0x60
#define SPRD_MASK 0x80

// All-bank registers
#define EIE   0x1B
#define EIR   0x1C
#define ESTAT 0x1D
#define ECON2 0x1E
#define ECON1 0x1F

// Bank 0 registers
#define ERDPT   (0x00 | 0x00)
#define EWRPT   (0x02 | 0x00)
#define ETXST   (0x04 | 0x00)
#define ETXND   (0x06 | 0x00)
#define ERXST   (0x08 | 0x00)
#define ERXND   (0x0A | 0x00)
#define ERXRDPT (0x0C | 0x00)

// #define ERXWRPT         (0x0E|0x00)
#define EDMAST (0x10 | 0x00)
#define EDMAND (0x12 | 0x00)

// #define EDMADST         (0x14|0x00)
#define EDMACS (0x16 | 0x00)

// Bank 1 registers
#define EHT0  (0x00 | 0x20)
#define EHT1  (0x01 | 0x20)
#define EHT2  (0x02 | 0x20)
#define EHT3  (0x03 | 0x20)
#define EHT4  (0x04 | 0x20)
#define EHT5  (0x05 | 0x20)
#define EHT6  (0x06 | 0x20)
#define EHT7  (0x07 | 0x20)
#define EPMM0 (0x08 | 0x20)
#define EPMM1 (0x09 | 0x20)
#define EPMM2 (0x0A | 0x20)
#define EPMM3 (0x0B | 0x20)
#define EPMM4 (0x0C | 0x20)
#define EPMM5 (0x0D | 0x20)
#define EPMM6 (0x0E | 0x20)
#define EPMM7 (0x0F | 0x20)
#define EPMCS (0x10 | 0x20)

// #define EPMO            (0x14|0x20)
#define EWOLIE  (0x16 | 0x20)
#define EWOLIR  (0x17 | 0x20)
#define ERXFCON (0x18 | 0x20)
#define EPKTCNT (0x19 | 0x20)

// Bank 2 registers
#define MACON1   (0x00 | 0x40 | 0x80)
#define MACON3   (0x02 | 0x40 | 0x80)
#define MACON4   (0x03 | 0x40 | 0x80)
#define MABBIPG  (0x04 | 0x40 | 0x80)
#define MAIPG    (0x06 | 0x40 | 0x80)
#define MACLCON1 (0x08 | 0x40 | 0x80)
#define MACLCON2 (0x09 | 0x40 | 0x80)
#define MAMXFL   (0x0A | 0x40 | 0x80)
#define MAPHSUP  (0x0D | 0x40 | 0x80)
#define MICON    (0x11 | 0x40 | 0x80)
#define MICMD    (0x12 | 0x40 | 0x80)
#define MIREGADR (0x14 | 0x40 | 0x80)
#define MIWR     (0x16 | 0x40 | 0x80)
#define MIRD     (0x18 | 0x40 | 0x80)

// Bank 3 registers
#define MAADR1  (0x00 | 0x60 | 0x80)
#define MAADR0  (0x01 | 0x60 | 0x80)
#define MAADR3  (0x02 | 0x60 | 0x80)
#define MAADR2  (0x03 | 0x60 | 0x80)
#define MAADR5  (0x04 | 0x60 | 0x80)
#define MAADR4  (0x05 | 0x60 | 0x80)
#define EBSTSD  (0x06 | 0x60)
#define EBSTCON (0x07 | 0x60)
#define EBSTCS  (0x08 | 0x60)
#define MISTAT  (0x0A | 0x60 | 0x80)
#define EREVID  (0x12 | 0x60)
#define ECOCON  (0x15 | 0x60)
#define EFLOCON (0x17 | 0x60)
#define EPAUS   (0x18 | 0x60)

// ENC28J60 ERXFCON Register Bit Definitions
#define ERXFCON_UCEN  0x80
#define ERXFCON_ANDOR 0x40
#define ERXFCON_CRCEN 0x20
#define ERXFCON_PMEN  0x10
#define ERXFCON_MPEN  0x08
#define ERXFCON_HTEN  0x04
#define ERXFCON_MCEN  0x02
#define ERXFCON_BCEN  0x01

// ENC28J60 EIE Register Bit Definitions
#define EIE_INTIE  0x80
#define EIE_PKTIE  0x40
#define EIE_DMAIE  0x20
#define EIE_LINKIE 0x10
#define EIE_TXIE   0x08
#define EIE_WOLIE  0x04
#define EIE_TXERIE 0x02
#define EIE_RXERIE 0x01

// ENC28J60 EIR Register Bit Definitions
#define EIR_PKTIF  0x40
#define EIR_DMAIF  0x20
#define EIR_LINKIF 0x10
#define EIR_TXIF   0x08
#define EIR_WOLIF  0x04
#define EIR_TXERIF 0x02
#define EIR_RXERIF 0x01

// ENC28J60 ESTAT Register Bit Definitions
#define ESTAT_INT     0x80
#define ESTAT_LATECOL 0x10
#define ESTAT_RXBUSY  0x04
#define ESTAT_TXABRT  0x02
#define ESTAT_CLKRDY  0x01

// ENC28J60 ECON2 Register Bit Definitions
#define ECON2_AUTOINC 0x80
#define ECON2_PKTDEC  0x40
#define ECON2_PWRSV   0x20
#define ECON2_VRPS    0x08

// ENC28J60 ECON1 Register Bit Definitions
#define ECON1_TXRST  0x80
#define ECON1_RXRST  0x40
#define ECON1_DMAST  0x20
#define ECON1_CSUMEN 0x10
#define ECON1_TXRTS  0x08
#define ECON1_RXEN   0x04
#define ECON1_BSEL1  0x02
#define ECON1_BSEL0  0x01

// ENC28J60 MACON1 Register Bit Definitions
#define MACON1_LOOPBK  0x10
#define MACON1_TXPAUS  0x08
#define MACON1_RXPAUS  0x04
#define MACON1_PASSALL 0x02
#define MACON1_MARXEN  0x01

// ENC28J60 MACON3 Register Bit Definitions
#define MACON3_PADCFG2 0x80
#define MACON3_PADCFG1 0x40
#define MACON3_PADCFG0 0x20
#define MACON3_TXCRCEN 0x10
#define MACON3_PHDRLEN 0x08
#define MACON3_HFRMLEN 0x04
#define MACON3_FRMLNEN 0x02
#define MACON3_FULDPX  0x01

// ENC28J60 MICMD Register Bit Definitions
#define MICMD_MIISCAN 0x02
#define MICMD_MIIRD   0x01

// ENC28J60 MISTAT Register Bit Definitions
#define MISTAT_NVALID 0x04
#define MISTAT_SCAN   0x02
#define MISTAT_BUSY   0x01

// ENC28J60 EBSTCON Register Bit Definitions
#define EBSTCON_PSV2   0x80
#define EBSTCON_PSV1   0x40
#define EBSTCON_PSV0   0x20
#define EBSTCON_PSEL   0x10
#define EBSTCON_TMSEL1 0x08
#define EBSTCON_TMSEL0 0x04
#define EBSTCON_TME    0x02
#define EBSTCON_BISTST 0x01

// PHY registers
#define PHCON1  0x00
#define PHSTAT1 0x01
#define PHHID1  0x02
#define PHHID2  0x03
#define PHCON2  0x10
#define PHSTAT2 0x11
#define PHIE    0x12
#define PHIR    0x13
#define PHLCON  0x14

// ENC28J60 PHY PHCON1 Register Bit Definitions
#define PHCON1_PRST    0x8000
#define PHCON1_PLOOPBK 0x4000
#define PHCON1_PPWRSV  0x0800
#define PHCON1_PDPXMD  0x0100
// ENC28J60 PHY PHSTAT1 Register Bit Definitions
#define PHSTAT1_PFDPX  0x1000
#define PHSTAT1_PHDPX  0x0800
#define PHSTAT1_LLSTAT 0x0004
#define PHSTAT1_JBSTAT 0x0002
// ENC28J60 PHY PHCON2 Register Bit Definitions
#define PHCON2_FRCLINK 0x4000
#define PHCON2_TXDIS   0x2000
#define PHCON2_JABBER  0x0400
#define PHCON2_HDLDIS  0x0100

// ENC28J60 Packet Control Byte Bit Definitions
#define PKTCTRL_PHUGEEN   0x08
#define PKTCTRL_PPADEN    0x04
#define PKTCTRL_PCRCEN    0x02
#define PKTCTRL_POVERRIDE 0x01

// SPI operation codes
#define ENC28J60_READ_CTRL_REG  0x00
#define ENC28J60_READ_BUF_MEM   0x3A
#define ENC28J60_WRITE_CTRL_REG 0x40
#define ENC28J60_WRITE_BUF_MEM  0x7A
#define ENC28J60_BIT_FIELD_SET  0x80
#define ENC28J60_BIT_FIELD_CLR  0xA0
#define ENC28J60_SOFT_RESET     0xFF

/**
 * variable to know the bank
 */

uint8_t bank = 0;

/**
 * Functions to set the ENC28J60
 */

// Function to write using an operation
static void write_operation(
    FuriHalSpiBusHandle* spi,
    const uint8_t operation,
    const uint8_t address,
    const uint8_t data) {
    uint8_t buffer[] = {operation | (address & ADDR_MASK), data};

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, buffer, sizeof(buffer), TIMEOUT_SPI);
    furi_hal_spi_release(spi);
}

// Funtion to read using an operation
static uint8_t
    read_operation(FuriHalSpiBusHandle* spi, const uint8_t operation, const uint8_t address) {
    uint8_t buffer = operation | (address & ADDR_MASK);

    uint8_t result = 0;

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, &buffer, 1, TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, &result, 1, TIMEOUT_SPI);

    if(address & 0x80) furi_hal_spi_bus_rx(spi, &result, 1, TIMEOUT_SPI);

    furi_hal_spi_release(spi);

    return result;
}

// This function works to get a new bank
static uint8_t get_current_bank(FuriHalSpiBusHandle* spi) {
    return read_operation(spi, ENC28J60_READ_CTRL_REG, ECON1) & 0x03;
}

// Function to set the bank
static void set_bank_with_mask(FuriHalSpiBusHandle* spi, const uint8_t address) {
    // Set the bank
    uint8_t bank_to_set = (address & BANK_MASK) >> 5;

    // Via software to know if it is the same
    if(bank_to_set == bank) return;

    // Read Actual Bank
    uint8_t current_bank = get_current_bank(spi);

    // If the current bank is different, set the new bank
    if(current_bank != (address & BANK_MASK)) {
        write_operation(spi, ENC28J60_BIT_FIELD_CLR, ECON1, 0x03);
        write_operation(spi, ENC28J60_BIT_FIELD_SET, ECON1, bank_to_set);

        bank = bank_to_set;
    }
}

// Function to write just one byte of a register
static void
    write_register_byte(FuriHalSpiBusHandle* spi, const uint8_t address, const uint8_t data) {
    set_bank_with_mask(spi, address);
    write_operation(spi, ENC28J60_WRITE_CTRL_REG, address, data);
}

// Function to write or set a register
static void write_register(FuriHalSpiBusHandle* spi, uint8_t address, const uint16_t data) {
    write_register_byte(spi, address, data);
    write_register_byte(spi, address + 1, data >> 8);
}

// Function to read a byte register
static uint8_t read_register_byte(FuriHalSpiBusHandle* spi, const uint8_t address) {
    set_bank_with_mask(spi, address);
    return read_operation(spi, ENC28J60_READ_CTRL_REG, address);
}

// Function to read a register
static uint16_t read_register(FuriHalSpiBusHandle* spi, const uint8_t address) {
    return read_register_byte(spi, address) + (read_register_byte(spi, address + 1) << 8);
}

// Function to send buffer
static void write_buffer(FuriHalSpiBusHandle* spi, uint16_t len, uint8_t* data) {
    uint8_t command = ENC28J60_WRITE_BUF_MEM;

    if(len == 0) return;

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, &command, 1, TIMEOUT_SPI);
    furi_hal_spi_bus_tx(spi, data, len, TIMEOUT_SPI);
    furi_hal_spi_release(spi);
}

// Function to read buffer
static void read_buffer(FuriHalSpiBusHandle* spi, uint16_t len, uint8_t* data) {
    uint8_t command = ENC28J60_READ_BUF_MEM;

    if(len == 0) return;

    furi_hal_spi_acquire(spi);
    furi_hal_spi_bus_tx(spi, &command, 1, TIMEOUT_SPI);
    furi_hal_spi_bus_rx(spi, data, len, TIMEOUT_SPI);
    furi_hal_spi_release(spi);
}

// To write the registers PHY
static void write_Phy(FuriHalSpiBusHandle* spi, const uint8_t address, const uint16_t data) {
    write_register_byte(spi, MIREGADR, address);
    write_register(spi, MIWR, data);
    while(read_register_byte(spi, MISTAT) & MISTAT_BUSY)
        furi_delay_us(1);
}

// To read the register Phy
static uint16_t read_Phy_byte(FuriHalSpiBusHandle* spi, const uint8_t address) {
    write_register_byte(spi, MIREGADR, address);
    write_register_byte(spi, MICMD, MICMD_MIIRD);
    while(read_register_byte(spi, MISTAT) & MISTAT_BUSY)
        ;
    write_register_byte(spi, MICMD, 0x00);
    return read_register_byte(spi, MIRD + 1);
}

// Alloc memory for the enc28j60 struct
enc28j60_t* enc28j60_alloc(uint8_t* mac_address, uint8_t* ip_address) {
    enc28j60_t* ethernet_enc = (enc28j60_t*)malloc(sizeof(enc28j60_t));
    ethernet_enc->spi = spi_alloc();
    // F0.5a — chip-level mutex serializes register access across threads.
    // Each public function below acquires this around set_bank + SPI op
    // sequences. Closes B-6 (unprotected file-static `bank`).
    ethernet_enc->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    memcpy(ethernet_enc->mac_address, mac_address, 6);
    memcpy(ethernet_enc->ip_address, ip_address, 4);
    return ethernet_enc;
}

// Free enc28j60
void free_enc28j60(enc28j60_t* instance) {
    spi_free(instance->spi);
    if(instance->mutex) furi_mutex_free(instance->mutex);
    free(instance);
}

// Function to give a soft reset to the enc28j60
void enc28j60_soft_reset(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;

    if(!spi) return;

    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_operation(spi, ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
    furi_mutex_release(instance->mutex);

    furi_delay_ms(2);
}

//  Set MAC Adress
void enc28j60_set_mac(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, MAADR5, instance->mac_address[0]);
    write_register_byte(spi, MAADR4, instance->mac_address[1]);
    write_register_byte(spi, MAADR3, instance->mac_address[2]);
    write_register_byte(spi, MAADR2, instance->mac_address[3]);
    write_register_byte(spi, MAADR1, instance->mac_address[4]);
    write_register_byte(spi, MAADR0, instance->mac_address[5]);
    furi_mutex_release(instance->mutex);
}

// Function to start
uint8_t enc28j60_start(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;

    // To know if the SPI is not initialized
    if(!spi) return 0xff;

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    uint32_t prev_time = furi_get_tick();

    while(!(read_operation(spi, ENC28J60_READ_CTRL_REG, ESTAT) & ESTAT_CLKRDY)) {
        if((furi_get_tick()) > (prev_time + 1000)) {
            furi_mutex_release(instance->mutex);
            return 0xff;
        }
        furi_delay_us(1);
    }

    write_register(spi, ERXST, RXSTART_INIT);
    write_register(spi, ERXRDPT, RXSTART_INIT);
    write_register(spi, ERXND, RXSTOP_INIT);
    write_register(spi, ETXST, TXSTART_INIT);
    write_register(spi, ETXND, TXSTOP_INIT);

    write_register_byte(spi, ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_PMEN | ERXFCON_BCEN);
    write_register(spi, EPMM0, 0x303f);
    write_register(spi, EPMCS, 0xf7f9);

    write_Phy(spi, PHLCON, 0x476);

    write_register_byte(spi, MACON1, MACON1_MARXEN);

    write_operation(
        spi, ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);
    write_register(spi, MAIPG, 0x0C12);
    write_register_byte(spi, MABBIPG, 0x12);
    write_register(spi, MAMXFL, MAX_FRAMELEN);

    write_register_byte(spi, MAADR5, instance->mac_address[0]);
    write_register_byte(spi, MAADR4, instance->mac_address[1]);
    write_register_byte(spi, MAADR3, instance->mac_address[2]);
    write_register_byte(spi, MAADR2, instance->mac_address[3]);
    write_register_byte(spi, MAADR1, instance->mac_address[4]);
    write_register_byte(spi, MAADR0, instance->mac_address[5]);

    write_Phy(spi, PHCON2, PHCON2_HDLDIS);
    set_bank_with_mask(spi, ECON1);
    write_operation(spi, ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE | EIE_PKTIE);
    write_operation(spi, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);

    uint8_t rev = read_register_byte(spi, EREVID);

    furi_mutex_release(instance->mutex);

    if(rev > 5) ++rev;

    return rev;
}

// Get if the ENC is linked
bool is_link_up(enc28j60_t* instance) {
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    bool result = (read_Phy_byte(instance->spi, PHSTAT2) >> 2) & 1;
    furi_mutex_release(instance->mutex);
    return result;
}

// To know if it is link up in a determinated time
bool is_the_network_connected(enc28j60_t* instance) {
    bool ret = false;
    uint32_t last_time = furi_get_tick();
    while(((furi_get_tick() - last_time) < 1500) && (!ret)) {
        ret = is_link_up(instance);
    }
    return ret;
}

// Send a Ethernet Packet
void send_packet(enc28j60_t* instance, uint8_t* buffer, uint16_t len) {
    uint8_t retry = 0;

    FuriHalSpiBusHandle* spi = instance->spi;

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    while(1) {
        write_operation(spi, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRST);
        write_operation(spi, ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRST);
        write_operation(spi, ENC28J60_BIT_FIELD_CLR, EIR, EIR_TXERIF | EIR_TXIF);

        if(retry == 0) {
            write_register(spi, EWRPT, TXSTART_INIT);
            write_register(spi, ETXND, TXSTART_INIT + len);
            write_operation(spi, ENC28J60_WRITE_BUF_MEM, 0, 0x00);
            write_buffer(spi, len, buffer);
        }

        // Init Transmission
        write_operation(spi, ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);

        uint16_t count = 0;
        while((read_register_byte(spi, EIR) & (EIR_TXIF | EIR_TXERIF)) == 0 && ++count < 1000)
            furi_delay_us(1);

        if(!(read_register_byte(spi, EIR) & EIR_TXERIF) && count < 1000U) {
            break;
        }

        // cancel previous transmission if stuck
        write_operation(spi, ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);

        uint8_t tsv[7];

        uint16_t etxnd = read_register(spi, ETXND);
        write_register(spi, ERDPT, etxnd + 1);
        read_buffer(spi, sizeof(tsv), tsv);

        if(!((read_register_byte(spi, EIR) & EIR_TXERIF) && (tsv[3] & 1 << 5)) || retry > 16) {
            break;
        }

        retry++;
    }

    furi_mutex_release(instance->mutex);
}

// To get a packet from
uint16_t receive_packet(enc28j60_t* instance, uint8_t* buffer, uint16_t size) {
    FuriHalSpiBusHandle* spi = instance->spi;
    static uint16_t get_next_packet = RXSTART_INIT;
    static bool unreleased_packet = false;
    uint16_t len = 0;

    furi_mutex_acquire(instance->mutex, FuriWaitForever);

    if(unreleased_packet) {
        if(get_next_packet == 0)
            write_register(spi, ERXRDPT, RXSTOP_INIT);
        else
            write_register(spi, ERXRDPT, get_next_packet - 1);
        unreleased_packet = false;
    }

    if(read_register_byte(spi, EPKTCNT) > 0) {
        write_register(spi, ERDPT, get_next_packet);

        struct {
            uint16_t next_packet;
            uint16_t byteCount;
            uint16_t status;
        } header;

        read_buffer(spi, sizeof(header), (uint8_t*)&header);

        get_next_packet = header.next_packet;
        len = header.byteCount - 4; //remove the CRC count
        if(len > size - 1) len = size - 1;
        if((header.status & 0x80) == 0)
            len = 0;
        else
            read_buffer(spi, len, buffer);
        buffer[len] = 0;
        unreleased_packet = true;

        write_operation(spi, ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
    }

    furi_mutex_release(instance->mutex);

#if SHOW_PACKETS_RECEIVED
    analize_packet(buffer, len);
#endif

    return len;
}

void enable_broadcast(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, ERXFCON, read_register_byte(spi, ERXFCON) | ERXFCON_BCEN);
    furi_mutex_release(instance->mutex);
}

void disable_broadcast(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, ERXFCON, read_register_byte(spi, ERXFCON) & ~ERXFCON_BCEN);
    furi_mutex_release(instance->mutex);
}

void enable_multicast(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, ERXFCON, read_register_byte(spi, ERXFCON) | ERXFCON_MCEN);
    furi_mutex_release(instance->mutex);
}

void disable_multicast(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, ERXFCON, read_register_byte(spi, ERXFCON) & ~ERXFCON_MCEN);
    furi_mutex_release(instance->mutex);
}

void enable_promiscuous(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, ERXFCON, read_register_byte(spi, ERXFCON) & ERXFCON_CRCEN);
    furi_mutex_release(instance->mutex);
}

void disable_promiscuous(enc28j60_t* instance) {
    FuriHalSpiBusHandle* spi = instance->spi;
    furi_mutex_acquire(instance->mutex, FuriWaitForever);
    write_register_byte(spi, ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_PMEN | ERXFCON_BCEN);
    furi_mutex_release(instance->mutex);
}

// To generate a random MAC
void generate_random_mac(uint8_t* mac) {
    // Generate random bytes for the MAC address
    for(int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)furi_hal_random_get() & 0xFF;
    }

    // Set the locally administered bit (bit 1) and clear the multicast bit (bit 0)
    // This ensures the MAC address is valid for local use and is unicast
    mac[0] = (mac[0] & 0xFC) | 0x02;
}
