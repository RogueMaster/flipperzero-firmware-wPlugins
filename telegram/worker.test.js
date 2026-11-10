import { test } from "node:test";
import assert from "node:assert/strict";
import { handle, AlertCounter, FREE_DAILY_LIMIT, PRO_PRICE_STARS } from "./worker.js";

function mkKv() {
  const m = new Map();
  const kv = {
    m,
    writes: 0,
    get: async (k) => (m.has(k) ? m.get(k) : null),
    put: async (k, v) => {
      kv.writes++;
      m.set(k, v);
    },
    delete: async (k) => void m.delete(k),
    list: async ({ prefix }) => ({ keys: [...m.keys()].filter((k) => k.startsWith(prefix)).map((name) => ({ name })), list_complete: true }),
  };
  return kv;
}

// Fake Durable Object namespace: one AlertCounter per name, storage in a Map, alarms recorded.
function mkCounterNs() {
  const objs = new Map();
  const ns = {
    objs,
    idFromName: (name) => name,
    get: (id) => {
      if (!objs.has(id)) {
        const data = new Map();
        const storage = {
          data,
          alarm: null,
          get: async (k) => data.get(k),
          put: async (k, v) => void data.set(k, structuredClone(v)),
          setAlarm: async (t) => void (storage.alarm = t),
          deleteAll: async () => void data.clear(),
        };
        objs.set(id, new AlertCounter({ storage }));
      }
      const obj = objs.get(id);
      return { fetch: (url, init) => obj.fetch(new Request(url, init)) };
    },
  };
  return ns;
}

function setup(t0 = Date.UTC(2026, 0, 1, 12), { counter = false } = {}) {
  const sent = [];
  const calls = [];
  const clock = { t: t0 };
  const flags = {};
  const env = { PHR: mkKv(), BOT_TOKEN: "tok", WEBHOOK_SECRET: "sec", ADMIN_CHAT_ID: "999" };
  if (counter) env.ALERT_COUNTER = mkCounterNs();
  const opts = {
    now: () => clock.t,
    fetch: async (url, init) => {
      if (flags.failInvoice && url.endsWith("/createInvoiceLink")) throw new TypeError("network down");
      sent.push(init.body instanceof FormData ? Object.fromEntries(init.body) : JSON.parse(init.body));
      calls.push({ method: url.split("/").pop(), body: sent.at(-1) });
      return new Response('{"ok":true,"result":"https://t.me/$inv"}', { status: 200 });
    },
  };
  const call = (path, body, headers = {}, method = "POST") =>
    handle(new Request("https://x" + path, { method, headers, body: body && JSON.stringify(body) }), env, opts);
  const say = (text, from = {}, chat = 1) =>
    call("/tg", { message: { chat: { id: chat }, from, text } }, { "X-Telegram-Bot-Api-Secret-Token": "sec" });
  const hook = { "X-Telegram-Bot-Api-Secret-Token": "sec" };
  const upd = (u) => call("/tg", u, hook);
  const pay = (id, chat = 1, extra = {}) =>
    upd({
      message: {
        chat: { id: chat },
        successful_payment: {
          currency: "XTR",
          total_amount: PRO_PRICE_STARS,
          invoice_payload: "pro30:" + chat,
          telegram_payment_charge_id: id,
          subscription_expiration_date: Math.floor(clock.t / 1000) + 2592000,
          ...extra,
        },
      },
    });
  return { env, sent, calls, clock, call, say, upd, pay, flags };
}

async function pair(s, name = "GAMING-PC", chat = 1) {
  await s.say("/link", {}, chat);
  const code = /: ([A-Z0-9]{6})/.exec(s.sent.at(-1).text)[1];
  const r = await s.call("/pair", { code, pc_name: name });
  return { code, r, body: await r.clone().json() };
}

test("GET / is up", async () => {
  const s = setup();
  const r = await s.call("/", null, {}, "GET");
  assert.equal(await r.text(), "phr-telegram up");
});

test("webhook secret check", async () => {
  const s = setup();
  const upd = { message: { chat: { id: 1 }, text: "/start" } };
  const bad = await s.call("/tg", upd, { "X-Telegram-Bot-Api-Secret-Token": "no" });
  assert.equal(bad.status, 403);
  const none = await s.call("/tg", upd);
  assert.equal(none.status, 403);
  assert.equal(s.sent.length, 0);
});

