#!/usr/bin/env python3
"""
build_presets.py — genère un presets.txt pour Flipper OSM Logger
à partir du schéma officiel OSM id-tagging-schema.

Récupère https://github.com/openstreetmap/id-tagging-schema, filtre pour les
POIs en géométrie point, et émet un fichier presets.txt compatible avec le
loader dynamique de l'app (format : `label;key;value;category`).

Dépose le fichier sur la microSD sous :
    /ext/apps_data/osm_logger/presets.txt

Usage:
    python3 build_presets.py                    # tout en anglais
    python3 build_presets.py --locale fr        # noms en français
    python3 build_presets.py --max 300          # limite à 300 presets
    python3 build_presets.py -o my_presets.txt  # chemin de sortie custom
    python3 build_presets.py --input presets.json  # mode hors-ligne

Requiert Python 3.8+ et une connexion réseau (sauf --input).
"""

import argparse
import json
import os
import sys
import unicodedata
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Constantes
# ---------------------------------------------------------------------------
SCHEMA_URL = "https://raw.githubusercontent.com/openstreetmap/id-tagging-schema/main/dist/presets.json"
LOCALE_URL = "https://raw.githubusercontent.com/openstreetmap/id-tagging-schema/main/dist/translations/{locale}.json"
LABEL_MAX = 18
PRESETS_MAX = 128  # doit matcher PRESETS_MAX_ENTRIES dans src/presets.c

# Doit matcher PresetCategory enum dans src/presets.h
CAT_STREET = 0
CAT_ROADS = 1
CAT_PARKING = 2
CAT_SPORTS = 3
CAT_WASTE = 4
CAT_SHOPS = 5
CAT_SERVICES = 6
CAT_EMERGENCY = 7
CAT_TOURISM = 8
CAT_NATURE = 9
CAT_EDUCATION = 10
CAT_RELIGION = 11
CAT_TRANSPORT = 12
CAT_ADDRESS = 13
CAT_OTHER = 14

