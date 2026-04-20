# Custom presets

The app ships with **65 built-in OSM presets** covering the most frequent POIs (street furniture, shops, transport, etc.). That's enough for most field sessions.

If you want **more presets** (up to **128**, the current `PRESETS_MAX_ENTRIES` cap) or **presets in your language**, drop a `presets.txt` on the Flipper's microSD:

```
/ext/apps_data/osm_logger/presets.txt
```

The app detects the file at startup and **overrides** the compiled list. No recompile needed.

---

## Method 1 — Manual editing

The format is one preset per line:

```
Label;key;value[,variant1,variant2,...];category
```

- **Label**: on-screen name, max 18 ASCII chars (no accents, no `;`, no `,`)
- **key / value**: main OSM tag (e.g. `amenity=bench`)
- **variants** *(optional)*: alternative values (`pub,bar,fast_food`) **OR** additional tags (`material=wood`, cyclable with ←/→ on Quick Log)
- **category**: 0-14 integer (see table below)

### Category table

| ID | Category           |
|----|--------------------|
| 0  | Street furniture   |
| 1  | Roads & signs      |
| 2  | Parking            |
| 3  | Sports & leisure   |
| 4  | Waste              |
| 5  | Shops              |
| 6  | Services           |
| 7  | Emergency          |
| 8  | Tourism            |
| 9  | Nature             |
| 10 | Education          |
| 11 | Religion           |
| 12 | Transport          |
| 13 | Address            |
| 14 | Other              |

### Example (`presets.txt`)

```
# Lines starting with # are ignored
Bench;amenity;bench,material=wood,material=metal;0
Bin;amenity;waste_basket;0
Drinking water;amenity;drinking_water;0
Cafe;amenity;cafe,pub,bar,fast_food;5
Bike parking;amenity;bicycle_parking;2
```

See [presets.txt.sample](../presets.txt.sample) for a full commented example.

---

## Method 2 — Generate from the official iD schema (`build_presets.py`)

To leverage the **hundreds of community-maintained presets** in the official OSM [id-tagging-schema](https://github.com/openstreetmap/id-tagging-schema) project (used by the iD editor), use `scripts/build_presets.py`.

The script downloads the official schema, filters point presets, maps them to our 15 categories, and outputs a compatible `presets.txt`.

### Requirements

- Python **3.8+**
- Network connection (or a manually downloaded `presets.json`, see `--input`)

### Usage

```bash
# 1. From the repo root
cd scripts/

# 2. Generate an English presets.txt (default)
python3 build_presets.py

# 3. French version (translated names)
python3 build_presets.py --locale fr -o presets_fr.txt

# 4. Cap to 100 presets and show per-category stats
python3 build_presets.py --locale fr --max 100 --stats -o presets_fr.txt

# 5. Offline mode (presets.json downloaded separately)
python3 build_presets.py --input presets.json --locale fr
```

Options:
- `--locale`: iD locale code (`en`, `fr`, `de`, `es`, `it`, `pt`, `nl`, …). Default: `en`.
- `--max N`: cap to N presets (default 128, the app limit).
- `-o FILE`: output file path (default `presets.txt` in current dir).
- `--input FILE`: use a local `presets.json` instead of downloading.
- `--stats`: print the per-category breakdown to stderr.

### Deploy to the Flipper

1. Run the script → you get `presets.txt` locally.
2. Plug the Flipper, open qFlipper (or mount the microSD).
3. Copy `presets.txt` to `/ext/apps_data/osm_logger/`.
4. Restart the OSM Logger app → the new presets are loaded.

### Under the hood

The script:
1. Downloads `presets.json` from `openstreetmap/id-tagging-schema` (`develop` branch).
2. Filters entries: keeps only those with `geometry: ["point", ...]` and a concrete tag.
3. Extracts the primary tag (`emergency`, `amenity`, `shop`, `tourism`, …).
4. Maps to one of our 15 categories via `CATEGORY_RULES` (first-match-wins).
5. Loads the requested locale and replaces English names with the translations when available.
6. Normalises labels: decomposes accents (`é → e`), strips diacritics, limits to 18 ASCII chars.
7. Sorts by category then label, caps to the requested count.
8. Writes `presets.txt` in our format.

### Customising the mapping

If you want specific tags to land in a different category, edit the `CATEGORY_RULES` list at the top of the script. Rules are applied **in order**, first match wins. Example to classify `amenity=fountain` as Nature instead of Street furniture:

```python
CATEGORY_RULES = [
    ("amenity", "fountain", CAT_NATURE),  # add BEFORE the generic Street rule
    # ... rest unchanged
]
```

---

## Feedback / contributions

If you generate a particularly useful list (e.g. `presets_fr_urban_complete.txt` for urban mapping), feel free to open a PR to add it to a future `presets_samples/` folder in the repo.

Additions to `CATEGORY_RULES` are welcome: the current logic is "generic OSM", but specialists (EV chargers, cycling-focused mapping, etc.) will have different preferences.
