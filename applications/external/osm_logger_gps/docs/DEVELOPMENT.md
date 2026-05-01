# Développement

## Structure du code

```
src/
  osm_logger.c       # entry point (redirige vers app)
  app.h / app.c      # struct App, init/teardown, ViewDispatcher, ISR UART, tick
  nmea.h / nmea.c    # parser NMEA (RMC, GGA) sans atof/strtok (+ heading)
  quick_log.h / .c   # écran Quick Log + variantes + note + mode preview
  track.h / .c       # écran Mode trace + FuriTimer + filtre distance + heading
  status.h / .c      # écran Statut GPS diagnostic
  about.h / .c       # écran About (auteur, lien GitHub, licence)
  settings.h / .c    # Settings persistés sur SD + écran dédié (VariableItemList)
  last_points.h / .c # browser des N derniers points + actions Delete last / Clear all
  point_detail.h / .c# écran de détail d'un point (cliqué depuis Last points)
  notes_cache.h / .c # cache persistant des notes par preset (notes_cache.txt sur SD)
  presets.h / .c     # loader de presets (fichier SD + fallback defaults + variantes)
  storage_helpers.h  # API publique du module storage
  storage.c          # écriture + lecture + suppression des fichiers de sortie
application.fam      # manifest ufbt
```

### Vues (`AppView` enum dans app.h)

| ID | Vue               | Type         | Back → |
|----|-------------------|--------------|--------|
| 0  | `AppViewMenu`     | Menu (icons) | exit app (nav callback retourne false) |
| 1  | `AppViewPresets`  | Submenu      | `AppViewMenu` (previous_callback) |
| 2  | `AppViewQuickLog` | View+model   | `AppViewPresets` (previous_callback) |
| 3  | `AppViewTrack`    | View+model   | `AppViewMenu` (previous_callback) |
| 4  | `AppViewStatus`   | View+model   | `AppViewMenu` (previous_callback) |
| 5  | `AppViewNote`     | TextInput    | retour Quick Log via result callback |
| 6  | `AppViewAbout`    | View (static)    | `AppViewMenu` (previous_callback) |
| 7  | `AppViewSettings` | VariableItemList | `AppViewMenu` (previous_callback) |
| 8  | `AppViewLastPoints` | Submenu (dynamic)| `AppViewMenu` (previous_callback) |
| 9  | `AppViewPointDetail` | View (static) | `AppViewLastPoints` (previous_callback) |
| 10 | `AppViewCategories` | Submenu       | `AppViewMenu` (previous_callback) |

Le menu principal `AppViewMenu` a été refactoré de `Submenu` vers **`Menu`** (module Flipper qui supporte un icône 10×10 par item). Les icônes sont dans `images/` et packagées via `fap_icon_assets="images"`, exposées en code sous `I_<filename>_10x10`.

### Vue Quick Log : view model + tick refresh

L'écran Quick Log utilise un **view model** (`ViewModelTypeLockFree`) qui contient un snapshot des données GPS (lat, lon, hdop, alt, sats, fix_age, counter). Le ViewDispatcher fait tourner un **tick callback toutes les 500 ms** (`view_dispatcher_set_tick_event_callback`), qui appelle `quick_log_refresh()` — cette fonction copie `app->*` vers le modèle via `with_view_model(..., true)`, ce qui déclenche un redraw.

C'est la seule façon propre de rafraîchir une vue depuis "l'extérieur" sur Flipper : pas de modèle = pas de moyen de forcer un redraw hors events d'input.

### ISR UART

`serial_rx_callback` tourne en contexte IRQ. Elle :
1. Assemble les octets UART en ligne NMEA complète (`\r` / `\n`)
2. Appelle `nmea_parse_line` (qui ne fait aucune allocation)
3. Écrit directement dans les champs `app->lat`, `app->lon`, etc.

⚠️ Pas de mutex : sur Cortex-M4 mono-cœur, les lectures/écritures 32-bit alignées sont atomiques. Entre deux champs il peut y avoir une micro-incohérence (ex. `lat` mis à jour, `lon` pas encore) mais négligeable pour un GPS à 1 Hz.

## Ajouter des presets

### Option A — Sans recompiler (via microSD)

