"""Generate SUPPORTED_CHIPS.md from chip_db.c so the two can never drift."""
import re
import pathlib

SRC = pathlib.Path('fake_chip_detector/chip_db.c')
OUT = pathlib.Path('fake_chip_detector/SUPPORTED_CHIPS.md')
src = SRC.read_text(encoding='utf-8')

MASKS = {'M8': 0xFF, 'M16': 0xFFFF}


def num(tok):
    tok = tok.strip()
    if tok in MASKS:
        return MASKS[tok]
    return int(tok, 0)


# ---- IdCheck arrays -------------------------------------------------------
checks = {}
for m in re.finditer(
        r'static const IdCheck (\w+)\[\]\s*=\s*\{(.*?)\};', src, re.S):
    name, body = m.group(1), m.group(2)
    rows = []
    # A trailing // comment names the register, but only when it really is one:
    # two checks often share a line, and the second brace is not a comment.
    for cm in re.finditer(r'\{([^{}]*)\}\s*,?[ \t]*(?://[ \t]*([^\n]*))?', body):
        fields = [f.strip() for f in cm.group(1).split(',')]
        if len(fields) < 5:
            continue
        comment = ' '.join((cm.group(2) or '').split())
        rows.append({
            'reg': num(fields[0]),
            'expected': num(fields[1]),
            'mask': num(fields[2]),
            'wide': fields[3] == 'true',
            'reg16': fields[4] == 'true',
            'label': comment,
        })
    checks[name] = rows

# ---- chip table -----------------------------------------------------------
table = re.search(r'static const ChipEntry chip_db\[\]\s*=\s*\{(.*?)\n\};', src, re.S).group(1)
entries = []
for line in table.splitlines():
    line = line.strip()
    if not line.startswith('{"'):
        continue
    m = re.match(
        r'\{"([^"]*)",\s*"([^"]*)",\s*\{([^}]*)\},\s*([^,]+),\s*([^,]+),\s*'
        r'([\w]+|NULL),\s*(\d+),\s*(NULL|"[^"]*")\s*\},?',
        line)
    if not m:
        raise SystemExit('unparsed chip line: ' + line)
    addrs = [num(a) for a in m.group(3).split(',') if a.strip()]
    addrs = [a for a in addrs if a != 0xFF]
    lo, hi = num(m.group(4)), num(m.group(5))
    note = None if m.group(8) == 'NULL' else m.group(8).strip('"')
    entries.append({
        'name': m.group(1),
        'kind': m.group(2),
        'addrs': addrs,
        'lo': lo,
        'hi': hi,
        'checks': [] if m.group(6) == 'NULL' else checks[m.group(6)][:int(m.group(7))],
        'note': note,
    })


def addr_str(e):
    if e['lo']:
        return '0x%02X-0x%02X' % (e['lo'], e['hi'])
    return ', '.join('0x%02X' % a for a in e['addrs'])


def reg_cell(e):
    if not e['checks']:
        return '—'
    out = []
    for c in e['checks']:
        w = 4 if c['reg16'] else 2
        s = '`0x%0*X`' % (w, c['reg'])
        if c['label']:
            s += ' ' + c['label']
        out.append(s)
    return '<br>'.join(out)


def val_cell(e):
    if not e['checks']:
        return '—'
    out = []
    for c in e['checks']:
        w = 4 if c['wide'] else 2
        s = '`0x%0*X`' % (w, c['expected'])
        full = 0xFFFF if c['wide'] else 0xFF
        if c['mask'] not in (full, 0):
            s += ' (mask `0x%0*X`)' % (w, c['mask'])
        out.append(s)
    return '<br>'.join(out)


def width_cell(e):
    if not e['checks']:
        return '—'
    return '<br>'.join('16-bit' if c['wide'] else '8-bit' for c in e['checks'])


ided = [e for e in entries if e['checks']]
noid = [e for e in entries if not e['checks']]