# ---------------------------------------------------------------------------
# Mapping tag → catégorie (rules, first match wins)
# ---------------------------------------------------------------------------
# Chaque entrée : ((key, value_prefix_or_None), category_id)
# value_prefix_or_None=None -> matche n'importe quelle value pour ce key
CATEGORY_RULES = [
    # --- Emergency (prioritaire) ---
    ("emergency", None, CAT_EMERGENCY),
    ("amenity", "defibrillator", CAT_EMERGENCY),
    ("amenity", "fire_station", CAT_EMERGENCY),
    ("amenity", "police", CAT_EMERGENCY),
    ("amenity", "hospital", CAT_EMERGENCY),
    ("amenity", "clinic", CAT_EMERGENCY),

    # --- Education ---
    ("amenity", "school", CAT_EDUCATION),
    ("amenity", "kindergarten", CAT_EDUCATION),
    ("amenity", "university", CAT_EDUCATION),
    ("amenity", "college", CAT_EDUCATION),
    ("amenity", "library", CAT_EDUCATION),
    ("amenity", "research_institute", CAT_EDUCATION),

    # --- Religion ---
    ("amenity", "place_of_worship", CAT_RELIGION),
    ("amenity", "grave_yard", CAT_RELIGION),
    ("building", "chapel", CAT_RELIGION),
    ("landuse", "cemetery", CAT_RELIGION),
    ("historic", "wayside_cross", CAT_RELIGION),
    ("historic", "wayside_shrine", CAT_RELIGION),

    # --- Transport ---
    ("public_transport", None, CAT_TRANSPORT),
    ("railway", None, CAT_TRANSPORT),
    ("aerialway", None, CAT_TRANSPORT),
    ("amenity", "taxi", CAT_TRANSPORT),
    ("amenity", "bicycle_rental", CAT_TRANSPORT),
    ("amenity", "ferry_terminal", CAT_TRANSPORT),
    ("amenity", "bus_station", CAT_TRANSPORT),
    ("highway", "bus_stop", CAT_TRANSPORT),

    # --- Parking ---
    ("amenity", "parking", CAT_PARKING),
    ("amenity", "bicycle_parking", CAT_PARKING),
    ("amenity", "motorcycle_parking", CAT_PARKING),
    ("amenity", "charging_station", CAT_PARKING),
    ("amenity", "parking_entrance", CAT_PARKING),

    # --- Sports & leisure ---
    ("leisure", None, CAT_SPORTS),
    ("sport", None, CAT_SPORTS),
    ("amenity", "basketball_hoop", CAT_SPORTS),
    ("amenity", "gym", CAT_SPORTS),
    ("amenity", "swimming_pool", CAT_SPORTS),

    # --- Waste ---
    ("amenity", "recycling", CAT_WASTE),
    ("amenity", "waste_basket", CAT_STREET),  # poubelle = mobilier urbain
    ("amenity", "waste_disposal", CAT_WASTE),
    ("amenity", "compost", CAT_WASTE),

    # --- Services (bank/post/health etc.) ---
    ("amenity", "bank", CAT_SERVICES),
    ("amenity", "atm", CAT_SERVICES),
    ("amenity", "post_office", CAT_SERVICES),
    ("amenity", "doctors", CAT_SERVICES),
    ("amenity", "dentist", CAT_SERVICES),
    ("amenity", "veterinary", CAT_SERVICES),
    ("amenity", "fuel", CAT_SERVICES),
    ("amenity", "car_wash", CAT_SERVICES),
    ("amenity", "car_rental", CAT_SERVICES),
    ("amenity", "car_sharing", CAT_SERVICES),
    ("office", None, CAT_SERVICES),
    ("shop", "laundry", CAT_SERVICES),
    ("shop", "dry_cleaning", CAT_SERVICES),

    # --- Shops ---
    ("shop", None, CAT_SHOPS),
    ("amenity", "cafe", CAT_SHOPS),
    ("amenity", "pub", CAT_SHOPS),
    ("amenity", "bar", CAT_SHOPS),
    ("amenity", "fast_food", CAT_SHOPS),
    ("amenity", "restaurant", CAT_SHOPS),
    ("amenity", "food_court", CAT_SHOPS),
    ("amenity", "pharmacy", CAT_SHOPS),
    ("amenity", "marketplace", CAT_SHOPS),

    # --- Tourism ---
    ("tourism", None, CAT_TOURISM),
    ("historic", None, CAT_TOURISM),

    # --- Address ---
    ("building", None, CAT_ADDRESS),
    ("entrance", None, CAT_ADDRESS),
    ("addr:housenumber", None, CAT_ADDRESS),

    # --- Nature ---
    ("natural", None, CAT_NATURE),

    # --- Roads & signs ---
    ("highway", None, CAT_ROADS),
    ("traffic_calming", None, CAT_ROADS),
    ("barrier", None, CAT_ROADS),
    ("traffic_sign", None, CAT_ROADS),

    # --- Street furniture ---
    ("amenity", "bench", CAT_STREET),
    ("amenity", "drinking_water", CAT_STREET),
    ("amenity", "fountain", CAT_STREET),
    ("amenity", "toilets", CAT_STREET),
    ("amenity", "post_box", CAT_STREET),
    ("amenity", "picnic_table", CAT_STREET),
    ("amenity", "telephone", CAT_STREET),
    ("amenity", "shelter", CAT_STREET),
    ("amenity", "vending_machine", CAT_STREET),
    ("amenity", "public_bookcase", CAT_STREET),
    ("amenity", "clock", CAT_STREET),
    ("amenity", "hunting_stand", CAT_STREET),
    ("amenity", "lounger", CAT_STREET),
    ("man_made", None, CAT_STREET),  # lampadaires, tours, etc.

    # --- Fallback pour amenity ---
    ("amenity", None, CAT_SERVICES),
]


def infer_category(tags: dict) -> int:
    """Mappe les tags iD vers notre enum de catégorie (0-14)."""
    if not tags:
        return CAT_OTHER
    for key, value, cat in CATEGORY_RULES:
        if key in tags:
            v = tags[key]
            if value is None:
                return cat
            if isinstance(v, str) and v == value:
                return cat
    return CAT_OTHER


# ---------------------------------------------------------------------------
# Normalisation des labels (ASCII, 18 chars max)
# ---------------------------------------------------------------------------
def normalize_label(label: str) -> str:
    """Déaccentue, limite aux chars ASCII, tronque, nettoie les séparateurs."""
    if not label:
        return ""
    # Décompose (é → e + ́), puis retire les diacritiques
    nfd = unicodedata.normalize('NFD', label)
    ascii_only = "".join(c for c in nfd if unicodedata.category(c) != 'Mn' and ord(c) < 128)
    # Remplace les caractères qui cassent notre format presets.txt
    ascii_only = ascii_only.replace(';', ' ').replace(',', ' ')
    # snake_case → espaces (on retombe sur ça quand la traduction manque et
    # qu'on prend primary_val comme fallback, ex. "vending_machine")
    if "_" in ascii_only and " " not in ascii_only:
        ascii_only = ascii_only.replace("_", " ")
    # Collapse les espaces multiples
    while "  " in ascii_only:
        ascii_only = ascii_only.replace("  ", " ")
    ascii_only = ascii_only.strip()
    # Capitalise la première lettre si le label est entièrement en minuscules
    # (cas du fallback raw value sans traduction)
    if ascii_only and ascii_only == ascii_only.lower():
        ascii_only = ascii_only[0].upper() + ascii_only[1:]
    # Tronque à 18 chars
    return ascii_only[:LABEL_MAX].strip()


