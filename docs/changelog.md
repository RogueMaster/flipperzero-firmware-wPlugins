v6.10.2 (Stable):
- Enemy movement slowed dramatically: random interval 80-180 frames
  (~1.3-3 seconds per move), 12-frame smooth interpolation transition
  (eliminates teleport-flicker). Overall speed reduced by 50%+.
- Enemy HP fixed at 3, bullet/melee damage = 1. Strictly 3 hits to kill.
  Fixed the "enemies cannot be killed" bug from earlier builds.
  Collision radius increased to 0.5 cell for forgiving hit detection.
- Near-death persistent flashing: when enemy HP = 1, hurt_flash stays
  enabled (never decrements) so the body keeps blinking until death.
  Eyes hidden while hurt for clearer feedback.
- Wall occlusion: multi-column depth-buffer check across the enemy's
  width — any visible column renders it. Non-developer mode: enemies
  behind walls are completely hidden. Developer mode: full vision.
- MC jump anti-crash: MC mode jump height capped at 3px (was 4px),
  removed the mc_hop wall-bypass logic (jump is now purely visual),
  strict collision detection prevents "hold forward into wall" freezes
  that previously caused device reboots.

v6.10.1-beta:
- Enemy movement slowed + randomized (interval 40-90 -> 80-180 frames).
- Wall occlusion: depth-buffer check hides enemies behind walls in
  non-dev mode (single-column check).
- MC jump height reduced (4px -> 3px), removed wall-bypass logic.
- Enemy HP normalized to 3, bullet damage = 1 (3 shots to kill).

v6.10.0-beta:
- Enemy AI: smooth interpolated movement + line-of-sight ranged shots
  + auto-approach when player is not visible.
- Pistol auto-lock: bullets bias toward the nearest enemy in the view
  cone (70% original + 30% target direction, max 30 degrees).
- Ammo auto-regen: timer-based ammunition replenishment in combat levels.
- Score system + shop: +10 points per kill, spendable in shop.
- Pure-maze levels (1-10) cleared instantly when first key is picked up.
- Story expansion: additional chapters and branching choices.

v6.9.1-beta:
- Settings menu fully translated to Chinese.
- List selection permanently centered (scrolling list UI).
- MC mode: jump on top of blocks (stand on placed blocks).

v6.9.0-beta:
- Developer mode with 20+ runtime settings (turn sensitivity, move
  speed, jump height, brightness, fog, sky/ceiling, floor texture,
  SFX volume, maze scale, HP start, regen rate, ammo multiplier,
  endless start floor, MC size, MC day length, MC jump, etc.).
- List selection permanently centered.
- Render/audio/MC all configurable.

v6.8.0-beta:
- Control overhaul: short tap = precise small step, hold = accelerating
  movement (the longer you hold, the faster it goes).
- MC jump (parabolic arc, 28 frames, peak 9px, landing dust).
- Triple long-press Back to exit (anti-mistouch beta protection).
- Texture table expanded to 24 slots (12 reserved for future blocks).

v6.7-beta:
- Jump mechanic introduced (parabolic arc).
- Tweaked rendering for performance.

v6.6:
- Fixed maze dead-end bugs; BFS connectivity verification.
- World pre-generation cache: full maze snapshots saved to App Data
  for instant loading.

v6.5:
- Health system rebuild: base 10 HP (+2 per 5 levels, max 20),
  2-second invincibility after hit, +1 HP/sec regen.

v6.4:
- Exit bug fix.
- MC mode: flat terrain, day/night cycle, inventory, HP bar.

v6.3:
- MC 材质包大升级 (仿 Minecraft):
  * 新增 3 种方块: 土方块(WALL_DIRT)、沙子(WALL_SAND, 真正的沙)、原木(WALL_LOG)
  * 贴图表 9→12 张: 草方块侧面(上草下土)、土颗粒、原木树皮、树叶密点、沙细颗粒、光柱
  * 草方块纹理改为 MC 风格: 顶部 2 行草层(密), 下 6 行土颗粒
