#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/modules/byte_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <expansion/expansion.h>

#include <stm32wbxx_ll_lpuart.h>
#include <stm32wbxx_ll_usart.h>

#include "scenes_config/app_scene_functions.h"

#define PATHAPP    "apps_data/modbus"
#define PATHAPPEXT EXT_PATH(PATHAPP)
#define PATHLOGS   PATHAPPEXT "/logs"

#define BR_VALUES            12
#define DATAWIDTH_VALUES     3
#define STOPBITS_VALUES      4
#define PARITY_VALUES        3
#define TIMEOUT_VALUES       255
#define DIGITALFORMAT_VALUES 2
#define ANALOGFORMAT_VALUES  2
#define SAVE_LOG_VALUES      2

#define RX_BUF_SIZE                        255
#define UART_CH                            FuriHalSerialIdUsart
#define TEXT_BOX_LEN                       4096
#define FURI_HAL_SERIAL_USART_OVERSAMPLING 0x00000000U
#define TIMEOUT_SCALER                     50

#define FixedModbusSize 4
#define FixedPaket \
    ((!app->modbus->slave && FUNCTION <= 0x06) || (app->modbus->slave && FUNCTION >= 0x0F))
#define SLAVE        buf[0]
#define FUNCTION     buf[1]
#define EXCEPTION    buf[2] - 1
#define STARTADDRESS (buf[2] << 8 | buf[3])
#define QUANTITY     (buf[4] << 8 | buf[5])
#define BYTECOUNT    buf[6]
#define CRCH         buf[len - 2]
#define CRCL         buf[len - 1]

//////////////////////////   Defining Structs  //////////////////////////
typedef enum {
    Submenu_View,
    VarList_View,
    TextBox_View,
    ByteInput_View
} Views;
typedef enum {
    Settings_Option,
    Sniffer_Option,
    Sender_Option,
    Read_LOG_Option,
    About_Option,
    Manual_Sender_Option,
    Buffer_Sender_Option
} Main_options;

typedef struct {
    uint8_t baudrate;
    uint8_t dataWidth;
    uint8_t stopBits;
    uint8_t parity;
    uint8_t timeout;
    bool hexOutput;
    bool saveLOG;
} Config;

typedef struct {
    Config* cfg;
    FuriThread* rxThread;
    FuriStreamBuffer* rxStream;
    FuriHalSerialHandle* serial_handle;
    uint8_t rxBuff[RX_BUF_SIZE + 1];
} Uart;
typedef struct {
    bool slave;
    FuriString* timeout;
} Modbus;
#define Ring_Buf_Size 255
typedef struct {
    uint8_t delimiters[32];
    uint8_t ringBuffer[Ring_Buf_Size];
    uint16_t writeIdx;
    uint8_t delimiterIdx;
    uint8_t readIdx;
} RingBuffer;

typedef struct {
    SceneManager* sceneManager;
    ViewDispatcher* viewDispatcher;
    Submenu* subMenu;
    VariableItemList* varList;
    ByteInput* byteInput;
    Uart* uart;
    Modbus* modbus;
    DialogsApp* dialogs;
    Storage* storage;
    File* LOGfile;
    char* logFilePath;
    bool LOGfileReady;

    size_t rows;
    size_t textLen;

    FuriTimer* timer;
    TextBox* textBox;
    FuriString* text;

    uint8_t msgBuf[RX_BUF_SIZE + 1];
    size_t msgLen;
    RingBuffer* ringBuffer;
    Expansion* expansion;
} App;

typedef enum {
    BaudRate_Option,
    DataWidth_Option,
    StopBits_Option,
    Parity_Option,
    TimeOut_Option,
    OutputFormat_Option,
    SaveLOG_Option
} Settings_Options;

typedef enum {
    Refresh = 0
} UartEvents;

typedef enum {
    WorkerEvtStop = (1 << 0),
    WorkerEvtRxDone = (1 << 1),
    WorkerEvtTxStart = (1 << 2),
    WorkerEvtCfgChange = (1 << 3),

} WorkerEvtFlags;
#define WORKER_ALL_EVENTS (WorkerEvtStop | WorkerEvtRxDone | WorkerEvtTxStart | WorkerEvtCfgChange)

extern const char* baudrateValues[];
extern const char* dataWidthValues[];
extern const char* stopBitsValues[];
extern const char* parityValues[];
extern const char* saveLOGValues[];
extern const char* outputFormatValues[];
extern const char* functionNames[];
extern const char* exceptionCodes[];