# ---------------------------------------------------------------------------
# Extraction depuis le schéma iD
# ---------------------------------------------------------------------------
def extract_primary_tag(tags: dict) -> tuple:
    """Retourne (key, value) du tag principal, ou (None, None) si absent."""
    if not tags:
        return None, None
    # Préfère les clés "bien connues" dans cet ordre
    preferred = [
        "emergency", "amenity", "shop", "tourism", "leisure", "sport",
        "natural", "highway", "railway", "aerialway", "historic",
        "man_made", "building", "entrance", "office", "addr:housenumber",
        "traffic_calming", "traffic_sign", "barrier", "public_transport",
    ]
    for key in preferred:
        if key in tags:
            v = tags[key]
            if isinstance(v, str) and v and v != "*":
                return key, v
    # Fallback : la première paire du dict
    for k, v in tags.items():
        if isinstance(v, str) and v and v != "*":
            return k, v
    return None, None


def fetch_json(url: str) -> dict:
    """Récupère un JSON depuis une URL."""
    print(f"Téléchargement : {url}", file=sys.stderr)
    with urllib.request.urlopen(url, timeout=30) as resp:
        return json.load(resp)


def load_translations(locale: str) -> dict:
    """Charge les traductions pour une locale (ex. 'fr'). Retourne
    un dict preset_path → name_translated. Vide si locale indisponible."""
    if not locale or locale == "en":
        return {}
    url = LOCALE_URL.format(locale=locale)
    try:
        data = fetch_json(url)
    except Exception as e:
        print(f"⚠️  Impossible de charger les traductions {locale}: {e}", file=sys.stderr)
        return {}
    # Structure : {"fr": {"presets": {"presets": {"amenity/bench": {"name": "Banc"}}}}}
    try:
        root = data.get(locale) or next(iter(data.values()))
        presets = root["presets"]["presets"]
    except (KeyError, StopIteration, TypeError):
        return {}
    result = {}
    for path, entry in presets.items():
        if isinstance(entry, dict) and "name" in entry:
            result[path] = entry["name"]
    return result


def build_presets(schema: dict, translations: dict) -> list:
    """Extrait les presets utilisables depuis le schéma iD."""
    presets = schema.get("presets", schema)
    out = []
    seen_keys = set()  # dédoublonnage sur (primary_key, primary_value)
    for path, entry in presets.items():
        if not isinstance(entry, dict):
            continue

        tags = entry.get("tags") or {}
        geometries = entry.get("geometry") or []

        # On veut des POIs en points seulement
        if "point" not in geometries and "vertex" not in geometries:
            continue

        # Skip les presets "catalogues" (pas de tags concrets)
        if not tags:
            continue

        # Skip les wildcards
        if any(v == "*" for v in tags.values() if isinstance(v, str)):
            continue

        primary_key, primary_val = extract_primary_tag(tags)
        if not primary_key or not primary_val:
            continue

        # Dédoublonnage : garde le premier match
        key_tuple = (primary_key, primary_val)
        if key_tuple in seen_keys:
            continue
        seen_keys.add(key_tuple)

        # Nom : priorité aux traductions, sinon le nom iD
        raw_name = translations.get(path) or entry.get("name") or primary_val
        label = normalize_label(raw_name)
        if not label:
            continue

        category = infer_category(tags)

        out.append({
            "path": path,
            "label": label,
            "key": primary_key,
            "value": primary_val,
            "category": category,
            "all_tags": tags,
        })

    return out


