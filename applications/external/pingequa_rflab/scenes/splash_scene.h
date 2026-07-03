/**
 * @file splash_scene.h
 * @brief 启动屏 Scene.规范 §6.2.
 *
 * 流程:T+0 进入 → worker 等 250ms LDO + 150ms PoR → pq_board_detect()
 *      → T+1000ms 切到 scanner_scene(成功)或 error_scene(失败).
 *
 * 输入:Back 直接退出 app(SceneManager 默认行为).
 */
#pragma once

#include <gui/scene_manager.h>

void splash_scene_on_enter(void* context);
bool splash_scene_on_event(void* context, SceneManagerEvent event);
void splash_scene_on_exit(void* context);
