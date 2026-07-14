#ifndef __MCP_CAN_2515_H_
#define __MCP_CAN_2515_H_

#include "Spi_lib.h"
#include "log_user.h"

#ifdef __LOG_USER_H_
// for debugging errors
#define DEBUG
#define DEEPDEBUG

// If you want to debug errors from the code comment the undef DEEPDEBUG
// #undef DEBUG
// #undef DEEPDEBUG

#define PROVE
#undef PROVE

#endif

// This is to know if the  frame is extended or standard
#define CAN_IS_EXTENDED       0x80000000
#define CAN_IS_REMOTE_REQUEST 0x40000000
#define CAN_EXTENDED_ID       0x1FFFFFFF
#define MAX_LEN               8

// Mask of the bits
static const uint8_t CANCTRL_REQOP = 0xE0;
static const uint8_t CANCTRL_ABAT = 0x10;
static const uint8_t CANCTRL_OSM = 0x08;
static const uint8_t CANCTRL_CLKEN = 0x04;
static const uint8_t CANCTRL_CLKPRE = 0x03;

static const uint8_t CANSTAT_OPM = 0xE0;
static const uint8_t CANSTAT_ICOD = 0x0E;

// Instructions for the CLKOUT
#define CLKOUT_ENABLE  0x04
#define CLKOUT_DISABLE 0x00
#define CLKOUT_PS1     0x00
#define CLKOUT_PS2     0x01
#define CLKOUT_PS4     0x02
#define CLKOUT_PS8     0x03
#define TIMEOUT        2500

/*
 *   CAN CLOCK CONFIGURATION
 */

/*
 *  The clock frequency of the MCP2515 module is 16MHZ but if anyone change of
 * module you can use another clock value At the moment the library only have a
 * bitrate of 1000KBPS, 500KBPS, 250 KBPS, 125 KBPS
 */

// These are to config the clock in 16MHz
#define MCP_16MHz_1000kBPS_CFG1 (0x00)
#define MCP_16MHz_1000kBPS_CFG2 (0xD0)
#define MCP_16MHz_1000kBPS_CFG3 (0x82)

#define MCP_16MHz_500kBPS_CFG1 (0x00)
#define MCP_16MHz_500kBPS_CFG2 (0xF0)
#define MCP_16MHz_500kBPS_CFG3 (0x86)

#define MCP_16MHz_250kBPS_CFG1 (0x41)
#define MCP_16MHz_250kBPS_CFG2 (0xF1)
#define MCP_16MHz_250kBPS_CFG3 (0x85)

#define MCP_16MHz_125kBPS_CFG1 (0x03)
#define MCP_16MHz_125kBPS_CFG2 (0xF0)
#define MCP_16MHz_125kBPS_CFG3 (0x86)

// These are to config the clock in 8MHz
#define MCP_8MHz_1000kBPS_CFG1 (0x00)
#define MCP_8MHz_1000kBPS_CFG2 (0xC0)
#define MCP_8MHz_1000kBPS_CFG3 (0x80)

#define MCP_8MHz_500kBPS_CFG1 (0x00)
#define MCP_8MHz_500kBPS_CFG2 (0xD1)
#define MCP_8MHz_500kBPS_CFG3 (0x81)

#define MCP_8MHz_250kBPS_CFG1 (0x80)
#define MCP_8MHz_250kBPS_CFG2 (0xE5)
#define MCP_8MHz_250kBPS_CFG3 (0x83)

#define MCP_8MHz_125kBPS_CFG1 (0x01)
#define MCP_8MHz_125kBPS_CFG2 (0xB1)
#define MCP_8MHz_125kBPS_CFG3 (0x85)

// These are to config the clock in 20MHz
#define MCP_20MHz_1000kBPS_CFG1 (0x00)
#define MCP_20MHz_1000kBPS_CFG2 (0xD9)
#define MCP_20MHz_1000kBPS_CFG3 (0x82)

#define MCP_20MHz_500kBPS_CFG1 (0x00)
#define MCP_20MHz_500kBPS_CFG2 (0xFA)
#define MCP_20MHz_500kBPS_CFG3 (0x87)

