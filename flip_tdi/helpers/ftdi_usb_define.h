#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

// # Clocks and baudrates
#define FTDIBUS_CLOCK_BASE 6000000UL
#define FTDIBUS_CLOCK_HIGH 30000000UL
#define FTDI_BITBANG_BAUDRATE_RATIO_BASE 16
#define FTDI_BITBANG_BAUDRATE_RATIO_HIGH 5
#define FTDI_BAUDRATE_REF_BASE 3000000UL
#define FTDI_BAUDRATE_REF_HIGH 12000000UL

/*descriptor type*/
typedef enum {
    FtdiDescriptorTypeDevice = 0x01,
    FtdiDescriptorTypeConfig = 0x02,
    FtdiDescriptorTypeString = 0x03,
    FtdiDescriptorTypeInterface = 0x04,
    FtdiDescriptorTypeEndpoint = 0x05,
} FtdiDescriptorType;

/*endpoint direction*/
typedef enum {
    FtdiEndpointIn = 0x80,
    FtdiEndpointOut = 0x00,
} FtdiEndpointDirection;

/*endpoint type*/
typedef enum {
    FtdiEndpointTypeCtrl = 0x00,
    FtdiEndpointTypeIso = 0x01,
    FtdiEndpointTypeBulk = 0x02,
    FtdiEndpointTypeIntr = 0x03,
} FtdiEndpointType;

/*control request type*/
typedef enum {
    FtdiControlTypeStandard = (0 << 5),
    FtdiControlTypeClass = (1 << 5),
    FtdiControlTypeVendor = (2 << 5),
    FtdiControlTypeReserved = (3 << 5),
} FtdiControlType;

/*control request recipient*/
typedef enum {
    FtdiControlRecipientDevice = 0,
    FtdiControlRecipientInterface = 1,
    FtdiControlRecipientEndpoint = 2,
    FtdiControlRecipientOther = 3,
} FtdiControlRecipient;

/*control request direction*/
typedef enum {
    FtdiControlOut = 0x00,
    FtdiControlIn = 0x80,
} FtdiControlDirection;

/*endpoint address mask*/
typedef enum {
    FtdiEndpointAddrMask = 0x0f,
    FtdiEndpointDirMask = 0x80,
    FtdiEndpointTransferTypeMask = 0x03,
    FtdiCtrlDirMask = 0x80,
} FtdiEndpointMask;

// typedef enum {
//     FtdiBitModeReset = 0x00, /**< switch off bitbang mode, back to regular serial/FIFO */
//     FtdiBitModeBitbang =
//         0x01, /**< classical asynchronous bitbang mode, introduced with B-type chips */
//     FtdiBitModeMpsse = 0x02, /**< MPSSE mode, available on 2232x chips */
//     FtdiBitModeSyncbb = 0x04, /**< synchronous bitbang mode, available on 2232x and R-type chips */
//     FtdiBitModeMcu = 0x08, /**< MCU Host Bus Emulation mode, available on 2232x chips */
//     /* CPU-style fifo mode gets set via EEPROM */
//     FtdiBitModeOpto =
//         0x10, /**< Fast Opto-Isolated Serial Interface Mode, available on 2232x chips */
//     FtdiBitModeCbus =
//         0x20, /**< Bitbang on CBUS pins of R-type chips, configure in EEPROM before */
//     FtdiBitModeSyncff =
//         0x40, /**< Single Channel Synchronous FIFO mode, available on 2232H chips */
//     FtdiBitModeFt1284 = 0x80, /**< FT1284 mode, available on 232H chips */
// } FtdiBitMode;

