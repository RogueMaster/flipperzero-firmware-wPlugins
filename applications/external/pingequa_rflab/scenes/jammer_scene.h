/**
 * @file jammer_scene.h
 * @brief NRF24 Jammer Scene.规范 §6.2 v1.2 + §13 Phase 5.
 *
 * 流程:on_enter alloc nrf24 + 启动 worker;worker 在 idle 等待 jammer_running,
 * 转 active 时调 setup_cw + CE high,steady state 在 chunk callback 内每 chunk
 * 16 channels(sweep)或单次 RF_CH 同步(CW).
 *
 * 输入(经 jammer_view 的 input_callback,不走 scene_on_event):
 *   ←/→  CW 模式下信道 ±1(短按)/ ±5(长按或重复);Sweep 模式下无效
 *   ↑/↓  在 CW / Sweep 模式间切换
 *   OK   启动/停止干扰
 *   Back 退回 main_menu(经 SceneManager nav 路径,§6.1)
 */
#pragma once

#include <gui/scene_manager.h>

void jammer_scene_on_enter(void* context);
bool jammer_scene_on_event(void* context, SceneManagerEvent event);
void jammer_scene_on_exit(void* context);