test("/start greets by language and stores source tag", async () => {
  const s = setup();
  await s.say("/start src_reddit", { language_code: "ru" });
  assert.match(s.sent[0].text, /Привет/);
  assert.equal(JSON.parse(s.env.PHR.m.get("user:1")).src, "reddit");
  const s2 = setup();
  await s2.say("/start", { language_code: "en" });
  assert.match(s2.sent[0].text, /Hi!/);
  assert.equal(JSON.parse(s2.env.PHR.m.get("user:1")).src, null);
});

test("pairing: code works once and returns device key", async () => {
  const s = setup();
  const { code, r, body } = await pair(s);
  assert.equal(r.status, 200);
  assert.match(body.device_key, /^[0-9a-f]{64}$/);
  const again = await s.call("/pair", { code, pc_name: "X" });
  assert.equal(again.status, 400);
  await s.say("/pcs");
  assert.match(s.sent.at(-1).text, /GAMING-PC/);
});

test("pairing: code expires after 10 min", async () => {
  const s = setup();
  await s.say("/link");
  const code = /: ([A-Z0-9]{6})/.exec(s.sent.at(-1).text)[1];
  s.clock.t += 10 * 60 * 1000 + 1;
  const r = await s.call("/pair", { code, pc_name: "PC" });
  assert.equal(r.status, 400);
});

test("alert delivery", async () => {
  const s = setup();
  const { body } = await pair(s);
  const r = await s.call(
    "/alert",
    { rule: "cpu_temp", value: 92, threshold: 85, unit: "C", pc_name: "GAMING-PC" },
    { "X-Device-Key": body.device_key },
  );
  assert.equal(r.status, 200);
  assert.equal(s.sent.at(-1).text, "🔥 GAMING-PC: CPU temp 92°C (limit 85°C)");
  assert.equal(s.sent.at(-1).chat_id, 1);
});

test("alert with bad or missing key is 401", async () => {
  const s = setup();
  const body = { rule: "cpu_load", value: 1 };
  assert.equal((await s.call("/alert", body)).status, 401);
  assert.equal((await s.call("/alert", body, { "X-Device-Key": "nope" })).status, 401);
  assert.equal(s.sent.length, 0);
});

test("daily limit, one note, resets next UTC day", async () => {
  const s = setup();
  const { body } = await pair(s);
  const h = { "X-Device-Key": body.device_key };
  const a = { rule: "cpu_temp", value: 90, threshold: 85, unit: "C" };
  const msgs = () => s.calls.filter((c) => c.method === "sendMessage").length;
  const before = msgs();
  for (let i = 0; i < FREE_DAILY_LIMIT; i++) assert.equal((await s.call("/alert", a, h)).status, 200);
  assert.equal(msgs() - before, FREE_DAILY_LIMIT);
  assert.equal((await s.call("/alert", a, h)).status, 429);
  assert.equal((await s.call("/alert", a, h)).status, 429);
  assert.equal(msgs() - before, FREE_DAILY_LIMIT + 1);
  assert.match(s.sent.at(-1).text, /limit/i);
  // Upsell: the one limit note carries the Pro invoice link.
  assert.match(s.sent.at(-1).text, /https:\/\/t\.me\/\$inv/);
  assert.equal(s.calls.filter((c) => c.method === "createInvoiceLink").length, 1);
  s.clock.t += 24 * 3600 * 1000;
  assert.equal((await s.call("/alert", a, h)).status, 200);
});

test("/unlink revokes the device key", async () => {
  const s = setup();
  const { body } = await pair(s);
  await s.say("/unlink GAMING-PC");
  const r = await s.call("/alert", { rule: "cpu_load", value: 1 }, { "X-Device-Key": body.device_key });
  assert.equal(r.status, 401);
  await s.say("/pcs");
  assert.match(s.sent.at(-1).text, /No linked/);
});

test("alert rejects unknown rule, non-numeric value and keeps the paired PC name", async () => {
  const s = setup();
  const { body } = await pair(s);
  const h = { "X-Device-Key": body.device_key };
  const before = s.sent.length;
  assert.equal((await s.call("/alert", { rule: "Click http://evil", value: 1 }, h)).status, 400);
  assert.equal((await s.call("/alert", { rule: "cpu_temp", value: "99 visit x.y" }, h)).status, 400);
  assert.equal((await s.call("/alert", { rule: "cpu_temp", value: 9, threshold: "x" }, h)).status, 400);
  assert.equal((await s.call("/alert", { rule: "__proto__", value: 1 }, h)).status, 400);
  assert.equal(s.sent.length, before);
  const r = await s.call("/alert", { rule: "ram_load", value: 97, pc_name: "Spoofed text" }, h);
  assert.equal(r.status, 200);
  assert.equal(s.sent.at(-1).text, "🔥 GAMING-PC: RAM 97%");
});

