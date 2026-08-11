#!/usr/bin/env python3
"""
Build the offline documentation site into docs/.

GitHub Pages here is configured as "deploy from main, folder /docs" with no
build step, so whatever this produces has to be committed. Run it after editing
anything under docs-src/ and commit the result.

    python tools/docs-build/build.py

Everything the site needs is inlined — CSS, JavaScript, and every diagram — so
a page makes zero network requests once loaded. That is not tidiness for its
own sake: the site's job is to open on a phone, in a field, with no signal.

    python tools/docs-build/build.py --check    # verify, do not write
"""

from __future__ import annotations

import argparse
import html
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "docs-src")
OUT = os.path.join(ROOT, "docs")
SVG_DIR = os.path.join(SRC, "svg")

SITE_TITLE = "MeshCore Config"
SITE_TAGLINE = "Greyrock Labs"

# Pages serves this repo at /flipperMeshCoreConfig/, so every path has to be
# relative and the service worker scope has to be "./" — an absolute "/" scope
# would claim the wrong origin root and the PWA would never install.
PAGES = [
    {
        "file": "index.html",
        "title": "Инструкции",
        "source": None,  # generated below
    },
    {
        "file": "app-guide.html",
        "title": "Мануал приложения",
        "source": "app-guide.md",
    },
    {
        "file": "contacts-and-mesh.html",
        "title": "Контакты и сеть",
        "source": "contacts-and-mesh.md",
    },
    {
        "file": "guide-sergey.html",
        "title": "Полевой тест — Сергей",
        "source": "guide-sergey.md",
        "fallback": (
            "Исходник `MESHCORE_TESTING.md` ещё не закоммичен в репозиторий.\n\n"
            "🚨 Эта страница соберётся автоматически, как только файл появится: "
            "положи его в корень репозитория и запусти сборку сайта.\n"
        ),
    },
    {
        "file": "guide-mark.html",
        "title": "Полевой тест — Марк",
        # Sourced straight from the repo root and transformed on the way
        # through, so re-running the build after the author updates his guide
        # keeps exactly the two edits we are allowed to make and nothing else.
        "root_source": "MESHCORE_MARK.md",
        "transform": "mark",
        "fallback": (
            "Исходник `MESHCORE_MARK.md` ещё не закоммичен в репозиторий.\n\n"
            "🚨 Эта страница соберётся автоматически, как только файл появится: "
            "положи его в корень репозитория и запусти сборку сайта.\n"
        ),
    },
]

# The only two changes Mark's guide is allowed to receive. His methodology is
# not ours to edit: he stays on the phone, and anything that quietly switched
# his firmware or turned his radio off would leave him unable to see his nodes.
MARK_INTRO = """> 💡 **Ты ничего не паяешь и ничего не меняешь в своём процессе.**
> Наша прошивка сама определяет, воткнут ли флиппер в ноду. У тебя он не воткнут,
> значит нода всегда поднимает WiFi или BLE — ровно как раньше. Termux, приложение,
> Trace Path: всё работает как работало.

> ✅ Логгер, который упоминается ниже: [скачать meshlog.py](meshlog.py)

> 🛶 **Часть 3 переписана под сплав по реке** — старая версия обходила ровную
> знакомую площадку, а её больше нет. Части 1–2 (дома) не изменились. Полевая
> часть у вас с Сергеем **одна и та же**, слово в слово: сценарии совместные.

"""

MARK_WIRING_NOTE = """
---

## Справочно: как это выглядит у Сергея

{{svg:wiring}} | Распайка нод к флипперу — **это сторона Сергея, тебе она не нужна.** Показано, чтобы вы понимали друг друга, когда будете сводить логи.

"""


