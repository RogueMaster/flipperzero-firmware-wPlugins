#pragma once

#include <stdint.h>

#define UPDI_PHY_SYNC 0x55u
#define UPDI_PHY_ACK  0x40u
#define UPDI_BREAK    0x00u

#define UPDI_LDS    0x00u
#define UPDI_STS    0x40u
#define UPDI_LD     0x20u
#define UPDI_ST     0x60u
#define UPDI_LDCS   0x80u
#define UPDI_STCS   0xC0u
#define UPDI_REPEAT 0xA0u
#define UPDI_KEY    0xE0u

#define UPDI_PTR_INC     0x04u
#define UPDI_PTR_ADDRESS 0x08u

#define UPDI_ADDRESS_16 0x04u
#define UPDI_ADDRESS_24 0x08u

#define UPDI_DATA_8  0x00u
#define UPDI_DATA_16 0x01u
#define UPDI_DATA_24 0x02u

#define UPDI_KEY_SIB 0x04u
#define UPDI_SIB_32  0x02u

#define UPDI_MAX_REPEAT 256u

#define UPDI_CS_STATUSA     0x00u
#define UPDI_CS_CTRLA       0x02u
#define UPDI_CS_CTRLB       0x03u
#define UPDI_ASI_KEY_STATUS 0x07u
#define UPDI_ASI_RESET_REQ  0x08u
#define UPDI_ASI_CTRLA      0x09u
#define UPDI_ASI_SYS_STATUS 0x0Bu

#define UPDI_CTRLA_IBDLY_BIT    7u
#define UPDI_CTRLA_RSD_BIT      3u
#define UPDI_CTRLB_CCDETDIS_BIT 3u
#define UPDI_CTRLB_UPDIDIS_BIT  2u

#define UPDI_ASI_KEY_STATUS_CHIPERASE  3u
#define UPDI_ASI_KEY_STATUS_NVMPROG    4u
#define UPDI_ASI_SYS_STATUS_NVMPROG    3u
#define UPDI_ASI_SYS_STATUS_LOCKSTATUS 0u

#define UPDI_RESET_REQ_VALUE 0x59u

#define UPDI_CTRLA_IBDLY    (1u << UPDI_CTRLA_IBDLY_BIT)
#define UPDI_CTRLA_RSD      (1u << UPDI_CTRLA_RSD_BIT)
#define UPDI_CTRLB_CCDETDIS (1u << UPDI_CTRLB_CCDETDIS_BIT)
#define UPDI_CTRLB_UPDIDIS  (1u << UPDI_CTRLB_UPDIDIS_BIT)

/* 8 MHz UPDI clock. Default after reset is 4 MHz (0x3), max 225 kbaud. */
#define UPDI_ASI_CTRLA_CLK_8MHZ 0x02u

#define UPDI_BAUD_SAFE 115200u
#define UPDI_BAUD_FAST 230400u

#define UPDI_SIB_NVM_INDEX 8u
#define UPDI_SIB_SIZE      32u

#define UPDI_FUSE_MAX       16u
#define UPDI_FLASH_PAGE_MAX 512u