//if(FtdiBitMode&0xff00==0) FtdiBitModeReset switch off bitbang mode, back to regular serial/FIFO
typedef struct {
    //uint8_t MASK : 8; /*Mask*/
    uint8_t BITBANG : 1; /*classical asynchronous bitbang mode, introduced with B-type chips*/
    uint8_t MPSSE : 1; /*MPSSE mode, available on 2232x chips*/
    uint8_t SYNCBB : 1; /*synchronous bitbang mode, available on 2232x and R-type chips*/
    uint8_t MCU : 1; /*MCU Host Bus Emulation mode, available on 2232x chips*/
    uint8_t OPTO : 1; /*Fast Opto-Isolated Serial Interface Mode, available on 2232x chips*/
    uint8_t CBUS : 1; /*Bitbang on CBUS pins of R-type chips, configure in EEPROM before*/
    uint8_t SYNCFF : 1; /*Single Channel Synchronous FIFO mode, available on 2232H chips*/
    uint8_t FT1284 : 1; /*FT1284 mode, available on 232H chips*/
} FtdiBitMode;
static_assert(sizeof(FtdiBitMode) == sizeof(uint8_t), "Wrong FtdiBitMode");
/* FTDI MPSSE commands */
typedef enum {

    //     /* Shifting commands IN MPSSE Mode*/
    // #define MPSSE_WRITE_NEG 0x01   /* Write TDI/DO on negative TCK/SK edge*/
    // #define MPSSE_BITMODE   0x02   /* Write bits, not bytes */
    // #define MPSSE_READ_NEG  0x04   /* Sample TDO/DI on negative TCK/SK edge */
    // #define MPSSE_LSB       0x08   /* LSB first */
    // #define MPSSE_DO_WRITE  0x10   /* Write TDI/DO */
    // #define MPSSE_DO_READ   0x20   /* Read TDO/DI */
    // #define MPSSE_WRITE_TMS 0x40   /* Write TMS/CS */

    /*MPSSE Commands*/
    FtdiMpsseCommandsWriteBytesPveMsb = 0x10, /**< Write bytes with positive edge clock, MSB first */
    FtdiMpsseCommandsWriteBytesNveMsb = 0x11, /**< Write bytes with negative edge clock, MSB first */
    FtdiMpsseCommandsWriteBitsPveMsb = 0x12, /**< Write bits with positive edge clock, MSB first */
    FtdiMpsseCommandsWriteBitsNveMsb = 0x13, /**< Write bits with negative edge clock, MSB first */
    FtdiMpsseCommandsWriteBytesPveLsb = 0x18, /**< Write bytes with positive edge clock, LSB first */
    FtdiMpsseCommandsWriteBytesNveLsb = 0x19, /**< Write bytes with negative edge clock, LSB first */
    FtdiMpsseCommandsWriteBitsPveLsb = 0x1a, /**< Write bits with positive edge clock, LSB first */
    FtdiMpsseCommandsWriteBitsNveLsb = 0x1b, /**< Write bits with negative edge clock, LSB first */
    FtdiMpsseCommandsReadBytesPveMsb = 0x20, /**< Read bytes with positive edge clock, MSB first */
    FtdiMpsseCommandsReadBytesNveMsb = 0x24, /**< Read bytes with negative edge clock, MSB first */
    FtdiMpsseCommandsReadBitsPveMsb = 0x22, /**< Read bits with positive edge clock, MSB first */
    FtdiMpsseCommandsReadBitsNveMsb = 0x26, /**< Read bits with negative edge clock, MSB first */
    FtdiMpsseCommandsReadBytesPveLsb = 0x28, /**< Read bytes with positive edge clock, LSB first */
    FtdiMpsseCommandsReadBytesNveLsb = 0x2c, /**< Read bytes with negative edge clock, LSB first */
    FtdiMpsseCommandsReadBitsPveLsb = 0x2a, /**< Read bits with positive edge clock, LSB first */
    FtdiMpsseCommandsReadBitsNveLsb = 0x2e, /**< Read bits with negative edge clock, LSB first */
    FtdiMpsseCommandsRwBytesPveNveMsb = 0x31, /**< Read/Write bytes with positive edge clock, MSB first */
    FtdiMpsseCommandsRwBytesNvePveMsb = 0x34, /**< Read/Write bytes with negative edge clock, MSB first */
    FtdiMpsseCommandsRwBitsPveNveMsb = 0x33, /**< Read/Write bits with positive edge clock, MSB first */
    FtdiMpsseCommandsRwBitsNvePveMsb = 0x36, /**< Read/Write bits with negative edge clock, MSB first */
    FtdiMpsseCommandsRwBytesPveNveLsb = 0x39, /**< Read/Write bytes with positive edge clock, LSB first */
    FtdiMpsseCommandsRwBytesNvePveLsb = 0x3c, /**< Read/Write bytes with negative edge clock, LSB first */
    FtdiMpsseCommandsRwBitsPveNveLsb = 0x3b, /**< Read/Write bits with positive edge clock, LSB first */
    FtdiMpsseCommandsRwBitsNvePveLsb = 0x3e, /**< Read/Write bits with negative edge clock, LSB first */
    FtdiMpsseCommandsWriteBitsTmsPve = 0x4a, /**< Write bits with TMS, positive edge clock */
    FtdiMpsseCommandsWriteBitsTmsNve = 0x4b, /**< Write bits with TMS, negative edge clock */
    FtdiMpsseCommandsRwBitsTmsPvePve = 0x6a, /**< Read/Write bits with TMS, positive edge clock, MSB first */
    FtdiMpsseCommandsRwBitsTmsPveNve = 0x6b, /**< Read/Write bits with TMS, positive edge clock, MSB first */
    FtdiMpsseCommandsRwBitsTmsNvePve = 0x6e, /**< Read/Write bits with TMS, negative edge clock, MSB first */
    FtdiMpsseCommandsRwBitsTmsNveNve = 0x6f, /**< Read/Write bits with TMS, negative edge clock, MSB first */


    FtdiMpsseCommandsSetBitsLow = 0x80, /**< Change LSB GPIO output */
    /*BYTE DATA*/
    /*BYTE Direction*/
    FtdiMpsseCommandsSetBitsHigh = 0x82, /**< Change MSB GPIO output */
    /*BYTE DATA*/
    /*BYTE Direction*/
    FtdiMpsseCommandsGetBitsLow = 0x81, /**< Get LSB GPIO output */
    FtdiMpsseCommandsGetBitsHigh = 0x83, /**< Get MSB GPIO output */
    FtdiMpsseCommandsLoopbackStart = 0x84, /**< Enable loopback */
    FtdiMpsseCommandsLoopbackEnd = 0x85, /**< Disable loopback */
    FtdiMpsseCommandsSetTckDivisor = 0x86, /**< Set clock */
    /* H Type specific commands */
    FtdiMpsseCommandsDisDiv5 = 0x8a, /**< Disable divide by 5 */
    FtdiMpsseCommandsEnDiv5 = 0x8b, /**< Enable divide by 5 */
    FtdiMpsseCommandsEnableClk3Phase = 0x8c, /**< Enable 3-phase data clocking (I2C) */
    FtdiMpsseCommandsDisableClk3Phase = 0x8d, /**< Disable 3-phase data clocking */
    FtdiMpsseCommandsClkBitsNoData = 0x8e, /**< Allows JTAG clock to be output w/o data */
    FtdiMpsseCommandsClkBytesNoData = 0x8f, /**< Allows JTAG clock to be output w/o data */
    FtdiMpsseCommandsClkWaitOnHigh = 0x94, /**< Clock until GPIOL1 is high */
    FtdiMpsseCommandsClkWaitOnLow = 0x95, /**< Clock until GPIOL1 is low */
    FtdiMpsseCommandsEnableClkAdaptive = 0x96, /**< Enable JTAG adaptive clock for ARM */
    FtdiMpsseCommandsDisableClkAdaptive = 0x97, /**< Disable JTAG adaptive clock */
    FtdiMpsseCommandsClkCountWaitOnHigh = 0x9c, /**< Clock byte cycles until GPIOL1 is high */
    FtdiMpsseCommandsClkCountWaitOnLow = 0x9d, /**< Clock byte cycles until GPIOL1 is low */
    //FT232H only
    FtdiMpsseCommandsDriveZero = 0x9e, /**< Drive-zero mode */
    /* Commands in MPSSE and Host Emulation Mode */
    FtdiMpsseCommandsSendImmediate = 0x87, /**< Send immediate */
    FtdiMpsseCommandsWaitOnHigh = 0x88, /**< Wait until GPIOL1 is high */
    FtdiMpsseCommandsWaitOnLow = 0x89, /**< Wait until GPIOL1 is low */
    /* Commands in Host Emulation Mode */
    FtdiMpsseCommandsReadShort = 0x90, /**< Read short */
    FtdiMpsseCommandsReadExtended = 0x91, /**< Read extended */
    FtdiMpsseCommandsWriteShort = 0x92, /**< Write short */
    FtdiMpsseCommandsWriteExtended = 0x93, /**< Write extended */

} FtdiMpsseCommands;

