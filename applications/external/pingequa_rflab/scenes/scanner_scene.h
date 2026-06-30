/**
 * @file scanner_scene.h
 * @brief Channel Scan Scene.规范 §6.2(主功能).
 *
 * 流程:on_enter 启动 worker 持续做 126 通道 RPD 扫描.
 * 输入处理(经 scanner_view 的 input_callback,不走 scene_on_event):
 *   ←/→  光标 ±1 通道
 *   ↑/↓  dwell ±10µs(短按)/ ±50µs(长按或重复)
 *   OK   暂停/恢复
 *   Back 退出 app
 */
#pragma once

#include <gui/scene_manager.h>

void scanner_scene_on_enter(void* context);
bool scanner_scene_on_event(void* context, SceneManagerEvent event);
void scanner_scene_on_exit(void* context);
