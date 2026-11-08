// PC Health Remote - Telegram bot (Cloudflare Worker, plain ES module).
// Stores only: chat id, language, source tag, device keys, pc names, counters.
// Logs never contain keys or message text.

export const FREE_DAILY_LIMIT = 10; // alerts per chat per UTC day (Pro: unlimited)
export const PRO_PRICE_STARS = 99; // Telegram Stars per 30 days
export const PRO_PERIOD_SEC = 2592000;
export const FREE_MAX_PCS = 1;
export const PRO_MAX_PCS = 5;
export const CODE_TTL_SEC = 600;
export const SUPPORT_WINDOW_SEC = 3600; // after /paysupport, the next free-text message goes to the admin
export const SUPPORT_URL = "https://github.com/ataman-lab/pc-health-remote/issues";
// Shown in the bot menu (setMyCommands via admin /setcommands). Telegram requires /paysupport for Stars bots.
export const COMMANDS = {
  en: [
    ["help", "All commands and buttons"],
    ["link", "Get a code to link a PC"],
    ["pcs", "Linked PCs"],
    ["status", "Plan and alerts today"],
    ["pro", "Pro: unlimited alerts, up to 5 PCs"],
    ["paysupport", "Help with a payment"],
    ["terms", "Terms of Pro"],
    ["support", "Report a problem"],
  ],
  ru: [
    ["help", "Все команды и кнопки"],
    ["link", "Код для привязки ПК"],
    ["pcs", "Привязанные ПК"],
    ["status", "Тариф и уведомления за сегодня"],
    ["pro", "Pro: без лимита, до 5 ПК"],
    ["paysupport", "Помощь с оплатой"],
    ["terms", "Условия Pro"],
    ["support", "Сообщить о проблеме"],
  ],
};
// Persistent reply keyboard: button label -> command. Shown on /start and /help.
export const BUTTONS = {
  en: [
    ["🔗 Link PC", "/link"],
    ["💻 My PCs", "/pcs"],
    ["📊 Status", "/status"],
    ["⭐ Pro", "/pro"],
    ["❓ Help", "/help"],
  ],
  ru: [
    ["🔗 Привязать ПК", "/link"],
    ["💻 Мои ПК", "/pcs"],
    ["📊 Статус", "/status"],
    ["⭐ Pro", "/pro"],
    ["❓ Помощь", "/help"],
  ],
};
const BUTTON_CMD = new Map(Object.values(BUTTONS).flat());
const keyboard = (lang) => {
  const b = BUTTONS[lang].map(([text]) => ({ text }));
  return { keyboard: [[b[0], b[1]], [b[2], b[3]], [b[4]]], resize_keyboard: true, is_persistent: true };
};
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
  en: "Hi! I send PC Health Remote alerts to your phone.\n\n1. Send /link to get a 6-character code.\n2. On the PC run: pc-health-remote.exe --link-telegram CODE\n\nCommands: /link, /pcs, /unlink <name>, /status, /pro\nHelp: /help",
  ru: "Привет! Я присылаю уведомления PC Health Remote на телефон.\n\n1. Отправьте /link, чтобы получить 6-значный код.\n2. На ПК выполните: pc-health-remote.exe --link-telegram КОД\n\nКоманды: /link, /pcs, /unlink <имя>, /status, /pro\nПомощь: /help",
};

