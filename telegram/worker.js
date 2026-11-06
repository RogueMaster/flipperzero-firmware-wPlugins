// PC Health Remote - Telegram bot (Cloudflare Worker, plain ES module).
// Stores only: chat id, language, source tag, device keys, pc names, counters.
// Logs never contain keys or message text.

export const FREE_DAILY_LIMIT = 10; // alerts per chat per UTC day (Pro comes later)
export const CODE_TTL_SEC = 600;
const CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

// Rule keys sent by the backend (backend/src/alerts.rs Rule::key) and their display units.
export const RULES = {
  cpu_temp: ["CPU temp", "°C"],
  gpu_temp: ["GPU temp", "°C"],
  cpu_load: ["CPU load", "%"],
  gpu_load: ["GPU load", "%"],
  ram_load: ["RAM", "%"],
  vram_load: ["VRAM", "%"],
  disk_load: ["Disk", "%"],
  battery_low: ["Battery", "%"],
};
const isNum = (v) => typeof v === "number" && Number.isFinite(v);

const GREETING = {
  en: "Hi! I send PC Health Remote alerts to your phone.\n\n1. Send /link to get a 6-character code.\n2. Enter it in the PC tray app.\n\nCommands: /link, /pcs, /unlink <name>",
  ru: "Привет! Я присылаю уведомления PC Health Remote на телефон.\n\n1. Отправьте /link, чтобы получить 6-значный код.\n2. Введите его в трей-приложении на ПК.\n\nКоманды: /link, /pcs, /unlink <имя>",
};

const T = {
  en: {
    code: (c) => "Your code: " + c + "\nValid for 10 minutes. Enter it in the PC tray app.",
    nopcs: "No linked PCs. Send /link to add one.",
    pcs: "Linked PCs:",
    unlinkUsage: "Usage: /unlink <name>",
    unlinked: (n) => "Unlinked: " + n,
    notFound: (n) => "No such PC: " + n,
    limit: "Daily free limit of " + FREE_DAILY_LIMIT + " alerts reached. More alerts will be dropped until tomorrow (UTC).",
  },
  ru: {
    code: (c) => "Ваш код: " + c + "\nДействует 10 минут. Введите его в трей-приложении на ПК.",
    nopcs: "Нет привязанных ПК. Отправьте /link, чтобы добавить.",
    pcs: "Привязанные ПК:",
    unlinkUsage: "Использование: /unlink <имя>",
    unlinked: (n) => "Отвязан: " + n,
    notFound: (n) => "Нет такого ПК: " + n,
    limit: "Достигнут бесплатный лимит: " + FREE_DAILY_LIMIT + " уведомлений в сутки. Остальные до завтра (UTC) не будут отправлены.",
  },
};

const json = (obj, status = 200) =>
  new Response(JSON.stringify(obj), { status, headers: { "content-type": "application/json" } });

const langOf = (code) => (String(code || "").toLowerCase().startsWith("ru") ? "ru" : "en");

function randomHex(bytes) {
  const a = new Uint8Array(bytes);
  crypto.getRandomValues(a);
  return [...a].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function randomCode() {
  const a = new Uint8Array(6);
  crypto.getRandomValues(a);
  return [...a].map((b) => CODE_ALPHABET[b % CODE_ALPHABET.length]).join("");
}

function safeEqual(a, b) {
  if (typeof a !== "string" || typeof b !== "string" || a.length !== b.length) return false;
  let r = 0;
  for (let i = 0; i < a.length; i++) r |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return r === 0;
}

async function getJson(kv, key) {
  const v = await kv.get(key);
  return v ? JSON.parse(v) : null;
}

async function send(env, f, chat, text) {
  const res = await f("https://api.telegram.org/bot" + env.BOT_TOKEN + "/sendMessage", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ chat_id: chat, text }),
  });
  return res.ok;
}

async function getUser(kv, chat) {
  return (await getJson(kv, "user:" + chat)) || { lang: "en", src: null, pcs: {} };
}

async function handleMessage(msg, env, f, now) {
  const chat = msg.chat && msg.chat.id;
  if (chat === undefined || typeof msg.text !== "string") return;
  const kv = env.PHR;
  const user = await getUser(kv, chat);
  user.lang = langOf(msg.from && msg.from.language_code);
  const t = T[user.lang];
  const [cmdRaw, ...rest] = msg.text.trim().split(/\s+/);
  const cmd = cmdRaw.split("@")[0].toLowerCase();

  if (cmd === "/start") {
    const m = /^src_([A-Za-z0-9_-]{1,32})$/.exec(rest[0] || "");
    if (m) user.src = m[1];
    await kv.put("user:" + chat, JSON.stringify(user));
    await send(env, f, chat, GREETING[user.lang]);
  } else if (cmd === "/link") {
    const code = randomCode();
    await kv.put("code:" + code, JSON.stringify({ chat, exp: now() + CODE_TTL_SEC * 1000 }), {
      expirationTtl: CODE_TTL_SEC,
    });
    await kv.put("user:" + chat, JSON.stringify(user));
    await send(env, f, chat, t.code(code));
  } else if (cmd === "/pcs") {
    const names = Object.keys(user.pcs);
    await send(env, f, chat, names.length ? t.pcs + "\n" + names.join("\n") : t.nopcs);
  } else if (cmd === "/unlink") {
    const name = rest.join(" ");
    if (!name) return void (await send(env, f, chat, t.unlinkUsage));
    if (!(name in user.pcs)) return void (await send(env, f, chat, t.notFound(name)));
    await kv.delete("dev:" + user.pcs[name]);
    delete user.pcs[name];
    await kv.put("user:" + chat, JSON.stringify(user));
    await send(env, f, chat, t.unlinked(name));
  }
}