test("/pro creates a Stars subscription invoice link", async () => {
  const s = setup();
  await s.say("/pro", {}, 5);
  const c = s.calls.find((x) => x.method === "createInvoiceLink");
  assert.equal(c.body.currency, "XTR");
  assert.equal(c.body.subscription_period, 2592000);
  assert.deepEqual(c.body.prices, [{ label: "Pro 30d", amount: PRO_PRICE_STARS }]);
  assert.equal(c.body.payload, "pro30:5");
  assert.match(s.sent.at(-1).text, /https:\/\/t\.me\/\$inv/);
});

test("pre_checkout_query: ok for valid payload, rejected otherwise", async () => {
  const s = setup();
  const q = (over) => ({
    pre_checkout_query: { id: "q1", from: { id: 7 }, currency: "XTR", total_amount: PRO_PRICE_STARS, invoice_payload: "pro30:7", ...over },
  });
  await s.upd(q({}));
  assert.equal(s.calls.at(-1).method, "answerPreCheckoutQuery");
  assert.equal(s.calls.at(-1).body.ok, true);
  await s.upd(q({ invoice_payload: "pro30:8" }));
  assert.equal(s.calls.at(-1).body.ok, false);
  await s.upd(q({ total_amount: 1 }));
  assert.equal(s.calls.at(-1).body.ok, false);
});

test("successful_payment counted once; duplicate ignored; records source", async () => {
  const s = setup();
  await s.say("/start src_reddit", {}, 1);
  await s.pay("ch1");
  const n = s.env.PHR.m.size;
  const sentN = s.sent.length;
  await s.pay("ch1");
  assert.equal(s.env.PHR.m.size, n);
  assert.equal(s.sent.length, sentN);
  const u = JSON.parse(s.env.PHR.m.get("user:1"));
  assert.equal(u.pro.until, s.clock.t + 2592000 * 1000);
  assert.equal(JSON.parse(s.env.PHR.m.get("pay:ch1")).src, "reddit");
  await s.pay("bad", 1, { invoice_payload: "pro30:2" });
  assert.equal(s.env.PHR.m.has("pay:bad"), false);
});

test("Pro expires; free vs Pro alert limits (fresh non-admin user)", async () => {
  const s = setup();
  const { body } = await pair(s);
  await s.pay("ch1");
  const h = { "X-Device-Key": body.device_key };
  const a = { rule: "cpu_temp", value: 90 };
  for (let i = 0; i < FREE_DAILY_LIMIT + 5; i++) assert.equal((await s.call("/alert", a, h)).status, 200);
  await s.say("/status");
  assert.match(s.sent.at(-1).text, /Plan: Pro/);
  s.clock.t += 31 * 24 * 3600 * 1000;
  await s.say("/status");
  assert.match(s.sent.at(-1).text, /Plan: Free/);
  for (let i = 0; i < FREE_DAILY_LIMIT; i++) assert.equal((await s.call("/alert", a, h)).status, 200);
  assert.equal((await s.call("/alert", a, h)).status, 429);
});

test("PC limit: free 1, Pro 5", async () => {
  const s = setup();
  assert.equal((await pair(s, "A")).r.status, 200);
  assert.equal((await pair(s, "B")).r.status, 403);
  assert.equal((await pair(s, "A")).r.status, 200); // re-pair same name is fine
  await s.pay("ch1");
  for (const n of ["B", "C", "D", "E"]) assert.equal((await pair(s, n)).r.status, 200);
  assert.equal((await pair(s, "F")).r.status, 403);
});

