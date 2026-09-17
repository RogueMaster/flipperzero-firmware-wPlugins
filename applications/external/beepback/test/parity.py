#!/usr/bin/env python3
"""Hold the browser build and the firmware to the same numbers.

BEEPBACK exists twice and the two have to be the same game. Everything
else in test/ checks the firmware against itself; this loads
reference/beepback.html in a real browser, drives it, and compares what
comes out against the constants and tables compiled into the .fap.

Needs playwright and a chromium. Without either it prints why and exits
0, because a missing browser is not a failing build.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HTML = ROOT / "reference" / "beepback.html"

# the two days the firmware's own suite pins, so all three agree
PINNED = {
    20260910: (2, 2, 0, "102114334400314231030040221432"),
    20260911: (3, 2, 0, "033221042032412034102302402014"),
}


def firmware():
    """the numbers and tables the .fap is built from"""
    h = (ROOT / "beepback.h").read_text()
    tab = (ROOT / "beepback_tables.c").read_text()

    def define(name):
        m = re.search(r"#define\s+" + name + r"\s+([0-9]+)", h)
        return int(m.group(1)) if m else None

    def strings(name):
        m = re.search(re.escape(name) + r"[^=]*=\s*\{?\s*(.*?)\};", tab, re.S)
        return re.findall(r'"([^"]*)"', m.group(1)) if m else None

    return {
        "NOTE_POINTS": define("BB_NOTE_POINTS"),
        "ROUND_BONUS": define("BB_ROUND_BONUS"),
        "SCORE_MAX": define("BB_SCORE_MAX"),
        "STAT_ROWS": define("BB_STAT_ROWS"),
        "STAT_VIS": define("BB_STAT_VIS"),
        "CONFIRM_MS": define("BB_CONFIRM_MS"),
        "BUZZ_GAP": define("BB_BUZZ_GAP"),
        "BUZZ_MAX": define("BB_BUZZ_MAX"),
        "BUZZ_MIN": define("BB_BUZZ_MIN"),
        "BUZZ_HELLO": define("BB_BUZZ_HELLO"),
        "RX_START": define("BB_RX_START"),
        "DIFF_NAME": strings("bb_diff_name"),
        "ASSIST_NAME": strings("bb_assist_name"),
        "MODE_NAME": strings("bb_mode_name"),
        "PRAISE": strings("bb_praise"),
        "RULE_LABELS": strings("bb_rule_label"),
    }


PROBE = """() => {
  const r = {
    NOTE_POINTS, ROUND_BONUS, SCORE_MAX, STAT_ROWS, STAT_VIS, CONFIRM_MS,
    BUZZ_GAP, BUZZ_MAX, BUZZ_MIN, BUZZ_HELLO, RX_START,
    DIFF_NAME, ASSIST_NAME, MODE_NAME, PRAISE,
    RULE_LABELS: RULES.map(x => x.label),
    daily: {}
  };
  for(const day of [20260910, 20260911]){
    bbSeedOverride = day;
    S.mode = 4; startGame();
    for(let i = 0; i < 29; i++) challengeGrow();
    r.daily[day] = [S.ruleIdx, S.ruleA, S.ruleB, S.base.slice(0, 30).join('')];
  }
  bbSeedOverride = 0;

  /* a round of classic, played correctly, on two opposite settings */
  const playRoundOne = (diff, speed) => {
    S.stats = emptyStats();
    S.mode = 0; SET.assist = 2; SET.volume = 0; S.diff = diff; SET.speed = speed;
    startGame();
    let guard = 0;
    while(S.round === 1 && guard++ < 6000){
      now += 20;
      update();
      if(S.scene === 'input') for(const b of S.expected) takePress(b);
    }
    return [S.score, S.stats.notes, S.stats.rounds];
  };
  r.normal = playRoundOne(1, 1);
  r.insane = playRoundOne(3, 2);

  /* NEW BEST is judged against the slot played, not a maximum */
  S.best = emptyBest();
  S.best[0][0][0][2] = 5000;
  S.runGameMode = 0; S.runDiff = 3; S.runSpeed = 2; S.runMode = 2;
  S.score = 400; S.lockUntil = 0; enter('gameover');
  r.newBest = [S.newBest, S.best[0][3][2][2], S.best[0][0][0][2]];
  return r;
}"""


def main():
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("--   no playwright, skipping the browser build check")
        return 0
    if not HTML.exists():
        print("--   reference/beepback.html is missing, skipping")
        return 0

    errors = []
    try:
        with sync_playwright() as pw:
            browser = pw.chromium.launch(executable_path="/opt/pw-browsers/chromium")
            page = browser.new_page()
            page.on("pageerror", lambda e: errors.append(str(e)))
            page.goto(HTML.as_uri())
            page.wait_for_timeout(500)
            got = page.evaluate(PROBE)
            browser.close()
    except Exception as exc:  # noqa: BLE001
        print(f"--   could not run a browser ({exc}), skipping")
        return 0

    fw = firmware()
    fails = 0

    def check(name, a, b):
        nonlocal fails
        ok = a == b
        fails += not ok
        print(
            ("ok   " if ok else "FAIL ") + name + ("" if ok else f"\n       browser {a!r}\n       firmware {b!r}")
        )

    for key in sorted(fw):
        check(f"the two builds agree on {key}", got[key], fw[key])
    for day, want in PINNED.items():
        check(f"{day} is the same run in both", tuple(got["daily"][str(day)]), want)
    check("round one pays 1+2+3 notes and a round bonus", got["normal"], [160, 6, 1])
    check("and pays exactly the same on insane and fast", got["insane"], [160, 6, 1])
    check("a new best lands in the slot it was played on", got["newBest"], [True, 400, 5000])
    if errors:
        fails += 1
        print("FAIL the browser build threw:", errors)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