# The field part is the one place the two guides must agree word for word: the
# scenarios in it are performed by the two of them together, and a difference
# there is two people running different experiments and comparing the numbers
# afterwards. So it lives in one file and both pages include it.
MARK_FIELD_HEAD = """
---

# ЧАСТЬ 3. В ПОЛЕ — сплав по реке 🛶

### Что взять

Три ноды (подписанные) · телефон · повербанки · кабели · пароль от `REP3` ·
гермомешок и зип-пакеты · оффлайн-карты, скачанные заранее.

### Как ты пишешь лог

Всё как раньше, инструмент не изменился:

```bash
termux-wake-lock
cd ~
python meshlog.py --tcp 192.168.4.1:5000 --ping BASE,REP1,REP2 --interval 15
```

**Метки в логе без рук:** отправь с `POCKET-M` сообщение в канал вроде
`TOCHKA povorot`. Оно прилетит на оба ровера и запишется в `events.csv` с
временем — потом сопоставишь с координатами.

**Точечные трейсы.** В ключевых точках с приложения: **Tools → Trace Path** до
каждой фиксированной ноды. **SNR** — главное число, сразу видно слабое звено.

{{include:field-common}}

---

## Сворачиваемся

- `Ctrl-C` в Termux. CSV пишутся на диск сразу, так что даже если телефон
  вырубится — данные на месте.
- Скопируй логи в память телефона: `cp -r ~/meshtest ~/storage/downloads/`
- **Переименуй папку в `meshtest-mark`** и отправь Сергею — иначе при сведении
  двух комплектов файлы перепутаются.

> 💡 На сплаве несколько дней. **Копируй логи в конце каждого дня**, а не в конце
> похода: утонувший или разряженный телефон на четвёртый день унесёт всё сразу.

"""

MARK_FIELD_START = "# ЧАСТЬ 3. В ПОЛЕ — совместный прогон 🏕️"
MARK_FIELD_END = "# 🪤 ГРАБЛИ"


def transform_mark(md: str) -> str:
    """Swap the flasher, prepend the note, and swap in the shared field part.

    The first two edits keep his methodology intact — he stays on the phone. The
    third replaces Part 3 wholesale because the trip itself changed: the old
    part walks a known flat site, and there is no such site any more.
    """
    # His three images live on our flasher now, alongside ours.
    md = md.replace(
        "Веб-флешер: **flasher.meshcore.io**",
        "Веб-флешер: **hleserg.github.io/meshcore-node-fw**",
    )
    md = md.replace("flasher.meshcore.io", "hleserg.github.io/meshcore-node-fw")

    start = md.find(MARK_FIELD_START)
    end = md.find(MARK_FIELD_END)
    if start == -1 or end == -1 or end < start:
        raise SystemExit(
            "Mark's Part 3 could not be located -- refusing to build a guide "
            "whose field part silently disagrees with Sergey's."
        )
    # Back up over the horizontal rule that introduces the old part, so the new
    # head can bring its own.
    rule = md.rfind("\n---\n", 0, start)
    if rule != -1 and start - rule < 8:
        start = rule
    md = md[:start] + MARK_FIELD_HEAD + "\n" + md[end:]

    return MARK_INTRO + md + MARK_WIRING_NOTE


TRANSFORMS = {"mark": transform_mark}


def expand_includes(md: str) -> str:
    """Splice {{include:name}} in from docs-src/name.md.

    Deliberately not recursive: one level is all the shared field part needs,
    and nesting would make it hard to see what a page actually contains.
    """
    def one(match: "re.Match[str]") -> str:
        path = os.path.join(SRC, match.group(1) + ".md")
        if not os.path.exists(path):
            raise SystemExit(f"include not found: {path}")
        with open(path, encoding="utf-8") as handle:
            return handle.read().strip("\n")

    return re.sub(r"^\{\{include:([a-z0-9-]+)\}\}$", one, md, flags=re.M)

# Cached by the service worker. meshlog.py is here because the site offers it
# for download and that download has to work offline too.
CACHE_FILES = [
    "./",
    "./index.html",
    "./app-guide.html",
    "./contacts-and-mesh.html",
    "./guide-sergey.html",
    "./guide-mark.html",
    "./meshlog.py",
    "./manifest.webmanifest",
]

