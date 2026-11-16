#pragma once

// PCB encodings shared by the transport and its wire-level tests.
#define DFC_ISO_DEP_RATS         0xE0
#define DFC_ISO_DEP_I_BLOCK      0x02
#define DFC_ISO_DEP_R_BLOCK      0xA2
#define DFC_ISO_DEP_S_BLOCK      0xC2
#define DFC_ISO_DEP_R_NAK        0x10
#define DFC_ISO_DEP_BLOCK_MASK   0xC0
#define DFC_ISO_DEP_CID          0x08
#define DFC_ISO_DEP_NAD          0x04
#define DFC_ISO_DEP_MORE         0x10
#define DFC_ISO_DEP_SEQUENCE_BIT 0x01
#define DFC_ISO_DEP_MAX_INF      48
