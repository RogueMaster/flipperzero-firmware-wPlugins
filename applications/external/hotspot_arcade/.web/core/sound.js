/* Tiny WebAudio SFX engine + haptics. No audio files: every sound is a short
   oscillator+gain envelope, synthesised on the fly. Exposes A.sfx(name),
   A.vibe(pattern), and a mute toggle persisted to localStorage. Audio is only
   created on the first user gesture (A.initAudio, called from the Play button)
   so mobile autoplay policies never block it. */
(function () {
  var ctx = null;
  var muted = false;
  try { muted = localStorage.getItem("ha_mute") === "1"; } catch (e) {}

  function ensure() {
    if (!ctx) {
      var AC = window.AudioContext || window.webkitAudioContext;
      if (AC) { try { ctx = new AC(); } catch (e) { ctx = null; } }
    }
    // A tab that backgrounds can suspend the context; resume on demand.
    if (ctx && ctx.state === "suspended") { try { ctx.resume(); } catch (e) {} }
    return ctx;
  }

  // One oscillator note with an attack/decay envelope. Times are seconds.
  function note(c, o) {
    var osc = c.createOscillator(), g = c.createGain();
    var t = c.currentTime + (o.at || 0);
    var dur = o.dur || 0.1;
    osc.type = o.type || "square";
    osc.frequency.setValueAtTime(o.f0, t);
    if (o.f1) osc.frequency.exponentialRampToValueAtTime(o.f1, t + dur);
    var vol = o.vol == null ? 0.14 : o.vol;
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(vol, t + 0.006);
    g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
    osc.connect(g); g.connect(c.destination);
    osc.start(t); osc.stop(t + dur + 0.02);
  }

  function chord(c, notes) { for (var i = 0; i < notes.length; i++) note(c, notes[i]); }

  // Named effects. Each is a function of the audio context.
  var SFX = {
    start:   function (c) { chord(c, [{ f0: 440, f1: 880, dur: 0.14, type: "square", vol: 0.12 }]); },
    join:    function (c) { chord(c, [{ f0: 520, f1: 780, dur: 0.09, type: "square" }]); },
    buzz:    function (c) { chord(c, [{ f0: 300, dur: 0.045, type: "square", vol: 0.12 }]); },
    correct: function (c) { chord(c, [{ f0: 660, dur: 0.09 }, { at: 0.09, f0: 990, dur: 0.12 }]); },
    wrong:   function (c) { chord(c, [{ f0: 200, f1: 110, dur: 0.2, type: "sawtooth", vol: 0.13 }]); },
    win:     function (c) { chord(c, [{ f0: 523, dur: 0.1 }, { at: 0.1, f0: 659, dur: 0.1 }, { at: 0.2, f0: 784, dur: 0.16 }]); },
    lose:    function (c) { chord(c, [{ f0: 392, dur: 0.14, type: "sawtooth" }, { at: 0.14, f0: 262, dur: 0.24, type: "sawtooth" }]); },
    drop:    function (c) { chord(c, [{ f0: 420, f1: 120, dur: 0.11, type: "square", vol: 0.13 }]); },
    tick:    function (c) { chord(c, [{ f0: 900, dur: 0.03, type: "square", vol: 0.1 }]); },
    score:   function (c) { chord(c, [{ f0: 740, f1: 980, dur: 0.08, type: "square" }]); }
  };

  A.sfx = function (name) {
    if (muted) return;
    var c = ensure(); if (!c) return;
    var fn = SFX[name];
    if (fn) { try { fn(c); } catch (e) {} }
  };

  // Haptics. No-op on desktop and iOS Safari (where vibrate is absent). Guarded.
  A.vibe = function (pattern) {
    try { if (navigator.vibrate) navigator.vibrate(pattern); } catch (e) {}
  };

  A.initAudio = function () { ensure(); };
  A.isMuted = function () { return muted; };

  function paint() {
    var b = document.getElementById("mute");
    if (!b) return;
    b.classList.toggle("muted", muted);
    b.setAttribute("aria-label", muted ? "Unmute" : "Mute");
    b.setAttribute("aria-pressed", muted ? "true" : "false");
  }

  A.toggleMute = function () {
    muted = !muted;
    try { localStorage.setItem("ha_mute", muted ? "1" : "0"); } catch (e) {}
    if (!muted) A.sfx("tick");   // tiny confirmation blip when re-enabling
    paint();
    return muted;
  };

  document.addEventListener("DOMContentLoaded", function () {
    var b = document.getElementById("mute");
    if (b) b.addEventListener("click", function () { A.initAudio(); A.toggleMute(); });
    paint();
  });
})();
