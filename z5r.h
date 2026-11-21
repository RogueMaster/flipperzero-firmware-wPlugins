#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Z-5R key memory ("Z5" format), recovered from BaseZ5R 1.1.0 (z2::PrepareZ5 / DoZ5rToDb):
 *   [0]    format byte: 0x33 = valid, 0x34 = write in progress, <0x11 = legacy 4-byte records
 *   [1..2] preserved as-is
 *   [3..4] ~open_time_code, big-endian (factory 0xFF 0xD1)
 *   [5]    memory code: 0x05 = 4 KB (680 keys), 0x15 = 8 KB (1363 keys)
 *   [6..]  6-byte records: b0..b4 key, b5 = flags | key nibble
 *          b5 bit7 = end of list, bits6..5 = attribute, bit4 = 3-byte key, bits3..0 = key MSB nibble
 * Key bytes are LSB first (b0 = DS1990A ROM[1]); shown MSB first as 12 hex digits.
 */

#define Z5R_PAGE        32
#define Z5R_MEM_MAX     8192
#define Z5R_HDR         6
#define Z5R_KEYS_MAX    1363 /* what an 8 KB controller holds */
#define Z5R_DB_KEYS     3000 /* keys the app keeps in RAM; fit is checked on write / save */

typedef enum {
    Z5rAttrNormal = 1,
    Z5rAttrBlocking = 2,
    Z5rAttrMaster = 3,
    Z5rAttrInactive = 4,
} Z5rAttr;

typedef struct {
    uint8_t b[6]; /* LSB first */
    uint8_t len; /* 6, or 3 for short keys */
    uint8_t attr; /* Z5rAttr */
} Z5rKey;

typedef struct {
    uint8_t mem[Z5R_MEM_MAX]; /* raw controller memory image */
    uint16_t mem_size; /* 4096 or 8192 */
    Z5rKey keys[Z5R_DB_KEYS];
    uint16_t count;
} Z5rDb;

/* format */
uint16_t z5r_mem_size_from_code(uint8_t code); /* 0 if unknown */
uint16_t z5r_capacity(uint16_t mem_size);
bool z5r_parse(Z5rDb* db); /* mem -> keys; false if not a Z5 image */
void z5r_init_blank(Z5rDb* db, uint16_t mem_size); /* empty valid image */
size_t z5r_build(Z5rDb* db); /* keys -> mem (byte0 = 0x34); returns used bytes */
uint16_t z5r_open_time_code(const Z5rDb* db);
int z5r_find(const Z5rDb* db, const Z5rKey* k);
const char* z5r_attr_name(uint8_t attr);
void z5r_key_str(const Z5rKey* k, char* out, size_t n); /* "00000123ABCD" */

/* Flipper key files: DS1990 .ibtn (or EM4100 .rfid, em=true) for 6-byte keys, H10301 .rfid for 3-byte keys */
typedef struct Storage Storage;
bool z5r_key_export(Storage* st, const Z5rKey* k, bool em, char* path, size_t n); /* path = file written */
bool z5r_key_import(Storage* st, const char* path, Z5rKey* k);

/* Z-2 frame checksum: sum(frame incl. checksum) + 0x5A == 0 (mod 256) */
uint8_t z2_cs(const uint8_t* d, size_t n);

/* controller link (I2C EEPROM on external bus, 8-bit addr 0xA0, 16-bit memory address) */
bool z5r_link_present(void);
bool z5r_link_read(uint16_t addr, uint8_t* buf, size_t len);
bool z5r_link_write_page(uint16_t addr, const uint8_t* page); /* 32 bytes, page-aligned */
void z5r_link_power(bool on5v);

/* iButton (Flipper's own contact pad): read DS1990A ROM, true if CRC ok */
bool z5r_ibutton_read(uint8_t rom[8]);

/* Z-2 adapter emulation over USB CDC (channel 1, CLI stays on channel 0) */
typedef struct Z2Emu Z2Emu;
typedef enum {
    Z2BackendController, /* pass page reads/writes to the I2C link */
    Z2BackendImage, /* serve the in-RAM image as a virtual controller */
} Z2Backend;
Z2Emu* z2emu_start(Z2Backend backend, Z5rDb* db);
void z2emu_stop(Z2Emu* emu);
void z2emu_usb_release(void); /* restore the stock USB config; call on app exit */
uint32_t z2emu_pages(Z2Emu* emu); /* page ops served, for UI */
bool z2emu_dirty(Z2Emu* emu); /* image was written by PC */
