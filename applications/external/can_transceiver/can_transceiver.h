// Header for can_transceiver.c.

#ifndef CAN_TRANSCEIVER_H
#define CAN_TRANSCEIVER_H

// ==== INCLUDES ====

// TODO: Delete unused includes.

// Libc
#include <stdint.h>

// Furi
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>

// GUI
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <dialogs/dialogs.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/text_input.h>

// Input
#include <input/input.h>

// Storage
#include <storage/storage.h>

// App-specific
#include "can_transceiver_globalconf.h"
#include "can_transceiver_rxhistory.h"
#include "scenes/can_transceiver_scene_generator.h"
#include "views/can_transceiver_view_canrx.h"
#include "views/can_transceiver_view_cantx.h"
#include "lib/mcp2515/mcp2515_spi_worker.h"

// ==== MACROS ====

#define ASSERT(x)          \
    if(!(x)) {             \
        furi_check(false); \
    }

// ==== TYPEDEFS ====

// View types.
typedef enum {
    CANTransceiverViewSubmenu,
    CANTransceiverViewWidget,
    CANTransceiverViewVariableItemList,
    CANTransceiverViewCANRx,
    CANTransceiverViewCANTx,
    CANTransceiverViewByteInput,
    CANTransceiverViewTextInput,
    // TODO as needed
} CANTransceiverView;

// Crystal oscillator types.
typedef enum {
    XtalOsc_Unknown,
    XtalOsc_16MHz,
    // TODO: Support more oscillator frequencies for greater breakout board compatibility.
    // If clocks under 16 MHz are used, I don't think a 1 Mbps baud rate is feasible.
} XtalOscType;

// All globals go in this struct and are to be dynamically allocated.
// Global static allocation is a bad idea in a FAP.
typedef struct {
    // GUI controllers.
    Gui* pGUI;
    ViewDispatcher* pViewDispatcher;
    SceneManager* pSceneManager;

    // GUI elements. Only one of each is used at a time.
    Submenu* pSubmenu;
    Widget* pWidget;
    VariableItemList* pVarItemList;
    CANTransceiver_ViewCANRx* pViewCANRx;
    CANTransceiver_ViewCANTx* pViewCANTx;
    ByteInput* pByteInput;
    TextInput* pTextInput;
    // TODO as more items are needed

    // Filesystem stuff.
    Storage* pFSApi;

    // Global CAN transceiver configuration, stored in a separate struct. See can_transceiver_globalconf.h.
    CANTransceiverConfig* pGlobalConfig;

    // MCP2515 worker struct.
    MCP2515Worker* pMCP2515Worker;

    // Rx state information.
    uint16_t u16HistoryNumElements;
    uint16_t u16HistoryIndex;
    uint16_t u16HistoryListOffset;
    CANTransceiver_RxHistory* pRxHistory;
    uint32_t u32LastUIRefreshTime;
    char i8pLogFilename[64];

    // Tx state information.
    uint32_t u32FrameID;
    uint8_t u8pFrameData[8];
} CANTransceiverGlobals;

#endif // CAN_TRANSCEIVER_H
