// Parallel-running worker thread for interacting with the MCP2515.

// TODO: When dealing with interrupts, check out flipperzero-firmware/targets/f7/furi_hal/furi_hal_gpio.h.

// ==== INCLUDES ====

// Furi
#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

// App-specific
#include "mcp2515_spi.h"
#include "mcp2515_spi_worker.h"


// ==== CONSTANTS ====

const GpioPin* pIRQPin = &gpio_ext_pb2;


// ==== HANDLERS AND CALLBACKS ====

static void MCP2515Worker_InterruptCallback(void* pContext)
{
	MCP2515Worker* pWorker;

	MCP2515_ASSERT(pContext != NULL);
	pWorker = pContext;

	if (pWorker->bWaitingForIRQ)
	{
		pWorker->bIRQInvoked = true;
		pWorker->bWaitingForIRQ = false;
		furi_thread_flags_set(furi_thread_get_id(pWorker->pWorkerThread), u32MCP2515_WORKER_SIGNAL_IRQ);
	}
}


// ==== PRIVATE FUNCTIONS ====

// TODO: Should add a means of aborting a task without killing the runner thread.
static bool MCP2515Worker_IsStopping()
{
	return (furi_thread_flags_get() & u32MCP2515_WORKER_TASK_KILLTHREAD) == u32MCP2515_WORKER_TASK_KILLTHREAD;
}

static void MCP2515Worker_TaskInit(MCP2515Worker* pWorker)
{
	MCP2515_ASSERT(pWorker != NULL);

	MCP2515_Init();
	MCP2515_SetCNFRegs(pWorker->u8CNF1, pWorker->u8CNF2, pWorker->u8CNF3);
}

static void MCP2515Worker_TaskRx(MCP2515Worker* pWorker)
{
	uint8_t u8CANINTF, u8EFLG;
	uint8_t u8FlagsToReset;

	MCP2515_ASSERT(pWorker != NULL);
	MCP2515_ASSERT(pWorker->pRxCallback != NULL);

	// Set the transceiver to normal or listen-only mode, then enable relevant interrupts.
	if (pWorker->bListenOnly)
	{
		// Listen-only mode emits no ACKs or errors.
		MCP2515_SetMode(u8MCP2515_MODE_LISTENONLY);
		while (MCP2515_GetMode() != u8MCP2515_MODE_LISTENONLY);
	}
	else
	{
		MCP2515_SetMode(u8MCP2515_MODE_NORMAL);
		while (MCP2515_GetMode() != u8MCP2515_MODE_NORMAL);
	}
	MCP2515_EnableInterrupts(u8MCP2515_IRQ_ERRI | u8MCP2515_IRQ_MERR | u8MCP2515_IRQ_RX0I | u8MCP2515_IRQ_RX1I);

	// Enable message rollover to RXB1, to reduce the risk of missed messages.
	// Then constantly wait for messages until the thread stops.
	// FIXME: This is WAY too easy to overwhelm. Can I mitigate this by increasing the SPI bus speed?
	MCP2515_SetRXB0Rollover(true);
	FURI_CRITICAL_ENTER();
	pWorker->bIRQInvoked = false;
	pWorker->bWaitingForIRQ = true;
	FURI_CRITICAL_EXIT();
	do
	{
		pWorker->bErrorOccurred = false;
#if 0
		while (!pWorker->bIRQInvoked && !MCP2515Worker_IsStopping())
		{
			(void)pWorker->bIRQInvoked;
		}
#else
		if (!pWorker->bIRQInvoked && !MCP2515Worker_IsStopping())
		{
			furi_thread_flags_wait(u32MCP2515_WORKER_SIGNAL_IRQ | u32MCP2515_WORKER_TASK_KILLTHREAD,
						FuriFlagWaitAny | FuriFlagNoClear, FuriWaitForever);

			// Clear the IRQ signal, but don't clear the kill signal if activated! Main loop has to handle it.
			furi_thread_flags_clear(u32MCP2515_WORKER_SIGNAL_IRQ);
		}
#endif
		// Immediately start waiting for interrupts again, in case a message gets received and rolled over while processing.
		FURI_CRITICAL_ENTER();
		pWorker->bIRQInvoked = false;
		pWorker->bWaitingForIRQ = true;
		FURI_CRITICAL_EXIT();

		// Handle the message interrupt conditions.
		// Note that if a message is placed into RXB1, or some other IRQ happens, it won't get handled until the next loop.
		MCP2515_READ(u8MCP2515_ADDR_CANINTF, &u8CANINTF, 1);
		u8FlagsToReset = 0;
		if (u8CANINTF & u8MCP2515_IRQ_RX0I)
		{
			// Received a message in RXB0. Grab it, then call the callback to process it.
			MCP2515_ReadRXB(0, &pWorker->u32FrameID_Rx, pWorker->u8pFrameData_Rx, &pWorker->u8FrameDataLen_Rx);
			pWorker->pRxCallback(pWorker->pRxCallbackContext);
			u8FlagsToReset |= u8MCP2515_IRQ_RX0I;
			//FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Rx - RX0I");
		}
		if (u8CANINTF & u8MCP2515_IRQ_RX1I)
		{
			// Received a message in RXB1. Grab it, then call the callback to process it.
			MCP2515_ReadRXB(1, &pWorker->u32FrameID_Rx, pWorker->u8pFrameData_Rx, &pWorker->u8FrameDataLen_Rx);
			pWorker->pRxCallback(pWorker->pRxCallbackContext);
			u8FlagsToReset |= u8MCP2515_IRQ_RX1I;
			//FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Rx - RX1I");
		}
		if (u8CANINTF & u8MCP2515_IRQ_ERRI)
		{
			// If we got an error that's described in EFLG, we don't need to do anything unless RX0OVR or RX1OVR is set.
			// Let's assume for simplicity's sake that one or both are set, and clear them.
			MCP2515_READ(u8MCP2515_ADDR_EFLG, &u8EFLG, 1);
			MCP2515_ClearOverflowErrorFlags();
			u8FlagsToReset |= u8MCP2515_IRQ_ERRI;
			//FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Rx - ERRI %hhx", u8EFLG);
		}
		if (u8CANINTF & u8MCP2515_IRQ_MERR)
		{
			// Receive error. Can't be transmit since we're not transmitting.
			// Update the REC and call the callback to log the error having happened.
			MCP2515_READ(u8MCP2515_ADDR_REC, &pWorker->u8REC, 1);
			pWorker->bErrorOccurred = true;
			pWorker->pRxCallback(pWorker->pRxCallbackContext);
			u8FlagsToReset |= u8MCP2515_IRQ_MERR;
			//FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Rx - MERR %hhd", pWorker->u8REC);
		}

		// Reset the interrupt flags.
		MCP2515_ClearInterruptFlags(0xFF);	// FIXME: Eh, let's just clear 'em all for now. //(u8FlagsToReset);
	}
	while (!MCP2515Worker_IsStopping());
	pWorker->bWaitingForIRQ = false;

	// Done. Disable interrupts.
	MCP2515_DisableInterrupts(u8MCP2515_IRQ_ERRI | u8MCP2515_IRQ_MERR | u8MCP2515_IRQ_RX0I | u8MCP2515_IRQ_RX1I);
}

