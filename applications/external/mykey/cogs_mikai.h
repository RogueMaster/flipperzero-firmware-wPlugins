#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_box.h>
#include <dialogs/dialogs.h>
#include <notification/notification_messages.h>

#define TAG "COGSMyKai"

// SRIX4K Constants
#define SRIX_BLOCK_LENGTH 4
#define SRIX_UID_LENGTH 8
#define SRIX4K_BLOCKS 128
#define SRIX4K_BYTES 512

typedef enum {
    COGSMyKaiViewSubmenu,
    COGSMyKaiViewTextInput,
    COGSMyKaiViewPopup,
    COGSMyKaiViewWidget,
    COGSMyKaiViewTextBox,
} COGSMyKaiView;

typedef enum {
    COGSMyKaiSceneStart,
    COGSMyKaiSceneRead,
    COGSMyKaiSceneInfo,
    COGSMyKaiSceneWriteCard,
    COGSMyKaiSceneAddCredit,
    COGSMyKaiSceneSetCredit,
    COGSMyKaiSceneReset,
    COGSMyKaiSceneSaveFile,
    COGSMyKaiSceneLoadFile,
    COGSMyKaiSceneDebug,
    COGSMyKaiSceneAbout,
    COGSMyKaiSceneCount,
} COGSMyKaiScene;

typedef struct {
    uint32_t eeprom[SRIX4K_BLOCKS];
    uint64_t uid;
    uint32_t encryption_key;
    bool is_loaded;
    bool is_reset;
    bool is_modified;  
    uint16_t current_credit;
} MyKeyData;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    TextInput* text_input;
    Popup* popup;
    Widget* widget;
    TextBox* text_box;
    FuriString* text_box_store;
    DialogsApp* dialogs;
    NotificationApp* notifications;

    MyKeyData mykey;
    char text_buffer[32];
    uint32_t temp_credit_value; 
} COGSMyKaiApp;

// Scene handler function declarations
void cogs_mikai_scene_start_on_enter(void* context);
bool cogs_mikai_scene_start_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_start_on_exit(void* context);

void cogs_mikai_scene_read_on_enter(void* context);
bool cogs_mikai_scene_read_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_read_on_exit(void* context);

void cogs_mikai_scene_info_on_enter(void* context);
bool cogs_mikai_scene_info_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_info_on_exit(void* context);

void cogs_mikai_scene_add_credit_on_enter(void* context);
bool cogs_mikai_scene_add_credit_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_add_credit_on_exit(void* context);

void cogs_mikai_scene_set_credit_on_enter(void* context);
bool cogs_mikai_scene_set_credit_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_set_credit_on_exit(void* context);

void cogs_mikai_scene_reset_on_enter(void* context);
bool cogs_mikai_scene_reset_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_reset_on_exit(void* context);

void cogs_mikai_scene_write_card_on_enter(void* context);
bool cogs_mikai_scene_write_card_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_write_card_on_exit(void* context);

void cogs_mikai_scene_save_file_on_enter(void* context);
bool cogs_mikai_scene_save_file_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_save_file_on_exit(void* context);

void cogs_mikai_scene_load_file_on_enter(void* context);
bool cogs_mikai_scene_load_file_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_load_file_on_exit(void* context);

void cogs_mikai_scene_debug_on_enter(void* context);
bool cogs_mikai_scene_debug_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_debug_on_exit(void* context);

void cogs_mikai_scene_about_on_enter(void* context);
bool cogs_mikai_scene_about_on_event(void* context, SceneManagerEvent event);
void cogs_mikai_scene_about_on_exit(void* context);

// Scene handlers declaration
extern const SceneManagerHandlers cogs_mikai_scene_handlers;

// MyKey operations
bool mykey_read_from_nfc(COGSMyKaiApp* app);
bool mykey_write_to_nfc(COGSMyKaiApp* app);
void mykey_calculate_encryption_key(MyKeyData* key);
bool mykey_is_reset(MyKeyData* key);
uint16_t mykey_get_current_credit(MyKeyData* key);
uint16_t mykey_get_credit_from_history(MyKeyData* key); 
void mykey_encode_decode_block(uint32_t* block);
bool mykey_add_cents(MyKeyData* key, uint16_t cents, uint8_t day, uint8_t month, uint8_t year);
bool mykey_set_cents(MyKeyData* key, uint16_t cents, uint8_t day, uint8_t month, uint8_t year);
void mykey_reset(MyKeyData* key);
uint32_t mykey_get_block(MyKeyData* key, uint8_t block_num);
void mykey_modify_block(MyKeyData* key, uint32_t block, uint8_t block_num);
bool mykey_save_raw_data(COGSMyKaiApp* app, const char* path); 