CSS = """
:root{
  --bg:#0d0f0f; --panel:#151918; --panel-2:#1c2120; --ink:#d7e0dc; --dim:#8fa39b;
  --line:#2a312f; --accent:#5ef2a4; --accent-dim:#2f7d59; --amber:#ffb340;
  --warn:#ffcc66; --danger:#ff7a6b; --tip:#7fd6ff; --ok:#5ef2a4;
  --mono:ui-monospace,'Cascadia Mono','Segoe UI Mono','SF Mono',Menlo,Consolas,monospace;
}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{
  margin:0;background:var(--bg);color:var(--ink);
  font:16px/1.65 var(--mono);
  padding:0 0 4rem;
}
.wrap{max-width:52rem;margin:0 auto;padding:0 1rem}
header.site{
  border-bottom:1px solid var(--line);background:linear-gradient(180deg,#121615,#0d0f0f);
  position:sticky;top:0;z-index:20;backdrop-filter:blur(6px)
}
header.site .wrap{display:flex;align-items:center;gap:.75rem;padding-top:.7rem;padding-bottom:.7rem}
header.site a.brand{color:var(--accent);text-decoration:none;font-weight:700;letter-spacing:.02em}
header.site .tag{color:var(--dim);font-size:.78rem;margin-left:auto}
h1,h2,h3,h4{line-height:1.25;margin:2rem 0 .75rem;color:#eef5f2}
h1{font-size:1.6rem;margin-top:1.5rem}
h2{font-size:1.25rem;border-bottom:1px solid var(--line);padding-bottom:.35rem}
h3{font-size:1.05rem;color:var(--accent)}
p,li{overflow-wrap:anywhere}
a{color:var(--accent);text-underline-offset:.2em}
code{background:var(--panel-2);padding:.12em .38em;border-radius:.25rem;font-size:.92em}
pre{
  background:var(--panel);border:1px solid var(--line);border-radius:.5rem;
  padding:.85rem 1rem;overflow-x:auto;position:relative;margin:1rem 0
}
pre code{background:none;padding:0;font-size:.86rem;line-height:1.5}
.copy{
  position:absolute;top:.4rem;right:.4rem;background:var(--panel-2);color:var(--dim);
  border:1px solid var(--line);border-radius:.35rem;padding:.28rem .6rem;
  font:inherit;font-size:.72rem;cursor:pointer;transition:.15s
}
.copy:hover{color:var(--accent);border-color:var(--accent-dim)}
.copy.done{color:#0d0f0f;background:var(--accent);border-color:var(--accent)}
table{border-collapse:collapse;width:100%;margin:1rem 0;font-size:.9rem;display:block;overflow-x:auto}
th,td{border:1px solid var(--line);padding:.5rem .6rem;text-align:left;vertical-align:top}
th{background:var(--panel-2);color:#eef5f2}
tr:nth-child(even) td{background:rgba(255,255,255,.015)}
blockquote{margin:0;padding:0}
.callout{
  border-left:3px solid var(--dim);background:var(--panel);border-radius:.4rem;
  padding:.8rem 1rem;margin:1.1rem 0
}
.callout p:first-child{margin-top:0}.callout p:last-child{margin-bottom:0}
.callout.warning{border-color:var(--warn);background:rgba(255,204,102,.07)}
.callout.danger{border-color:var(--danger);background:rgba(255,122,107,.08)}
.callout.tip{border-color:var(--tip);background:rgba(127,214,255,.07)}
.callout.success{border-color:var(--ok);background:rgba(94,242,164,.07)}
.toc{background:var(--panel);border:1px solid var(--line);border-radius:.5rem;margin:1.5rem 0}
.toc summary{cursor:pointer;padding:.7rem 1rem;color:var(--accent);font-weight:700}
.toc ol{margin:0;padding:0 1rem 1rem 2rem}
.toc li{margin:.25rem 0}
.toc a{color:var(--ink);text-decoration:none}
.toc a:hover{color:var(--accent)}
.toc .lvl3{margin-left:1rem;font-size:.9rem;color:var(--dim)}
.cards{display:grid;gap:1rem;grid-template-columns:1fr;margin:1.5rem 0}
@media(min-width:34rem){.cards{grid-template-columns:1fr 1fr}}
.card{
  display:block;background:var(--panel);border:1px solid var(--line);border-radius:.6rem;
  padding:1.1rem;text-decoration:none;color:var(--ink);transition:.15s
}
.card:hover{border-color:var(--accent-dim);transform:translateY(-2px)}
.card h3{margin:.1rem 0 .4rem;color:var(--accent)}
.card p{margin:0;color:var(--dim);font-size:.88rem}
.card .who{font-size:.72rem;color:var(--amber);text-transform:uppercase;letter-spacing:.08em}
figure{margin:1.5rem 0;text-align:center}
figure svg{max-width:100%;height:auto;border:1px solid var(--line);border-radius:.5rem;background:#120e05}
figcaption{color:var(--dim);font-size:.82rem;margin-top:.5rem;text-align:left}
.gotcha{border:1px solid var(--line);border-radius:.5rem;margin:.6rem 0;background:var(--panel)}
.gotcha summary{cursor:pointer;padding:.7rem 1rem;color:var(--warn)}
.gotcha .body{padding:0 1rem 1rem;border-top:1px solid var(--line)}
.top{
  position:fixed;right:1rem;bottom:1rem;background:var(--panel-2);border:1px solid var(--line);
  color:var(--dim);border-radius:.5rem;padding:.5rem .7rem;text-decoration:none;font-size:.8rem;
  opacity:0;pointer-events:none;transition:.2s;z-index:30
}
.top.show{opacity:1;pointer-events:auto}
footer.site{border-top:1px solid var(--line);margin-top:3rem;padding-top:1rem;color:var(--dim);font-size:.8rem}
.offline-note{color:var(--dim);font-size:.78rem;margin-top:.4rem}
"""