def sort_and_limit(presets: list, max_count: int) -> list:
    """Trie par catégorie puis par label. Si max_count est une limite,
    distribue équitablement entre les catégories actives pour éviter
    qu'une catégorie alphabétiquement basse sature tout le budget."""
    presets.sort(key=lambda p: (p["category"], p["label"].lower()))
    if max_count <= 0 or len(presets) <= max_count:
        return presets

    # Regroupe par catégorie (dicts ordonnés par insertion Python 3.7+)
    by_cat = {}
    for p in presets:
        by_cat.setdefault(p["category"], []).append(p)

    # Quota de base par catégorie active, puis distribution round-robin
    # du reste pour coller au max_count exact.
    n_cats = len(by_cat)
    base = max_count // n_cats
    remaining = max_count - base * n_cats

    kept = []
    # Tour 1 : base quota par catégorie
    for cat, items in by_cat.items():
        kept.extend(items[:base])

    # Tour 2 : redistribue le reste en prenant une entrée supplémentaire
    # dans les catégories qui ont encore du stock, par ordre de catégorie
    slot = 0
    for cat in sorted(by_cat.keys()):
        if remaining <= 0:
            break
        items = by_cat[cat]
        if len(items) > base:
            kept.append(items[base])
            remaining -= 1

    # Si des catégories avaient < base entries, on a du budget libre : on
    # le redistribue sur les catégories qui en ont encore.
    kept_by_cat = {}
    for p in kept:
        kept_by_cat.setdefault(p["category"], []).append(p)
    leftover = max_count - len(kept)
    if leftover > 0:
        for cat in sorted(by_cat.keys()):
            if leftover <= 0:
                break
            items = by_cat[cat]
            already = len(kept_by_cat.get(cat, []))
            extras = items[already:already + leftover]
            kept.extend(extras)
            leftover -= len(extras)

    kept.sort(key=lambda p: (p["category"], p["label"].lower()))
    return kept


def emit_presets_txt(presets: list, output: Path, locale: str) -> None:
    """Écrit le fichier presets.txt dans notre format."""
    header = f"""# presets.txt genere par scripts/build_presets.py
# Locale : {locale}
# Source : openstreetmap/id-tagging-schema
# Total  : {len(presets)} presets
#
# Copier ce fichier sur la microSD sous /ext/apps_data/osm_logger/presets.txt
# pour remplacer la liste compilee par defaut.
#
# Categories : 0=Street 1=Roads 2=Parking 3=Sports 4=Waste 5=Shops
#              6=Services 7=Emergency 8=Tourism 9=Nature 10=Education
#              11=Religion 12=Transport 13=Address 14=Other

"""
    lines = [header]
    current_cat = -1
    cat_names = [
        "Street furniture", "Roads & signs", "Parking", "Sports & leisure",
        "Waste", "Shops", "Services", "Emergency", "Tourism", "Nature",
        "Education", "Religion", "Transport", "Address", "Other",
    ]
    for p in presets:
        if p["category"] != current_cat:
            current_cat = p["category"]
            lines.append(f"\n# --- {cat_names[current_cat]} ({current_cat}) ---\n")
        lines.append(f"{p['label']};{p['key']};{p['value']};{p['category']}\n")

    output.write_text("".join(lines), encoding="utf-8")
    print(f"✅ {len(presets)} presets écrits dans {output}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--locale", default="en", help="code locale iD (en, fr, de, es, ...) — défaut: en")
    parser.add_argument("--max", type=int, default=PRESETS_MAX, help=f"limite sur le nombre de presets — défaut: {PRESETS_MAX}")
    parser.add_argument("-o", "--output", default="presets.txt", help="chemin du fichier de sortie — défaut: ./presets.txt")
    parser.add_argument("--input", help="fichier local presets.json (mode hors-ligne)")
    parser.add_argument("--stats", action="store_true", help="affiche un résumé par catégorie")
    args = parser.parse_args()

    # Chargement du schéma
    if args.input:
        print(f"Lecture : {args.input}", file=sys.stderr)
        schema = json.loads(Path(args.input).read_text(encoding="utf-8"))
    else:
        try:
            schema = fetch_json(SCHEMA_URL)
        except Exception as e:
            print(f"❌ Erreur téléchargement du schéma : {e}", file=sys.stderr)
            print("   Conseil : télécharger presets.json à la main et utiliser --input", file=sys.stderr)
            sys.exit(1)

    # Chargement traductions
    translations = load_translations(args.locale)
    if translations:
        print(f"✓ {len(translations)} traductions chargées pour la locale '{args.locale}'", file=sys.stderr)

    # Extraction + filtrage
    presets = build_presets(schema, translations)
    print(f"✓ {len(presets)} presets extraits du schéma", file=sys.stderr)

    # Tri + limite
    presets = sort_and_limit(presets, args.max)

    # Émission
    emit_presets_txt(presets, Path(args.output), args.locale)

    # Stats
    if args.stats:
        cat_names = [
            "Street furniture", "Roads & signs", "Parking", "Sports & leisure",
            "Waste", "Shops", "Services", "Emergency", "Tourism", "Nature",
            "Education", "Religion", "Transport", "Address", "Other",
        ]
        counts = [0] * 15
        for p in presets:
            counts[p["category"]] += 1
        print("\nRépartition par catégorie :", file=sys.stderr)
        for i, name in enumerate(cat_names):
            if counts[i]:
                print(f"  {i:2d} {name:18s} : {counts[i]:3d}", file=sys.stderr)


if __name__ == "__main__":
    main()
