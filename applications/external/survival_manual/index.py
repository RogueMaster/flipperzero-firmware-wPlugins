import os, re

OUT = "flipper_app/assets"
MAXCHUNK = 3500  # bytes per loadable section

PAGES = __import__("clean")  # reuse PAGES list
from clean import PAGES

def sections_for(text):
    """Yield (title, start, length) ranges over `text` (bytes/utf-8 str)."""
    # Find heading line positions
    heads = [(m.start(), m.group(1).strip())
             for m in re.finditer(r'^== (.+?) ==\s*$', text, flags=re.M)]
    spans = []
    if not heads or heads[0][0] > 0:
        spans.append(("Overview", 0, heads[0][0] if heads else len(text)))
    for i,(pos,title) in enumerate(heads):
        end = heads[i+1][0] if i+1 < len(heads) else len(text)
        spans.append((title, pos, end-pos))
    # Split oversized spans on paragraph/newline boundaries
    out = []
    for title, start, length in spans:
        if length <= MAXCHUNK:
            out.append((title, start, length)); continue
        seg = text[start:start+length]
        part = 1; off = 0
        while off < len(seg):
            window = seg[off:off+MAXCHUNK]
            if off+MAXCHUNK < len(seg):
                cut = window.rfind('\n\n')
                if cut < MAXCHUNK//2: cut = window.rfind('\n')
                if cut < MAXCHUNK//2: cut = len(window)
            else:
                cut = len(window)
            t = title if (part==1 and off+cut>=len(seg)) else f"{title} ({part})"
            out.append((t, start+off, cut))
            off += cut; part += 1
    return out

def san(t):
    t = re.sub(r'[^\x20-\x7E]', ' ', t).strip()
    return (t[:31]) if len(t) > 31 else t

total_sec = 0
for base, _ in PAGES:
    p = os.path.join(OUT, base + ".txt")
    if not os.path.exists(p): continue
    text = open(p, encoding="utf-8").read()
    secs = sections_for(text)
    total_sec += len(secs)
    with open(os.path.join(OUT, base + ".idx"), "w", encoding="utf-8") as f:
        for title, start, length in secs:
            # byte offsets (utf-8) — recompute in bytes to be safe
            b_start = len(text[:start].encode("utf-8"))
            b_len   = len(text[start:start+length].encode("utf-8"))
            f.write(f"{b_start}\t{b_len}\t{san(title)}\n")

print(f"Indexed {len(PAGES)} pages, {total_sec} sections total")
# show distribution
import glob
mx = 0
for ix in glob.glob(OUT+"/*.idx"):
    for line in open(ix):
        mx = max(mx, int(line.split('\t')[1]))
print(f"Largest single section: {mx} bytes (well under TextBox limits)")
