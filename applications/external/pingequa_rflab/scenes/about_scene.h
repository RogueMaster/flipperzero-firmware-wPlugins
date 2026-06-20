/**
 * @file about_scene.h
 * @brief About Scene.规范 §6.2(信息屏).
 *
 * 纯静态信息屏:品牌 + 引流 QR(pingequa.com)+ 法律声明.
 * on_enter 只切到 about_view;无 worker、无芯片访问、无可变状态.
 * 输入:Back 退回 main_menu —— 交给 ViewDispatcher navigation callback
 * 自动处理,本 scene 不设 input callback、on_event 不处理任何事件.
 */
#pragma once

#include <gui/scene_manager.h>

void about_scene_on_enter(void* context);
bool about_scene_on_event(void* context, SceneManagerEvent event);
void about_scene_on_exit(void* context);