static void MCP2515Worker_TaskTx(MCP2515Worker* pWorker)
{
	uint8_t u8CANSTAT, u8CANCTRL, u8CANINTF, u8EFLG;
	uint8_t u8FlagsToReset;

	MCP2515_ASSERT(pWorker != NULL);

	// Set the transceiver to normal mode and enable interrupts.
	MCP2515_SetMode(u8MCP2515_MODE_NORMAL);
	while (MCP2515_GetMode() != u8MCP2515_MODE_NORMAL);
	MCP2515_READ(u8MCP2515_ADDR_CANINTF, &u8CANINTF, 1);
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Tx - IRQ flags before enabling interrupts: %02hhX", u8CANINTF);
	pWorker->bIRQInvoked = false;
	pWorker->bWaitingForIRQ = true;
	MCP2515_EnableInterrupts(u8MCP2515_IRQ_ERRI | u8MCP2515_IRQ_TX0I | u8MCP2515_IRQ_MERR);
	if (pWorker->bOneShot)
	{
		MCP2515_SetOneShotTxEnabled(true);
	}
	else
	{
		MCP2515_SetOneShotTxEnabled(false);
	}

	// Reset TEC and bus-off indicators. We'll pull them straight from the device once we're done.
	pWorker->u8TEC = 0;
	pWorker->bBusOff = false;

	// Send the message and wait until it's sent or bus off occurs. In one-shot mode, also stop on message transmit failure.
	// Also quit if the thread is stopping.
	if (pWorker->u8FrameIDLen == 29)
	{
		MCP2515_LoadTXB(0, pWorker->u32FrameID | 0x80000000, pWorker->u8pFrameData, pWorker->u8FrameDataLen, true);
	}
	else if (pWorker->u8FrameIDLen == 11)
	{
		MCP2515_LoadTXB(0, pWorker->u32FrameID, pWorker->u8pFrameData, pWorker->u8FrameDataLen, true);
	}
	else
	{
		MCP2515_ASSERT(false);
	}
	while (!pWorker->bIRQInvoked && !MCP2515Worker_IsStopping());

	// Clear the interrupt conditions.
	// FYI, can't do this in an ISR as it'll cause a crash. Might be because the IRQ happens in the middle of a SPI transfer.
	// Scenarios:
	//	- Successful transmission
	//	- Transmit error in one-shot mode
	//	- Transmit error, gone error-passive
	//	- Transmit error, gone bus-off
	u8FlagsToReset = 0;
	MCP2515_READ(u8MCP2515_ADDR_CANSTAT, &u8CANSTAT, 1);
	u8CANSTAT &= u8MCP2515_ICOD_MASK;
	MCP2515_READ(u8MCP2515_ADDR_CANCTRL, &u8CANCTRL, 1);
	MCP2515_READ(u8MCP2515_ADDR_CANINTF, &u8CANINTF, 1);
	MCP2515_READ(u8MCP2515_ADDR_EFLG, &u8EFLG, 1);
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Tx - Regs: %02hhX %02hhX %02hhX %02hhX", u8CANSTAT, u8CANCTRL, u8CANINTF,
			u8EFLG);
	if (u8CANSTAT == u8MCP2515_ICOD_TXB0)
	{
		// Break if message transmitted successfully.
		u8FlagsToReset |= u8MCP2515_IRQ_TX0I;
	}
	if (((u8CANINTF & u8MCP2515_IRQ_MERR) != 0))
	{
		// Message failed to transmit.
		u8FlagsToReset |= u8MCP2515_IRQ_MERR;
	}
	if (u8CANSTAT == u8MCP2515_ICOD_ERROR)
	{
		// Some other error happened.
		u8FlagsToReset |= u8MCP2515_IRQ_ERRI;

		if (u8EFLG & u8MCP2515_EFLG_TXBO)
		{
			// Bus has gone off.
			pWorker->bBusOff = true;
		}
	}
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Tx - IRQ flags to be cleared: %02hhX", u8FlagsToReset);
	MCP2515_ClearInterruptFlags(0xFF);	// FIXME: Eh, let's just clear 'em all for now. //(u8FlagsToReset);
	MCP2515_READ(u8MCP2515_ADDR_CANINTF, &u8CANINTF, 1);
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Tx - Flags are now %02hhX", u8CANINTF);

	// Done. Disable the enabled interrupts and update the TEC.
	// If the transceiver has gone bus-off, bBusOff will be set in the interrupt handler, so no need to do it here.
	MCP2515_DisableInterrupts(u8MCP2515_IRQ_ERRI | u8MCP2515_IRQ_TX0I | u8MCP2515_IRQ_MERR);
	MCP2515_READ(u8MCP2515_ADDR_TEC, &pWorker->u8TEC, 1);
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Tx - TEC: %hhd", pWorker->u8TEC);

	// Abort all pending transmissions. If not, and we're not in one-shot mode, we may never clear the MERR IRQ flag.
	MCP2515_AbortAllTransmissions();
}

