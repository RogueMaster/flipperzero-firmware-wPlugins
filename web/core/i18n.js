/* Phone-UI localization. The host picks the language; the ESP echoes it to each phone in
   `welcome`, and the client renders from a message catalog. English is the default and the
   fallback for any missing key. Static markup carries data-i18n / data-i18n-ph attributes;
   dynamic strings call t(key, params). ES5 (var/function, no template literals). Content
   packs are a separate, host-side concern.

   English values here must match the source markup exactly (applyI18n runs on load even for
   English). A missing pt-BR key falls back to English, so partial coverage is safe. Dynamic
   in-game strings (scores, notes, turn labels) are still being moved to t() screen by screen. */
var MESSAGES = {
  en: {
    // shared
    "common.ready": "I'm ready",
    "common.ready_cancel": "Ready. Tap to cancel",
    "common.ready_short": "Ready",
    "common.play_again": "Play again",
    "common.final": "Final",
    "common.starting": "Starting",
    "common.send": "Send",
    "common.go": "Go",
    "common.leave": "Leave",
    "common.rematch": "Rematch",
    "common.back_lobby": "Back to lobby",
    "common.challenge_player": "Challenge a player",
    "common.get_ready": "Get ready",
    "common.vote_pack": "Vote a pack",
    "common.board": "Board",
    // landing
    "landing.tagline": "Offline · Multiplayer · No signal",
    "landing.nick": "Your nickname",
    "landing.avatar": "Pick an avatar",
    "landing.play": "Play",
    "landing.nick_ph": "e.g. NOVA",
    "landing.hint": "If this page closes, rejoin the WiFi and open 192.168.4.1 in your browser.",
    // identity editor
    "id.nick": "Your nickname",
    "id.nick_ph": "e.g. NOVA",
    "id.avatar": "Your avatar",
    "id.save": "Save",
    "id.cancel": "Cancel",
    // net / lobby
    "net.reconnecting": "Reconnecting...",
    "lobby.title": "Lobby",
    "lobby.waiting": "Waiting for the host to pick a game.",
    "lobby.chat": "Chat",
    "lobby.say_ph": "Say something",
    // games
    "trivia.title": "Trivia",
    "trivia.vote_topic": "Vote a topic",
    "draw.title": "Draw & Guess",
    "draw.clear": "Clear",
    "draw.guess_ph": "Your guess",
    "wyr.title": "Would You Rather",
    "wyr.count_topic": "Would you rather",
    "wyr.wrap": "That's a wrap",
    "wyr.wrap_body": "Thanks for playing. Ready up for another round.",
    "scr.title": "Word Scramble",
    "scr.count_topic": "Unscramble",
    "scr.type_ph": "Type the word",
    "rc.title": "Reaction Duel",
    "rc.wait": "Wait...",
    "gc.title": "Guess the Color",
    "gc.match": "Match this color — dial in R / G / B",
    "gc.submit": "Submit guess",
    "gc.submitted": "Submitted — waiting for others...",
    "gc.answer": "Answer",
    "gc.legend": "◀ answer · guess ▶",
    "bs.title": "Battleship",
    "bs.place": "Place your fleet",
    "bs.rotate": "Rotate",
    "bs.random": "Random",
    "bs.wait_opp": "Waiting for opponent...",
    "bs.enemy_waters": "Enemy waters",
    "bs.your_fleet": "Your fleet",
    "bs.enemy_revealed": "Enemy fleet revealed",
    "sp.title": "Spectrum",
    "sp.count_topic": "Spectrum",
    "sp.clue_ph": "Type your clue…",
    "sp.lock_guess": "Lock in guess",
    "kmk.title": "Kiss Marry Kill",
    "kmk.count_topic": "Kiss Marry Kill",
    "kmk.lock": "Lock in",
    // captive handoff
    "captive.title": "Open in your browser",
    "captive.body": "This Wi-Fi popup can't keep the game connected. To play, leave it and open the game in your phone's browser:",
    "captive.step1": "Tap Cancel, then Use Without Internet",
    "captive.step2": "Open your browser and go to this address:",
    "captive.copy": "Copy",
    "captive.dismiss": "Dismiss",
    // dynamic (in-game)
    "common.round": "Round {n} / {total}",
    "scr.word": "Word {n} / {total}",
    "scr.answer": "Answer",
    "scr.solved": "Solved! Waiting for the round to end.",
    "scr.letters": "{n} letters",
    "gc.answer_val": "Answer  {r}, {g}, {b}",
    "gc.you_closest": "You were closest!",
    "gc.was_closest": "{nick} was closest",
  },
  "pt-br": {
    "common.ready": "Estou pronto",
    "common.ready_cancel": "Pronto. Toque para cancelar",
    "common.ready_short": "Pronto",
    "common.play_again": "Jogar de novo",
    "common.final": "Fim",
    "common.starting": "Começando",
    "common.send": "Enviar",
    "common.go": "Vai",
    "common.leave": "Sair",
    "common.rematch": "Revanche",
    "common.back_lobby": "Voltar ao lobby",
    "common.challenge_player": "Desafie um jogador",
    "common.get_ready": "Prepare-se",
    "common.vote_pack": "Vote num pacote",
    "common.board": "Placar",
    "landing.tagline": "Offline · Multijogador · Sem sinal",
    "landing.nick": "Seu apelido",
    "landing.avatar": "Escolha um avatar",
    "landing.play": "Jogar",
    "landing.nick_ph": "ex. NOVA",
    "landing.hint": "Se esta página fechar, reconecte no WiFi e abra 192.168.4.1 no navegador.",
    "id.nick": "Seu apelido",
    "id.nick_ph": "ex. NOVA",
    "id.avatar": "Seu avatar",
    "id.save": "Salvar",
    "id.cancel": "Cancelar",
    "net.reconnecting": "Reconectando...",
    "lobby.title": "Lobby",
    "lobby.waiting": "Aguardando o anfitrião escolher um jogo.",
    "lobby.chat": "Bate-papo",
    "lobby.say_ph": "Diga algo",
    "trivia.title": "Trivia",
    "trivia.vote_topic": "Vote num tema",
    "draw.title": "Desenhe e Adivinhe",
    "draw.clear": "Limpar",
    "draw.guess_ph": "Seu palpite",
    "wyr.title": "O Que Você Prefere",
    "wyr.count_topic": "O que você prefere",
    "wyr.wrap": "Fim de jogo",
    "wyr.wrap_body": "Obrigado por jogar. Fique pronto para outra rodada.",
    "scr.title": "Palavra Embaralhada",
    "scr.count_topic": "Desembaralhe",
    "scr.type_ph": "Digite a palavra",
    "rc.title": "Duelo de Reflexo",
    "rc.wait": "Espere...",
    "gc.title": "Adivinhe a Cor",
    "gc.match": "Iguale esta cor — ajuste R / G / B",
    "gc.submit": "Enviar palpite",
    "gc.submitted": "Enviado — aguardando os outros...",
    "gc.answer": "Resposta",
    "gc.legend": "◀ resposta · palpite ▶",
    "bs.title": "Batalha Naval",
    "bs.place": "Posicione sua frota",
    "bs.rotate": "Girar",
    "bs.random": "Aleatório",
    "bs.wait_opp": "Aguardando o oponente...",
    "bs.enemy_waters": "Águas inimigas",
    "bs.your_fleet": "Sua frota",
    "bs.enemy_revealed": "Frota inimiga revelada",
    "sp.title": "Espectro",
    "sp.count_topic": "Espectro",
    "sp.clue_ph": "Digite sua dica…",
    "sp.lock_guess": "Confirmar palpite",
    "kmk.title": "Kiss Marry Kill",
    "kmk.count_topic": "Kiss Marry Kill",
    "kmk.lock": "Confirmar",
    "captive.title": "Abra no navegador",
    "captive.body": "Este popup de Wi-Fi não mantém o jogo conectado. Para jogar, saia dele e abra o jogo no navegador do seu celular:",
    "captive.step1": "Toque em Cancelar, depois em Usar Sem Internet",
    "captive.step2": "Abra o navegador e vá para este endereço:",
    "captive.copy": "Copiar",
    "captive.dismiss": "Fechar",
    // dynamic (in-game)
    "common.round": "Rodada {n} / {total}",
    "scr.word": "Palavra {n} / {total}",
    "scr.answer": "Resposta",
    "scr.solved": "Resolvido! Aguardando o fim da rodada.",
    "scr.letters": "{n} letras",
    "gc.answer_val": "Resposta  {r}, {g}, {b}",
    "gc.you_closest": "Você chegou mais perto!",
    "gc.was_closest": "{nick} chegou mais perto",
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