/* USB control requests */
typedef enum {
    FtdiControlRequestsOut = (FtdiControlTypeVendor | FtdiControlRecipientDevice | FtdiControlOut),
    FtdiControlRequestsIn = (FtdiControlTypeVendor | FtdiControlRecipientDevice | FtdiControlIn),
} FtdiControlRequests;

/* Requests */
typedef enum {
    FtdiRequestsSiOReqReset = 0x0, /**< Reset the port */
    FtdiRequestsSiOReqSetModemCtrl = 0x1, /**< Set the modem control register */
    FtdiRequestsSiOReqSetFlowCtrl = 0x2, /**< Set flow control register */
    FtdiRequestsSiOReqSetBaudrate = 0x3, /**< Set baud rate */
    FtdiRequestsSiOReqSetData = 0x4, /**< Set the data characteristics of the port */
    FtdiRequestsSiOReqPollModemStatus = 0x5, /**< Get line status */
    FtdiRequestsSiOReqSetEventChar = 0x6, /**< Change event character */
    FtdiRequestsSiOReqSetErrorChar = 0x7, /**< Change error character */
    FtdiRequestsSiOReqSetLatencyTimer = 0x9, /**< Change latency timer */
    FtdiRequestsSiOReqGetLatencyTimer = 0xa, /**< Get latency timer */
    FtdiRequestsSiOReqSetBitmode = 0xb, /**< Change bit mode */
    FtdiRequestsSiOReqReadPins = 0xc, /**< Read GPIO pin value (or "get bitmode") */
    FtdiRequestsSiOReqReadEeprom = 0x90, /**< Read EEPROM */
    FtdiRequestsSiOReqWriteEeprom = 0x91, /**< Write EEPROM */
    FtdiRequestsSiOReqEraseEeprom = 0x92, /**< Erase EEPROM */
} FtdiRequests;