#define MCP_20MHz_250kBPS_CFG1 (0x41)
#define MCP_20MHz_250kBPS_CFG2 (0xFB)
#define MCP_20MHz_250kBPS_CFG3 (0x86)

#define MCP_20MHz_125kBPS_CFG1 (0x03)
#define MCP_20MHz_125kBPS_CFG2 (0xFA)
#define MCP_20MHz_125kBPS_CFG3 (0x87)

/*
 *   Begin mt
 */

#define TIMEOUTVALUE 1000 // In microseconds
#define MCP_SIDH     0
#define MCP_SIDL     1
#define MCP_EID8     2
#define MCP_EID0     3
#define MPC_DLC      4
#define MPC_DATA     5

#define MCP_TXB_EXIDE_M 0x08 /* In TXBnSIDL                  */
#define MCP_DLC_MASK    0x0F /* 4 LSBits                     */
#define MCP_RTR_MASK    0x40 /* (1<<6) Bit 6                 */

#define MCP_RXB_RX_ANY    0x60
#define MCP_RXB_RX_EXT    0x40
#define MCP_RXB_RX_STD    0x20
#define MCP_RXB_RX_STDEXT 0x00
#define MCP_RXB_RX_MASK   0x60
#define MCP_RXB_BUKT_MASK (1 << 2)

/*
 *   Bits in the TXBnCRTL
 */
typedef enum {
    MCP_TXB_TXBUFE_M = 0x80,
    MCP_TXB_ABTF_M = 0x40,
    MCP_TXB_MLOA_M = 0x20,
    MCP_TXB_TXERR_M = 0x10,
    MCP_TXB_TXREQ_M = 0x08,
    MCP_TXB_TXIE_M = 0x04,
    MCP_TXB_TXP10_M = 0x03,
    //
    MCP_TXB_RTR_M = 0x40, /* In TXBnDLC     */
    MCP_RXB_IDE_M = 0x08, /* In RXBnSIDL   */
    MCP_RXB_RTR_M = 0x40, /* In RXBnDLC  */
    //
    MCP_STAT_TXIF_MASK = 0xA8,
    MCP_STAT_TX0IF = 0x08,
    MCP_STAT_TX1IF = 0x20,
    MCP_STAT_TX2IF = 0x80,
    MCP_STAT_TX_PENDING_MASK = 0x54,
    MCP_STAT_TX0_PENDING = 0x04,
    MCP_STAT_TX1_PENDING = 0x10,
    MCP_STAT_TX2_PENDING = 0x40,

    //
    MCP_STAT_RXIF_MASK = (0x03),
    MCP_STAT_RX0IF = (1 << 0),
    MCP_STAT_RX1IF = (1 << 1),
    // For erro detection
    MCP_EFLG_RX1OVR = (1 << 7),
    MCP_EFLG_RX0OVR = (1 << 6),
    //
    MCP_EFLG_TXBO = (1 << 5),
    MCP_EFLG_TXEP = (1 << 4),
    MCP_EFLG_RXEP = (1 << 3),
    MCP_EFLG_TXWAR = (1 << 2),
    MCP_EFLG_RXWAR = (1 << 1),
    MCP_EFLG_EWARN = (1 << 0),
    MCP_EFLG_ERRORMASK = (0xF8), /* 5 MS-Bits                    */
    //
    MCP_BxBFS_MASK = 0x30,
    MCP_BxBFE_MASK = 0x0C,
    MCP_BxBFM_MASK = 0x03,
    //
    MCP_BxRTS_MASK = 0x38,
    MCP_BxRTSM_MASK = 0x07,
} TXBnCRTL;

// CANINTF bits
typedef enum {
    MCP_RX0IF = 0x01,
    MCP_RX1IF = 0x02,
    MCP_TX0IF = 0x04,
    MCP_TX1IF = 0x08,
    MCP_TX2IF = 0x10,
    MCP_ERRIF = 0x20,
    MCP_WAKIF = 0x40,
    MCP_MERRF = 0x80,
} CANINTF;

// MCP2515 MODES
typedef enum {
    MCP_NORMAL = 0x00,
    MCP_SLEEP = 0x20,
    MCP_LOOPBACK = 0x40,
    MCP_LISTENONLY = 0x60,
    MODE_CONFIG = 0x80,
} MCP_MODE;

