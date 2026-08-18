"""Generate SUPPORTED_CHIPS.md from chip_db.c so the two can never drift.

    python tools/gen_supported_chips.py            rewrite the file
    python tools/gen_supported_chips.py --check    exit 1 if it is out of date

The --check mode is what CI runs. "Can never drift" is only true while somebody
remembers to re-run the generator, and the drift that actually happened was not
even a chip edit: clang-format rewrapped the table and the parser stopped seeing
half of it. A silent generator is the failure mode, so this one fails loudly.
"""
import re
import sys
import pathlib
import difflib

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


def top_level_groups(text):
    """Yield each outermost {...} of the table as one whitespace-collapsed string.

    Parsing this line by line broke the moment clang-format decided an entry was
    too long and wrapped it, which is not a thing the formatter should be able to
    do to a generator. Braces are what actually delimit an entry, so count those.
    A // comment can sit between entries and must not be folded into one.
    """
    depth = 0
    start = None
    in_str = False
    i = 0
    while i < len(text):
        c = text[i]
        if in_str:
            if c == '\\':
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == '/' and text[i:i + 2] == '//':
            end = text.find('\n', i)
            i = len(text) if end < 0 else end
            continue
        elif c == '{':
            if depth == 0:
                start = i
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                yield ' '.join(text[start:i + 1].split())
        i += 1


entries = []
for line in top_level_groups(table):
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


# ---- kind length ----------------------------------------------------------
# A kind is drawn on the NOT YOURS screen with its article in front, starting at
# x=28, which leaves 100px of the 128. Nothing here can measure that: the real
# limit is pixels in FontSecondary (u8g2_font_haxrcorp4089_tr), and characters
# predict it badly -- "Temp/humidity sensor" is 20 of them and overflows by one
# pixel, while "Magnetic angle sensor" is 21 and fits with two to spare.
#
# So this is a tripwire, not a guarantee. 21 is simply the longest kind that has
# actually been measured. A longer one is not necessarily too wide, but it has
# not been checked, and the failure mode is a word quietly cut off on the one
# screen that tells somebody they were sold the wrong part.
#
# To measure one instead of guessing, and with the article NOT YOURS puts in
# front of it:
#
#     python tools/screen_width.py "a magnetic angle sensor"
#
# If it fits the not-yours slot, raise this to the new length.
KIND_MAX = 21


def check_kind_lengths(pairs, source):
    over = ['%s: "%s" is %d characters' % (name, kind, len(kind))
            for name, kind in pairs if len(kind) > KIND_MAX]
    if over:
        raise SystemExit(
            'kind longer than %d characters in %s, and no one has measured it:\n  %s'
            % (KIND_MAX, source, '\n  '.join(over)))


check_kind_lengths([(e['name'], e['kind']) for e in entries], 'chip_db.c')


# ---- live tests -----------------------------------------------------------
# One module per part, all listed in live_test.c. Parsed rather than hand-kept
# for the same reason as the chip table: a doc that can drift will.
registry = pathlib.Path('fake_chip_detector/live_test.c').read_text(encoding='utf-8')
registered = set(re.findall(r'&live_test_(\w+)\s*,', registry))

# Every definition found in the sources, keyed by the symbol it defines. The
# files are scanned rather than derived from the symbol names, because a module
# may define more than one test: the MPU-6050, 6500 and 9250 share an
# accelerometer register map and therefore share an implementation.
defined = {}
for mod in sorted(pathlib.Path('fake_chip_detector').glob('live_*.c')):
    body = mod.read_text(encoding='utf-8')
    for symbol, fields in re.findall(
            r'const LiveTest live_test_(\w+)\s*=\s*\{(.*?)\n\};', body, re.S):
        chip = re.search(r'\.chip\s*=\s*"([^"]*)"', fields)
        offer = re.search(r'\.offer\s*=\s*"([^"]*)"', fields)
        addrs = re.search(r'\.addrs\s*=\s*\{([^}]*)\}', fields)
        if chip and offer:
            defined[symbol] = {
                'slug': symbol, 'chip': chip.group(1),
                'offer': offer.group(1), 'file': mod.name,
                'addrs': [num(a) for a in addrs.group(1).split(',')
                          if a.strip()] if addrs else None,
            }

missing = sorted(registered - set(defined))
if missing:
    raise SystemExit('registered in live_test.c but never defined: %s' % ', '.join(missing))

orphans = sorted(set(defined) - registered)
if orphans:
    raise SystemExit('defined but not registered in live_test.c: %s' % ', '.join(orphans))

live_tests = {defined[s]['chip']: defined[s] for s in registered}

for e in entries:
    e['live'] = live_tests.get(e['name'])