test("/stats and /refund are admin only", async () => {
  const s = setup();
  await s.say("/start src_x", {}, 1);
  await s.pay("ch1");
  const before = s.sent.length;
  await s.say("/stats", {}, 1);
  await s.say("/refund ch1", {}, 1);
  assert.equal(s.sent.length, before);
  assert.equal(s.calls.some((c) => c.method === "refundStarPayment"), false);
  await s.say("/stats", {}, 999);
  assert.match(s.sent.at(-1).text, /Users: 1[\s\S]*Pro: 1[\s\S]*Stars: 99[\s\S]*x: users 1, pro 1, stars 99/);
  await s.say("/refund ch1", {}, 999);
  const r = s.calls.find((c) => c.method === "refundStarPayment");
  assert.deepEqual(r.body, { user_id: 1, telegram_payment_charge_id: "ch1" });
  await s.say("/status", {}, 1);
  assert.match(s.sent.at(-1).text, /Plan: Free/);
  await s.say("/stats", {}, 999);
  assert.match(s.sent.at(-1).text, /Stars: 0/);
});

test("/pair is rate limited per IP when PAIR_LIMIT is bound", async () => {
  const s = setup();
  const seen = [];
  s.env.PAIR_LIMIT = { limit: async ({ key }) => (seen.push(key), { success: seen.length <= 2 }) };
  const h = { "CF-Connecting-IP": "203.0.113.7" };
  assert.equal((await s.call("/pair", { code: "AAAAAA", pc_name: "x" }, h)).status, 400);
  assert.equal((await s.call("/pair", { code: "BBBBBB", pc_name: "x" }, h)).status, 400);
  assert.equal((await s.call("/pair", { code: "CCCCCC", pc_name: "x" }, h)).status, 429);
  assert.deepEqual(seen, ["203.0.113.7", "203.0.113.7", "203.0.113.7"]);
});

// KV write budget (issue #27): free tier = 1000 KV writes/day.
async function alertWrites(counter, users, perUser) {
  const s = setup(undefined, { counter });
  const keys = [];
  for (let u = 0; u < users; u++) keys.push((await pair(s, "PC" + u, 100 + u)).body.device_key);
  const a = { rule: "cpu_temp", value: 90 };
  const before = s.env.PHR.writes;
  const codes = [];
  for (const k of keys) for (let i = 0; i < perUser; i++) codes.push((await s.call("/alert", a, { "X-Device-Key": k })).status);
  return { writes: s.env.PHR.writes - before, codes, s };
}

test("KV writes per 100 alerts: before (KV counter) vs after (Durable Object counter)", async () => {
  const kv10x10 = await alertWrites(false, 10, 10);
  const do10x10 = await alertWrites(true, 10, 10);
  const kv1x100 = await alertWrites(false, 1, 100);
  const do1x100 = await alertWrites(true, 1, 100);
  console.log("KV writes per 100 alerts, 10 users x 10:", kv10x10.writes, "->", do10x10.writes);
  console.log("KV writes per 100 alerts, 1 user x 100:", kv1x100.writes, "->", do1x100.writes);
  assert.equal(kv10x10.writes, 100);
  assert.equal(do10x10.writes, 0);
  assert.equal(kv1x100.writes, 11);
  assert.equal(do1x100.writes, 0);
  assert.deepEqual(do10x10.codes, kv10x10.codes);
  assert.deepEqual(do1x100.codes, kv1x100.codes);
});

test("Durable Object counter: limit enforced, one note, resets next UTC day, /status shows usage", async () => {
  const s = setup(undefined, { counter: true });
  const { body } = await pair(s);
  const h = { "X-Device-Key": body.device_key };
  const a = { rule: "cpu_temp", value: 90 };
  const msgs = () => s.calls.filter((c) => c.method === "sendMessage").length;
  const before = msgs();
  for (let i = 0; i < FREE_DAILY_LIMIT; i++) assert.equal((await s.call("/alert", a, h)).status, 200);
  assert.equal((await s.call("/alert", a, h)).status, 429);
  assert.equal((await s.call("/alert", a, h)).status, 429);
  assert.equal(msgs() - before, FREE_DAILY_LIMIT + 1); // 10 alerts + one limit note (with Pro offer)
  await s.say("/status");
  assert.match(s.sent.at(-1).text, new RegExp(String(FREE_DAILY_LIMIT)));
  s.clock.t += 24 * 3600 * 1000;
  assert.equal((await s.call("/alert", a, h)).status, 200);
});

test("Durable Object counter forgets the day via its alarm", async () => {
  const s = setup(undefined, { counter: true });
  const { body } = await pair(s);
  await s.call("/alert", { rule: "cpu_temp", value: 90 }, { "X-Device-Key": body.device_key });
  const [obj] = s.env.ALERT_COUNTER.objs.values();
  assert.ok(obj.storage.alarm > Date.now());
  await obj.alarm();
  assert.equal(obj.storage.data.size, 0);
});

