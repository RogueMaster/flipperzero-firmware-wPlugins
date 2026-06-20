/**
 * @file error_scene.h
 * @brief 错误屏 Scene.规范 §6.2(DialogEx).
 *
 * 显示 detect.failure_hint(板卡探测失败时由 board_detect 填充),
 * 中央按钮 "Exit" 或 Back 键退出 app.
 */
#pragma once

#include <gui/scene_manager.h>

void error_scene_on_enter(void* context);
bool error_scene_on_event(void* context, SceneManagerEvent event);
void error_scene_on_exit(void* context);
