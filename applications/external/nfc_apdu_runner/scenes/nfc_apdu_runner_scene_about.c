/*
 * @Author: SpenserCai
 * @Date: 2025-03-07 16:32:13
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-13 13:07:05
 * @Description: file content
 */
#include "../nfc_apdu_runner.h"
#include "nfc_apdu_runner_scene.h"

// 关于场景进入回调
void nfc_apdu_runner_scene_about_on_enter(void* context) {
    NfcApduRunner* app = context;
    TextBox* text_box = app->text_box;

    text_box_reset(text_box);
    text_box_set_font(text_box, TextBoxFontText);

    text_box_set_text(
        text_box,
        "NFC APDU Runner\n"
        "Version: 0.3\n"
        "Auther: SpenserCai\n\n"
        "This app allows you to run APDU commands from script files.\n\n"
        "Supported card types:\n"
        "- ISO14443-4A\n"
        "- ISO14443-4B\n\n"
        "Place your script files in:\n" APP_DIRECTORY_PATH "\n\n"
        "File format:\n"
        "Filetype: APDU Script\n"
        "Version: 1\n"
        "CardType: iso14443_4a\n"
        "Data: [\"APDU1\",\"APDU2\",...]\n");

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcApduRunnerViewTextBox);
}

// 关于场景事件回调
bool nfc_apdu_runner_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

// 关于场景退出回调
void nfc_apdu_runner_scene_about_on_exit(void* context) {
    NfcApduRunner* app = context;
    text_box_reset(app->text_box);
}