/*Eeprom requests*/
typedef enum {
    FtdiEepromRequestsEeprom = 0x90,
    FtdiEepromRequestsReadEeprom = FtdiEepromRequestsEeprom + 0, /**< Read EEPROM content */
    FtdiEepromRequestsWriteEeprom = FtdiEepromRequestsEeprom + 1, /**< Write EEPROM content */
    FtdiEepromRequestsEraseEeprom = FtdiEepromRequestsEeprom + 2, /**< Erase EEPROM content */
} FtdiEepromRequests;

/*Reset arguments*/
typedef enum {
    FtdiResetSio = 0, /**< Reset device */
    FtdiResetPurgeRx = 1, /**< Drain USB RX buffer (host-to-ftdi) */
    FtdiResetPurgeTx = 2, /**< Drain USB TX buffer (ftdi-to-host) */
} FtdiReset;

/*Flow control arguments*/
typedef enum {
    FtdiFlowControlDisable = 0x0,
    FtdiFlowControlRtsCtsHs = 0x1 << 8,
    FtdiFlowControlDtrDsrHs = 0x2 << 8,
    FtdiFlowControlXonXoffHs = 0x4 << 8,
    FtdiFlowControlSetDtrMask = 0x1,
    FtdiFlowControlSetDtrHigh = FtdiFlowControlSetDtrMask | (FtdiFlowControlSetDtrMask << 8),
    FtdiFlowControlSetDtrLow = 0x0 | (FtdiFlowControlSetDtrMask << 8),
    FtdiFlowControlSetRtsMask = 0x2,
    FtdiFlowControlSetRtsHigh = FtdiFlowControlSetRtsMask | (FtdiFlowControlSetRtsMask << 8),
    FtdiFlowControlSetRtsLow = 0x0 | (FtdiFlowControlSetRtsMask << 8),
} FtdiFlowControl;

/*Parity bits */
typedef enum {
    FtdiParityNone = 0,
    FtdiParityOdd = 1,
    FtdiParityEven = 2,
    FtdiParityMark = 3,
    FtdiParitySpace = 4,
} FtdiParity;

/*Number of stop bits*/
typedef enum {
    FtdiStopBits1 = 0,
    FtdiStopBits15 = 1,
    FtdiStopBits2 = 2,
} FtdiStopBits;

/*Number of bits*/
typedef enum {
    FtdiBits7 = 7,
    FtdiBits8 = 8,
} FtdiBits;

/*Break type*/
typedef enum {
    FtdiBreakOff = 0,
    FtdiBreakOn = 1,
} FtdiBreak;

typedef struct {
    uint8_t BITS : 4; /*Cound data bits*/
    uint8_t RESERVED : 4; /*Reserved0*/
    uint8_t PARITY : 3; /*Parity*/
    uint8_t STOP_BITS : 2; /*Number of stop bits*/
    uint8_t BREAK : 1; /*Break type*/
} FtdiDataConfig;
static_assert(sizeof(FtdiDataConfig) == sizeof(uint16_t), "Wrong FtdiDataConfig");

typedef struct {
    uint8_t RESERVED0 : 1; /*Reserved0*/
    uint8_t RESERVED1 : 1; /*Reserved1*/
    uint8_t RESERVED2 : 1; /*Reserved2*/
    uint8_t RESERVED3 : 1; /*Reserved3*/
    uint8_t CTS : 1; /*Clear to send (CTS)*/
    uint8_t DTR : 1; /*Data set ready (DTR)*/
    uint8_t RI : 1; /*Ring indicator (RI)*/
    uint8_t RLSD : 1; /*Receive line signal detect (RLSD)*/
    uint8_t DR : 1; /*Data ready (DR)*/
    uint8_t OE : 1; /*Overrun error (OE)*/
    uint8_t PE : 1; /*Parity error (PE)*/
    uint8_t FE : 1; /*Framing error (FE)*/
    uint8_t BI : 1; /*Break interrupt (BI)*/
    uint8_t THRE : 1; /*Transmitter holding register (THRE)*/
    uint8_t TEMT : 1; /*Transmitter empty (TEMT)*/
    uint8_t RCVR_FIFO : 1; /*Error in RCVR FIFO*/
} FtdiModemStatus;
static_assert(sizeof(FtdiModemStatus) == sizeof(uint16_t), "Wrong FtdiModemStatus");