- 水流动画: texture_sample 对水纹理按 g.tick 做垂直偏移, 波纹实时滚动
- MC 天空+太阳: MC 模式天花板改为亮天空渐变(地平线渐亮→顶部全亮);
  右上角画 5x5 太阳+十字光芒, 缓慢左右飘动
- 沙子落地粒子: 放置沙/土方块时爆发 6 粒尘土 + 3 粒火花 (落地效果)
- MC 生存玩法核心: 挖掘方块后手持自动切换为该方块(挖草得草/挖沙得沙/挖木得木);
  对空地长按 OK = 循环切换手持方块(创造模式选块)
- MC 地形重生: 草25%/树12%/原木12%/沙12%/土6%/水6%/空25%, 边界改为石头(可挖)
- 手持方块 6→8 种: Brick/Stone/Wood/Grass/Dirt/Sand/Log/Leaf
- HUD 更新: 8 种方块名显示 + 操作提示 "长OK挖/切 Back放"

v6.2:
- Control scheme (用户硬规定):
  * OK short press = SHOOT in ALL modes (MC, campaign, endless, visitor).
    Plays "No Ammo!" feedback when the magazine is empty.
  * OK long press  = MINE in MC mode, or open inventory/task panel
    in other modes (preserves the original OK-long behavior).
  * Back short press = PLACE block in MC mode (mc_place).
  * Long/short move keys (up/down/left/right) unchanged: front/back + rotate.
- Crosshair permanently visible (所有游戏模式) as a high-contrast
  XOR-drawn inverted-color square with four spiked arms — always the
  opposite of whatever is on screen (black on white, white on black).
  Crosshair drops 4 px on shoot (recoil) and rebounds over 4 frames.
- Particle system (32 slots): shoot sparks (~5 at muzzle), hit explosion
  (~6 on enemy hit, ~10 on kill, ~3 on wall hit), per-frame bullet trail
  pellets, and footstep dust every other frame when walking.
- Global brightness bump: shade distance thresholds relaxed (2.5/4.5/7.5/11)
  and Bayer density thresholds raised (14/10/6/3), so walls, floor and
  ceiling are noticeably brighter at all ranges.
- Enemy speed-up: AI cooldown reduced from 22 → 15 ticks per move so
  enemies and NPCs visibly move more often.

v6.1:
- FIX: MC sandbox mode was completely broken — players could clip through
  grass/tree/water blocks (blocking() missed WALL_GRASS/WALL_WATER/WALL_VINE)
  while the renderer drew them as solid walls, producing a "无法玩" experience.
  All WALL_* types now correctly block movement. MC terrain regenerated with
  ~30% blocks / 70% open space so the player actually has room to walk.
- Combat overhaul: pistol now fires real entity bullets (Bullet pool, 12 max)
  that travel and collide instead of the old instant raycast hit.
  - Player bullet speed 0.55 cell/frame, enemy bullet 0.35.
  - Bullet life 30 frames; despawns on wall hit or target hit.
- Enemy AI expanded: enemies now chase the player (greedy move, 60% chance)
  AND shoot back when they have line-of-sight at 2-6 cells. Fire cooldown
  scales with level (slower at higher levels for fairness).
- Visual feedback (the "激励动画和音效" request):
  - Enemy sprites: head + body + blinking eyes, bob animation, hurt-flash
    (white-out) when damaged, muzzle warning dot before they fire.
  - Bullets rendered as glowing dots with distance-based size + trail.
  - Muzzle flash on player shoot, screen shake on hit, hurt screen-flash.
  - HUD redesigned with icons: heart HP, star kill count, skull remaining
    enemies, ammo counter (flashes "AM!" when empty).
- HUD always shows all key info: level + hearts + skull enemies + star kills
  + ammo in combat; MC mode shows block type + mined + hearts + achievements.

v6.0:
- FIX: "instant clear" exploit — clicking the exit no longer completes the
  level. Exits are now CELL_LOCKED_EXIT in puzzle (11-20) and combat (21-50)
  stages; the player is bounced back with a "Locked! Do Task" toast + SFX
  until all subtasks are done.