JS = """
// Copy button on every code block. The site is read on a phone in a field —
// retyping a command from a screen is exactly where mistakes come from.
document.querySelectorAll('pre').forEach(function(pre){
  var btn = document.createElement('button');
  btn.className = 'copy';
  btn.type = 'button';
  btn.textContent = 'Копировать';
  btn.addEventListener('click', function(){
    var text = pre.querySelector('code') ? pre.querySelector('code').innerText : pre.innerText;
    var done = function(){
      btn.textContent = 'Скопировано ✓';
      btn.classList.add('done');
      setTimeout(function(){ btn.textContent = 'Копировать'; btn.classList.remove('done'); }, 1400);
    };
    if(navigator.clipboard && navigator.clipboard.writeText){
      navigator.clipboard.writeText(text).then(done, fallback);
    } else { fallback(); }
    function fallback(){
      // execCommand is deprecated but still the only thing that works when the
      // page is opened from a file:// URL or an old mobile browser.
      var ta = document.createElement('textarea');
      ta.value = text; ta.style.position='fixed'; ta.style.opacity='0';
      document.body.appendChild(ta); ta.select();
      try { document.execCommand('copy'); done(); } catch(e) {}
      document.body.removeChild(ta);
    }
  });
  pre.appendChild(btn);
});

// Back to top, once the reader is far enough down to want it.
var top = document.querySelector('.top');
if(top){
  window.addEventListener('scroll', function(){
    top.classList.toggle('show', window.scrollY > 700);
  }, {passive:true});
}

// Register the service worker with an explicit relative scope: the site lives
// on a sub-path, and an absolute scope would claim the wrong root.
if('serviceWorker' in navigator){
  window.addEventListener('load', function(){
    navigator.serviceWorker.register('sw.js', {scope: './'});
  });
}
"""


# ----------------------------------------------------------------- markdown
CALLOUTS = {"⚠️": "warning", "🚨": "danger", "💡": "tip", "✅": "success"}


def slugify(text: str) -> str:
    text = re.sub(r"<[^>]+>", "", text)
    text = text.strip().lower()
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    text = re.sub(r"[\s_]+", "-", text)
    return text.strip("-") or "section"


def inline(text: str) -> str:
    """Inline markdown. Escaping happens first so source text cannot inject."""
    out = html.escape(text, quote=False)
    out = re.sub(r"`([^`]+)`", lambda m: "<code>" + m.group(1) + "</code>", out)
    out = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", out)
    out = re.sub(r"(?<![\w*])\*([^*\n]+)\*(?![\w*])", r"<em>\1</em>", out)
    out = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', out)
    return out


def render_table(rows: list[str]) -> str:
    def cells(line: str) -> list[str]:
        return [c.strip() for c in line.strip().strip("|").split("|")]

    head = cells(rows[0])
    body = [cells(r) for r in rows[2:]]
    out = ["<table><thead><tr>"]
    out += [f"<th>{inline(c)}</th>" for c in head]
    out.append("</tr></thead><tbody>")
    for row in body:
        out.append("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>")
    out.append("</tbody></table>")
    return "".join(out)