# A test's `.addrs` is what a manual launch probes before it starts writing
# registers, so an address listed there that the part cannot actually use is a
# write aimed at whatever else happens to answer. The database already knows
# the right answer; check the test against it rather than trusting two hand-kept
# lists to stay equal.
by_name = {e['name']: e for e in entries}
addr_problems = []
for chip_name, test in sorted(live_tests.items()):
    entry = by_name.get(chip_name)
    if entry is None:
        addr_problems.append(
            '%s: .chip "%s" matches no row in chip_db.c' % (test['file'], chip_name))
        continue
    if test['addrs'] is None:
        addr_problems.append('%s: live_test_%s has no .addrs' % (test['file'], test['slug']))
        continue
    if entry['lo']:
        expected = list(range(entry['lo'], entry['hi'] + 1))
        described = 'the 0x%02X-0x%02X range' % (entry['lo'], entry['hi'])
    else:
        expected = entry['addrs']
        described = ', '.join('0x%02X' % a for a in expected)
    stray = [a for a in test['addrs'] if a not in expected]
    if stray:
        addr_problems.append(
            '%s: live_test_%s claims %s, but chip_db.c gives %s' % (
                test['file'], test['slug'],
                ', '.join('0x%02X' % a for a in stray), described))

if addr_problems:
    raise SystemExit('live-test addresses disagree with chip_db.c:\n  ' +
                     '\n  '.join(addr_problems))


def live_cell(e):
    return e['live']['offer'] if e['live'] else '—'


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
L.append('- **Live test** — an ID register is one byte, and one byte is what a relabeller can')
L.append('  copy. Where a module exists, the app offers to make the part *do* its job and prove')
L.append('  it. See [LIVE_TESTS.md](LIVE_TESTS.md) for how to write one.')
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
L.append('| Chip | What it is | I2C address | Register | Expected | Width | Live test | Notes |')
L.append('|---|---|---|---|---|---|---|---|')
for e in ided:
    L.append('| **%s** | %s | %s | %s | %s | %s | %s | %s |' % (
        e['name'], e['kind'], addr_str(e), reg_cell(e), val_cell(e), width_cell(e),
        live_cell(e), e['note'] or ''))
L.append('')
L.append('## Chips recognised by address only (%d)' % len(noid))
L.append('')
L.append('These parts carry no ID register at all — there is nothing to read, so no honest tool')
L.append('can confirm which one it is. The app reports them as DETECTED rather than pretending')
L.append('to a verdict it cannot support.')
L.append('')
L.append('This is exactly where a live test earns its keep. For a chip in the table above, a')
L.append('live test is a second opinion; for one down here it is the *only* evidence that can')
L.append('ever exist, because asking the part to do its job is the one question left to ask.')
L.append('')
L.append('| Chip | What it is | I2C address | Live test | Notes |')
L.append('|---|---|---|---|---|')
for e in noid:
    L.append('| **%s** | %s | %s | %s | %s |' % (
        e['name'], e['kind'], addr_str(e), live_cell(e), e['note'] or ''))
L.append('')

# ---- shared addresses -----------------------------------------------------
# The single most common way to misread a scan is to assume the address names
# the part. It does not, and the tables above only show that if you read all
# eighty rows and cross-reference them yourself.
by_addr = {}
for e in entries:
    addrs = range(e['lo'], e['hi'] + 1) if e['lo'] else e['addrs']
    for a in addrs:
        by_addr.setdefault(a, []).append(e)

shared = sorted((a, es) for a, es in by_addr.items() if len(es) > 1)
movable = [e for e in entries if not e['lo'] and len(e['addrs']) > 1]

L.append('## Addresses more than one chip answers on (%d)' % len(shared))
L.append('')
L.append('**An I2C address does not name a part.** It is seven bits chosen by the manufacturer,')
L.append('and plenty of unrelated chips chose the same ones. This is why the app probes rather')
L.append('than looks up: for every candidate registered at the address that answered, it reads')
L.append('that candidate\'s ID registers and keeps the one with the most matches. A scan that')
L.append('reports one part at a crowded address has already ruled the others out.')
L.append('')
L.append('| Address | Chips that use it |')
L.append('|---|---|')
for a, es in shared:
    L.append('| `0x%02X` | %s |' % (
        a, ', '.join('**%s** (%s)' % (e['name'], e['kind']) for e in es)))
L.append('')
L.append('Reading this the other way: a chip whose neighbours all have ID registers is safe to')
L.append('identify by probing, and one sharing an address with an address-only part is not — the')
L.append('app will say DETECTED rather than guess between them.')
L.append('')
L.append('### Chips that can sit at more than one address (%d)' % len(movable))
L.append('')
L.append('A pin on the module picks which. If a scan finds nothing, the pin is worth checking')
L.append('before the wiring is: the app searches every address in this list, but only the ones')
L.append('in it.')
L.append('')
L.append('| Chip | Addresses | Notes |')
L.append('|---|---|---|')
for e in movable:
    L.append('| **%s** | %s | %s |' % (
        e['name'], ', '.join('`0x%02X`' % a for a in e['addrs']), e['note'] or ''))
L.append('')
L.append('The BNO055 is the one to know about, because its datasheet and every breakout board')
L.append('disagree. Bosch BST-BNO055-DS000 Table 4-7 calls `0x29` the *default* and `0x28` the')
L.append('alternative, selected by the COM3 pin: HIGH gives `0x29`, LOW gives `0x28`. Boards tie')
L.append('COM3 low, so in practice almost every module answers on `0x28` and everyone calls that')
L.append('the default. Both are in the database.')
L.append('')
L.append('That same chip is also the clearest case of an address proving nothing: `0x29` is')
L.append('shared with three ST time-of-flight rangefinders and two light sensors, and the app')
L.append('separates them by reading four ID registers rather than one — CHIP_ID plus the')
L.append('BMA280, BMM150 and BMG160 sub-IDs, which clones get wrong far more often than they get')
L.append('CHIP_ID wrong.')
L.append('')

