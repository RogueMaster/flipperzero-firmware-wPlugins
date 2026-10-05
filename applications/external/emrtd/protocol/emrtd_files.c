/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The catalogue of elementary files, ICAO Doc 9303 part 10 table 36.
 *
 * The labels are what the file menu shows on a four line screen, so they are
 * kept to what fits; the descriptions are the one line the detail view has
 * room for.
 */

#include "emrtd_files.h"

static const EmrtdFileInfo emrtd_files[] = {
    {EmrtdFileCom,
     0x011E,
     0x1E,
     0x60,
     -1,
     false,
     "EF.COM",
     "Common data",
     "LDS version and the list of groups the chip carries"},
    {EmrtdFileSod,
     0x011D,
     0x1D,
     0x77,
     -1,
     false,
     "EF.SOD",
     "Security object",
     "Signed hashes of every data group"},
    {EmrtdFileDg1,
     0x0101,
     0x01,
     0x61,
     1,
     false,
     "EF.DG1",
     "MRZ",
     "The machine readable zone, as printed on the document"},
    {EmrtdFileDg2,
     0x0102,
     0x02,
     0x75,
     2,
     false,
     "EF.DG2",
     "Face",
     "The facial image, ISO/IEC 19794-5"},
    {EmrtdFileDg3,
     0x0103,
     0x03,
     0x63,
     3,
     true,
     "EF.DG3",
     "Fingerprints",
     "Fingerprints; needs a state issued terminal certificate"},
    {EmrtdFileDg4,
     0x0104,
     0x04,
     0x76,
     4,
     true,
     "EF.DG4",
     "Iris",
     "Iris images; needs a state issued terminal certificate"},
    {EmrtdFileDg5,
     0x0105,
     0x05,
     0x65,
     5,
     false,
     "EF.DG5",
     "Portrait",
     "The portrait as printed on the document"},
    {EmrtdFileDg6,
     0x0106,
     0x06,
     0x66,
     6,
     false,
     "EF.DG6",
     "Reserved",
     "Reserved for future use by ICAO"},
    {EmrtdFileDg7,
     0x0107,
     0x07,
     0x67,
     7,
     false,
     "EF.DG7",
     "Signature",
     "The handwritten signature or usual mark"},
    {EmrtdFileDg8,
     0x0108,
     0x08,
     0x68,
     8,
     false,
     "EF.DG8",
     "Data features",
     "Data features, rarely present"},
    {EmrtdFileDg9,
     0x0109,
     0x09,
     0x69,
     9,
     false,
     "EF.DG9",
     "Structure",
     "Structure features, rarely present"},
    {EmrtdFileDg10,
     0x010A,
     0x0A,
     0x6A,
     10,
     false,
     "EF.DG10",
     "Substance",
     "Substance features, rarely present"},
    {EmrtdFileDg11,
     0x010B,
     0x0B,
     0x6B,
     11,
     false,
     "EF.DG11",
     "Personal details",
     "Full name, place of birth, address and telephone"},
    {EmrtdFileDg12,
     0x010C,
     0x0C,
     0x6C,
     12,
     false,
     "EF.DG12",
     "Document details",
     "Issuing authority, date of issue and endorsements"},
    {EmrtdFileDg13,
     0x010D,
     0x0D,
     0x6D,
     13,
     false,
     "EF.DG13",
     "Optional details",
     "Whatever the issuing state chose to add"},
    {EmrtdFileDg14,
     0x010E,
     0x0E,
     0x6E,
     14,
     false,
     "EF.DG14",
     "Security infos",
     "Chip Authentication and PACE parameters"},
    {EmrtdFileDg15,
     0x010F,
     0x0F,
     0x6F,
     15,
     false,
     "EF.DG15",
     "AA public key",
     "Public key for Active Authentication"},
    {EmrtdFileDg16,
     0x0110,
     0x10,
     0x70,
     16,
     false,
     "EF.DG16",
     "Persons to notify",
     "Who to contact in an emergency"},
};

_Static_assert(
    sizeof(emrtd_files) / sizeof(emrtd_files[0]) == EmrtdFileCount,
    "the catalogue must describe every EmrtdFileId");

const EmrtdFileInfo* emrtd_file_info(EmrtdFileId id) {
    if((size_t)id >= (size_t)EmrtdFileCount) {
        return NULL;
    }
    return &emrtd_files[id];
}

const EmrtdFileInfo* emrtd_file_by_tag(uint8_t tag) {
    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        if(emrtd_files[i].tag == tag) {
            return &emrtd_files[i];
        }
    }
    return NULL;
}

const EmrtdFileInfo* emrtd_file_by_dg_number(int dg_number) {
    /* EF.COM and EF.SOD carry -1, which no caller can ask for by number. */
    if(dg_number < 1 || dg_number > 16) {
        return NULL;
    }
    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        if(emrtd_files[i].dg_number == (int8_t)dg_number) {
            return &emrtd_files[i];
        }
    }
    return NULL;
}

EmrtdFileMask emrtd_file_default_mask(void) {
    /*
     * Everything except the groups behind Extended Access Control. Asking for
     * DG3 or DG4 without a terminal certificate earns a refusal from the chip
     * and, on some documents, a security counter that only resets when the
     * document leaves the field - so they are left out unless asked for.
     */
    EmrtdFileMask mask = 0;
    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        if(!emrtd_files[i].eac_protected) {
            mask |= EMRTD_FILE_BIT(emrtd_files[i].id);
        }
    }
    return mask;
}

void emrtd_file_fid_bytes(const EmrtdFileInfo* info, uint8_t out[2]) {
    if(out == NULL) {
        return;
    }
    if(info == NULL) {
        out[0] = 0x00;
        out[1] = 0x00;
        return;
    }
    out[0] = (uint8_t)(info->fid >> 8);
    out[1] = (uint8_t)(info->fid & 0xFF);
}
