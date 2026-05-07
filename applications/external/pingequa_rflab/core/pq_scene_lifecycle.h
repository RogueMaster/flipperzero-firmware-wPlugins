/**
 * @file pq_scene_lifecycle.h
 * @brief Scene 进出日志 + app shutdown 总开关.规范 §5.7.
 *
 * 三个公开符号都极简,但作为统一入口便于后期加入更多埋点(性能计时、
 * crash dump 等)而无需改散落在各 scene 的代码.
 *
 * 退出策略(规范 §5.7):App 期间始终保持 OTG 开,退出时**不**主动关 OTG.
 * Flipper 系统层维护 OTG 计数,我们退出时让系统管理.
 */
#pragma once

/** Scene 进入时调一次,Furi log info 级别.scene_name 通常 = scene 文件名. */
void pq_scene_enter(const char* scene_name);

/** Scene 退出时调一次. */
void pq_scene_exit(const char* scene_name);

/**
 * App 退出兜底.必须在所有 scene 都已 exit、ViewDispatcher 已 free 之后调用.
 * 流程:
 *   1. pq_chip_arbiter_force_release()  — 释放 mutex 兜底
 *   2. pq_chip_arbiter_deinit()         — PB2 / 两 CSN → Analog,SPI off
 *   3. **不关 OTG**(规范 §5.7)
 */
void pq_app_shutdown(void);