// MCP2515 registers
enum Registers {
    MCP_RXF0SIDH = 0x00,
    MCP_RXF0SIDL = 0x01,
    MCP_RXF0EID8 = 0x02,
    MCP_RXF0EID0 = 0x03,
    MCP_RXF1SIDH = 0x04,
    MCP_RXF1SIDL = 0x05,
    MCP_RXF1EID8 = 0x06,
    MCP_RXF1EID0 = 0x07,
    MCP_RXF2SIDH = 0x08,
    MCP_RXF2SIDL = 0x09,
    MCP_RXF2EID8 = 0x0A,
    MCP_RXF2EID0 = 0x0B,
    MCP_BFPCTRL = 0x0C,
    MCP_TXRTSCTRL = 0x0D,
    MCP_CANSTAT = 0x0E,
    MCP_CANCTRL = 0x0F,
    MCP_RXF3SIDH = 0x10,
    MCP_RXF3SIDL = 0x11,
    MCP_RXF3EID8 = 0x12,
    MCP_RXF3EID0 = 0x13,
    MCP_RXF4SIDH = 0x14,
    MCP_RXF4SIDL = 0x15,
    MCP_RXF4EID8 = 0x16,
    MCP_RXF4EID0 = 0x17,
    MCP_RXF5SIDH = 0x18,
    MCP_RXF5SIDL = 0x19,
    MCP_RXF5EID8 = 0x1A,
    MCP_RXF5EID0 = 0x1B,
    MCP_TEC = 0x1C,
    MCP_REC = 0x1D,
    MCP_RXM0SIDH = 0x20,
    MCP_RXM0SIDL = 0x21,
    MCP_RXM0EID8 = 0x22,
    MCP_RXM0EID0 = 0x23,
    MCP_RXM1SIDH = 0x24,
    MCP_RXM1SIDL = 0x25,
    MCP_RXM1EID8 = 0x26,
    MCP_RXM1EID0 = 0x27,
    MCP_CNF3 = 0x28,
    MCP_CNF2 = 0x29,
    MCP_CNF1 = 0x2A,
    MCP_CANINTE = 0x2B,
    MCP_CANINTF = 0x2C,
    MCP_EFLG = 0x2D,
    MCP_TXB0CTRL = 0x30,
    MCP_TXB0SIDH = 0x31,
    MCP_TXB0SIDL = 0x32,
    MCP_TXB0EID8 = 0x33,
    MCP_TXB0EID0 = 0x34,
    MCP_TXB0DLC = 0x35,
    MCP_TXB0DATA = 0x36,
    MCP_TXB1CTRL = 0x40,
    MCP_TXB1SIDH = 0x41,
    MCP_TXB1SIDL = 0x42,
    MCP_TXB1EID8 = 0x43,
    MCP_TXB1EID0 = 0x44,
    MCP_TXB1DLC = 0x45,
    MCP_TXB1DATA = 0x46,
    MCP_TXB2CTRL = 0x50,
    MCP_TXB2SIDH = 0x51,
    MCP_TXB2SIDL = 0x52,
    MCP_TXB2EID8 = 0x53,
    MCP_TXB2EID0 = 0x54,
    MCP_TXB2DLC = 0x55,
    MCP_TXB2DATA = 0x56,
    MCP_RXB0CTRL = 0x60,
    MCP_RXB0SIDH = 0x61,
    MCP_RXB0SIDL = 0x62,
    MCP_RXB0EID8 = 0x63,
    MCP_RXB0EID0 = 0x64,
    MCP_RXB0DLC = 0x65,
    MCP_RXB0DATA = 0x66,
    MCP_RXB1CTRL = 0x70,
    MCP_RXB1SIDH = 0x71,
    MCP_RXB1SIDL = 0x72,
    MCP_RXB1EID8 = 0x73,
    MCP_RXB1EID0 = 0x74,
    MCP_RXB1DLC = 0x75,
    MCP_RXB1DATA = 0x76
};