async function handleWebhook(req, env, f, now) {
  const got = req.headers.get("X-Telegram-Bot-Api-Secret-Token");
  if (!env.WEBHOOK_SECRET || !safeEqual(got, env.WEBHOOK_SECRET)) {
    return json({ error: "forbidden" }, 403);
  }
  let update;
  try {
    update = await req.json();
  } catch {
    return json({ error: "bad request" }, 400);
  }
  if (update && update.message) await handleMessage(update.message, env, f, now);
  return json({ ok: true });
}

async function handlePair(req, env, now) {
  let body;
  try {
    body = await req.json();
  } catch {
    return json({ error: "bad request" }, 400);
  }
  const code = String((body && body.code) || "").toUpperCase();
  const pcName = String((body && body.pc_name) || "").trim().slice(0, 64);
  if (!/^[A-Z0-9]{6}$/.test(code) || !pcName) return json({ error: "bad request" }, 400);
  const kv = env.PHR;
  const rec = await getJson(kv, "code:" + code);
  if (!rec) return json({ error: "invalid or expired code" }, 400);
  await kv.delete("code:" + code); // one-time use
  if (rec.exp <= now()) return json({ error: "invalid or expired code" }, 400);
  const user = await getUser(kv, rec.chat);
  if (user.pcs[pcName]) await kv.delete("dev:" + user.pcs[pcName]);
  const key = randomHex(32);
  user.pcs[pcName] = key;
  await kv.put("dev:" + key, JSON.stringify({ chat: rec.chat, pc_name: pcName }));
  await kv.put("user:" + rec.chat, JSON.stringify(user));
  return json({ device_key: key });
}

async function handleAlert(req, env, f, now) {
  const key = req.headers.get("X-Device-Key");
  const dev = key ? await getJson(env.PHR, "dev:" + key) : null;
  if (!dev) return json({ error: "unauthorized" }, 401);
  let b;
  try {
    b = await req.json();
  } catch {
    return json({ error: "bad request" }, 400);
  }
  // Only known rules and numeric values: a paired device cannot push arbitrary text to the chat.
  const rule = b && Object.hasOwn(RULES, b.rule) ? RULES[b.rule] : null;
  if (!rule || !isNum(b.value) || (b.threshold != null && !isNum(b.threshold))) {
    return json({ error: "bad request" }, 400);
  }
  const kv = env.PHR;
  const user = await getUser(kv, dev.chat);
  const t = T[user.lang];
  const day = new Date(now()).toISOString().slice(0, 10);
  const ckey = "cnt:" + dev.chat + ":" + day;
  const c = (await getJson(kv, ckey)) || { n: 0, noted: false };
  if (c.n >= FREE_DAILY_LIMIT) {
    if (!c.noted) {
      c.noted = true;
      await kv.put(ckey, JSON.stringify(c), { expirationTtl: 172800 });
      await send(env, f, dev.chat, t.limit);
    }
    return json({ ok: false, limited: true }, 429);
  }
  c.n++;
  await kv.put(ckey, JSON.stringify(c), { expirationTtl: 172800 });
  const [label, unit] = rule;
  let text = "🔥 " + dev.pc_name + ": " + label + " " + b.value + unit;
  if (b.threshold != null) text += " (limit " + b.threshold + unit + ")";
  const ok = await send(env, f, dev.chat, text);
  return ok ? json({ ok: true }) : json({ error: "delivery failed" }, 502);
}

export async function handle(req, env, { fetch: f = fetch, now = Date.now } = {}) {
  const { pathname } = new URL(req.url);
  if (req.method === "GET" && pathname === "/") return new Response("phr-telegram up");
  if (req.method === "POST" && pathname === "/tg") return handleWebhook(req, env, f, now);
  if (req.method === "POST" && pathname === "/pair") return handlePair(req, env, now);
  if (req.method === "POST" && pathname === "/alert") return handleAlert(req, env, f, now);
  return json({ error: "not found" }, 404);
}

export default {
  async fetch(req, env) {
    try {
      return await handle(req, env);
    } catch (e) {
      console.log("error", e && e.name); // never log keys or message text
      return json({ error: "internal" }, 500);
    }
  },
};