L = []
L.append('# Supported chips')
L.append('')
L.append('Every part **Fake Chip Detector** knows how to recognise, and exactly what it reads to')
L.append('do it. Generated from [`chip_db.c`](chip_db.c) — the app and this table cannot disagree.')
L.append('')
L.append('- **Register** — the ID register the app reads, with the datasheet name where the')
L.append('  datasheet gives one. A four-digit register index means the chip takes a 16-bit')
L.append('  register address (ST time-of-flight parts and Goodix touch controllers do).')
L.append('- **Expected** — the value a genuine part returns. A mask means only those bits are')
L.append('  compared; the rest are revision or configuration bits that legitimately vary.')
L.append('- **Width** — how many bytes the value itself is.')
L.append('- Several rows in one cell mean the app checks all of them. Every one has to match')
L.append('  before it will say GENUINE.')
L.append('')
L.append('If your chip is missing, the app says so plainly rather than calling it a fake — see')
L.append('[Adding a chip](#adding-a-chip) below.')
L.append('')
L.append('## Chips with a factory ID register (%d)' % len(ided))
L.append('')
L.append('These can be verified. A mismatch here is real evidence that the part is not what the')
L.append('label claims.')
L.append('')
L.append('| Chip | What it is | I2C address | Register | Expected | Width | Notes |')
L.append('|---|---|---|---|---|---|---|')
for e in ided:
    L.append('| **%s** | %s | %s | %s | %s | %s | %s |' % (
        e['name'], e['kind'], addr_str(e), reg_cell(e), val_cell(e), width_cell(e),
        e['note'] or ''))
L.append('')
L.append('## Chips recognised by address only (%d)' % len(noid))
L.append('')
L.append('These parts carry no ID register at all — there is nothing to read, so no honest tool')
L.append('can confirm which one it is. The app reports them as DETECTED rather than pretending')
L.append('to a verdict it cannot support.')
L.append('')
L.append('| Chip | What it is | I2C address | Notes |')
L.append('|---|---|---|---|')
for e in noid:
    L.append('| **%s** | %s | %s | %s |' % (
        e['name'], e['kind'], addr_str(e), e['note'] or ''))
L.append('')
# ---- 1-Wire families ------------------------------------------------------
ow_src = pathlib.Path('fake_chip_detector/onewire_worker.c').read_text(encoding='utf-8')
ow_body = re.search(
    r'static const OneWireFamily onewire_families\[\]\s*=\s*\{(.*?)\n\};', ow_src, re.S).group(1)
families = re.findall(r'\{(0x[0-9A-Fa-f]+),\s*"([^"]*)",\s*"([^"]*)",\s*OneWireRole(\w+)\}', ow_body)

L.append('## 1-Wire parts (%d)' % len(families))
L.append('')
L.append('A different bus, on **pin 17**, and a weaker guarantee. Every 1-Wire part carries a')
L.append('64-bit ROM code burned in at the factory, but any microcontroller can replay one, so')
L.append('finding the expected ID proves a device is *present* — never that it is authentic. The')
L.append('app says so on screen and never reports a 1-Wire part as GENUINE.')
L.append('')
L.append('What it does prove is which **part** answered: the family code (the low byte of the ROM)')
L.append('selects the command set and register layout, so a DS18S20 or DS1822 sold as a DS18B20 is')
L.append('a fact here, not a suspicion. Temperature parts are taken one step further — the app runs')
L.append('a real conversion and checks the scratchpad CRC, so it reports a working measurement')
L.append('rather than mere presence.')
L.append('')
L.append('| Family code | Part | What it is | Measured |')
L.append('|---|---|---|---|')
for fam, name, kind, role in families:
    L.append('| `%s` | **%s** | %s | %s |' % (
        fam.upper().replace('0X', '0x'), name, kind,
        'temperature' if role == 'Temperature' else '—'))
L.append('')
L.append('Family codes are from Analog Devices application note AN937 and the parts\' datasheets.')
L.append('')
L.append('## Adding a chip')
L.append('')
L.append('Add an `IdCheck` array and one `ChipEntry` row to `chip_db.c`, rebuild, then re-run')
L.append('`python tools/gen_supported_chips.py` from the repository root to regenerate this file —')
L.append('that regeneration step is the only thing keeping the table honest. The rule')
L.append('the database is held to: **every constant must come from the manufacturer datasheet or')
L.append('the vendor\'s own driver.** A wrong expected value makes the app accuse a genuine sensor')
L.append('of being counterfeit, which is far worse than not supporting the part at all. Anything')
L.append('that could not be pinned down to a primary source was deliberately left out.')
L.append('')
L.append('Cite the source in a comment, the way the existing entries do.')
L.append('')

OUT.write_text('\n'.join(L), encoding='utf-8')
print('%s: %d chips (%d with ID, %d address-only)' % (OUT, len(entries), len(ided), len(noid)))
