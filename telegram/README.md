# phr-telegram

Cloudflare Worker (plain JS ES module, no npm deps) that delivers PC Health Remote
alerts to Telegram. Endpoints: `POST /tg` (bot webhook), `POST /pair`, `POST /alert`, `GET /`.

Stores only chat id, language, source tag, device keys, PC names and counters
(KV binding `PHR`). Secrets `BOT_TOKEN` and `WEBHOOK_SECRET` are set via
`wrangler secret put` and never committed. Free tier: 10 alerts per chat per UTC day.

## Test

    cd telegram && node --test
