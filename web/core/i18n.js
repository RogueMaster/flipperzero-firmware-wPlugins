/* Phone-UI localization. The host picks the language; the ESP echoes it to each phone in
   `welcome`, and the client loads the matching catalog. English is the default and the
   fallback for any missing key. Static markup carries data-i18n / data-i18n-ph attributes;
   dynamic strings call t(key, params). ES5 (var/function, no template literals). Content
   packs are a separate, host-side concern.

   Stage 1 ships the plumbing plus the landing screen as a proof; the rest of the UI is
   extracted screen by screen, each new string added to both catalogs. A missing pt-BR key
   just falls back to English, so partial coverage is safe. */
var MESSAGES = {
  en: {
    "landing.tagline": "Offline · Multiplayer · No signal",
    "landing.nick": "Your nickname",
    "landing.avatar": "Pick an avatar",
    "landing.play": "Play",
    "landing.nick_ph": "e.g. NOVA",
    "landing.hint": "If this page closes, rejoin the WiFi and open 192.168.4.1 in your browser.",
  },
  "pt-br": {
    "landing.tagline": "Offline · Multijogador · Sem sinal",
    "landing.nick": "Seu apelido",
    "landing.avatar": "Escolha um avatar",
    "landing.play": "Jogar",
    "landing.nick_ph": "ex. NOVA",
    "landing.hint": "Se esta pagina fechar, reconecte no WiFi e abra 192.168.4.1 no navegador.",
  },
};

A.lang = "en";

// Look up a key in the active language, falling back to English then the key itself.
// {name} placeholders are filled from params.
function t(key, params) {
  var cat = MESSAGES[A.lang] || MESSAGES.en;
  var s = cat[key];
  if (s == null) s = MESSAGES.en[key];
  if (s == null) return key;
  if (params)
    s = s.replace(/\{(\w+)\}/g, function (_, k) { return params[k] != null ? params[k] : "{" + k + "}"; });
  return s;
}

// Fill static markup from the catalog: [data-i18n] -> textContent, [data-i18n-ph] -> placeholder.
function applyI18n(root) {
  root = root || document;
  var els = root.querySelectorAll("[data-i18n]");
  for (var i = 0; i < els.length; i++) els[i].textContent = t(els[i].getAttribute("data-i18n"));
  var ph = root.querySelectorAll("[data-i18n-ph]");
  for (var j = 0; j < ph.length; j++) ph[j].setAttribute("placeholder", t(ph[j].getAttribute("data-i18n-ph")));
}

// Set the language from `welcome` and re-render static text. Games re-render their dynamic
// strings on the next server message, so no full reload is needed.
A.setLang = function (lang) {
  A.lang = (lang && MESSAGES[lang]) ? lang : "en";
  applyI18n();
};

document.addEventListener("DOMContentLoaded", function () { applyI18n(); });
