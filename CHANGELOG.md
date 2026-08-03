v6.8.0-beta (私人仓库测试, 操控大优化):
- 新增: 全新操控模型 — 短按精确小步, 连按累积加速, 长按持续加速 (越长越快)
  - 转向: 短按转 11.5° 精确可控; 长按从 0.57°/帧起步, 每 8 帧加速, 上限 2.9°/帧
  - 移动: 短按前进 0.15 格/后退 0.11 格; 长按从 0.009 格/帧起步, 每 6 帧加速, 上限 0.042 格/帧
  - 连按: 多次短按累积, 实现快速移动 (不依赖长按)
- 优化: 转向插值响应从 45% 提升到 55% (更跟手)
- 优化: 前进单帧上限 0.5→0.55, 后退 0.3→0.38 (高速时充分释放)
- 优化: 后退速度为前进的 72% (更真实)

v6.7.0-beta (私人仓库测试, 不发官方):
- 新增: MC 模式跳跃 — 长按 Back 触发抛物线跳跃 (28 帧, 峰值 9 像素, 落地尘土特效)
- 新增: Beta 防误触退出 — 所有模式需"连续 3 次 Back 长按"才能退出 (1.5 秒内有效, 计数清零)
- 增强: 纹理系统升级为 24 槽 — 前 12 槽优化为更真实像素风, 后 12 槽全 0 预留 (圆石/煤矿/铁矿/红石/黑曜石/砖块/苔石/玻璃/南瓜/西瓜/菌丝/工作台 等未来方块)
- 增强: 跳跃视觉 — DDA 墙面、地板/天空边界、道具精灵、敌人、子弹、粒子均按距离投影垂直偏移

v6.6:
- FIX: Maze dead-ends — BFS pathfinding replaces Manhattan distance for exit placement, guaranteeing solvable mazes.
- FIX: Key reachability — BFS validation ensures keys are always placed in reachable area before doors.
- FIX: Endless mode stage mismatch — was passing floor+100 causing false combat branch.
- World pre-generation cache — complete maze snapshots saved to App Data for instant loading.