test("/paysupport, /terms, /support answer in EN and RU", async () => {
  const s = setup();
  for (const [cmd, re] of [["/paysupport", /refundable/i], ["/terms", /99 Stars per 30 days/], ["/support", /github\.com\/ataman-lab\/pc-health-remote\/issues/]]) {
    await s.say(cmd, { language_code: "en" }, 7);
    assert.match(s.sent.at(-1).text, re);
  }
  for (const [cmd, re] of [["/paysupport", /возврат/i], ["/terms", /99 Stars за 30 дней/], ["/support", /issue/]]) {
    await s.say(cmd, { language_code: "ru" }, 7);
    assert.match(s.sent.at(-1).text, re);
  }
  await s.say("/start", { language_code: "en" }, 7);
  assert.match(s.sent.at(-1).text, /\/help/);
});

test("PC limit: free user is told in chat with a Pro offer; Pro user without offer", async () => {
  const s = setup();
  await pair(s, "A");
  assert.equal((await pair(s, "B")).r.status, 403);
  assert.match(s.sent.at(-1).text, /not linked.*free plan/s);
  assert.match(s.sent.at(-1).text, /https:\/\/t\.me\/\$inv/);
  assert.equal(s.sent.at(-1).chat_id, 1);
  await s.pay("ch1");
  for (const n of ["B", "C", "D", "E"]) await pair(s, n);
  const inv = s.calls.filter((c) => c.method === "createInvoiceLink").length;
  assert.equal((await pair(s, "F")).r.status, 403);
  assert.match(s.sent.at(-1).text, /Pro allows up to 5/);
  assert.equal(s.calls.filter((c) => c.method === "createInvoiceLink").length, inv);
});

test("/setcommands is admin only and sets EN + RU menus", async () => {
  const s = setup();
  await s.say("/setcommands", {}, 1);
  assert.equal(s.calls.some((c) => c.method === "setMyCommands"), false);
  await s.say("/setcommands", {}, 999);
  const sc = s.calls.filter((c) => c.method === "setMyCommands");
  assert.equal(sc.length, 2);
  assert.ok(sc[0].body.commands.some((c) => c.command === "paysupport"));
  assert.equal(sc[1].body.language_code, "ru");
  assert.equal(s.sent.at(-1).text, "Commands set");
});

test("/brand is admin only: en default + uk/ru descriptions, EN/RU menus, JPEG avatar", async () => {
  const { PROFILE } = await import("./profile.js");
  const { checkProfile } = await import("./bot-profile.js");
  assert.deepEqual(checkProfile(PROFILE), []);
  const s = setup();
  await s.say("/brand", {}, 1);
  assert.equal(s.calls.some((c) => c.method.startsWith("setMy")), false);
  await s.say("/brand", {}, 999);
  const desc = s.calls.filter((c) => c.method === "setMyDescription");
  assert.deepEqual(desc.map((c) => c.body.language_code), [undefined, "uk", "ru"]);
  const menus = s.calls.filter((c) => c.method === "setMyCommands");
  assert.deepEqual(menus.map((c) => c.body.language_code), [undefined, "ru"]);
  assert.ok(menus[0].body.commands.some((c) => c.command === "paysupport"));
  assert.equal(s.calls.find((c) => c.method === "setMyProfilePhoto").body.avatar.type, "image/jpeg");
  assert.match(s.sent.at(-1).text, /^Profile set/);
});

test("/brand reports failed steps (ok:false, network error, bad JSON) without the token", async () => {
  const replies = [];
  const env = { PHR: mkKv(), BOT_TOKEN: "123:secret-token", WEBHOOK_SECRET: "sec", ADMIN_CHAT_ID: "999" };
  const f = async (url, init) => {
    const method = url.split("/").pop();
    if (method === "sendMessage") { replies.push(JSON.parse(init.body).text); return new Response('{"ok":true,"result":{}}'); }
    if (method === "setMyShortDescription") return new Response('{"ok":false,"description":"bad 123:secret-token"}');
    if (method === "setMyProfilePhoto") throw new TypeError("network down 123:secret-token");
    if (method === "setMyCommands") return new Response("not json");
    return new Response('{"ok":true,"result":true}');
  };
  await handle(new Request("https://x/tg", { method: "POST", headers: { "X-Telegram-Bot-Api-Secret-Token": "sec" },
    body: JSON.stringify({ message: { chat: { id: 999 }, from: {}, text: "/brand" } }) }), env, { now: () => 0, fetch: f });
  const reply = replies.at(-1);
  assert.match(reply, /^Profile: failed short:en, commands:en, short:uk, short:ru, commands:ru, photo$/);
  assert.equal(reply.includes("secret-token"), false);
});

