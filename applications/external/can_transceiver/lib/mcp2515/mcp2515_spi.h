// Header for mcp2515_spi.c.

#ifndef MCP2515_SPI_H
#define MCP2515_SPI_H

// ==== MACROS ====

#define MCP2515_ASSERT(x)	if (!(x))	\
				{		\
					FURI_LOG_I("ASSERT-FAILED", "%s %d", __FILE__, __LINE__);\
				}
//					furi_check(false);


// ==== CONSTANTS ====

#define u32MCP2515_SPI_TIMEOUT		(uint32_t)1000

#define u8MCP2515_CMD_RESET		(uint8_t)0xC0
#define u8MCP2515_CMD_READ		(uint8_t)0x03
#define u8MCP2515_CMD_READ_RX_BUFFER(n)	((uint8_t)0x90 | (((uint8_t)n & 0x03) << 1))
#define u8MCP2515_CMD_WRITE		(uint8_t)0x02
#define u8MCP2515_CMD_LOAD_TX_BUFFER(n)	((uint8_t)0x40 | ((uint8_t)n & 0x07))
#define u8MCP2515_CMD_RTS		(uint8_t)0x80
#define u8MCP2515_CMD_RTS_TXB0		(u8MCP2515_CMD_RTS | (uint8_t)0x01)
#define u8MCP2515_CMD_RTS_TXB1		(u8MCP2515_CMD_RTS | (uint8_t)0x02)
#define u8MCP2515_CMD_RTS_TXB2		(u8MCP2515_CMD_RTS | (uint8_t)0x04)
#define u8MCP2515_CMD_READ_STATUS	(uint8_t)0xA0
#define u8MCP2515_CMD_RX_STATUS		(uint8_t)0xB0
#define u8MCP2515_CMD_BIT_MODIFY	(uint8_t)0x05

#define u8MCP2515_ADDR_BFPCTRL		(uint8_t)0x0C
#define u8MCP2515_ADDR_TXRTSCTRL	(uint8_t)0x0D
#define u8MCP2515_ADDR_CANSTAT		(uint8_t)0x7E
#define u8MCP2515_ADDR_CANCTRL		(uint8_t)0x7F
#define u8MCP2515_ADDR_TEC		(uint8_t)0x1C
#define u8MCP2515_ADDR_REC		(uint8_t)0x1D
#define u8MCP2515_ADDR_CNF3		(uint8_t)0x28
#define u8MCP2515_ADDR_CNF2		(uint8_t)0x29
#define u8MCP2515_ADDR_CNF1		(uint8_t)0x2A
#define u8MCP2515_ADDR_CANINTE		(uint8_t)0x2B
#define u8MCP2515_ADDR_CANINTF		(uint8_t)0x2C
#define u8MCP2515_ADDR_EFLG		(uint8_t)0x2D
#define u8MCP2515_ADDR_TXB0CTRL		(uint8_t)0x30
#define u8MCP2515_ADDR_TXB1CTRL		(uint8_t)0x40
#define u8MCP2515_ADDR_TXB2CTRL		(uint8_t)0x50
#define u8MCP2515_ADDR_RXB0CTRL		(uint8_t)0x60
#define u8MCP2515_ADDR_RXB1CTRL		(uint8_t)0x70
#define u8MCP2515_ADDR_TXBnCTRL(n)	((((uint8_t)n << 4) & 0x30) + (uint8_t)0x30)
#define u8MCP2515_ADDR_TXBnSIDH(n)	(u8MCP2515_ADDR_TXBnCTRL(n) + (uint8_t)0x01)
#define u8MCP2515_ADDR_TXBnSIDL(n)	(u8MCP2515_ADDR_TXBnCTRL(n) + (uint8_t)0x02)
#define u8MCP2515_ADDR_TXBnEID8(n)	(u8MCP2515_ADDR_TXBnCTRL(n) + (uint8_t)0x03)
#define u8MCP2515_ADDR_TXBnEID0(n)	(u8MCP2515_ADDR_TXBnCTRL(n) + (uint8_t)0x04)
#define u8MCP2515_ADDR_TXBnDLC(n)	(u8MCP2515_ADDR_TXBnCTRL(n) + (uint8_t)0x05)
#define u8MCP2515_ADDR_TXBnDm(n, m)	(u8MCP2515_ADDR_TXBnCTRL(n) + (uint8_t)0x06 + (uint8_t)m)
#define u8MCP2515_ADDR_RXBnCTRL(n)	((((uint8_t)n << 4) & 0x10) + (uint8_t)0x60)
#define u8MCP2515_ADDR_RXBnSIDH(n)	(u8MCP2515_ADDR_RXBnCTRL(n) + (uint8_t)0x01)
#define u8MCP2515_ADDR_RXBnSIDL(n)	(u8MCP2515_ADDR_RXBnCTRL(n) + (uint8_t)0x02)
#define u8MCP2515_ADDR_RXBnEID8(n)	(u8MCP2515_ADDR_RXBnCTRL(n) + (uint8_t)0x03)
#define u8MCP2515_ADDR_RXBnEID0(n)	(u8MCP2515_ADDR_RXBnCTRL(n) + (uint8_t)0x04)
#define u8MCP2515_ADDR_RXBnDLC(n)	(u8MCP2515_ADDR_RXBnCTRL(n) + (uint8_t)0x05)
#define u8MCP2515_ADDR_RXBnDm(n, m)	(u8MCP2515_ADDR_RXBnCTRL(n) + (uint8_t)0x06 + (uint8_t)m)

