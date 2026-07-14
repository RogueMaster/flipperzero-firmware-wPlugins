// Low-level MCP2515 driver.

// ==== INCLUDES ====

// Furi
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi_config.h>

// Library-specific
#include "mcp2515_spi.h"


// ==== PRIVATE FUNCTIONS ====

/*
	NOTE REGARDING furi_hal_spi_bus_rx():
		This function seems to transmit the data in the receive buffer at the same time as receiving data into it...
		Bit weird considering it's not necessary as one can pass a null Rx buffer, but whatever.
		Not likely to be problematic here, but worth knowing for the future.
*/

bool MCP2515_SPI_PRIVATE_TxRx(uint8_t u8Cmd, uint8_t* u8pTxBuf, uint8_t u8TxLen, uint8_t* u8pRxBuf, uint8_t u8RxLen)
{
	bool bSucceeded;

	bSucceeded = false;
	furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);

	if ( !furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, &u8Cmd, 1, u32MCP2515_SPI_TIMEOUT) )
		goto MCP2515_SPI_PRIVATE_TxRx_GTFO;
	if ( u8pTxBuf && !furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, u8pTxBuf, u8TxLen, u32MCP2515_SPI_TIMEOUT) )
		goto MCP2515_SPI_PRIVATE_TxRx_GTFO;
	if ( u8pRxBuf && !furi_hal_spi_bus_rx(&furi_hal_spi_bus_handle_external, u8pRxBuf, u8RxLen, u32MCP2515_SPI_TIMEOUT) )
		goto MCP2515_SPI_PRIVATE_TxRx_GTFO;
	bSucceeded = true;

	MCP2515_SPI_PRIVATE_TxRx_GTFO:
	furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
	return bSucceeded;
}


// ==== PUBLIC FUNCTIONS ====

bool MCP2515_RESET()
{
	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_RESET, NULL, 0, NULL, 0);
}
bool MCP2515_READ(uint8_t u8Addr, uint8_t* u8pReadBuf, uint8_t u8ReadLen)
{
	MCP2515_ASSERT(u8pReadBuf != NULL);
	MCP2515_ASSERT(u8ReadLen > 0);

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_READ, &u8Addr, 1, u8pReadBuf, u8ReadLen);
}
bool MCP2515_READ_RX_BUFFER(uint8_t u8Buf, uint8_t* u8pReadBuf, uint8_t u8ReadLen)
{
	MCP2515_ASSERT(u8pReadBuf != NULL);
	MCP2515_ASSERT(u8ReadLen > 0);
	MCP2515_ASSERT(u8Buf <= 0x3);

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_READ_RX_BUFFER(u8Buf), NULL, 0, u8pReadBuf, u8ReadLen);
}
bool MCP2515_WRITE(uint8_t u8Addr, uint8_t* u8pWriteBuf, uint8_t u8WriteLen)
{
	uint8_t u8Cmd;
	bool bSucceeded;

	MCP2515_ASSERT(u8pWriteBuf != NULL);
	MCP2515_ASSERT(u8WriteLen > 0);

	u8Cmd = u8MCP2515_CMD_WRITE;
	bSucceeded = false;
	furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);

	if ( !furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, &u8Cmd, 1, u32MCP2515_SPI_TIMEOUT) )
		goto MCP2515_WRITE_GTFO;
	if ( !furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, &u8Addr, 1, u32MCP2515_SPI_TIMEOUT) )
		goto MCP2515_WRITE_GTFO;
	if ( !furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, u8pWriteBuf, u8WriteLen, u32MCP2515_SPI_TIMEOUT) )
		goto MCP2515_WRITE_GTFO;
	bSucceeded = true;

	MCP2515_WRITE_GTFO:
	furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
	return bSucceeded;
}
bool MCP2515_LOAD_TX_BUFFER(uint8_t u8Buf, uint8_t* u8pWriteBuf, uint8_t u8WriteLen)
{
	MCP2515_ASSERT(u8pWriteBuf != NULL);
	MCP2515_ASSERT(u8WriteLen > 0);
	MCP2515_ASSERT(u8Buf <= 0x5);

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_LOAD_TX_BUFFER(u8Buf), NULL, 0, u8pWriteBuf, u8WriteLen);
}
bool MCP2515_RTS(uint8_t u8Buffers)
{
	// There would be a check for u8Buffers != 0 here, but maybe a no-op command will prove to be useful. You never know.
	MCP2515_ASSERT((u8Buffers & ~0x7) == 0);

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_RTS | u8Buffers, NULL, 0, NULL, 0);
}
bool MCP2515_READ_STATUS(uint8_t* u8pStatusByte)
{
	MCP2515_ASSERT(u8pStatusByte != NULL);

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_READ_STATUS, NULL, 0, u8pStatusByte, 1);
}
bool MCP2515_RX_STATUS(uint8_t* u8pStatusByte)
{
	MCP2515_ASSERT(u8pStatusByte != NULL);

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_RX_STATUS, NULL, 0, u8pStatusByte, 1);
}
bool MCP2515_BIT_MODIFY(uint8_t u8Addr, uint8_t u8Mask, uint8_t u8Data)
{
	uint8_t u8pBuf[3];

	u8pBuf[0] = u8Addr;
	u8pBuf[1] = u8Mask;
	u8pBuf[2] = u8Data;

	return MCP2515_SPI_PRIVATE_TxRx(u8MCP2515_CMD_BIT_MODIFY, u8pBuf, 3, NULL, 0);
}

