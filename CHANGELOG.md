v7.1:
- Critical fix: replaced pixel-by-pixel framebuffer blit (8192 canvas_draw_dot calls) with a single canvas_draw_xbm call, eliminating the watchdog timeout that caused device restart.
- Only render when dirty; reduce render columns from 64 to 32.
- Reduce MAP_MAX from 31 to 23 (saves memory); increase stack size to 16KB.

v7.0:
- Rewritten as a minimal maze game: pure 3D raycasting, find the exit, next floor.
- Removed campaign, combat, shop, MC sandbox, story, achievements, particles, and inventory.
- Menu simplified to Play / Language / About.
- Default language is English; bilingual toggle (English/Chinese) persists to storage.
- Maze generation: recursive backtracker with loops, exit placed at the BFS-farthest cell.
- Exit direction arrow projects the exit cell to screen space.
- Sound effects: menu move, menu confirm, step, level clear.
- All documentation and descriptions are in English.