# ---- mode pins ------------------------------------------------------------
# A pin that takes a healthy part off the bus. The scan then finds nothing,
# which reads as "dead chip" unless something says otherwise -- and a working
# BNO055 has already gone back to a courier over exactly this.
mode_body = re.search(
    r'static const ChipModePin chip_mode_pins\[\]\s*=\s*\{(.*?)\n\};', src, re.S).group(1)

MODE_ALT = {'ModeAltSpi': 'SPI', 'ModeAltUart': 'UART', 'ModeAltOff': 'nothing — it is held off'}
mode_pins = []
for line in top_level_groups(mode_body):
    m = re.match(
        r'\{"([^"]*)",\s*"([^"]*)",\s*(\w+),\s*(\w+),\s*(true|false),\s*(true|false)\s*\},?',
        line)
    if not m:
        raise SystemExit('unparsed mode-pin line: ' + line)
    mode_pins.append({
        'chip': m.group(1), 'pad': m.group(2), 'kind': m.group(3),
        'alt': m.group(4), 'i2c_high': m.group(5) == 'true',
        'latched': m.group(6) == 'true',
    })

# Same reasoning as the live-test check above: a renamed chip must break the
# build, not silently strand the guidance that names it.
strays = [p['chip'] for p in mode_pins if p['chip'] not in by_name]
if strays:
    raise SystemExit('chip_mode_pins names no row in chip_db.c: ' + ', '.join(strays))

L.append('## Chips that can be strapped off the I2C bus (%d)' % len(mode_pins))
L.append('')
L.append('These parts have a pin that decides whether they speak I2C at all. Set the wrong way —')
L.append('by the board, by the factory, or by one glitch on the pad — the part is healthy, powered')
L.append('and completely invisible to any scan, because in that state it does not have an I2C')
L.append('address to answer on. An empty scan is not evidence that a chip is dead.')
L.append('')
L.append('| Chip | Pad | I2C needs it | Otherwise it speaks | After strapping |')
L.append('|---|---|---|---|---|')
for p in mode_pins:
    L.append('| **%s** | `%s` | %s | %s | %s |' % (
        p['chip'], p['pad'], 'HIGH' if p['i2c_high'] else 'LOW',
        MODE_ALT[p['alt']],
        'power-cycle it' if p['latched'] else 'takes effect at once'))
L.append('')
L.append('The last column is not a detail. A latched pin is sampled at reset and nowhere else, so')
L.append('strapping the pad and rescanning changes nothing and looks like proof the part is')
L.append('broken. Bosch put it plainly for the BMP280, BME280 and BME680: once `CSB` has been')
L.append('pulled down even once, *"the I2C interface is disabled until the next power-on-reset"*.')
L.append('')
L.append('The list is short because a row that could not be checked against a datasheet is not')
L.append('here. A wrong entry would send someone to tie a pin the wrong way round, which is worse')
L.append('than no entry at all. Address-select pins are deliberately excluded: the sweep covers')
L.append('`0x08`-`0x77`, so they cannot hide a part.')
L.append('')

# ---- 1-Wire families ------------------------------------------------------
ow_src = pathlib.Path('fake_chip_detector/onewire_worker.c').read_text(encoding='utf-8')
ow_body = re.search(
    r'static const OneWireFamily onewire_families\[\]\s*=\s*\{(.*?)\n\};', ow_src, re.S).group(1)
families = re.findall(r'\{(0x[0-9A-Fa-f]+),\s*"([^"]*)",\s*"([^"]*)",\s*OneWireRole(\w+)\}', ow_body)

# These never reach NOT YOURS, which is an I2C screen, so the cap is stricter
# than they need -- the widest slot they land in is the chips browser, centred
# across the full 128. One number for both is worth more than the few characters
# a second limit would buy back, and the sentence in the 1-Wire report is the
# same sentence.
check_kind_lengths([(name, kind) for _, name, kind, _ in families], 'onewire_worker.c')

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

generated = '\n'.join(L)
summary = '%s: %d chips (%d with ID, %d address-only)' % (OUT, len(entries), len(ided), len(noid))

if '--check' in sys.argv[1:]:
    on_disk = OUT.read_text(encoding='utf-8') if OUT.exists() else ''
    if on_disk == generated:
        print('up to date -- ' + summary)
    else:
        sys.stdout.writelines(difflib.unified_diff(
            on_disk.splitlines(True), generated.splitlines(True),
            fromfile=str(OUT) + ' (committed)', tofile=str(OUT) + ' (from chip_db.c)'))
        sys.stderr.write(
            '\n%s is out of date. Run: python tools/gen_supported_chips.py\n' % OUT)
        raise SystemExit(1)
else:
    OUT.write_text(generated, encoding='utf-8')
    print(summary)