void MCP2515_Init()
{
	MCP2515_RESET();
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_RXB0CTRL, 0xFF, 0x60);	// No masking/filtering or rollover.
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_RXB1CTRL, 0xFF, 0x60);	// No masking/filtering or rollover.
	// Stay in config mode for now, there's no telling which is more appropriate contextually.
}

void MCP2515_SetMode(uint8_t u8Mode)
{
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_CANCTRL, u8MCP2515_MODE_MASK, u8Mode);
}

uint8_t MCP2515_GetMode()
{
	uint8_t u8CANCTRL;

	MCP2515_READ(u8MCP2515_ADDR_CANSTAT, &u8CANCTRL, 1);

	return u8CANCTRL & u8MCP2515_MODE_MASK;
}

void MCP2515_SetCNFRegs(uint8_t u8CNF1, uint8_t u8CNF2, uint8_t u8CNF3)
{
	uint8_t u8PrevMode;

	u8PrevMode = MCP2515_GetMode();
	MCP2515_SetMode(u8MCP2515_MODE_CONFIGURATION);
	MCP2515_WRITE(u8MCP2515_ADDR_CNF1, &u8CNF1, 1);
	MCP2515_WRITE(u8MCP2515_ADDR_CNF2, &u8CNF2, 1);
	MCP2515_WRITE(u8MCP2515_ADDR_CNF3, &u8CNF3, 1);
	MCP2515_SetMode(u8PrevMode);
}

void MCP2515_SetOneShotTxEnabled(uint8_t bEnable)
{
	if (bEnable)
		MCP2515_BIT_MODIFY(u8MCP2515_ADDR_CANCTRL, 0x08, 0xFF);
	else
		MCP2515_BIT_MODIFY(u8MCP2515_ADDR_CANCTRL, 0x08, 0x00);
}

void MCP2515_SetRXB0Rollover(uint8_t bEnable)
{
	if (bEnable)
		MCP2515_BIT_MODIFY(u8MCP2515_ADDR_RXB0CTRL, 0x04, 0xFF);
	else
		MCP2515_BIT_MODIFY(u8MCP2515_ADDR_RXB0CTRL, 0x04, 0x00);
}

void MCP2515_ClearOverflowErrorFlags()
{
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_EFLG, 0xC0, 0x00);
}

void MCP2515_EnableInterrupts(uint8_t u8InterruptsToEnable)
{
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_CANINTE, u8InterruptsToEnable, 0xFF);
}

void MCP2515_DisableInterrupts(uint8_t u8InterruptsToDisable)
{
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_CANINTE, u8InterruptsToDisable, 0x00);
}

void MCP2515_ClearInterruptFlags(uint8_t u8InterruptFlagsToClear)
{
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_CANINTF, u8InterruptFlagsToClear, 0x00);
}

void MCP2515_ReadRXB(uint8_t u8Buffer, uint32_t* u32pFrameID, uint8_t* u8pRxData, uint8_t* u8pDataLen)
{
	uint8_t u8pInfoBuf[5];
	uint8_t* u8pFrameIDBuf;

	MCP2515_ASSERT(u8Buffer <= 1);

	// Get the RXnSIDH, RXnSIDL, RXnEID8, RXnEID0 and RXnDLC buffers in that order.
	// NOTE: This will also clear the respective flag in CANINTF.
	if (u8Buffer == 1)
	{
		MCP2515_READ_RX_BUFFER(2, u8pInfoBuf, 5);
	}
	else
	{
		MCP2515_READ_RX_BUFFER(0, u8pInfoBuf, 5);
	}

	// Set the data length and frame ID. Some work needed for the frame ID.
	// NOTE: The most significant bit of u32pFrameID is 1 for extended frames, 0 for standard.
	*u8pDataLen = u8pInfoBuf[4] & 0x0F;
	if (*u8pDataLen > 8)	// Can't trust even peripherals these days. Geez.
		*u8pDataLen = 0;
	*u32pFrameID = 0;
	u8pFrameIDBuf = (uint8_t*)u32pFrameID;
	u8pFrameIDBuf[3] = u8pInfoBuf[0];
	u8pFrameIDBuf[2] = u8pInfoBuf[1] & 0xE0;
	if (u8pInfoBuf[1] & 0x08)
	{
		// Extended frame.
		*u32pFrameID >>= 3;
		u8pFrameIDBuf[2] |= u8pInfoBuf[1] & 0x03;
		u8pFrameIDBuf[1] = u8pInfoBuf[2];
		u8pFrameIDBuf[0] = u8pInfoBuf[3];
		*u32pFrameID &= u32MCP2515_ID_EXT_MASK;
		*u32pFrameID |= 0x80000000;
	}
	else
	{
		// Standard frame.
		*u32pFrameID >>= 21;
		*u32pFrameID &= u32MCP2515_ID_STD_MASK;
	}
	if (u8pInfoBuf[1] & 0x10)
	{
		// Remote frame.
		*u32pFrameID |= 0x40000000;
	}

	// Now read the rest of the buffer, if there's any data to read.
	if (*u8pDataLen > 0)
	{
		if (u8Buffer == 1)
		{
			MCP2515_READ_RX_BUFFER(3, u8pRxData, *u8pDataLen);
		}
		else
		{
			MCP2515_READ_RX_BUFFER(1, u8pRxData, *u8pDataLen);
		}
	}
}