#define u8MCP2515_MODE_MASK		(uint8_t)0xE0
#define u8MCP2515_MODE_NORMAL		(uint8_t)0x00
#define u8MCP2515_MODE_SLEEP		(uint8_t)0x20
#define u8MCP2515_MODE_LOOPBACK		(uint8_t)0x40
#define u8MCP2515_MODE_LISTENONLY	(uint8_t)0x60
#define u8MCP2515_MODE_CONFIGURATION	(uint8_t)0x80

#define u8MCP2515_IRQ_MERR		(uint8_t)0x80
#define u8MCP2515_IRQ_WAKI		(uint8_t)0x40
#define u8MCP2515_IRQ_ERRI		(uint8_t)0x20
#define u8MCP2515_IRQ_TX2I		(uint8_t)0x10
#define u8MCP2515_IRQ_TX1I		(uint8_t)0x08
#define u8MCP2515_IRQ_TX0I		(uint8_t)0x04
#define u8MCP2515_IRQ_RX1I		(uint8_t)0x02
#define u8MCP2515_IRQ_RX0I		(uint8_t)0x01

#define u8MCP2515_ICOD_MASK		(uint8_t)0x0E
#define u8MCP2515_ICOD_NONE		(uint8_t)0x00
#define u8MCP2515_ICOD_ERROR		(uint8_t)0x02
#define u8MCP2515_ICOD_WAKEUP		(uint8_t)0x04
#define u8MCP2515_ICOD_TXB0		(uint8_t)0x06
#define u8MCP2515_ICOD_TXB1		(uint8_t)0x08
#define u8MCP2515_ICOD_TXB2		(uint8_t)0x0A
#define u8MCP2515_ICOD_RXB0		(uint8_t)0x0C
#define u8MCP2515_ICOD_RXB1		(uint8_t)0x0E

#define u8MCP2515_EFLG_RX1OVR		(uint8_t)0x80
#define u8MCP2515_EFLG_RX0OVR		(uint8_t)0x40
#define u8MCP2515_EFLG_TXBO		(uint8_t)0x20
#define u8MCP2515_EFLG_TXEP		(uint8_t)0x10
#define u8MCP2515_EFLG_RXEP		(uint8_t)0x08
#define u8MCP2515_EFLG_TXWAR		(uint8_t)0x04
#define u8MCP2515_EFLG_RXWAR		(uint8_t)0x02
#define u8MCP2515_EFLG_EWARN		(uint8_t)0x01

#define u8MCP2515_CANCTRL_ABAT		(uint8_t)0x10
#define u8MCP2515_TXBnCTRL_TXREQ	(uint8_t)0x08

#define u32MCP2515_ID_STD_MASK		(uint32_t)0x000007FF
#define u32MCP2515_ID_EXT_MASK		(uint32_t)0x1FFFFFFF


// ==== PROTOTYPES ====

// Base commands
bool MCP2515_RESET();
bool MCP2515_READ(uint8_t u8Addr, uint8_t* u8pReadBuf, uint8_t u8ReadLen);
bool MCP2515_READ_RX_BUFFER(uint8_t u8Buf, uint8_t* u8pReadBuf, uint8_t u8ReadLen);
bool MCP2515_WRITE(uint8_t u8Addr, uint8_t* u8pWriteBuf, uint8_t u8WriteLen);
bool MCP2515_LOAD_TX_BUFFER(uint8_t u8Buf, uint8_t* u8pWriteBuf, uint8_t u8WriteLen);
bool MCP2515_RTS(uint8_t u8Buffers);
bool MCP2515_READ_STATUS(uint8_t* u8pStatusByte);
bool MCP2515_RX_STATUS(uint8_t* u8pStatusByte);
bool MCP2515_BIT_MODIFY(uint8_t u8Addr, uint8_t u8Mask, uint8_t u8Data);

// Complex commands
void MCP2515_Init();
void MCP2515_SetMode(uint8_t u8Mode);
uint8_t MCP2515_GetMode();
void MCP2515_SetCNFRegs(uint8_t u8CNF1, uint8_t u8CNF2, uint8_t u8CNF3);
void MCP2515_SetOneShotTxEnabled(uint8_t bEnable);
void MCP2515_SetRXB0Rollover(uint8_t bEnable);
void MCP2515_ClearOverflowErrorFlags();
void MCP2515_EnableInterrupts(uint8_t u8Interrupts);
void MCP2515_DisableInterrupts(uint8_t u8Interrupts);
void MCP2515_ClearInterruptFlags(uint8_t u8Interrupts);
void MCP2515_ReadRXB(uint8_t u8Buffer, uint32_t* u32pFrameID, uint8_t* u8pRxData, uint8_t* u8pDataLen);
void MCP2515_LoadTXB(uint8_t u8Buffer, uint32_t u32FrameID, uint8_t* u8pTxData, uint8_t u8DataLen, bool bSendNow);
void MCP2515_AbortAllTransmissions();
// Interrupt handling and the like is not this file's problem.


#endif	// MCP2515_SPI_H