- Level structure reworked into 3 progressive stages:
  * 1-10:  pure maze (find the exit, no key/door/enemy).
  * 11-20: puzzle stage — must collect N keys (1 + every 3 levels, cap 3)
           and open the door blocking the locked exit.
  * 21-50: combat stage — pistol required, must kill ALL enemies before the
           exit unlocks. Enemy count = 2 + every 3 levels (cap 6); enemy HP
           scales 2/3/4 at L21/L28/L35. Maze size grows 7+level (cap 25).
  Each stage's tasks, enemies and maze size scale up level by level.
- Combat: pistol shooting (OK key) with limited ammo (enemy count + 3 per
  level), 4-cell range, raycast hit detection. Empty magazine plays a dry
  click and shows "No Ammo!". A center crosshair is drawn in combat stages
  and MC mode. Dashing still does melee damage in a pinch.
- Achievement system (persistent across sessions, MAZ5 save format):
  First Blood, First Clear, 10/50 Kills, 10/25 Clears, Miner 50, Reach
  Combat (L21), Reach Late (L35). Each unlock fires a popup toast + a
  triumphant arpeggio SFX, and is saved immediately.
- MC sandbox mode expanded: 15x15 world with grass / tree / water / sand /
  wood / brick terrain generated by position hash, bedrock border. 6 holdable
  block types (brick/stone/wood/grass/sand/leaf), OK mines the targeted cell
  (grass/tree/wood/brick/stone/vine; water & bedrock are unmineable with a
  failure SFX), long-OK places the held block. Mining counts toward the
  Miner achievement. Top bar shows held block, mined count, and lifetime
  kill/clear stats; a crosshair aids aiming.
- More feedback: 4 new SFX (shoot / locked / achievement / no-ammo), task
  progress toasts, achievement popups, and full HUD info (level, HP, enemy
  count, key count, ammo) always visible over the 3D view.
- Save format bumped to MAZ5 with achievement fields; MAZ4/MAZ3 saves still
  load (achievements default to 0). Backward compatible.

v5.0.1 (Beta):
- New "MC" sandbox mode (Beta): selectable as menu item #5 "MC Beta".
  A small 11x11 space where you can mine and place blocks Minecraft-style.
  Controls: Up/Down move, Left/Right turn, short OK mines the block in
  front of you, long OK places the currently held block, short Back cycles
  the held block type (brick / stone / metal / vine), long Back returns to
  the menu. The outer border is unmineable bedrock so you can't fall out of
  the world; any placed block (incl. metal) can be mined again, so you
  can't trap yourself. Top bar shows the held block and total blocks mined.
  This is a Beta — no quest/combat, just creative digging in a pocket space.

v5.0:
- Pickup toasts: every item you pick up (key / torch / potion / amulet) now
  pops a bordered, blinking message box in the bottom-left corner. Bilingual
  (Chinese XBM bitmap or English text). Also fires a "Quest Done!" toast when
  a level's quest is completed.
- Always-on top status bar over the 3D view: shows level/floor + HP on the
  left, and on the right the live enemy count (combat stages) or key count
  (puzzle stages). No longer needs long-press OK to see vital stats.
- FIX: exit arrow was inaccurate — it used a linear angle*40 approximation
  that drifted badly near the screen edges. Rewritten to use the same inverse-
  determinant screen projection as sprites, so the arrow now points at the
  exit's true on-screen position. When the exit is behind you, both screen
  edges show turn-around chevrons; when it's dead ahead, a big blinking
  "exit ahead" frame appears.
- Smoother shading: distance darkening switched from harsh checkerboard /
  sparse-dot patterns to a 4x4 Bayer ordered-dither, so walls and the
  floor/ceiling fade into the distance smoothly instead of vanishing in
  steps. Floor/ceiling now gradient by distance from the horizon.
- Redrawn wall textures (brick running-bond, cobblestone, riveted metal
  panels, leafy vine) for clearer visual identity.

v4.4.1:
- Opening animation is no longer skippable by keys — it plays to completion
  (~4.5s, full BGM). It can only be disabled entirely from Settings.
  Previously a single keypress cut it off, leaving no time to listen or to
  enter the developer-mode unlock sequence.
