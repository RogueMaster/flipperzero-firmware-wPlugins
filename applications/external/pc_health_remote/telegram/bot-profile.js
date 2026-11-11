// Bot profile through the official Bot API: description, short description and command menu per language,
// plus the profile photo. Shared file: identical copies in infra/knm-drill, infra/social-post/lib and
// pc-health-remote/telegram (keep them in sync). Each Worker calls applyProfile from an admin-only /brand command.
// profile = { default: 'uk', langs: { uk: { description, short, commands: [[command, text], ...] }, en: {...} } }
// The default language is sent without language_code, so it is what every other client language sees.
export const LIMITS = { description: 512, short: 120, command: 256 };

export function checkProfile(profile) {
  const errors = [];
  if (!profile.langs?.[profile.default]) errors.push('default language missing');
  for (const [lang, l] of Object.entries(profile.langs || {})) {
    if (!l.description || [...l.description].length > LIMITS.description) errors.push(`${lang}: description length`);
    if (!l.short || [...l.short].length > LIMITS.short) errors.push(`${lang}: short description length`);
    for (const [c, d] of l.commands || []) {
      if (!/^[a-z0-9_]{1,32}$/.test(c) || !d || [...d].length > LIMITS.command) errors.push(`${lang}: command ${c}`);
    }
  }
  return errors;
}

const bytes = b64 => Uint8Array.from(atob(b64), c => c.charCodeAt(0));

// Never throws and never returns Telegram's response text (it could echo the token URL). Returns failed step names.
export async function applyProfile({ token, profile, avatar, fetch: f = fetch }) {
  const failed = [];
  const call = async (step, method, init) => {
    try {
      const r = await f(`https://api.telegram.org/bot${token}/${method}`, { method: 'POST', ...init });
      if (!(await r.json())?.ok) failed.push(step);
    } catch { failed.push(step); }
  };
  const json = body => ({ headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  for (const [lang, l] of Object.entries(profile.langs)) {
    const code = lang === profile.default ? {} : { language_code: lang };
    await call(`description:${lang}`, 'setMyDescription', json({ description: l.description, ...code }));
    await call(`short:${lang}`, 'setMyShortDescription', json({ short_description: l.short, ...code }));
    if (l.commands) await call(`commands:${lang}`, 'setMyCommands', json({ commands: l.commands.map(([command, description]) => ({ command, description })), ...code }));
  }
  if (avatar) {
    const form = new FormData();
    form.append('photo', JSON.stringify({ type: 'static', photo: 'attach://avatar' }));
    form.append('avatar', new Blob([bytes(avatar)], { type: 'image/jpeg' }), 'avatar.jpg');
    await call('photo', 'setMyProfilePhoto', { body: form });
  }
  return failed;
}

// Automatic mode: the Worker applies its profile once per profile version, without any admin command.
// Called from the default export (webhook, GET probes after deploy, cron) via ctx.waitUntil. KV key `bot-profile`
// holds the applied version; after a failure it holds `wait:<version>` for an hour so Telegram is not hammered.
const fnv = text => { let h = 0x811c9dc5; for (let i = 0; i < text.length; i++) h = Math.imul(h ^ text.charCodeAt(i), 16777619) >>> 0; return h.toString(36); };
let applied = '';
export async function autoProfile({ kv, token, profile, avatar, fetch: f = fetch }) {
  if (!kv?.get || !kv?.put || !token) return null;
  const version = fnv(JSON.stringify(profile) + (avatar || ''));
  if (applied === version) return null;
  const stored = await kv.get('bot-profile');
  if (stored === version || stored === `wait:${version}`) { applied = version; return null; }
  const failed = await applyProfile({ token, profile, avatar, fetch: f });
  applied = version;
  await kv.put('bot-profile', failed.length ? `wait:${version}` : version, failed.length ? { expirationTtl: 3600 } : undefined);
  console.log(JSON.stringify({ botProfile: failed.length ? { failed } : 'applied' }));
  return failed;
}
export const resetAutoProfile = () => { applied = ''; }; // tests only
