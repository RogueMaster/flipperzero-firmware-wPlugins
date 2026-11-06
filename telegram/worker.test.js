import { test } from "node:test";
import assert from "node:assert/strict";
import { handle, FREE_DAILY_LIMIT } from "./worker.js";

function mkKv() {
  const m = new Map();
  return {
    m,
    get: async (k) => (m.has(k) ? m.get(k) : null),
    put: async (k, v) => void m.set(k, v),
    delete: async (k) => void m.delete(k),
  };
}

function setup(t0 = Date.UTC(2026, 0, 1, 12)) {
  const sent = [];
  const clock = { t: t0 };
  const env = { PHR: mkKv(), BOT_TOKEN: "tok", WEBHOOK_SECRET: "sec" };
  const opts = {
    now: () => clock.t,
    fetch: async (url, init) => {
      sent.push(JSON.parse(init.body));
      return new Response("{}", { status: 200 });
    },
  };
  const call = (path, body, headers = {}, method = "POST") =>
    handle(new Request("https://x" + path, { method, headers, body: body && JSON.stringify(body) }), env, opts);
  const say = (text, from = {}, chat = 1) =>
    call("/tg", { message: { chat: { id: chat }, from, text } }, { "X-Telegram-Bot-Api-Secret-Token": "sec" });
  return { env, sent, clock, call, say };
}

async function pair(s, name = "GAMING-PC") {
  await s.say("/link");
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
  const before = s.sent.length;
  for (let i = 0; i < FREE_DAILY_LIMIT; i++) assert.equal((await s.call("/alert", a, h)).status, 200);
  assert.equal(s.sent.length - before, FREE_DAILY_LIMIT);
  assert.equal((await s.call("/alert", a, h)).status, 429);
  assert.equal((await s.call("/alert", a, h)).status, 429);
  assert.equal(s.sent.length - before, FREE_DAILY_LIMIT + 1);
  assert.match(s.sent.at(-1).text, /limit/i);
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