const T = {
  en: {
    code: (c) => "Your code: " + c + "\nValid for 10 minutes. On the PC run:\npc-health-remote.exe --link-telegram " + c,
    nopcs: "No linked PCs. Send /link to add one.",
    pcs: "Linked PCs:",
    unlinkUsage: "Usage: /unlink <name>",
    unlinked: (n) => "Unlinked: " + n,
    notFound: (n) => "No such PC: " + n,
    proTitle: "PC Health Remote Pro",
    proDesc: "No daily alert limit and up to " + PRO_MAX_PCS + " PCs (free: " + FREE_DAILY_LIMIT + " alerts/day, " + FREE_MAX_PCS + " PC). 30 days, renews automatically.",
    pro: (url) => "Pro: unlimited alerts, up to " + PRO_MAX_PCS + " PCs, " + PRO_PRICE_STARS + " Stars / 30 days.\n" + url,
    proFail: "Could not create the invoice, try again later.",
    thanks: (d) => "Thank you! Pro is active until " + d + ".",
    status: (plan, exp, pcs, n, id, admin) => "Plan: " + plan + (exp ? "\nExpires: " + exp : "") + "\nPCs: " + pcs + "\nAlerts today: " + n + "\nChat id: " + id + (admin ? " (admin)" : ""),
    limit: "Daily free limit of " + FREE_DAILY_LIMIT + " alerts reached. More alerts will be dropped until tomorrow (UTC).",
    help: "Commands:\n/link - code to link a PC\n/pcs - linked PCs\n/unlink <name> - remove a PC\n/status - plan, alerts today, chat id\n/pro - Pro subscription\n/paysupport - help with a payment\n/terms - terms of Pro\n/support - report a problem",
    payBtn: "Pay " + PRO_PRICE_STARS + " ⭐ for 30 days",
    pcLimit: "This PC was not linked: the free plan has " + FREE_MAX_PCS + " PC. Unlink one with /unlink <name>, or get Pro for up to " + PRO_MAX_PCS + " PCs.",
    pcLimitPro: "This PC was not linked: Pro allows up to " + PRO_MAX_PCS + " PCs. Unlink one with /unlink <name>.",
    paysupport:
      "Payment help: in the next hour, send one message here with what went wrong (date and amount). It goes privately to the developer, who answers in this chat." +
      "\nPro payments in Stars are refundable through Telegram. Refunding the current period also stops Pro and its renewal." +
      "\nTelegram support cannot help with purchases in this bot.",
    sentToSupport: "Sent to the developer. You will get an answer here.",
    terms:
      "Terms of PC Health Remote Pro\n1. Pro costs " + PRO_PRICE_STARS + " Stars per 30 days and renews until you cancel the subscription in Telegram. After Pro ends, linked PCs stay; the free limit of " + FREE_DAILY_LIMIT + " alerts/day applies again.\n2. Pro removes the daily limit of " + FREE_DAILY_LIMIT + " alerts and allows up to " + PRO_MAX_PCS + " PCs.\n3. Refunds: ask via /paysupport, refunds are made in Telegram Stars.\n4. Alerts are best effort, no warranty: do not rely on them for safety-critical equipment.\n5. Stored: chat id, language, source tag, PC names and device keys, counters, payment records (charge id, amount, date). /unlink removes a PC and its key.\nThe app is free and open source (MIT).",
    support: "Bug or idea? Open an issue: " + SUPPORT_URL + "\nPayment questions: /paysupport",
  },
  ru: {
    code: (c) => "Ваш код: " + c + "\nДействует 10 минут. На ПК выполните:\npc-health-remote.exe --link-telegram " + c,
    nopcs: "Нет привязанных ПК. Отправьте /link, чтобы добавить.",
    pcs: "Привязанные ПК:",
    unlinkUsage: "Использование: /unlink <имя>",
    unlinked: (n) => "Отвязан: " + n,
    notFound: (n) => "Нет такого ПК: " + n,
    proTitle: "PC Health Remote Pro",
    proDesc: "Без лимита уведомлений и до " + PRO_MAX_PCS + " ПК (бесплатно: " + FREE_DAILY_LIMIT + " в сутки, " + FREE_MAX_PCS + " ПК). 30 дней, продлевается автоматически.",
    pro: (url) => "Pro: без лимита уведомлений, до " + PRO_MAX_PCS + " ПК, " + PRO_PRICE_STARS + " Stars / 30 дней.\n" + url,
    proFail: "Не удалось создать счёт, попробуйте позже.",
    thanks: (d) => "Спасибо! Pro активен до " + d + ".",
    status: (plan, exp, pcs, n, id, admin) => "Тариф: " + plan + (exp ? "\nДо: " + exp : "") + "\nПК: " + pcs + "\nУведомлений сегодня: " + n + "\nChat id: " + id + (admin ? " (админ)" : ""),
    limit: "Достигнут бесплатный лимит: " + FREE_DAILY_LIMIT + " уведомлений в сутки. Остальные до завтра (UTC) не будут отправлены.",
    help: "Команды:\n/link - код для привязки ПК\n/pcs - привязанные ПК\n/unlink <имя> - убрать ПК\n/status - тариф, уведомления за сегодня, chat id\n/pro - подписка Pro\n/paysupport - помощь с оплатой\n/terms - условия Pro\n/support - сообщить о проблеме",
    payBtn: "Оплатить " + PRO_PRICE_STARS + " ⭐ за 30 дней",
    pcLimit: "ПК не привязан: в бесплатном тарифе " + FREE_MAX_PCS + " ПК. Отвяжите лишний через /unlink <имя> или возьмите Pro (до " + PRO_MAX_PCS + " ПК).",
    pcLimitPro: "ПК не привязан: в Pro до " + PRO_MAX_PCS + " ПК. Отвяжите лишний через /unlink <имя>.",
    paysupport:
      "Помощь с оплатой: в течение часа отправьте сюда одно сообщение, что случилось (дата и сумма). Оно лично уйдёт разработчику, ответ придёт в этот чат." +
      "\nОплату Pro в Stars можно вернуть через Telegram. Возврат за текущий период отключает Pro и его продление." +
      "\nПоддержка Telegram не помогает с покупками в этом боте.",
    sentToSupport: "Отправлено разработчику. Ответ придёт сюда.",
    terms:
      "Условия PC Health Remote Pro\n1. Pro стоит " + PRO_PRICE_STARS + " Stars за 30 дней и продлевается, пока вы не отмените подписку в Telegram. После окончания Pro привязанные ПК остаются, снова действует лимит " + FREE_DAILY_LIMIT + " уведомлений в сутки.\n2. Pro снимает лимит " + FREE_DAILY_LIMIT + " уведомлений в сутки и даёт до " + PRO_MAX_PCS + " ПК.\n3. Возврат: через /paysupport, возврат делается в Stars.\n4. Уведомления без гарантий: не полагайтесь на них для критичного оборудования.\n5. Храним: id чата, язык, метку источника, имена ПК и ключи устройств, счётчики, записи об оплате (id платежа, сумма, дата). /unlink удаляет ПК и его ключ.\nПриложение бесплатное, открытый код (MIT).",
    support: "Ошибка или идея? Создайте issue: " + SUPPORT_URL + "\nВопросы оплаты: /paysupport",
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

// Daily free-tier alert counter, one per chat per UTC day.
// With the ALERT_COUNTER Durable Object bound (wrangler.toml), counting costs zero KV writes: KV free tier allows
// only 1000 writes/day, the Durable Object free tier 100k. Without the binding (old deploys) it falls back to a KV
// counter, one write per free alert.
export class AlertCounter {
  constructor(state) {
    this.storage = state.storage;
  }
  async fetch(req) {
    const { pathname } = new URL(req.url);
    const c = (await this.storage.get("c")) || { n: 0, noted: false };
    if (pathname === "/peek") return json({ n: c.n });
    const { limit } = await req.json();
    if (c.n >= limit) {
      const noteNow = !c.noted;
      if (noteNow) await this.storage.put("c", { ...c, noted: true });
      return json({ n: c.n, allowed: false, noteNow });
    }
    const next = { ...c, n: c.n + 1 };
    await this.storage.put("c", next);
    if (c.n === 0) await this.storage.setAlarm(Date.now() + 172800 * 1000); // forget the day after 48 h
    return json({ n: next.n, allowed: true, noteNow: false });
  }
  async alarm() {
    await this.storage.deleteAll();
  }
}

function counter(env, ckey) {
  const ns = env.ALERT_COUNTER;
  if (ns) {
    const stub = ns.get(ns.idFromName(ckey));
    const call = async (path, body) => (await stub.fetch("https://counter" + path, body ? { method: "POST", body: JSON.stringify(body) } : {})).json();
    return {
      peek: async () => (await call("/peek")).n,
      hit: (limit) => call("/hit", { limit }),
    };
  }
  const kv = env.PHR;
  return {
    peek: async () => ((await getJson(kv, ckey)) || { n: 0 }).n,
    hit: async (limit) => {
      const c = (await getJson(kv, ckey)) || { n: 0, noted: false };
      if (c.n >= limit) {
        const noteNow = !c.noted;
        if (noteNow) await kv.put(ckey, JSON.stringify({ ...c, noted: true }), { expirationTtl: 172800 });
        return { n: c.n, allowed: false, noteNow };
      }
      c.n++;
      await kv.put(ckey, JSON.stringify(c), { expirationTtl: 172800 });
      return { n: c.n, allowed: true, noteNow: false };
    },
  };
}

async function send(env, f, chat, text, reply_markup) {
  let res;
  try {
    res = await f("https://api.telegram.org/bot" + env.BOT_TOKEN + "/sendMessage", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(reply_markup ? { chat_id: chat, text, reply_markup } : { chat_id: chat, text }),
    });
  } catch {
    return false;
  }
  return res.ok;
}

async function api(env, f, method, body) {
  let res;
  try {
    res = await f("https://api.telegram.org/bot" + env.BOT_TOKEN + "/" + method, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    });
  } catch {
    return null; // network error: callers fall back (e.g. plain limit text without the Pro offer)
  }
  let j = null;
  try {
    j = await res.json();
  } catch {}
  return res.ok && j && j.ok !== false ? j : null;
}

