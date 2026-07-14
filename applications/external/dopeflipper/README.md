# CK42X DopeWars for Flipper Zero

A small CK42X-branded Flipper Zero `.fap` port of the browser game, with the global bridge at <https://www.ck42x.com/dopeflipper>.

## Gameplay

- Start with `$2,000` cash, `$5,500` debt, `30` days, and `100` coat slots.
- Start through a CK42X loading/title flow with the approved 128x64 monochrome shōjo-noir dolphin frame.
- Use the custom stylized hub for `BUY`, `SELL`, `TRAVEL`, `LOAN`, `STATS`, `BOARD`, `SAVE/RST`, and `ABOUT` instead of a plain first-screen submenu.
- Buy low, sell high across NYC boroughs with separated BUY-only and SELL-only product screens.
- Travel rolls new prices, shows a quick borough-specific 1-bit arrival card, then drops straight into the BUY list unless cops interrupt.
- Travel still triggers multiple market rumors, market-intel buy/sell tips, debt-pressure events, heat decay, and cop encounters.
- Use quick 1 / 5 / 10 / max trade actions for Flipper-native pacing.
- Watch cheap/expensive price markers and product average hints.
- Product rows include compact ASCII icons: `{L}` weed leaf, `oo` shrooms, `[*]` acid blotter, `(D)` ecstasy pill, `>>` speed, `^^` cocaine pile, `H!` heroin, `[O]` oxy.
- Bank money to protect it from street events.
- Repay the loan shark before the clock runs out.
- Progress autosaves to `/ext/apps_data/ck42x_dopewars/save.bin` after deals, travel, bank/loan actions, and cop outcomes.
- Use `Run status + intel` to see net worth, rank, local inventory value, profit, heat, coat usage, biggest deal, autosave state, and last street intel.
- Use `Stats` to see current-run rank plus expanded all-time local stats persisted at `/ext/apps_data/ck42x_dopewars/stats.bin`: games played, wins/losses, win streaks, best/worst net worth, best profit, biggest deal, best cash/bank, lifetime bought/sold, cops fought/ran, and highest heat survived.
- Use `Leaderboard` to see local stats and open `Global leaderboard` after a finished run.
- In `Global leaderboard`, choose `Windows BadUSB`, `Linux BadUSB`, or `macOS BadUSB`. Plug the Flipper into the computer, let BadUSB open `https://www.ck42x.com/dopeflipper` with the run profile attached, then submit through the bridge.
- If BadUSB is not available, use the fallback URL and paste `/ext/apps_data/ck42x_dopewars/profile.txt` into the manual profile import on the site.
- The FAP stays honest: score codes/profile hashes are tamper-resistant checks, not cheat-proof online proof.
- Use `Save run` for a manual save checkpoint or `Reset save + run` to clear the saved run and start over.
- Hear short Flipper speaker cues for buy, sell, travel, cops, win, and loss events.

This Flipper build keeps the core loop and source-game flavor while using Flipper-native menus instead of the browser layout.

## Build

```bash
/home/x3y5x/.local/share/venvs/ufbt/bin/ufbt
```

Output:

```text
dist/ck42x_dopewars.fap
```

## Install

Copy `dist/ck42x_dopewars.fap` to the Flipper SD card under `/ext/apps/Games/`, or launch with uFBT when the Flipper is connected.