- Long-press Up/Down/Left/Right during the opening now feeds the
  developer-mode unlock sequence (same as in the menu), so the intro doubles
  as input time.
- Removed the misleading "Press any key" prompt on the final intro stage.

v4.4:
- Developer mode: unlocks all campaign levels from the level-select screen
  and enables a debug overlay. Persists across sessions.
- Debug overlay: a top status bar over the 3D view showing current level /
  floor, player X/Y, world tick, actor count, and facing direction. Toggled
  from Settings (the row appears once developer mode is active).
- Settings screen rewrite: option rows are horizontally centered (label +
  value as a group); the list scrolls when it overflows (^/v indicators);
  the bottom hint bar no longer overlaps the last option. Up/Down now move
  the cursor in opposite directions.
- Save format stays MAZ4 (new flags reuse the reserved bytes; old v4.3 saves
  load with defaults).

v4.3.1:
- FIX: no sound at all. v4.3's speaker_start() called the non-existent
  furi_hal_speaker_is_muted() which was patched to furi_hal_speaker_is_mine(),
  but the app never acquired the speaker first — so is_mine() always returned
  false and every sound was silently dropped. Now sfx_init() calls
  furi_hal_speaker_acquire(1000ms) to take ownership of the speaker, and
  sfx_deinit() releases it. speaker_start() just calls furi_hal_speaker_start
  directly. All SFX (menu, pickup, door, combat, quest/level clear, game over)
  and the opening BGM now play correctly.

v4.3:
- Opening animation on app start: fluent 4-stage intro with BGM —
  (1) logo drops from the top with bounce easing + impact wave,
  (2) 8-direction particle/star burst from logo center with twinkles,
  (3) "k20120509 presents" subtitle slides up from the bottom,
  (4) blinking "press any key" prompt.
  A happy C-major chiptune BGM (40 notes, ~4.5s) plays on a dedicated
  sequencer channel during the opening. Pressing any key skips straight
  to the menu. Can be disabled from the new Settings screen (persisted).
- SFX (sound effects) using the Flipper Zero speaker: menu move / confirm,
  pickup key / item, door open / locked, trap / damage, attack hit / enemy
  kill, quest complete, level clear, game over, story page turn. A tiny
  tick-based sequencer in sfx.c plays short note sequences (C/D/E/F/G/A/B/C6
  scale) with no external audio files. BGM has priority over SFX.
- New menu item #4 "Settings": two toggles — SFX on/off and Opening on/off,
  persisted to save (MAZ4 magic, MAZ3 old saves migrated). Up/Down choose
  row; OK or Left/Right toggle; Back returns to menu.
- Save format bumped to MAZ4 with explicit settings fields; older MAZ3 saves
  are still read (settings fall back to on/on).

v4.2:
- Quest / task system for story levels: long-press OK now pauses the game and
  opens the Inventory (instead of the map panel). The Inventory is now a 2-page
  view — page 1 lists items, page 2 shows the current level's quest. Press
  Left/Right to switch pages (page 2 only exists when the level has a quest).
- Only "story" levels carry quests: Level 1 (Find Exit), puzzle levels 10-19
  (Get Key + Open Door), combat levels 20+ (Kill all enemies). Other campaign
  levels and endless/visitor modes have no quest.
- Quest tracking: picking up a key, opening a door, reaching the exit, and
  killing enemies auto-update each subtask's progress with a checkbox and
  x/N counter. Completing all subtasks grants a one-time reward (full HP
  restore).
- Enemy combat: enemies now have HP (2). Dashing (OK) into an enemy in front
  of you deals 1 damage instead of moving; two dashes kill it. Dead enemies
  stop attacking.
- Map panel preserved: from the Inventory, long-press OK opens the full map
  panel (minimap + exit arrow + HUD); OK/Back returns to the game.