void MCP2515_LoadTXB(uint8_t u8Buffer, uint32_t u32FrameID, uint8_t* u8pTxData, uint8_t u8DataLen, bool bSendNow)
{
	uint8_t u8pInfoBuf[5];

	MCP2515_ASSERT(u8Buffer <= 2);

	// Set the frame's ID. If the most significant bit is 1, it's extended, otherwise it's standard.
	if (u32FrameID & 0x80000000)
	{
		u32FrameID &= 0x1FFFFFFF;
		u8pInfoBuf[0] = (uint8_t)(u32FrameID >> 21);
		u8pInfoBuf[1] = (uint8_t)(u32FrameID >> 13);
		u8pInfoBuf[1] |= 0x08;
		u8pInfoBuf[1] |= (uint8_t)(u32FrameID >> 16) & 0x3;
		u8pInfoBuf[2] = (uint8_t)(u32FrameID >> 8);
		u8pInfoBuf[3] = (uint8_t)u32FrameID;
	}
	else
	{
		u32FrameID &= 0x000007FF;
		u8pInfoBuf[0] = (uint8_t)(u32FrameID >> 3);
		u8pInfoBuf[1] = (uint8_t)(u32FrameID << 5);
		u8pInfoBuf[2] = 0;
		u8pInfoBuf[3] = 0;
	}
	u8pInfoBuf[4] = u8DataLen & 0x0F;

	// Now write the buffer and, if bSendNow is set, send it!
	if (u8Buffer == 2)
	{
		MCP2515_LOAD_TX_BUFFER(4, u8pInfoBuf, 5);
		MCP2515_LOAD_TX_BUFFER(5, u8pTxData, u8DataLen);
	}
	else if (u8Buffer == 1)
	{
		MCP2515_LOAD_TX_BUFFER(2, u8pInfoBuf, 5);
		MCP2515_LOAD_TX_BUFFER(3, u8pTxData, u8DataLen);
	}
	else
	{
		MCP2515_LOAD_TX_BUFFER(0, u8pInfoBuf, 5);
		MCP2515_LOAD_TX_BUFFER(1, u8pTxData, u8DataLen);
	}
	if (bSendNow)
		MCP2515_RTS(1 << u8Buffer);
}

void MCP2515_AbortAllTransmissions()
{
	uint8_t u8TXB0CTRL, u8TXB1CTRL, u8TXB2CTRL;

	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_TXB0CTRL, u8MCP2515_TXBnCTRL_TXREQ, 0x00);
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_TXB1CTRL, u8MCP2515_TXBnCTRL_TXREQ, 0x00);
	MCP2515_BIT_MODIFY(u8MCP2515_ADDR_TXB2CTRL, u8MCP2515_TXBnCTRL_TXREQ, 0x00);

	// Wait until all transmissions have been aborted, then reset the ABAT bit.
	do
	{
		MCP2515_READ(u8MCP2515_ADDR_TXB0CTRL, &u8TXB0CTRL, 1);
		MCP2515_READ(u8MCP2515_ADDR_TXB1CTRL, &u8TXB1CTRL, 1);
		MCP2515_READ(u8MCP2515_ADDR_TXB2CTRL, &u8TXB2CTRL, 1);
	}
	while ((u8TXB0CTRL & u8MCP2515_TXBnCTRL_TXREQ) != 0
		|| (u8TXB1CTRL & u8MCP2515_TXBnCTRL_TXREQ) != 0
		|| (u8TXB2CTRL & u8MCP2515_TXBnCTRL_TXREQ) != 0);

	FURI_LOG_I("CAN-Transceiver", "Aborted: %02hhX %02hhX %02hhX", u8TXB0CTRL, u8TXB1CTRL, u8TXB2CTRL);
}
