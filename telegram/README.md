# phr-telegram

Cloudflare Worker (plain JS ES module, no npm deps) that delivers PC Health Remote
alerts to Telegram. Endpoints: `POST /tg` (bot webhook), `POST /pair`, `POST /alert`, `GET /`.

Stores only chat id, language, source tag, device keys, PC names and counters
(KV binding `PHR`; the free daily alert counter lives in the `AlertCounter` Durable Object so alerts cost no KV writes). Secrets `BOT_TOKEN` and `WEBHOOK_SECRET` are set via
`wrangler secret put` and never committed. Free tier: 10 alerts per chat per UTC day.

Pro (Telegram Stars, `PRO_PRICE_STARS` in `worker.js`): `/pro` sends a 30-day subscription
invoice, Pro = no daily limit and up to 5 PCs (free: 1 PC). `/status` shows the plan.
Support: `/paysupport` (required by Telegram for Stars; the user's next message within 1 hour is forwarded privately to `ADMIN_CHAT_ID`, the admin answers with `/reply <chat_id> <text>`), `/terms`, `/support` (public GitHub issues, bugs only). Refunding the current charge also cancels the Stars subscription renewal. A free user who hits the daily limit or tries to link a second PC gets the Pro offer with an invoice link.
Admin-only `/stats`, `/refund <charge_id>` and `/setcommands` (fills the bot menu in EN + RU via `setMyCommands`; run once after deploy): set the secret `ADMIN_CHAT_ID`
(`wrangler secret put ADMIN_CHAT_ID`). The webhook must allow `pre_checkout_query` updates.

## Test

    cd telegram && node --test