v4.1:
- FIX: Endless Mode crash root cause found and fixed. The maze-carving DFS
  used two local int arrays of MAP_MAX*MAP_MAX (31*31*4 = 3844 bytes each,
  7688 bytes total) on the 8KB stack — combined with the rest of the call
  chain this overflowed the stack and crashed the app every time a new
  endless floor was generated. Arrays moved to static storage (BSS);
  stack_size also raised from 8KB to 12KB as a safety margin. Endless Mode
  now plays through floor transitions without freezing.

v4.0:
- Critical stability fixes for Endless Mode: tight size cap (max 19x19 maze cells, DDA steps capped, render loop guarded), cleared pending turn/move targets at every new level, better seed hashing to avoid repeated similar layouts — eliminates the freeze/crash when selecting or progressing in Endless
- Smooth controls rewrite: direction keys now set "target" rotation/move amounts; game_update applies a fractional step each frame (~45% of remaining turn target) so rotating, moving forward/back, and dashing all feel progressively smoother instead of one-shot jerky steps
- Big exit marker in 3D view: a giant black down-arrow (↓) painted at the top of the screen at the horizontal offset matching the exit direction relative to player facing; flickers every 16 frames; when exit is straight ahead (within ±20°), a big blinking rectangular frame is drawn around screen top to signal "exit is directly in front" — you see this anywhere in the maze, not just when nearby
- 3D sprite rendering of ground items: keys/torches/potions/amulets/traps/exit cells are raycast-projected onto the screen at proper floor depth with size scaling, each with a distinct glyph (key = hollow square, torch = cross, potion = solid square, amulet = diamond, trap = X, exit = blinking frame); flickers for visibility
v3.3:
- Level select screen rewrite: smaller FontSecondary text, scrollable list (7 visible rows) with up/down navigation and ^/v scroll indicators
- In-game HUD is now hidden by default for an unobstructed 3D view; long-press OK opens a full Map Panel instead
- New Map Panel (long-press OK): compact HUD bar (level/floor + HP + keys + torches + potions + amulets) in the smallest font, full-maze minimap centered on screen, player crosshair marker with facing line, and a large bold black arrow pointing from the nearest edge straight to the exit cell (exit cell also drawn as a solid black box for maximum visibility)
- Map Panel controls: short OK / Back closes the panel and resumes play; long-press OK again jumps into the Inventory
- Inventory screen now shows a compact HUD status bar (level/floor + HP) at the top so opening the inventory also serves as a status dashboard

v3.2:
- Full Chinese localization of story mode, inventory and level-select screens (XBM bitmaps for titles, body lines, item names, level tags)

v3.1:
- Force full rebuild of maze3d.fap so the published binary actually contains the v3.0 features (previous v3.0 fap was a stale artifact)

v3.0:
- Remove old endless-mode crash: cap maze size (<=21), always enable exit compass, strict actor limit
- Endless mode now lets you pick ANY level to enter (level select screen)
- New Inventory: long-press OK in game to open, Up/Down to move cursor, OK to use item, Back to resume
- New items: Potion (restore HP), Amulet (warp back to start) — shown on minimap and HUD
- New Story mode (renamed Campaign): 600-word prologue with 2 branching choices (Warrior +HP / Seeker +torch) that change starting stats
- Level select before entering Story mode: cleared levels replayable, uncleared levels locked
- Story text system (story.c) with paged English narration
- HUD shows Potion (P) and Amulet (A) counts

v2.1:
- Bilingual UI: switch Chinese / English from the menu (Left / Right key)
- All HUD labels, prompts and overlay text support both languages
- Updated application.fam metadata (author, version 2.1, English description)
- README.md and changelog added for App Catalog submission

v2.0:
- Half-resolution raycasting (64 columns) for stable performance
- On-demand world tick (~8 Hz) to prevent flicker / crash in endless mode
- Stack size raised to 8 KB
- Compass pointing to exit, minimap, exit highlight
- All in-game text replaced with Chinese XBM bitmaps (zh_chars.h)
- Pause (short Back) / Exit (long Back) flow

v1.0:
- Initial release: Campaign + Endless + Visitor modes
- Recursive-backtracker maze generation, difficulty scaling
- Items (key / torch / trap / door), enemy AI, NPC visitors
- 4 wall textures with distance & orientation shading