const isPro = (user, now) => !!(user.pro && user.pro.until > now());
const isAdmin = (env, chat) => !!env.ADMIN_CHAT_ID && String(chat) === String(env.ADMIN_CHAT_ID);
const payloadOf = (chat) => "pro30:" + chat;
const dayOf = (now) => new Date(now()).toISOString().slice(0, 10);

// Pro offer text with a fresh invoice link (null if Telegram refused to create one).
async function proOffer(env, f, chat, t) {
  if (!(chat > 0)) return null; // group chat: payload pro30:<group> would never match the payer, so no offer
  const r = await api(env, f, "createInvoiceLink", {
    title: t.proTitle,
    description: t.proDesc,
    payload: payloadOf(chat),
    currency: "XTR",
    prices: [{ label: "Pro 30d", amount: PRO_PRICE_STARS }],
    subscription_period: PRO_PERIOD_SEC,
  });
  if (!(r && r.result)) return null;
  return { text: t.pro(r.result), markup: { inline_keyboard: [[{ text: t.payBtn, url: r.result }]] } };
}

// Sends `note` (optional) followed by the Pro offer with a pay button; plain note when no offer.
async function sendWithOffer(env, f, chat, t, note) {
  const offer = await proOffer(env, f, chat, t);
  if (!offer) return send(env, f, chat, note || t.proFail);
  return send(env, f, chat, note ? note + "\n\n" + offer.text : offer.text, offer.markup);
}