def render(md: str, svgs: dict[str, str]) -> tuple[str, list[tuple[int, str, str]]]:
    """Markdown subset -> HTML. Returns the body and the heading list for a TOC."""
    lines = md.replace("\r\n", "\n").split("\n")
    out: list[str] = []
    toc: list[tuple[int, str, str]] = []
    i = 0

    while i < len(lines):
        line = lines[i]

        # fenced code
        if line.startswith("```"):
            i += 1
            block = []
            while i < len(lines) and not lines[i].startswith("```"):
                block.append(lines[i])
                i += 1
            i += 1
            body = html.escape("\n".join(block), quote=False)
            out.append(f"<pre><code>{body}</code></pre>")
            continue

        # inlined diagram
        m = re.match(r"^\{\{svg:([\w-]+)\}\}\s*(?:\|\s*(.*))?$", line.strip())
        if m:
            name, caption = m.group(1), (m.group(2) or "")
            svg = svgs.get(name)
            if svg is None:
                raise SystemExit(f"missing diagram: {name}.svg")
            out.append("<figure>" + svg)
            if caption:
                out.append(f"<figcaption>{inline(caption)}</figcaption>")
            out.append("</figure>")
            i += 1
            continue

        # heading
        m = re.match(r"^(#{1,4})\s+(.*)$", line)
        if m:
            level = len(m.group(1))
            text = m.group(2).strip()
            anchor = slugify(text)
            if level in (2, 3):
                toc.append((level, anchor, text))
            out.append(f'<h{level} id="{anchor}">{inline(text)}</h{level}>')
            i += 1
            continue

        # table
        if line.strip().startswith("|") and i + 1 < len(lines) and re.match(
            r"^\s*\|[\s:|-]+\|\s*$", lines[i + 1]
        ):
            block = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                block.append(lines[i])
                i += 1
            out.append(render_table(block))
            continue

        # blockquote -> callout, keyed by its leading marker
        if line.startswith(">"):
            block = []
            while i < len(lines) and lines[i].startswith(">"):
                block.append(lines[i][1:].lstrip())
                i += 1
            text = "\n".join(block).strip()
            kind = ""
            for marker, name in CALLOUTS.items():
                if text.startswith(marker):
                    kind = " " + name
                    break
            inner, _ = render(text, svgs)
            out.append(f'<div class="callout{kind}">{inner}</div>')
            continue

        # gotcha accordion: "?? symptom" then indented body
        if line.startswith("?? "):
            summary = line[3:].strip()
            i += 1
            block = []
            while i < len(lines) and (lines[i].startswith("    ") or not lines[i].strip()):
                if not lines[i].strip() and (
                    i + 1 >= len(lines) or not lines[i + 1].startswith("    ")
                ):
                    break
                block.append(lines[i][4:] if lines[i].startswith("    ") else "")
                i += 1
            inner, _ = render("\n".join(block), svgs)
            out.append(
                f'<details class="gotcha"><summary>{inline(summary)}</summary>'
                f'<div class="body">{inner}</div></details>'
            )
            continue

        # lists
        if re.match(r"^\s*[-*]\s+", line):
            items = []
            while i < len(lines) and re.match(r"^\s*[-*]\s+", lines[i]):
                items.append(re.sub(r"^\s*[-*]\s+", "", lines[i]))
                i += 1
            out.append("<ul>" + "".join(f"<li>{inline(x)}</li>" for x in items) + "</ul>")
            continue

        if re.match(r"^\s*\d+\.\s+", line):
            items = []
            while i < len(lines) and re.match(r"^\s*\d+\.\s+", lines[i]):
                items.append(re.sub(r"^\s*\d+\.\s+", "", lines[i]))
                i += 1
            out.append("<ol>" + "".join(f"<li>{inline(x)}</li>" for x in items) + "</ol>")
            continue

        if not line.strip():
            i += 1
            continue

        # paragraph
        block = []
        while i < len(lines) and lines[i].strip() and not re.match(
            r"^(#{1,4}\s|```|\||>|\s*[-*]\s|\s*\d+\.\s|\?\? )", lines[i]
        ):
            block.append(lines[i])
            i += 1
        out.append("<p>" + inline(" ".join(block)) + "</p>")

    return "".join(out), toc


def toc_html(entries: list[tuple[int, str, str]]) -> str:
    if len(entries) < 3:
        return ""
    items = []
    for level, anchor, text in entries:
        cls = ' class="lvl3"' if level == 3 else ""
        items.append(f'<li{cls}><a href="#{anchor}">{html.escape(text)}</a></li>')
    return (
        '<details class="toc" open><summary>Содержание</summary><ol>'
        + "".join(items)
        + "</ol></details>"
    )