test("/paysupport: next text goes privately to the admin once; admin answers with /reply", async () => {
  const s = setup();
  await s.say("hello", {}, 7); // free text without /paysupport is ignored
  assert.equal(s.sent.length, 0);
  await s.say("/paysupport", { language_code: "en" }, 7);
  await s.say("paid 99 stars yesterday, no Pro", {}, 7);
  const toAdmin = s.sent.find((m) => m.chat_id === "999");
  assert.match(toAdmin.text, /from 7/);
  assert.match(toAdmin.text, /paid 99 stars/);
  assert.match(s.sent.at(-1).text, /Sent to the developer/);
  const n = s.sent.length;
  await s.say("second message", {}, 7); // window used up
  assert.equal(s.sent.length, n);
  await s.say("/reply 7 Refunded, sorry", {}, 1); // not admin
  assert.equal(s.sent.length, n);
  await s.say("/reply 7 Refunded, sorry", {}, 999);
  assert.deepEqual([s.sent.at(-2).chat_id, s.sent.at(-2).text], [7, "Refunded, sorry"]);
  assert.equal(s.sent.at(-1).text, "Sent");
});

test("/paysupport window expires after 1 hour", async () => {
  const s = setup();
  await s.say("/paysupport", {}, 7);
  s.clock.t += 3601 * 1000;
  const n = s.sent.length;
  await s.say("late", {}, 7);
  assert.equal(s.sent.length, n);
});

test("/refund of the current charge also cancels the Stars subscription", async () => {
  const s = setup();
  await s.pay("ch1", 5);
  await s.say("/refund ch1", {}, 999);
  const c = s.calls.find((x) => x.method === "editUserStarSubscription");
  assert.deepEqual(c.body, { user_id: 5, telegram_payment_charge_id: "ch1", is_canceled: true });
});

test("upsell: no invoice in group chats; network error falls back to the plain limit text", async () => {
  const s = setup();
  await pair(s, "A", -100);
  assert.equal((await pair(s, "B", -100)).r.status, 403);
  assert.equal(s.calls.some((c) => c.method === "createInvoiceLink"), false);
  assert.match(s.sent.at(-1).text, /free plan/);
  // network error on createInvoiceLink: the limit note still arrives, without the offer
  const s2 = setup();
  const { body } = await pair(s2);
  const h = { "X-Device-Key": body.device_key };
  s2.flags.failInvoice = true;
  for (let i = 0; i < FREE_DAILY_LIMIT; i++) await s2.call("/alert", { rule: "cpu_temp", value: 90 }, h);
  assert.equal((await s2.call("/alert", { rule: "cpu_temp", value: 90 }, h)).status, 429);
  assert.match(s2.sent.at(-1).text, /limit/i);
  assert.doesNotMatch(s2.sent.at(-1).text, /t\.me/);
  assert.equal((await pair(s2, "B")).r.status, 403);
  assert.match(s2.sent.at(-1).text, /free plan/);
});

test("/status shows the chat id and marks the admin", async () => {
  const s = setup();
  await s.say("/status", {}, 42);
  assert.match(s.sent.at(-1).text, /Chat id: 42$/);
  await s.say("/status", {}, 999);
  assert.match(s.sent.at(-1).text, /Chat id: 999 \(admin\)/);
});

test("/start and /help show the reply keyboard; buttons act as commands", async () => {
  const s = setup();
  await s.say("/start", { language_code: "ru" }, 3);
  const kb = s.sent.at(-1).reply_markup;
  assert.equal(kb.is_persistent, true);
  assert.deepEqual(kb.keyboard.flat().map((b) => b.text), ["🔗 Привязать ПК", "💻 Мои ПК", "📊 Статус", "⭐ Pro", "❓ Помощь"]);
  await s.say("📊 Статус", { language_code: "ru" }, 3);
  assert.match(s.sent.at(-1).text, /Тариф: Free/);
  await s.say("🔗 Link PC", { language_code: "en" }, 3);
  assert.match(s.sent.at(-1).text, /Your code: [A-Z0-9]{6}/);
  await s.say("/commands", { language_code: "en" }, 3);
  assert.match(s.sent.at(-1).text, /\/paysupport/);
  assert.ok(s.sent.at(-1).reply_markup.keyboard);
  // a button label is never forwarded as a support message
  await s.say("/paysupport", {}, 3);
  const n = s.sent.length;
  await s.say("⭐ Pro", {}, 3);
  assert.equal(s.sent.filter((m) => m.chat_id === "999").length, 0);
  assert.ok(s.sent.length > n);
});