async function handlePayment(msg, env, f, now) {
  const p = msg.successful_payment;
  const chat = msg.chat.id;
  const id = p && p.telegram_payment_charge_id;
  if (!id || p.currency !== "XTR" || p.invoice_payload !== payloadOf(chat)) return;
  const kv = env.PHR;
  if (await kv.get("pay:" + id)) return; // duplicate delivery: count once
  const user = await getUser(kv, chat);
  const exp = p.subscription_expiration_date ? p.subscription_expiration_date * 1000 : now() + PRO_PERIOD_SEC * 1000;
  await kv.put("pay:" + id, JSON.stringify({ chat, stars: p.total_amount, src: user.src, t: now() }));
  user.pro = { until: Math.max(exp, (user.pro && user.pro.until) || 0), charge: id };
  await kv.put("user:" + chat, JSON.stringify(user));
  await send(env, f, chat, T[user.lang].thanks(new Date(user.pro.until).toISOString().slice(0, 10)));
}

async function handlePreCheckout(q, env, f) {
  const ok =
    q.currency === "XTR" &&
    q.total_amount === PRO_PRICE_STARS &&
    !!q.from &&
    q.invoice_payload === payloadOf(q.from.id);
  await api(
    env,
    f,
    "answerPreCheckoutQuery",
    ok ? { pre_checkout_query_id: q.id, ok: true } : { pre_checkout_query_id: q.id, ok: false, error_message: "Invalid invoice" },
  );
}

