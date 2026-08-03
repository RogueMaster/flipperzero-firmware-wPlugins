v6.6:
- FIX: Maze dead-ends — BFS pathfinding replaces Manhattan distance for exit placement, guaranteeing solvable mazes.
- FIX: Key reachability — BFS validation ensures keys are always placed in reachable area before doors.
- FIX: Endless mode stage mismatch — was passing floor+100 causing false combat branch.
- World pre-generation cache — complete maze snapshots saved to App Data for instant loading.