def page(title: str, body: str, toc: str, active: str) -> str:
    nav = ""
    if active != "index.html":
        nav = '<a href="index.html" style="color:var(--dim);text-decoration:none">&larr; Все инструкции</a>'
    return f"""<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0d0f0f">
<title>{html.escape(title)} — {SITE_TITLE}</title>
<link rel="manifest" href="manifest.webmanifest">
<style>{CSS}</style>
</head>
<body>
<header class="site"><div class="wrap">
<a class="brand" href="index.html">{SITE_TITLE}</a>
<span class="tag">{SITE_TAGLINE}</span>
</div></header>
<main class="wrap">
{nav}
{toc}
{body}
<footer class="site">
<p>{SITE_TITLE} — {SITE_TAGLINE}. Страница работает без сети: добавь её на главный экран телефона.</p>
</footer>
</main>
<a class="top" href="#">↑ наверх</a>
<script>{JS}</script>
</body>
</html>
"""


def build_index() -> str:
    cards = [
        (
            "app-guide.html",
            "Мануал приложения",
            "для всех",
            "Что умеет приложение на флиппере: три режима, навигация по меню, пресеты, как читать экран логгера.",
        ),
        (
            "contacts-and-mesh.html",
            "Контакты и сеть",
            "прочитать до похода",
            "Почему одна частота = одна сеть, как контакт привязан к ноде, как добавить контакт без QR и отдать себя телефону. Чеклист «в лесу».",
        ),
        (
            "guide-sergey.html",
            "Полевой тест — Сергей",
            "флиппер + 2 ноды",
            "Дома: подготовка роверов и риг. В поле: сплав по реке, программы «Лагерь» и «Сплав», критерии приёмки.",
        ),
        (
            "guide-mark.html",
            "Полевой тест — Марк",
            "телефон",
            "Домашняя часть без изменений: стоковая прошивка, Termux, приложение. Полевая — та же, что у Сергея.",
        ),
        (
            "meshlog.py",
            "Скачать meshlog.py",
            "логгер для ноутбука",
            "Тот же логгер, что собирает CSV с ноды через USB, WiFi или BLE. Скачивается и работает офлайн.",
        ),
    ]
    body = [
        "<h1>Инструкции проекта</h1>",
        "<p>Всё, что нужно в поле. Страницы открываются без интернета — "
        "добавь сайт на главный экран телефона заранее.</p>",
        '<p class="offline-note">🛶 Полевая часть переписана под <b>сплав по реке</b>. '
        "В обоих гайдах она одна и та же — сценарии совместные.</p>",
        '<div class="cards">',
    ]
    for href, title, who, text in cards:
        dl = ' download' if href.endswith(".py") else ""
        body.append(
            f'<a class="card" href="{href}"{dl}>'
            f'<div class="who">{html.escape(who)}</div>'
            f"<h3>{html.escape(title)}</h3><p>{html.escape(text)}</p></a>"
        )
    body.append("</div>")
    body.append(
        '<h2 id="proshivka">Прошивка нод</h2>'
        "<p>Прошивка роверов Сергея — наш флешер: "
        '<a href="https://hleserg.github.io/meshcore-node-fw">hleserg.github.io/meshcore-node-fw</a>'
        " (UART-сборка). Репитеры и все ноды Марка — официальный "
        '<a href="https://flasher.meshcore.io">flasher.meshcore.io</a>.</p>'
        '<p class="offline-note">Ссылки на флешеры требуют интернета — прошивай дома, '
        "не в поле.</p>"
    )
    return "".join(body)


def load_svgs() -> dict[str, str]:
    svgs = {}
    if os.path.isdir(SVG_DIR):
        for name in sorted(os.listdir(SVG_DIR)):
            if name.endswith(".svg"):
                with open(os.path.join(SVG_DIR, name), encoding="utf-8") as handle:
                    svgs[name[:-4]] = handle.read().strip()
    return svgs