static void MCP2515Worker_TaskWaitForBusOn(MCP2515Worker* pWorker)
{
	MCP2515_ASSERT(pWorker != NULL);

	MCP2515_ASSERT(false);	// TODO
}

static void MCP2515Worker_TaskDetectBaudRate(MCP2515Worker* pWorker)
{
	MCP2515_ASSERT(pWorker != NULL);

	MCP2515_ASSERT(false);	// TODO: Will be implemented someday. Too useful not to have, but I don't NEED need it for now.
}

static int32_t MCP2515Worker_Thread(void* pContext)
{
	MCP2515Worker* pWorker;
	uint32_t u32FlagsSet;

	pWorker = (MCP2515Worker*)pContext;
	MCP2515_ASSERT(pWorker != NULL);
	while (true)
	{
		pWorker->u32CurTask = u32MCP2515_WORKER_TASK_NONE;
		u32FlagsSet = furi_thread_flags_wait(u32MCP2515_WORKER_TASK_ALL, FuriFlagWaitAny, FuriWaitForever);
		FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Woken up!");
		if (u32FlagsSet == (uint32_t)FuriFlagErrorTimeout)
			continue;
		u32FlagsSet &= u32MCP2515_WORKER_TASK_ALL;
		if (u32FlagsSet == u32MCP2515_WORKER_TASK_KILLTHREAD)
			break;	// Can't exit inside a switch statement.
		switch (u32FlagsSet)
		{
			case u32MCP2515_WORKER_TASK_INIT:
				FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Init");
				pWorker->u32CurTask = u32MCP2515_WORKER_TASK_INIT;
				MCP2515Worker_TaskInit(pWorker);
				break;
			case u32MCP2515_WORKER_TASK_RX:
				FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Rx");
				pWorker->u32CurTask = u32MCP2515_WORKER_TASK_RX;
				MCP2515Worker_TaskRx(pWorker);
				break;
			case u32MCP2515_WORKER_TASK_TX:
				FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Tx");
				pWorker->u32CurTask = u32MCP2515_WORKER_TASK_TX;
				MCP2515Worker_TaskTx(pWorker);
				break;
			case u32MCP2515_WORKER_TASK_WAIT_FOR_BUS_ON:
				FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Waiting for bus on");
				pWorker->u32CurTask = u32MCP2515_WORKER_TASK_WAIT_FOR_BUS_ON;
				MCP2515Worker_TaskWaitForBusOn(pWorker);
				break;
			case u32MCP2515_WORKER_TASK_DETECT_BAUD_RATE:
				FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Baud rate detection");
				pWorker->u32CurTask = u32MCP2515_WORKER_TASK_DETECT_BAUD_RATE;
				MCP2515Worker_TaskDetectBaudRate(pWorker);
				break;
			default:
				MCP2515_ASSERT(false);
				break;
		}
		if (pWorker->pTaskFinishedCallback != NULL)
			pWorker->pTaskFinishedCallback(pWorker->pTaskFinishedCallbackContext);
		if (MCP2515Worker_IsStopping())
			break;	// Can't exit inside a switch statement.
	}

	return 0;
}


