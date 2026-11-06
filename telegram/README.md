# phr-telegram

Cloudflare Worker (plain JS ES module, no npm deps) that delivers PC Health Remote
alerts to Telegram. Endpoints: `POST /tg` (bot webhook), `POST /pair`, `POST /alert`, `GET /`.

Stores only chat id, language, source tag, device keys, PC names and counters
(KV binding `PHR`). Secrets `BOT_TOKEN` and `WEBHOOK_SECRET` are set via
`wrangler secret put` and never committed. Free tier: 10 alerts per chat per UTC day.

Pro (Telegram Stars, `PRO_PRICE_STARS` in `worker.js`): `/pro` sends a 30-day subscription
invoice, Pro = no daily limit and up to 5 PCs (free: 1 PC). `/status` shows the plan.
Admin-only `/stats` and `/refund <charge_id>`: set the secret `ADMIN_CHAT_ID`
(`wrangler secret put ADMIN_CHAT_ID`). The webhook must allow `pre_checkout_query` updates.

## Test

    cd telegram && node --test