async function listAll(kv, prefix) {
  const out = [];
  let cursor;
  do {
    const r = await kv.list({ prefix, cursor });
    for (const k of r.keys) out.push(await getJson(kv, k.name));
    cursor = r.list_complete ? undefined : r.cursor;
  } while (cursor);
  return out.filter(Boolean);
}

async function stats(env, now) {
  const users = await listAll(env.PHR, "user:");
  const pays = (await listAll(env.PHR, "pay:")).filter((p) => !p.refunded);
  const by = {};
  const row = (src) => (by[src || "-"] ||= { users: 0, pro: 0, stars: 0 });
  let pcs = 0;
  let pro = 0;
  for (const u of users) {
    pcs += Object.keys(u.pcs || {}).length;
    row(u.src).users++;
    if (isPro(u, now)) {
      pro++;
      row(u.src).pro++;
    }
  }
  let stars = 0;
  for (const p of pays) {
    stars += p.stars;
    row(p.src).stars += p.stars;
  }
  const lines = Object.entries(by).map(([k, v]) => k + ": users " + v.users + ", pro " + v.pro + ", stars " + v.stars);
  return ["Users: " + users.length, "PCs: " + pcs, "Pro: " + pro, "Stars: " + stars, ...lines].join("\n");
}

async function getUser(kv, chat) {
  return (await getJson(kv, "user:" + chat)) || { lang: "en", src: null, pcs: {} };
}

// One private message to the admin per /paysupport (window SUPPORT_WINDOW_SEC). Other free text is ignored.
async function handleSupportText(msg, env, f, now) {
  const chat = msg.chat.id;
  if (!(chat > 0) || !env.ADMIN_CHAT_ID) return;
  const kv = env.PHR;
  const user = await getUser(kv, chat);
  if (!user.ask || user.ask <= now()) return;
  delete user.ask; // one message per /paysupport
  await kv.put("user:" + chat, JSON.stringify(user));
  const text = msg.text.slice(0, 1000);
  const pay = user.pro && user.pro.charge ? "\nlast charge: " + user.pro.charge : "";
  await send(env, f, env.ADMIN_CHAT_ID, "Payment support from " + chat + " (src " + (user.src || "-") + ")" + pay + ":\n" + text + "\n\nAnswer: /reply " + chat + " <text>");
  await send(env, f, chat, T[user.lang].sentToSupport);
}