// ==== PUBLIC FUNCTIONS ====

void MCP2515Worker_StartThread(MCP2515Worker* pWorker)
{
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Thread starting!");
	furi_hal_gpio_init(pIRQPin, GpioModeInterruptFall, GpioPullUp, GpioSpeedVeryHigh);
	furi_hal_gpio_remove_int_callback(pIRQPin);
	furi_hal_gpio_add_int_callback(pIRQPin, MCP2515Worker_InterruptCallback, pWorker);
	furi_hal_gpio_enable_int_callback(pIRQPin);
	furi_thread_start(pWorker->pWorkerThread);
	while (furi_thread_get_state(pWorker->pWorkerThread) != FuriThreadStateRunning);
}

void MCP2515Worker_StopWorkerThread(MCP2515Worker* pWorker)
{
	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Thread stopping!");
	furi_hal_gpio_disable_int_callback(pIRQPin);
	furi_hal_gpio_remove_int_callback(pIRQPin);
	furi_hal_gpio_init_simple(pIRQPin, GpioModeInput);
	furi_thread_flags_set(furi_thread_get_id(pWorker->pWorkerThread), u32MCP2515_WORKER_TASK_KILLTHREAD);
	furi_thread_join(pWorker->pWorkerThread);
}

bool MCP2515Worker_GiveTask(MCP2515Worker* pWorker, uint32_t u32Task, MCP2515Worker_Callback pCallback, void* pCallbackContext)
{
	// NOTE: The MCP2515Worker struct is not opaque to the caller. It's up to them to configure it before calling this!
	if (pWorker->u32CurTask != u32MCP2515_WORKER_TASK_NONE)
	{
		FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Given a task while one is in progress!");
		return false;
	}

	FURI_LOG_I("CAN-Transceiver", "MCP2515 Worker - Starting task %lu!", u32Task);
	pWorker->u32CurTask = u32Task;
	pWorker->pTaskFinishedCallback = pCallback;
	pWorker->pTaskFinishedCallbackContext = pCallbackContext;
	furi_thread_flags_set(furi_thread_get_id(pWorker->pWorkerThread), u32Task);

	return true;
}

MCP2515Worker* MCP2515Worker_Alloc()
{
	MCP2515Worker* pWorker;

	pWorker = malloc(sizeof(MCP2515Worker));
	memset(pWorker, 0, sizeof(MCP2515Worker));
	// TODO: Rx-related state info, if any needs to be initialized to nonzero values.

	pWorker->pWorkerThread = furi_thread_alloc();
	furi_thread_set_name(pWorker->pWorkerThread, "MCP2515Worker");
	furi_thread_set_callback(pWorker->pWorkerThread, MCP2515Worker_Thread);
	furi_thread_set_context(pWorker->pWorkerThread, pWorker);
	furi_thread_set_stack_size(pWorker->pWorkerThread, u32MCP2515_WORKER_STACK_SIZE);

	return pWorker;
}

void MCP2515Worker_Free(MCP2515Worker* pWorker)
{
	furi_thread_free(pWorker->pWorkerThread);
	free(pWorker);
}
