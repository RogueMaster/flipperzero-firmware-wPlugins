/**
 * @file main_menu_scene.h
 * @brief 主菜单 Scene.规范 §6.2 v1.1+ 扩展点("v1.1+ 增强后再考虑 settings_scene
 *        / 主菜单"),v1.2 起作为 splash 落地后的总入口.
 *
 * 用官方 Submenu 模块(规范 §4.3 强制清单)展示功能列表:
 *   · Channel Scanner   → PqSceneScanner
 *   · NRF24 Jammer      → PqSceneJammer
 *
 * 输入:Submenu 自管 ↑/↓ + OK + Back.
 *   OK   → custom event 触发 next_scene
 *   Back → back event 默认 stop view_dispatcher(菜单是栈底)
 */
#pragma once

#include <gui/scene_manager.h>

void main_menu_scene_on_enter(void* context);
bool main_menu_scene_on_event(void* context, SceneManagerEvent event);
void main_menu_scene_on_exit(void* context);