async function handleMessage(msg, env, f, now) {
  const chat = msg.chat && msg.chat.id;
  if (chat === undefined) return;
  if (msg.successful_payment) return handlePayment(msg, env, f, now);
  if (typeof msg.text !== "string") return;
  const text = BUTTON_CMD.get(msg.text.trim()) || msg.text;
  if (!text.startsWith("/")) return handleSupportText(msg, env, f, now);
  const kv = env.PHR;
  const user = await getUser(kv, chat);
  user.lang = langOf(msg.from && msg.from.language_code);
  const t = T[user.lang];
  const [cmdRaw, ...rest] = text.trim().split(/\s+/);
  const cmd = cmdRaw.split("@")[0].toLowerCase();

  if (cmd === "/start") {
    const m = /^src_([A-Za-z0-9_-]{1,32})$/.exec(rest[0] || "");
    if (m) user.src = m[1];
    await kv.put("user:" + chat, JSON.stringify(user));
    await send(env, f, chat, GREETING[user.lang], keyboard(user.lang));
  } else if (cmd === "/help") {
    await send(env, f, chat, t.help, keyboard(user.lang));
  } else if (cmd === "/link") {
    const code = randomCode();
    await kv.put("code:" + code, JSON.stringify({ chat, exp: now() + CODE_TTL_SEC * 1000 }), {
      expirationTtl: CODE_TTL_SEC,
    });
    await kv.put("user:" + chat, JSON.stringify(user));
    await send(env, f, chat, t.code(code));
  } else if (cmd === "/pro") {
    await sendWithOffer(env, f, chat, t);
  } else if (cmd === "/paysupport") {
    user.ask = now() + SUPPORT_WINDOW_SEC * 1000;
    await kv.put("user:" + chat, JSON.stringify(user));
    await send(env, f, chat, t.paysupport);
  } else if (cmd === "/terms" || cmd === "/support") {
    await send(env, f, chat, t[cmd.slice(1)]);
  } else if (cmd === "/reply" && isAdmin(env, chat)) {
    const to = Number(rest[0]);
    const text = rest.slice(1).join(" ");
    if (!Number.isSafeInteger(to) || !text) return void (await send(env, f, chat, "Usage: /reply <chat_id> <text>"));
    await send(env, f, chat, (await send(env, f, to, text)) ? "Sent" : "Send failed");
  } else if (cmd === "/setcommands" && isAdmin(env, chat)) {
    let ok = true;
    for (const [lang, list] of Object.entries(COMMANDS)) {
      const commands = list.map(([command, description]) => ({ command, description }));
      ok = !!(await api(env, f, "setMyCommands", lang === "en" ? { commands } : { commands, language_code: lang })) && ok;
    }
    await send(env, f, chat, ok ? "Commands set" : "setMyCommands failed");
  } else if (cmd === "/status") {
    const used = await counter(env, "cnt:" + chat + ":" + dayOf(now)).peek();
    const pro = isPro(user, now);
    const exp = pro ? new Date(user.pro.until).toISOString().slice(0, 10) : null;
    await send(env, f, chat, t.status(pro ? "Pro" : "Free", exp, Object.keys(user.pcs).length, used, chat, isAdmin(env, chat)));
  } else if (cmd === "/stats" && isAdmin(env, chat)) {
    await send(env, f, chat, await stats(env, now));
  } else if (cmd === "/refund" && isAdmin(env, chat)) {
    const id = rest[0];
    const pay = id ? await getJson(kv, "pay:" + id) : null;
    if (!pay || pay.refunded) return void (await send(env, f, chat, "Unknown or already refunded charge"));
    const r = await api(env, f, "refundStarPayment", { user_id: pay.chat, telegram_payment_charge_id: id });
    if (!r) return void (await send(env, f, chat, "Refund failed"));
    pay.refunded = true;
    await kv.put("pay:" + id, JSON.stringify(pay));
    const payer = await getUser(kv, pay.chat);
    if (payer.pro && payer.pro.charge === id) {
      await api(env, f, "editUserStarSubscription", { user_id: pay.chat, telegram_payment_charge_id: id, is_canceled: true });
      delete payer.pro;
      await kv.put("user:" + pay.chat, JSON.stringify(payer));
    }
    await send(env, f, chat, "Refunded " + id);
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
  else if (update && update.pre_checkout_query) await handlePreCheckout(update.pre_checkout_query, env, f);
  return json({ ok: true });
}

async function handlePair(req, env, f, now) {
  // Optional Cloudflare rate-limit binding (wrangler.toml [[ratelimits]] PAIR_LIMIT): stops code guessing.
  if (env.PAIR_LIMIT) {
    const ip = req.headers.get("CF-Connecting-IP") || "unknown";
    const { success } = await env.PAIR_LIMIT.limit({ key: ip });
    if (!success) return json({ error: "too many attempts" }, 429);
  }
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
  if (!user.pcs[pcName] && Object.keys(user.pcs).length >= (isPro(user, now) ? PRO_MAX_PCS : FREE_MAX_PCS)) {
    const t = T[user.lang];
    if (isPro(user, now)) {
      await send(env, f, rec.chat, t.pcLimitPro);
    } else {
      await sendWithOffer(env, f, rec.chat, t, t.pcLimit);
    }
    return json({ error: "pc limit reached" }, 403);
  }
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
  const pro = isPro(user, now);
  if (!pro) {
    // Pro: no counter at all.
    const r = await counter(env, "cnt:" + dev.chat + ":" + dayOf(now)).hit(FREE_DAILY_LIMIT);
    if (!r.allowed) {
      if (r.noteNow) {
        await sendWithOffer(env, f, dev.chat, t, t.limit);
      }
      return json({ ok: false, limited: true }, 429);
    }
  }
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
  if (req.method === "POST" && pathname === "/pair") return handlePair(req, env, f, now);
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
