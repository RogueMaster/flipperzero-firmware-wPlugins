#pragma once
#include "structures.h"

//Setting the backlight on
#define BACKLIGHT_ON 1

//Creating different messages used for display
#define TAG                "YRM100X_PRO"
#define WRITE_EPC_VAL      "EPC Value"
#define WRITE_RES_MEM      "Reserved Memory"
#define WRITE_USR_MEM      "User Memory"
#define WRITE_TID_MEM      "TID Value"
#define WRITE_SELECT_BANK  "Select Bank"
#define WRITE_EPC_OK       "EPC Written!"
#define WRITE_TID_OK       "TID Written!"
#define WRITE_USR_OK       "User Written!"
#define WRITE_RES_OK       "Reserved Written!"
#define WRITE_EPC_CANCELED "Write Canceled!"
#define WRITE_EPC_FAIL     "Write Failed!"

//Content for the about screen
#define UHF_RFID_VERSION_APP   "YRM100X_PRO 1.7"
#define UHF_RFID_MEM_DEVELOPER "@AlexeySmirnov74"
#define UHF_RFID_GITHUB        "github.com/AlexeySmirnov74/YRM100X_PRO"
#define UHF_RFID_NAME          "\e#\e!          YRM100X_PRO          \e!\n"
#define UHF_RFID_BLANK_INV     "\e#\e!"
#define YRM100X_MODULE         1

#define NO_SAVE_ON_WRITE  1
#define YES_SAVE_ON_WRITE 2
//Defining different region values
#define USA_REGION        0
#define EU_REGION         1
#define KOREA_REGION      2
#define CHINA_800_REGION  3
#define CHINA_900_REGION  4

//Including the different views and helper files
#include "views/view_about.h"
#include "views/view_saved.h"
#include "views/view_config.h"
#include "views/view_write.h"
#include "views/view_delete.h"
#include "views/view_epc.h"
#include "views/view_delete_success.h"
#include "views/view_epc_info.h"
#include "views/view_bank_mem.h"
#include "views/view_read.h"
#include "views/view_tag_actions.h"
#include "views/view_lock.h"
#include "views/view_kill.h"
#include "views/view_clone_banks.h"
#include "views/view_clone.h"
#include "views/view_umi_test.h"
#include "views/view_bank_size.h"
#include "views/view_bank_tools.h"
#include "views/view_rewritable.h"
#include "views/view_antenna_info.h"
#include "helpers/extract_tag_info.h"
#include "helpers/saved_epc_functions.h"
#include "helpers/auto_dump.h"
#include "helpers/debug_log.h"
#include "helpers/umi_marker_store.h"
#include "helpers/uhf_notifications.h"

//Function declarations
void main_menu_alloc(UHFReaderApp* App);
void uhf_reader_main_menu_enter(void* context);

void uhf_reader_ensure_epc_views(UHFReaderApp* App);
void uhf_reader_ensure_epc_info_view(UHFReaderApp* App);
void uhf_reader_ensure_write_view(UHFReaderApp* App);
void uhf_reader_ensure_kill_view(UHFReaderApp* App);
void uhf_reader_ensure_delete_views(UHFReaderApp* App);
void uhf_reader_ensure_saved_view(UHFReaderApp* App);
void uhf_reader_ensure_tag_actions_view(UHFReaderApp* App);
void uhf_reader_ensure_lock_view(UHFReaderApp* App);
void uhf_reader_ensure_clone_views(UHFReaderApp* App);
void uhf_reader_ensure_umi_test_view(UHFReaderApp* App);
void uhf_reader_ensure_bank_size_view(UHFReaderApp* App);
void uhf_reader_ensure_bank_tools_view(UHFReaderApp* App);
void uhf_reader_ensure_rewritable_views(UHFReaderApp* App);
void uhf_reader_ensure_about_view(UHFReaderApp* App);
bool uhf_reader_ensure_worker(UHFReaderApp* App);
bool uhf_reader_require_antenna(UHFReaderApp* App, uint32_t return_view);

uint32_t uhf_reader_navigation_exit_callback(void* context);

void uhf_reader_submenu_callback(void* context, uint32_t index);