test("Pro offer carries an inline pay button with the invoice link", async () => {
  const s = setup();
  await s.say("/pro", { language_code: "en" }, 5);
  const m = s.sent.at(-1);
  assert.deepEqual(m.reply_markup, { inline_keyboard: [[{ text: "Pay 99 ⭐ for 30 days", url: "https://t.me/$inv" }]] });
  await pair(s, "A", 5);
  await pair(s, "B", 5);
  assert.match(s.sent.at(-1).text, /free plan/);
  assert.equal(s.sent.at(-1).reply_markup.inline_keyboard[0][0].url, "https://t.me/$inv");
});

test("navigation: Help submenu with Back, My PCs with per-PC unlink buttons", async () => {
  const s = setup();
  const labels = (m) => m.reply_markup.keyboard.flat().map((b) => b.text);
  await s.say("❓ Help", { language_code: "en" }, 8);
  assert.deepEqual(labels(s.sent.at(-1)), ["📋 Commands", "💬 Support", "💳 Payment help", "📜 Terms", "⬅️ Back"]);
  await s.say("📜 Terms", { language_code: "en" }, 8);
  assert.match(s.sent.at(-1).text, /Terms of PC Health Remote Pro/);
  await s.say("⬅️ Back", { language_code: "en" }, 8);
  assert.equal(s.sent.at(-1).text, "Main menu");
  assert.deepEqual(labels(s.sent.at(-1)), ["🔗 Link PC", "💻 My PCs", "📊 Status", "⭐ Pro", "❓ Help"]);
  // PCs submenu
  await s.say("💻 My PCs", { language_code: "ru" }, 8);
  assert.match(s.sent.at(-1).text, /Нет привязанных ПК/);
  await pair(s, "GAMING-PC", 8);
  await s.pay("ch8", 8);
  await pair(s, "Office PC", 8);
  await s.say("💻 Мои ПК", { language_code: "ru" }, 8);
  assert.deepEqual(labels(s.sent.at(-1)), ["❌ Отвязать: GAMING-PC", "❌ Отвязать: Office PC", "⬅️ Назад"]);
  await s.say("❌ Отвязать: Office PC", { language_code: "ru" }, 8);
  assert.match(s.sent.at(-1).text, /Отвязан: Office PC/);
  assert.deepEqual(labels(s.sent.at(-1)), ["❌ Отвязать: GAMING-PC", "⬅️ Назад"]);
  await s.say("❌ Отвязать: GAMING-PC", { language_code: "ru" }, 8);
  assert.deepEqual(labels(s.sent.at(-1)), ["🔗 Привязать ПК", "💻 Мои ПК", "📊 Статус", "⭐ Pro", "❓ Помощь"]);
  // unknown name via button text is a normal "no such PC", never a support message
  await s.say("❌ Отвязать: Ghost", { language_code: "ru" }, 8);
  assert.match(s.sent.at(-1).text, /Нет такого ПК: Ghost/);
});

test("default export applies the profile automatically once (GET / after deploy)", async (t) => {
  const { default: w } = await import("./worker.js");
  const calls = [];
  t.mock.method(globalThis, "fetch", async (url) => { calls.push(url.split("/").pop()); return new Response('{"ok":true}'); });
  const env = { PHR: mkKv(), BOT_TOKEN: "tok", WEBHOOK_SECRET: "sec", ADMIN_CHAT_ID: "999" };
  const waits = [];
  const ctx = { waitUntil: (p) => waits.push(p) };
  assert.equal((await w.fetch(new Request("https://x/"), env, ctx)).status, 200);
  await Promise.all(waits);
  assert.deepEqual(calls.filter((m) => m === "setMyDescription").length, 3);
  assert.ok(calls.includes("setMyProfilePhoto"));
  const n = calls.length;
  await w.fetch(new Request("https://x/"), env, ctx);
  await Promise.all(waits);
  assert.equal(calls.length, n, "second request: already applied");
});
