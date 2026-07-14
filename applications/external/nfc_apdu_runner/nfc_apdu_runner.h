/*
 * @Author: SpenserCai
 * @Date: 2025-02-28 18:02:04
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-13 12:22:41
 * @Description: file content
 */
#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/button_menu.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_3b/iso14443_3b_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>
#include <bit_lib/bit_lib.h>
#include <toolbox/hex.h>
#include <toolbox/path.h>

#define APP_DIRECTORY_PATH "/ext/apps_data/nfc_apdu_runner"
#define FILE_EXTENSION     ".apduscr"
#define RESPONSE_EXTENSION ".apdures"
#define MAX_APDU_COMMANDS  50
#define MAX_APDU_LENGTH    256
#define MAX_LOG_ENTRIES    100
#define MAX_FILENAME_LEN   64

// 自定义事件枚举
typedef enum {
    NfcApduRunnerCustomEventPopupClosed = 0,
    NfcApduRunnerCustomEventViewExit,
} NfcApduRunnerCustomEvent;

// 卡类型枚举
typedef enum {
    CardTypeIso14443_3a,
    CardTypeIso14443_3b,
    CardTypeIso14443_4a,
    CardTypeIso14443_4b,
    CardTypeUnknown,
} CardType;

// 应用程序视图枚举
typedef enum {
    NfcApduRunnerViewSubmenu,
    NfcApduRunnerViewWidget,
    NfcApduRunnerViewTextBox,
    NfcApduRunnerViewPopup,
    NfcApduRunnerViewTextInput,
} NfcApduRunnerView;

// 日志条目结构
typedef struct {
    char* message;
    bool is_error;
} LogEntry;

// APDU脚本文件结构
typedef struct {
    CardType card_type;
    char* commands[MAX_APDU_COMMANDS];
    uint32_t command_count;
} NfcApduScript;

// APDU响应结构
typedef struct {
    char* command;
    uint8_t* response;
    uint16_t response_length;
} NfcApduResponse;

// NFC Worker状态枚举
typedef enum {
    NfcWorkerStateReady, // 准备就绪
    NfcWorkerStateDetecting, // 正在检测卡片
    NfcWorkerStateRunning, // 正在执行APDU命令
    NfcWorkerStateStop // 停止
} NfcWorkerState;

// NFC Worker事件枚举
typedef enum {
    NfcWorkerEventSuccess, // 操作成功
    NfcWorkerEventFail, // 操作失败
    NfcWorkerEventCardDetected, // 检测到卡片
    NfcWorkerEventCardLost, // 卡片丢失
    NfcWorkerEventAborted, // 操作被中止
    NfcWorkerEventStop, // 停止事件
} NfcWorkerEvent;

// NFC Worker回调函数类型
typedef void (*NfcWorkerCallback)(NfcWorkerEvent event, void* context);

// NFC Worker结构体
typedef struct NfcWorker NfcWorker;

// 应用程序状态结构
typedef struct {
    // GUI
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    DialogsApp* dialogs;
    Widget* widget;
    TextBox* text_box;
    FuriString* text_box_store;
    Popup* popup;
    ButtonMenu* button_menu;
    TextInput* text_input;

    // NFC
    Nfc* nfc;
    NfcWorker* worker; // 添加NFC Worker

    // 文件
    Storage* storage;
    FuriString* file_path;
    NfcApduScript* script;
    NfcApduResponse* responses;
    uint32_t response_count;
    char file_name_buf[MAX_FILENAME_LEN + 1];

    // 日志
    LogEntry* log_entries;
    uint32_t log_count;

    // 状态
    bool running;
    bool card_lost;
} NfcApduRunner;

// 释放APDU脚本资源
void nfc_apdu_script_free(NfcApduScript* script);

// 释放APDU响应资源
void nfc_apdu_responses_free(NfcApduResponse* responses, uint32_t count);

// 解析APDU脚本文件
NfcApduScript* nfc_apdu_script_parse(Storage* storage, const char* file_path);

// 保存APDU响应结果
bool nfc_apdu_save_responses(
    Storage* storage,
    const char* file_path,
    NfcApduResponse* responses,
    uint32_t response_count,
    const char* custom_save_path);

// 添加日志
void add_log_entry(NfcApduRunner* app, const char* message, bool is_error);

// 释放日志资源
void free_logs(NfcApduRunner* app);

// 应用程序入口点
int32_t nfc_apdu_runner_app(void* p);

// NFC Worker函数声明
NfcWorker* nfc_worker_alloc(Nfc* nfc);
void nfc_worker_free(NfcWorker* worker);
void nfc_worker_start(
    NfcWorker* worker,
    NfcWorkerState state,
    NfcApduScript* script,
    NfcWorkerCallback callback,
    void* context);
void nfc_worker_stop(NfcWorker* worker);
bool nfc_worker_is_running(NfcWorker* worker);
void nfc_worker_get_responses(
    NfcWorker* worker,
    NfcApduResponse** out_responses,
    uint32_t* out_response_count);
const char* nfc_worker_get_error_message(NfcWorker* worker);
