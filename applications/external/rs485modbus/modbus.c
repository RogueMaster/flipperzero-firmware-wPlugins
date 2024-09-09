#include "Modbus.h"
#include "modbus_parser/modbus_parser.h"
#include "modbus_storage/modbus_storage.h"
#include "modbus_uart/modbus_uart.h"
#include "modbus_sender/modbus_sender.h"
#include "modbus_ring_buffer/modbus_ring_buffer.h"

const char* baudrateValues[] = {
    "1200",
    "2400",
    "4800",
    "9600",
    "19200",
    "28800",
    "38400",
    "57600",
    "115200",
    "230400",
    "460800",
    "921600",
};
const char* dataWidthValues[] = {"7", "8", "9"};
const char* stopBitsValues[] = {"0.5", "1", "1.5", "2"};
const char* parityValues[] = {"None", "Even", "Odd"};
const char* saveLOGValues[] = {"OFF", "ON"};
const char* outputFormatValues[] = {"Default", "HEX"};
const char* functionNames[] = {
    "Read Coils(01)",
    "Read Discrete Inputs(02)",
    "Read Holding Registers(03)",
    "Read Input Registers(04)",
    "Write Single Coil(05)",
    "Write Single Register(06)",
    "Write Multiple Coils(0F)",
    "Write Multiple Registers(10)"};
const char* exceptionCodes[] = {
    "ILLEGAL FUNCTION",
    "ILLEGAL DATA ADDRESS",
    "ILLEGAL DATA VALUE ",
    "SLAVE DEVICE FAILURE",
    "ACKNOWLEDGE",
    "SLAVE DEVICE BUSY",
    "",
    "MEMORY PARITY ERROR",
    "",
    "GATEWAY PATH\nUNAVAILABLE"
    "GATEWAY TARGET DEVICE\nFAILED TO RESPOND"};
//////////////////////////   ViewDispatcher Callbacks //////////////////////////
static bool CustomEventCB(void* context, uint32_t event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->sceneManager, event);
}
static bool BackEventCB(void* context) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_back_event(app->sceneManager);
}
static void ThickEventCB(void* context) {
    furi_assert(context);
    App* app = context;
    UNUSED(app);
    // scene_manager_handle_tick_event(app->sceneManager);
}
//////////////////////////   Allocating  //////////////////////////
Config* Config_Alloc() {
    Config* config = malloc(sizeof(Config));
    config->baudrate = 3;
    config->dataWidth = 1;
    config->stopBits = 1;
    config->parity = 0;
    config->timeout = 10;
    config->hexOutput = false;
    config->saveLOG = false;
    return config;
}

Uart* Uart_Alloc(void* context) {
    App* app = context;
    Uart* uart = malloc(sizeof(Uart));
    uart->cfg = Config_Alloc();
    uart->rxStream = furi_stream_buffer_alloc(RX_BUF_SIZE, 1);
    uart->rxThread = furi_thread_alloc_ex("RxThread", 1024, uart_worker, app);
    uart->serial_handle = NULL;
    // serial_init(uart, UART_CH);
    furi_thread_start(uart->rxThread);

    return uart;
}

Modbus* Modbus_alloc(void* context) {
    App* app = context;
    UNUSED(app);
    Modbus* modbus = malloc(sizeof(Modbus));
    modbus->slave = false;
    modbus->timeout = furi_string_alloc();
    return modbus;
}
void msgBuf_alloc(App* app) {
    uint8_t* buf = app->msgBuf;
    SLAVE = (uint8_t)1;
    FUNCTION = (uint8_t)1;
    buf[2] = (uint8_t)0;
    buf[3] = (uint8_t)0;
    buf[4] = (uint8_t)0;
    buf[5] = (uint8_t)1;
    app->msgLen = 8;
}
static App* modbus_app_alloc() {
    App* app = malloc(sizeof(App));
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->LOGfile = storage_file_alloc(app->storage);
    app->sceneManager = scene_manager_alloc(&app_scene_handlers, app);
    app->viewDispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(app->viewDispatcher, app);
    view_dispatcher_set_custom_event_callback(app->viewDispatcher, CustomEventCB);
    view_dispatcher_set_navigation_event_callback(app->viewDispatcher, BackEventCB);
    view_dispatcher_set_tick_event_callback(app->viewDispatcher, ThickEventCB, 100);
    app->subMenu = submenu_alloc();
    view_dispatcher_add_view(app->viewDispatcher, Submenu_View, submenu_get_view(app->subMenu));
    app->varList = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->viewDispatcher, VarList_View, variable_item_list_get_view(app->varList));
    app->textBox = text_box_alloc();
    view_dispatcher_add_view(app->viewDispatcher, TextBox_View, text_box_get_view(app->textBox));
    app->byteInput = byte_input_alloc();
    view_dispatcher_add_view(
        app->viewDispatcher, ByteInput_View, byte_input_get_view(app->byteInput));

    app->text = furi_string_alloc();
    app->logFilePath = (char*)malloc(100);
    furi_string_reserve(app->text, 1024);
    makePaths(app);

    app->timer = furi_timer_alloc(timerDone, FuriTimerTypeOnce, app);
    furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
    app->modbus = Modbus_alloc(app);
    app->uart = Uart_Alloc(app);
    app->ringBuffer = ring_buffer_alloc();
    msgBuf_alloc(app);
    return app;
}

void uartFree(void* context) {
    App* app = context;
    furi_thread_flags_set(furi_thread_get_id(app->uart->rxThread), WorkerEvtStop);
    furi_thread_join(app->uart->rxThread);
    furi_thread_free(app->uart->rxThread);
    close_log_file_stream(app);
    free(app->uart->cfg);
    free(app->uart);
    free(app->modbus);
    furi_timer_free(app->timer);
}
void ModbusFree(void* context) {
    Modbus* modbus = context;
    furi_string_free(modbus->timeout);
}
void modbus_app_free(App* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->viewDispatcher, ByteInput_View);
    view_dispatcher_remove_view(app->viewDispatcher, TextBox_View);
    free(app->logFilePath);
    furi_string_free(app->text);
    text_box_free(app->textBox);
    view_dispatcher_remove_view(app->viewDispatcher, VarList_View);
    variable_item_list_free(app->varList);
    view_dispatcher_remove_view(app->viewDispatcher, Submenu_View);
    submenu_free(app->subMenu);
    view_dispatcher_free(app->viewDispatcher);
    scene_manager_free(app->sceneManager);
    ring_buffer_free(app->ringBuffer);
    ModbusFree(app);
    uartFree(app);
    storage_file_free(app->LOGfile);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_EXPANSION);
    expansion_enable(app->expansion);
    free(app);
}

//////////////////////////   Entry Point   //////////////////////////
int32_t Modbus_app(void* p) {
    UNUSED(p);
    App* app = modbus_app_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);
    furi_hal_gpio_init_simple(&gpio_ext_pc0, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_ext_pc0, 0);
    furi_hal_gpio_write(&gpio_ext_pc1, 0);
    view_dispatcher_attach_to_gui(app->viewDispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->sceneManager, app_scene_main);
    view_dispatcher_run(app->viewDispatcher);
    furi_record_close(RECORD_GUI);
    modbus_app_free(app);
    furi_hal_gpio_init_simple(&gpio_ext_pc0, GpioModeAnalog);
    furi_hal_gpio_init_simple(&gpio_ext_pc1, GpioModeAnalog);
    return 0;
}
