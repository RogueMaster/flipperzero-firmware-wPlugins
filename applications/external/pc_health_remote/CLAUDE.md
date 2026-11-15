# PC Health Remote

Part of Vlad's lab (overclock-lab is the home base: rules, board, departments). Talk to Vlad in Russian, short;
keep repo files in English. Product: Flipper Zero app (`flipper/`, C) + Rust PC backend (`backend/`) + Telegram bot
(`telegram/`, Cloudflare Worker `phr-telegram`, plain JS, no npm deps). Public repo, MIT.

## Working style (same as overclock-lab)
- Vlad only picks: questions as decision cards with 2-4 options, recommended first. Find out yourself what you can.
- Every change: before/after check with numbers and a rollback path. Smallest thing that works, free first.
- When Vlad does a manual step, give a "Сделал" button and re-check the result right after (never read secrets).
- First and final message of every thread/run ends with `model: <model>, subagents: <N> (...)`.

## Tasks and merges
- One issue per task on the overclock-lab board (GitHub Project "Overclock Lab"); PRs say `Closes #N`.
- `.github/` and deploy/infra changes are merged by Vlad. Auto-merge per task only with the `automerge` label
  after Vlad's card tap (overclock-lab docs/automerge.md).
- GitHub Actions budget is $5 for the whole lab: no macOS runners, no reruns without a known cause.

## Money and metrics
- Pro = 99 Telegram Stars / 30 days (`PRO_PRICE_STARS` in `telegram/worker.js`). Price changes need Vlad's card.
- Kill line (all lab bots): 0 payments after 200 `/start`. Admin `/stats` in the bot shows users, Pro and Stars
  per source tag. One deep link per traffic source: `https://t.me/PCHealthRemoteBot?start=src_<tag>`
  (tags in docs/launch/telegram-alerts.md section 1).
- Promotion goes through the overclock-lab marketing department (social-post queue + desk approval); never ask
  Vlad to post by hand where an official API exists. Every post says which account it is sent from.

## Commands
- Bot tests: `cd telegram && node --test`. Backend: `cd backend && cargo test`. Flipper: see `flipper/README.md`.
- Bot deploy runs from a clean worktree of main on Vlad's PC (`npx wrangler deploy` in `telegram/`), never from a
  feature branch. Rollback: `npx wrangler rollback`.

## Guardrails
- Secrets (`BOT_TOKEN`, `WEBHOOK_SECRET`, `ADMIN_CHAT_ID`, `HQ_TOKEN`) live only in `wrangler secret`; never commit,
  print or ask for them. The bot stores only chat id, language, source tag, device keys, PC names, counters and
  payment records; keep it that way.
- No invented user counts, reviews or testimonials in any copy. Market numbers need a source link.
- Clean-room only: never copy code, assets or UI one-to-one from HWiNFO, NZXT CAM or other tools.
