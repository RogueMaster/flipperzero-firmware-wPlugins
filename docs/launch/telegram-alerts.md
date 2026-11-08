# Launch kit: Telegram alerts (v1.1.0)

Issue: ataman-lab/pc-health-remote#14. Date: 2026-11-08.

**Vlad publishes everything by hand. Nothing here is posted automatically.** Every draft is honest: free tier first,
open source, privacy, no invented user counts or reviews.

Facts used (checked in README.md, docs/TELEGRAM.md, telegram/worker.js, CHANGELOG.md):
- Bot: https://t.me/PCHealthRemoteBot. `/link` gives a 6-char code (valid 10 min); on the PC run
  `pc-health-remote.exe --link-telegram CODE`; restart the backend.
- Alerts: CPU/GPU temp, CPU/GPU load, RAM, VRAM, disk, low battery.
- Free: 1 PC, 10 alerts/day (UTC). Pro: 99 Telegram Stars / 30 days, unlimited alerts, up to 5 PCs.
- Stored: chat id, language, source tag, device keys, PC names, counters, payment records.
- Release: https://github.com/ataman-lab/pc-health-remote/releases/tag/v1.1.0

`/paysupport`, `/terms`, `/support` and the Pro upsell ship in the same PR as this kit (#31). After deploy the admin
sends `/setcommands` once so they show in the bot menu; check them live before any paid promotion (pre-ad gate).

## 1. Deep links (one per source)

Tag rule: `[A-Za-z0-9_-]{1,32}`. The bot saves it on `/start src_<tag>`.

| Source | Tag | Link |
|---|---|---|
| GitHub release | github_release | https://t.me/PCHealthRemoteBot?start=src_github_release |
| Flipper forum | flipper_forum | https://t.me/PCHealthRemoteBot?start=src_flipper_forum |
| r/flipperzero | reddit_flipper | https://t.me/PCHealthRemoteBot?start=src_reddit_flipper |
| r/pcmasterrace | reddit_pcmasterrace | https://t.me/PCHealthRemoteBot?start=src_reddit_pcmasterrace |
| TikTok | tiktok | https://t.me/PCHealthRemoteBot?start=src_tiktok |
| Telegram Ads | telegram_ads | https://t.me/PCHealthRemoteBot?start=src_telegram_ads |
| Friends | friends | https://t.me/PCHealthRemoteBot?start=src_friends |

Note: only the first `/start` with a tag matters; a user who already started and taps again with another tag
gets re-tagged (the code overwrites `src`). Do not cross-post one link.

## 2. Screen-video scripts (vertical 9:16, 20-30 s)

Setup for all: phone films the monitor and the Telegram chat on a second phone or a split view. Light room, no
personal data in the shot (blur chat names). Free music only, licence noted.

### Video A: "Your PC pings you when it overheats" (about 25 s)
| Sec | Shot | On-screen text |
|---|---|---|
| 0-3 | PC under load, fans, phone on desk | "PC too hot. You are not home." |
| 3-8 | Phone: open bot, send `/link`, code appears | "Step 1: /link" |
| 8-14 | PC terminal: `pc-health-remote.exe --link-telegram CODE` | "Step 2: one command" |
| 14-21 | Load tool pushes CPU temp up, alert lands in Telegram | "CPU temp 91 C (limit 85)" |
| 21-25 | Phone close-up, then bot link | "Free. Open source." |

Caption: Your PC pings you on Telegram when it runs hot. Free for 1 PC, open source, Windows/Linux.
Hashtags: #pcmasterrace #pcbuilding #cpu #gpu #opensource #telegram #homelab

### Video B: safe demo with `--simulate` (about 20 s)
| Sec | Shot | On-screen text |
|---|---|---|
| 0-3 | Terminal, type `pc-health-remote.exe --simulate --console` | "Test it without stressing your PC" |
| 3-9 | Console shows synthetic values rising | "Fake numbers, real alerts" |
| 9-16 | Telegram alert arrives on phone (test alert) | "Alert in 1 second" (only if true when filmed) |
| 16-20 | Bot link on screen | "Free tier: 10 alerts/day" |

Caption: Try the alerts safely: `--simulate` fires test alerts, no stress test needed.
Hashtags: #opensource #rust #pctips #telegrambot #monitoring #devtools
Remember: `--simulate` shows synthetic values; say so on screen so nobody thinks it is a real overheating.

### Video C: Flipper Zero + Telegram (about 30 s)
| Sec | Shot | On-screen text |
|---|---|---|
| 0-4 | Flipper on the desk showing Temps page | "Flipper shows it" |
| 4-12 | Alert screen on Flipper, vibro, LED | "At your desk: vibration" |
| 12-20 | Walk away; phone buzzes with the same alert | "Away from desk: Telegram" |
| 20-26 | Show `/status`: Free, 1 PC | "Free for 1 PC" |
| 26-30 | Repo and bot link | "Open source" |

Caption: Flipper Zero at the desk, Telegram when you are away. Same alert rules.
Hashtags: #flipperzero #flipper #pcmasterrace #opensource #hacker #telegram

## 3. Post drafts

### 3.1 GitHub release notes addition (v1.1.0)
```
### Telegram alerts (beta)
Get PC alerts on your phone, no Flipper needed. Free: 1 PC, 10 alerts/day.
1. Open https://t.me/PCHealthRemoteBot?start=src_github_release and send /link
2. On the PC: pc-health-remote.exe --link-telegram CODE, then restart the backend
Alerts: CPU/GPU temp, loads, RAM, VRAM, disk, battery. Pro (optional, 99 Telegram Stars / 30 days): unlimited alerts, up to 5 PCs.
Privacy: the bot stores only chat id, language, source tag, device keys, PC names and counters. Open source: telegram/worker.js.
```

### 3.2 r/flipperzero
Title: I made an open-source PC monitor for Flipper Zero, now it can also message you on Telegram
```
PC Health Remote shows CPU/GPU temps and loads on the Flipper (BLE or USB) with vibro/sound/LED alerts.
v1.1.0 adds Telegram alerts for when you are away from the desk. Free for 1 PC (10 alerts/day), open source,
Rust backend for Windows (Linux best-effort), NVIDIA-only GPU data. There is an optional paid Pro tier
(99 Stars/30 days); the free tier is fully usable. Bot stores only chat id, language, source tag, device keys,
PC names and counters. Code: https://github.com/ataman-lab/pc-health-remote
Try the bot: https://t.me/PCHealthRemoteBot?start=src_reddit_flipper
Feedback and bug reports welcome.
```
Check the sub's current rules and flair requirements before posting (not verified here).

### 3.3 Flipper forum (forum.flipper.net)
Title: PC Health Remote v1.1: PC temps on Flipper, plus optional Telegram alerts
```
Hi all. PC Health Remote is a free, open-source Flipper app (category Bluetooth) with a small Rust PC backend.
Works with official firmware (API 87.1) and RogueMaster (API 88.4). New in 1.1: each alert has its own sound,
and the PC can send the same alerts to Telegram (free: 1 PC, 10/day). Setup and protocol are documented in the repo.
Repo: https://github.com/ataman-lab/pc-health-remote
Telegram bot: https://t.me/PCHealthRemoteBot?start=src_flipper_forum
Known limits: GPU data is NVIDIA-only; some new CPUs need LibreHardwareMonitor for CPU temp.
```
Pick the right category and read forum rules first (not verified here).

### 3.4 r/pcmasterrace
Title: Free open-source tool: get Telegram alerts when your PC runs too hot
```
I built a small open-source monitor (Rust, Windows, Linux best-effort). It alerts you on Telegram when CPU/GPU temp,
load, RAM, VRAM, disk or battery cross your limits. Free for 1 PC (10 alerts/day), source on GitHub.
Optional Pro for more PCs/alerts. NVIDIA-only GPU data for now. I am the author.
Bot: https://t.me/PCHealthRemoteBot?start=src_reddit_pcmasterrace
Repo: https://github.com/ataman-lab/pc-health-remote
```
CAUTION (not verified, check the sidebar before posting): r/pcmasterrace is a large sub and, as I recall, restricts
self-promotion and may require a flair or only allow it on specific threads. This is an unverified recollection,
not a sourced fact. A removal or ban hurts more than skipping it. Disclose that you are the author, post once, no alts.

### 3.5 Telegram channel post (Russian)
```
PC Health Remote 1.1: алерты о перегреве ПК прямо в Telegram

Бесплатный проект с открытым кодом (Windows, Linux в режиме best-effort). Следит за температурой CPU/GPU,
загрузкой, RAM, VRAM, диском и батареей и пишет вам в Telegram, когда что-то идёт не так.

Бесплатно: 1 ПК, 10 уведомлений в сутки. Pro по желанию: 99 Telegram Stars за 30 дней, без лимита и до 5 ПК.

Как подключить: откройте бота, отправьте /link, на ПК выполните pc-health-remote.exe --link-telegram КОД.

Приватность: бот хранит только id чата, язык, метку источника, ключи устройств, имена ПК и счётчики.
Код открыт: https://github.com/ataman-lab/pc-health-remote

Бот: https://t.me/PCHealthRemoteBot?start=src_friends
```
Use `src_friends` for friends and chats. For a named public channel add a new tag (for example `tg_<channel>`) and a row in section 1.

## 4. Budget: $10 (ESTIMATE, not sourced)

| Item | Estimate | Note |
|---|---|---|
| GitHub release, Flipper forum, both subs, friends, TikTok | $0 | Free first; this is where the first 200 /start should come from |
| Small boost or ad test | up to $10 | Only after the pre-ad gate passes |
| Telegram Ads | unknown | I believe the minimum budget is far above $10 (estimate, unverified; check ads.telegram.org). If so, skip it at this budget |

Recommendation: spend $0 until the 200 /start target is reached or clearly stalled; keep $10 as reserve.

Kill line (current, decided 2026-11-08): zero Pro payments after 200 `/start` from the target audience -> rethink
price and features. Count only target-audience sources (not `friends`, not admin).

Pre-ad gate (all must pass before spending anything):
1. `/paysupport` answers (see note above).
2. Fresh non-admin Telegram account: `/start` -> `/link` -> run `--link-telegram` -> alert arrives -> `/pro` invoice opens.

## 5. Funnel to watch

Per source: opened -> linked a PC -> got an alert -> paid. Retention: still linked after 7 days.

| Metric | Measured today in /stats? |
|---|---|
| Opened (users per source) | Yes |
| PCs linked (total only) | Yes, total `PCs`, not per source |
| Pro users per source | Yes |
| Stars per source | Yes |
| Got an alert (per source) | No (only per-chat daily counters exist, not aggregated) |
| Linked per source (users with 1+ PC) | No (derivable from KV, not reported) |
| 7-day retention | Not measured |

Proposed next step (not done): extend `/stats` with per-source linked users and alert-received users.

## 6. Vlad's publishing checklist (lowest effort first)

1. Run the pre-ad gate on a fresh Telegram account; confirm `/paysupport`, `/terms`, `/support` answer.
2. Paste section 3.1 into the v1.1.0 release notes.
3. Send the Russian post (3.5) to friends and your channel.
4. Post to the Flipper forum (3.3) after reading its rules.
5. Post to r/flipperzero (3.2) after reading its rules.
6. Film Video B (`--simulate`, safest) and post to TikTok with the `src_tiktok` link.
7. Read r/pcmasterrace rules; post 3.4 only if self-promo is allowed.
8. Film Videos A and C.
9. After 7 days, send `/stats` to the bot and compare with the kill line.
