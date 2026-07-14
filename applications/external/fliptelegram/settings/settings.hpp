#pragma once
#include "easy_flipper/easy_flipper.h"

class FlipTelegramApp;

typedef enum {
    SettingsViewSSID = 0,
    SettingsViewPassword = 1,
    SettingsViewConnect = 2,
    SettingsViewToken = 3,
    SettingsViewChatID = 4,
} SettingsViewChoice;

class FlipTelegramSettings {
private:
    void* appContext; // reference to the app context
#ifndef FW_ORIGIN_Momentum
    UART_TextInput* text_input = nullptr; // UART text input instance
#else
    TextInput* text_input = nullptr; // Original text input instance for Momentum
#endif
    std::unique_ptr<char[]> text_input_buffer; // buffer for text input
    uint32_t text_input_buffer_size = 128; // size of the text input buffer
    std::unique_ptr<char[]> text_input_temp_buffer; // temporary buffer for text input
    VariableItemList* variable_item_list = nullptr; // variable item list for settings
    VariableItem* variable_item_connect = nullptr; // variable item for "Connect" button
    VariableItem* variable_item_wifi_ssid = nullptr; // variable item for WiFi SSID
    VariableItem* variable_item_wifi_pass = nullptr; // variable item for WiFi Password
    VariableItem* variable_item_token = nullptr; // variable item for Telegram Bot Token
    VariableItem* variable_item_chat_id = nullptr; // variable item for Telegram Chat ID
    ViewDispatcher** view_dispatcher_ref; // reference to the view dispatcher

    static uint32_t callbackToSubmenu(void* context); // callback to switch to the main menu
    static uint32_t callbackToSettings(void* context); // callback to switch to the settings view
    void freeTextInput(); // free the text input resources
    bool initTextInput(uint32_t view); // initialize the text input for a specific view
    static void settingsItemSelectedCallback(
        void* context,
        uint32_t index); // callback for settings item selection
    bool startTextInput(uint32_t view); // start the text input for a specific view
    void textUpdated(uint32_t view); // update the text input based on the view
    static void textUpdatedSsidCallback(void* context); // callback for WiFi SSID text update
    static void textUpdatedPassCallback(void* context); // callback for WiFi Password text update
    static void textUpdatedTokenCallback(void* context); // callback for Bot Token text update
    static void textUpdatedChatIDCallback(void* context); // callback for Chat ID text

public:
    FlipTelegramSettings(ViewDispatcher** view_dispatcher, void* appContext);
    ~FlipTelegramSettings();

    void settingsItemSelected(uint32_t index); // handle settings item selection
};
