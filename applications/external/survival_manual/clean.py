import os, re

SRC = "survival_wiki"
OUT = "flipper_app/assets"
os.makedirs(OUT, exist_ok=True)

# Reading order; (basename, display title). TranslatorNotes/_Footer/Home excluded.
PAGES = [
 ("Introduction","Introduction"),("Psychology","Psychology"),("Power","Power"),
 ("Planning","Planning"),("Kits","Kits"),("Apps","Apps"),("Medicine","Basic Medicine"),
 ("Shelter","Shelter"),("Water","Water Procurement"),("Fire","Firecraft"),
 ("Food","Food Procurement"),("Plants","Survival Use of Plants"),("Animals","Dangerous Animals"),
 ("Tools","Tools and Equipment"),("CarRepair","Car Repair"),
 ("BasicMechanicalSkills","Basic Mechanical Skills"),("BlackoutDriving","Blackout Driving"),
 ("Weapons","Weapons"),("Desert","Desert"),("Tropical","Tropical"),("Cold","Cold Weather"),
 ("Sea","Sea"),("WaterCrossing","Expedient Water Crossing"),
 ("DirectionFinding","Direction Finding"),("Signaling","Signaling Techniques"),
 ("HostileAreas","Survival in Hostile Areas"),("Camouflage","Camouflage"),
 ("People","Contact With People"),("ManMadeHazards","Man-Made Hazards"),
 ("MultiTool","MultiTool"),("DangerousArthropods","Dangerous Arthropods"),
 ("FishAndMollusks","Dangerous Fish & Mollusks"),("RopesAndKnots","Ropes and Knots"),
 ("Poisonous-Plants","Poisonous Plants"),("Self-defense","Self-Defense"),
 ("FAQ","FAQ"),("Credits","Credits"),
]

def clean(md):
    s = md
    s = re.sub(r'<!--.*?-->', '', s, flags=re.S)          # html comments
    s = re.sub(r'<a name="[^"]*">\s*</a>', '', s)          # anchor tags
    s = re.sub(r'<[^>]+>', '', s)                          # any other html tags
    s = re.sub(r'!\[[^\]]*\]\([^)]*\)', '[figure]', s)     # images -> marker
    s = re.sub(r'\[([^\]]+)\]\([^)]*\)', r'\1', s)         # links -> text
    s = re.sub(r'`([^`]*)`', r'\1', s)                     # inline code
    s = re.sub(r'\*\*([^*]+)\*\*', r'\1', s)               # bold
    s = re.sub(r'__([^_]+)__', r'\1', s)                   # bold _
    lines = []
    for ln in s.split('\n'):
        h = re.match(r'^(#{1,6})\s+(.*)$', ln)
        if h:
            title = h.group(2).strip().rstrip('\\').strip()
            lines.append('')
            lines.append('== ' + title.upper() + ' ==')
            continue
        ln = re.sub(r'^\s*>\s?', '', ln)                   # blockquote
        ln = re.sub(r'^(\s*)[\*\-]\s+', r'\1- ', ln)       # bullets
        ln = re.sub(r'\*([^*]+)\*', r'\1', ln)             # italics
        ln = ln.replace('\\.', '.').replace('\\-', '-').replace('\\*','*')
        ln = ln.rstrip()
        lines.append(ln)
    s = '\n'.join(lines)
    s = re.sub(r'\n{3,}', '\n\n', s).strip() + '\n'
    return s

index = []
for base, title in PAGES:
    p = os.path.join(SRC, base + ".md")
    if not os.path.exists(p):
        print("MISSING", p); continue
    txt = clean(open(p, encoding="utf-8").read())
    out = base + ".txt"
    open(os.path.join(OUT, out), "w", encoding="utf-8").write(txt)
    index.append((title, out, len(txt)))

print(f"Wrote {len(index)} pages to {OUT}")
total = sum(i[2] for i in index)
print(f"Total cleaned text: {total//1024} KB; largest:")
for t,o,n in sorted(index, key=lambda x:-x[2])[:4]:
    print(f"  {n//1024:>3} KB  {o}  ({t})")

# Emit the C page table so the app and assets never drift
with open("flipper_app/pages.h","w") as f:
    f.write("// Auto-generated page table. Do not edit by hand.\n#pragma once\n")
    f.write("typedef struct { const char* title; const char* file; } SurvivalPage;\n")
    f.write(f"#define SURVIVAL_PAGE_COUNT {len(index)}\n")
    f.write("static const SurvivalPage survival_pages[SURVIVAL_PAGE_COUNT] = {\n")
    for t,o,_ in index:
        f.write(f'    {{ "{t}", "{o}" }},\n')
    f.write("};\n")
print("Wrote flipper_app/pages.h")
