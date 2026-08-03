v6.8:
- Precision controls: short tap = small precise step, hold = accelerating movement (longer hold = faster). Turn short tap = 11.5 deg, hold up to 2.9 deg/frame. Move short tap = 0.15 cells, hold up to 0.042 cells/frame.
- MC mode jump: long press Back to jump (parabolic arc, 28 frames, peak 9px, landing dust).
- Triple long-press Back to exit (anti-misclick protection, 1.5s window).
- Texture system expanded to 24 slots (12 active + 12 reserved for future blocks).
- Jump visual: all projected elements (walls, sprites, enemies, bullets, particles) shift with camera height.

v6.6:
- BFS pathfinding replaces Manhattan distance for exit placement — guarantees solvable mazes (no dead ends).
- BFS reachability check for key placement — keys never hidden behind doors.
- World pre-generation cache — complete maze snapshots saved to App Data for instant loading.
- Endless mode stage mismatch fix — was passing floor+100 causing false combat branch.