Copier [presets.txt.sample](../presets.txt.sample) sur la SD sous `/ext/apps_data/osm_logger/presets.txt`. Format :

```
# Commentaire
Banc;amenity;bench
Poubelle;amenity;waste_basket
Mon POI;shop;florist
```

Séparateur strict `;`, une entrée par ligne, lignes vides ignorées, `#` en début = commentaire. Au démarrage, l'app cherche ce fichier — s'il est présent et parsable, il remplace la liste compilée.

Limite : 64 entrées max, fichier ≤ 4 Ko (`PRESETS_MAX_ENTRIES` et `PRESETS_MAX_FILE_SIZE` dans `presets.c`).

### Option B — Compilé en dur (par défaut)

Éditer le tableau `DEFAULT_PRESETS[]` dans [src/presets.c](../src/presets.c), ajouter une ligne :

```c
{"Mon label", "amenity", "ma_valeur"},
```

### Contraintes (valables pour les deux méthodes)

- Label ≤ 18 caractères (sinon tronqué à l'écran Quick Log en FontPrimary)
- Pas d'accents (la font par défaut ne les rend pas)
- `key` et `value` suivent la nomenclature OSM — chercher sur https://taginfo.openstreetmap.org/

Le submenu des presets est généré automatiquement à partir de `presets_get(i)` au démarrage de l'app, donc rien d'autre à modifier.

## Build & flash

```bash
ufbt              # compile dans dist/
ufbt launch       # compile + upload + lance sur le Flipper
ufbt cli          # CLI série (log, storage, ...)
```

**Avant de lancer `ufbt`, ferme qFlipper** — il monopolise le port série.

## Debug

### Logs

```bash
ufbt cli
> log debug
```

Niveaux : `none | default | error | warning | info | debug | trace`.

Nos logs perso :
- `FURI_LOG_I("OSM", ...)` : statut GPS à chaque OK sur "Statut GPS"
- `FURI_LOG_E("OSM", ...)` : échec de `mkdir` du dossier de data

### Vérifier les fichiers produits sans retirer la SD

```bash
ufbt cli
> storage list /ext/apps_data/osm_logger
> storage read /ext/apps_data/osm_logger/points.jsonl
```

### Crash (écran rouge)

Au crash, noter la raison + fichier:ligne affichée. Causes fréquentes déjà vues :
- **Stack watermark too low** : bumper `stack_size` dans `application.fam`
- **Expansion service** qui intercepte l'UART (déjà géré — l'app appelle `expansion_disable`)
- **`furi_check` storage** : dossier parent inexistant, SD pleine, permission read-only

## Pièges connus

### 1. Stack size trop petit

Flipper alloue le stack de l'app selon `stack_size` dans `application.fam`. Les opérations de storage et les buffers `char buf[256]` peuvent déborder. Actuellement à **4096**, ne pas descendre en-dessous.

### 2. Expansion service (Momentum, firmware récent)

Les firmwares récents (stock ≥ 0.103 et Momentum) ajoutent un service *Expansion* qui lit l'UART en quête d'un handshake. Il rentre en conflit direct avec l'accès GPS. L'app appelle `expansion_disable(app->expansion)` en init et `expansion_enable` en teardown — ne pas retirer ces appels.

### 3. Include header pour forward decl

`quick_log.c` doit `#include "quick_log.h"` sinon l'usage de `quick_log_refresh` dans l'input callback (avant sa définition plus bas dans le fichier) déclenche un `implicit declaration` fatal.

## Ajouter une nouvelle vue

Patron pour ajouter une vue custom (exemple : écran de stats) :

1. Ajouter un ID dans `AppView` enum (app.h)
2. Créer `my_screen.h/.c` sur le modèle de `quick_log.c` (view_alloc, callbacks, éventuellement view_allocate_model)
3. Dans `app.c` init : créer la vue, `view_dispatcher_add_view`, `view_set_previous_callback` pour le Back
4. Dans `app.c` teardown : `view_dispatcher_remove_view`, `view_free`
5. Déclencher l'ouverture depuis le menu principal ou un autre endroit avec `view_dispatcher_switch_to_view`
6. Ajouter le `.c` dans `sources=[]` de `application.fam`
