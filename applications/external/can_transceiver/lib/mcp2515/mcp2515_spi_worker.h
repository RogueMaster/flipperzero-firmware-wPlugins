// Header for mcp2515_spi_worker.c.

#ifndef MCP2515_SPI_WORKER_H
#define MCP2515_SPI_WORKER_H

// ==== CONSTANTS ====

#define u32MCP2515_WORKER_TASK_NONE		(uint32_t)0x00000000
#define u32MCP2515_WORKER_TASK_KILLTHREAD	(uint32_t)0x00000001
#define u32MCP2515_WORKER_TASK_INIT		(uint32_t)0x00000002
#define u32MCP2515_WORKER_TASK_RX		(uint32_t)0x00000004
#define u32MCP2515_WORKER_TASK_TX		(uint32_t)0x00000008
#define u32MCP2515_WORKER_TASK_WAIT_FOR_BUS_ON	(uint32_t)0x00000010
#define u32MCP2515_WORKER_TASK_DETECT_BAUD_RATE	(uint32_t)0x00000020
#define u32MCP2515_WORKER_TASK_ALL		(uint32_t)0x0000003F

#define u32MCP2515_WORKER_SIGNAL_IRQ		(uint32_t)0x00000040

#define u32MCP2515_WORKER_STACK_SIZE	(uint32_t)2048	// TODO: Check stack usage and optimize it. Even this might be a bit much.


// ==== TYPEDEFS ====

typedef void (*MCP2515Worker_Callback)(void* pContext);

typedef struct {
	FuriThread* pWorkerThread;
	volatile uint32_t u32CurTask;	// MUST be volatile as other threads may use this to check thread status!
	MCP2515Worker_Callback pTaskFinishedCallback;
	void* pTaskFinishedCallbackContext;

	// General state info
	uint8_t u8CNF1;
	uint8_t u8CNF2;
	uint8_t u8CNF3;
	volatile bool bWaitingForIRQ;
	volatile bool bIRQInvoked;

	// Receiving
	// Input
	bool bListenOnly;
	MCP2515Worker_Callback pRxCallback;
	void* pRxCallbackContext;
	// Output
	uint8_t u8REC;
	bool bErrorOccurred;
	// FIXME: I'm wasting space by including these. Why the hell don't I just reuse the vars from the Tx side?
	uint32_t u32FrameID_Rx;
	uint8_t u8FrameDataLen_Rx;
	uint8_t u8pFrameData_Rx[8];

	// Transmitting
	// Input
	uint32_t u32FrameID;
	uint8_t u8FrameIDLen;	// In bits. Not to be confused with CANTransceiverGlobals.u8FrameIDLen; that takes different values!
	uint8_t u8FrameDataLen;
	uint8_t u8pFrameData[8];
	bool bOneShot;
	// Output
	uint8_t u8TEC;		// The scene must update the view with this value!
	bool bBusOff;		// Updated by the Tx and wait for bus-on tasks.
} MCP2515Worker;


// ==== PUBLIC FUNCTIONS ====

void MCP2515Worker_StartThread(MCP2515Worker* pWorker);
void MCP2515Worker_StopWorkerThread(MCP2515Worker* pWorker);
bool MCP2515Worker_GiveTask(MCP2515Worker* pWorker, uint32_t u32Task, MCP2515Worker_Callback pCallback, void* pCallbackContext);
MCP2515Worker* MCP2515Worker_Alloc();
void MCP2515Worker_Free(MCP2515Worker* pWorker);


#endif	// MCP2515_SPI_WORKER_H
