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