//  COMMAND INSTRUCTIONS
enum INSTRUCTION {
    INSTRUCTION_WRITE = 0x02,
    INSTRUCTION_READ = 0x03,
    INSTRUCTION_BITMOD = 0x05,
    INSTRUCTION_LOAD_TX0 = 0x40,
    INSTRUCTION_LOAD_TX1 = 0x42,
    INSTRUCTION_LOAD_TX2 = 0x44,
    INSTRUCTION_RTS_TX0 = 0x81,
    INSTRUCTION_RTS_TX1 = 0x82,
    INSTRUCTION_RTS_TX2 = 0x84,
    INSTRUCTION_RTS_ALL = 0x87,
    INSTRUCTION_READ_RX0 = 0x90,
    INSTRUCTION_READ_RX1 = 0x94,
    INSTRUCTION_READ_STATUS = 0xA0,
    INSTRUCTION_RX_STATUS = 0xB0,
    INSTRUCTION_RESET = 0xC0
};

// CAN ERRORS
typedef enum {
    ERROR_OK = 0,
    ERROR_FAIL = 1,
    ERROR_ALLTXBUSY = 2,
    ERROR_FAILINIT = 3,
    ERROR_FAILTX = 4,
    ERROR_NOMSG = 5,
    ERROR_GET_TXB_FTIMEOUT = 6,
    ERROR_SEND_MSG_TIMEOUT = 7,
    ERROR_WRONG_BITRATE = 8,
} ERROR_CAN;

// MCP2515 BITRATES VALUES
typedef enum {
    MCP_125KBPS,
    MCP_250KBPS,
    MCP_500KBPS,
    MCP_1000KBPS,
} MCP_BITRATE;

// MCP CLOCKS
typedef enum {
    MCP_8MHZ,
    MCP_16MHZ,
    MCP_20MHZ,
} MCP_CLOCK;

// This a struct to define the Can Frame
typedef struct {
    uint32_t canId;
    uint8_t ext;
    uint8_t req;
    uint8_t data_lenght;
    uint8_t buffer[MAX_LEN];
} CANFRAME;

// This Struct is to cinfig and work with the MCP2515 device
typedef struct {
    MCP_MODE mode;
    FuriHalSpiBusHandle* spi;
    MCP_BITRATE bitRate;
    MCP_CLOCK clck;
} MCP2515;

/*  This are the function that we can work
 *   in the main code to config, write and read
 *   the CANBUS network.
 */

// Config the paramaters of MCP2515
MCP2515* mcp_alloc(MCP_MODE mode, MCP_CLOCK clck, MCP_BITRATE bitrate);

// To init the MCP2515
ERROR_CAN mcp2515_init(MCP2515* mcp_can);

// To close the MCP2515
void deinit_mcp2515(MCP2515* mcp_can);

// free instance
void free_mcp2515(MCP2515* mcp_can);

// This is to get the status
bool mcp_get_status(FuriHalSpiBusHandle* spi, uint8_t* data);

// To set a new mode
bool set_new_mode(MCP2515* mcp_can, MCP_MODE new_mode);

// The function works to compare if the chip is in a specific mode
bool is_mode(MCP2515* mcp_can, MCP_MODE mode);

// The modes we want to work
bool set_config_mode(MCP2515* mcp_can);
bool set_normal_mode(MCP2515* mcp_can);
bool set_listen_only_mode(MCP2515* mcp_can);
bool set_sleep_mode(MCP2515* mcp_can);
bool set_loop_back_mode(MCP2515* mcp_can);

// To set the mask
void init_mask(MCP2515* mcp_can, uint8_t num_mask, uint32_t mask);

// To set the filters
void init_filter(MCP2515* mcp_can, uint8_t num_filter, uint32_t filter);

// To read and write a message
uint8_t get_error(MCP2515* mcp_can);
ERROR_CAN check_error(MCP2515* mcp_can);
ERROR_CAN check_receive(MCP2515* mcp_can);
ERROR_CAN read_can_message(MCP2515* mcp_can,
                           CANFRAME* frame); // Read a CAN BUS message

ERROR_CAN is_this_bitrate(MCP2515* mcp_can, MCP_BITRATE bitrate);

ERROR_CAN send_can_frame(MCP2515* mcp_can,
                         CANFRAME* frame); // Send a CANBUS Frame

#endif
