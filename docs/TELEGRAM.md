# Telegram alerts

The PC backend runs the same alert rules as the Flipper app and sends fired alerts to a Telegram bot
(`telegram/worker.js`, a Cloudflare Worker). Flipper is optional.

## For users

1. Open **@PCHealthRemoteBot** in Telegram and send `/link`. You get a 6-character code, valid for 10 minutes.
2. Run `pc-health-remote.exe --link-telegram <CODE>`. The device key is saved to `config.toml`, section `[telegram]`.
3. Restart the backend (tray icon -> Quit, then start it again, or reboot).

Commands: `/link` (add a PC), `/pcs` (list), `/unlink <name>`, `/status` (plan, alerts today), `/pro` (upgrade).

| | Free | Pro |
|---|---|---|
| PCs | 1 | 5 |
| Alerts per day (UTC) | 10 | unlimited |
| Price | 0 | 99 Telegram Stars / 30 days, renews automatically, cancel in Telegram |

The same alert repeats at most every 5 minutes while the condition lasts.

**Privacy.** The bot stores only: chat id, language, the source tag from the start link, device keys, PC names,
daily counters and payment records. No temperatures history, no message text in logs. `/unlink` deletes a device key.

## For the maintainer (deploy)

Everything ships from `.github/workflows/telegram.yml` on every push to `main` that touches `telegram/`.
It runs the tests, finds or creates the KV namespace `phr-telegram-PHR`, runs `wrangler deploy`, copies the
secrets into the Worker (the webhook secret is derived from `BOT_TOKEN`), sets the Telegram webhook and checks `GET /` (the run summary shows the bot and Worker URL).
Without the secrets the deploy job is skipped with a notice.

Repository secrets (Settings -> Secrets and variables -> Actions):

| Secret | Where it comes from |
|---|---|
| `BOT_TOKEN` | @BotFather -> `/newbot` |
| `ADMIN_CHAT_ID` | your numeric Telegram id (for example from @userinfobot); enables `/stats` and `/refund` |
| `CLOUDFLARE_API_TOKEN` | dash.cloudflare.com -> My Profile -> API Tokens -> template "Edit Cloudflare Workers" |
| `CLOUDFLARE_ACCOUNT_ID` | dash.cloudflare.com -> Workers & Pages, right column "Account ID" |

Admin commands (only from `ADMIN_CHAT_ID`): `/stats` (users, Pro, payments, per source), `/refund <charge_id>`.

Tracking links per traffic source: `https://t.me/PCHealthRemoteBot?start=src_<tag>` (tag: letters, digits, `_`, `-`).

Limits to watch (Cloudflare free plan): 100k Worker requests/day, KV 1,000 writes/day (each delivered alert writes
one counter). `/pair` is limited to 5 attempts per IP per minute.

**Rollback:** `cd telegram && npx wrangler@4 rollback` (previous Worker version), or revert the commit on `main`.
To stop the bot: `https://api.telegram.org/bot<TOKEN>/deleteWebhook`.