def service_worker() -> str:
    listed = ",\n  ".join(f'"{path}"' for path in CACHE_FILES)
    return f"""// Offline cache for the documentation site.
// Bumping CACHE invalidates everything, which is what a rebuild wants.
const CACHE = "meshcore-docs-v1";
const FILES = [
  {listed}
];

self.addEventListener("install", (event) => {{
  event.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)));
  self.skipWaiting();
}});

self.addEventListener("activate", (event) => {{
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
}});

// Cache first: in the field there is no network, and a stale page beats no page.
self.addEventListener("fetch", (event) => {{
  if (event.request.method !== "GET") return;
  event.respondWith(
    caches.match(event.request).then((hit) => hit || fetch(event.request))
  );
}});
"""


MANIFEST = """{
  "name": "MeshCore Config — инструкции",
  "short_name": "MeshCore",
  "start_url": "./index.html",
  "scope": "./",
  "display": "standalone",
  "background_color": "#0d0f0f",
  "theme_color": "#0d0f0f",
  "lang": "ru",
  "icons": [
    {
      "src": "icon.svg",
      "sizes": "any",
      "type": "image/svg+xml",
      "purpose": "any maskable"
    }
  ]
}
"""

ICON = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 192 192">
<rect width="192" height="192" rx="36" fill="#0d0f0f"/>
<g fill="none" stroke="#5ef2a4" stroke-width="9" stroke-linecap="round">
<circle cx="52" cy="58" r="13"/><circle cx="140" cy="58" r="13"/><circle cx="96" cy="136" r="13"/>
<path d="M62 68 L88 124"/><path d="M130 68 L104 124"/><path d="M65 58 L127 58"/>
</g></svg>
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify only, write nothing")
    args = parser.parse_args()

    svgs = load_svgs()
    os.makedirs(OUT, exist_ok=True)

    problems: list[str] = []
    written: list[str] = []

    for spec in PAGES:
        if spec["file"] == "index.html":
            body, toc = build_index(), ""
        else:
            if spec.get("root_source"):
                path = os.path.join(ROOT, spec["root_source"])
                name_for_error = spec["root_source"]
            else:
                path = os.path.join(SRC, spec["source"])
                name_for_error = spec["source"]

            if os.path.exists(path):
                with open(path, encoding="utf-8") as handle:
                    md = handle.read()
                transform = TRANSFORMS.get(spec.get("transform", ""))
                if transform:
                    md = transform(md)
                # Includes are expanded before the effective copy is written so
                # that copy is literally what got rendered — the verifier counts
                # sections against it, and a marker left in would undercount.
                md = expand_includes(md)
                if transform:
                    # Keep the transformed markdown next to the sources: it is
                    # what actually got rendered, so a reviewer can diff it
                    # against the original and see the two edits, and the
                    # verifier can count sections against reality.
                    if not args.check:
                        effective = os.path.join(SRC, spec["file"].replace(".html", ".effective.md"))
                        with open(effective, "w", encoding="utf-8", newline="\n") as out_handle:
                            out_handle.write(md)
            else:
                spec = dict(spec, source=name_for_error)
                fallback = spec.get("fallback")
                if not fallback:
                    problems.append(f"missing source: {spec['source']}")
                    continue
                md = f"# {spec['title']}\n\n> {fallback}"
                problems.append(f"placeholder used for {spec['file']} ({spec['source']} absent)")
            body, headings = render(md, svgs)
            toc = toc_html(headings)

        html_text = page(spec["title"], body, toc, spec["file"])
        target = os.path.join(OUT, spec["file"])
        if not args.check:
            with open(target, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(html_text)
        written.append(spec["file"])

    if not args.check:
        with open(os.path.join(OUT, "sw.js"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(service_worker())
        with open(
            os.path.join(OUT, "manifest.webmanifest"), "w", encoding="utf-8", newline="\n"
        ) as handle:
            handle.write(MANIFEST)
        with open(os.path.join(OUT, "icon.svg"), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(ICON)
        # The site offers the logger for download, so it has to live under docs/.
        shutil.copyfile(os.path.join(ROOT, "meshlog.py"), os.path.join(OUT, "meshlog.py"))
        with open(os.path.join(OUT, ".nojekyll"), "w", encoding="utf-8") as handle:
            handle.write("")

    print("built: " + ", ".join(written))
    for problem in problems:
        print("  note: " + problem)
    return 0


if __name__ == "__main__":
    sys.exit(main())
