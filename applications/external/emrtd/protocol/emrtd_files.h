/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The catalogue of elementary files, ICAO Doc 9303 part 10.
 *
 * A passport is not a card with sectors. Its data lives in elementary files
 * inside the eMRTD application, each with a two byte file identifier, a short
 * identifier, and an outer template tag that EF.COM uses to announce it.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Index into the file table. Also the bit position in a selection mask. */
typedef enum {
    EmrtdFileCom,
    EmrtdFileSod,
    EmrtdFileDg1,
    EmrtdFileDg2,
    EmrtdFileDg3,
    EmrtdFileDg4,
    EmrtdFileDg5,
    EmrtdFileDg6,
    EmrtdFileDg7,
    EmrtdFileDg8,
    EmrtdFileDg9,
    EmrtdFileDg10,
    EmrtdFileDg11,
    EmrtdFileDg12,
    EmrtdFileDg13,
    EmrtdFileDg14,
    EmrtdFileDg15,
    EmrtdFileDg16,
    EmrtdFileCount,
} EmrtdFileId;

/** A bitmask over EmrtdFileId. */
typedef uint32_t EmrtdFileMask;

#define EMRTD_FILE_BIT(id) ((EmrtdFileMask)1u << (id))

typedef struct {
    EmrtdFileId id;
    uint16_t fid; /**< File identifier, e.g. 0x0101. */
    uint8_t sfi; /**< Short file identifier. */
    uint8_t tag; /**< Outer template tag as listed in EF.COM DO'5C'. */
    int8_t dg_number; /**< 1..16, or -1 for EF.COM and EF.SOD. */
    bool eac_protected; /**< Needs a state issued terminal certificate. */
    const char* name; /**< "EF.DG1". */
    const char* label; /**< "MRZ" - what the menu shows. */
    const char* description; /**< One line for the detail view. */
} EmrtdFileInfo;

const EmrtdFileInfo* emrtd_file_info(EmrtdFileId id);
const EmrtdFileInfo* emrtd_file_by_tag(uint8_t tag);
const EmrtdFileInfo* emrtd_file_by_dg_number(int dg_number);

/** The files this reader attempts by default: everything but the EAC groups. */
EmrtdFileMask emrtd_file_default_mask(void);

/** Split a file identifier into the two bytes SELECT wants. */
void emrtd_file_fid_bytes(const EmrtdFileInfo* info, uint8_t out[2]);

#ifdef __cplusplus
}
#endif
