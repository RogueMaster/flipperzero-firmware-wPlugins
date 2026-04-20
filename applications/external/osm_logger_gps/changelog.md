# Changelog

## 0.15 — 2026-04-19

Data quality release — élimine 4 sources d'incohérence dans les fichiers de sortie révélés par une session de test.

### Fixed
- **Refuse les saves à lat=0, lon=0** — avant, un force save (OK long) indoor avec GPS pas encore initialisé écrivait un point à (0,0) dans les 6 fichiers de sortie. Polluait `points.jsonl`/`points.gpx`/etc. et cassait les imports QGIS (point au large de la côte d'Afrique). Maintenant : save refusé avec message `GPS not initialized (0,0)`, même en force. La seule façon de sauver est d'avoir au moins une trame GPS qui a reporté des coords non-nulles.
- **Icône OsmAnd "TBD"** — pour les presets avec valeur placeholder (`House number` → `addr:housenumber=TBD`), l'hint icon OsmAnd était `TBD` → OsmAnd affichait une icône générique cassée. Maintenant : liste de placeholders reconnus (`TBD`, `?`, `tbd`, `fill_me`) → fallback sur `special_marker` (icône neutre).
- **Couleur Address = `#ffffff` invisible** — les favoris blancs disparaissaient sur fond carte clair dans OsmAnd. Remplacé par `#a020f0` (violet), distinct des 14 autres catégories.
- **Workflow House number cassé** — le tag restait `addr:housenumber=TBD` même avec une note contenant le numéro → 0 valeur pour OSM. Nouveau comportement "note-as-value" : quand la valeur primaire d'un preset est un placeholder (`TBD`, `?`, `tbd`, `fill_me`) ET que la note user est "safe" (pas de `=` ni `;`), la note remplace le placeholder au save. Ex. preset "House number" + note "42" → tag final `addr:housenumber=42`. La note est alors retirée du champ `note` pour éviter la duplication. `auto_photo_id` reste appliqué normalement (`photo:N` seul dans la note).

### Note pour les utilisateurs existants
Les fichiers historiques ne sont **pas rétro-corrigés** : les vieux points à (0,0) restent dans `points.jsonl`/`points.gpx`/etc. Pour repartir propre : **Last points → Clear all** ou **Archive session** avant la prochaine vraie sortie.

### Convention étendue pour les presets custom
Dans le `presets.txt`, il est maintenant possible d'utiliser `TBD` comme valeur placeholder pour les presets où le numéro/identifiant est saisi à la volée :
```
Room number;addr:flats;TBD;13
```
L'user tape "23" en note → tag final `addr:flats=23`.

## 0.14 — 2026-04-19

### Added
- **Survey mode** — toggle dans Settings (`Survey mode: on/off`, défaut off). Quand activé, chaque save ajoute automatiquement les deux tags OSM canoniques : `source=survey` et `survey:date=YYYY-MM-DD` (date courante du Flipper). C'est la norme community pour signaler "POI vérifié sur place par le contributeur" — très apprécié au moment du review des edits OSM. Les tags sont ajoutés après le tag principal (et les variantes éventuelles) via le séparateur `;`, donc traversent proprement les 6 formats de sortie (JSONL/CSV string, GPX `<desc>`, GeoJSON `properties.tag`, et éclatés en `<tag/>` séparés dans l'OSM XML).
- Nouvelle clé `survey_mode=0|1` dans `settings.txt`.

### Changed
- `quick_log_write_point` : tag buffer passe de 64 à 128 octets pour accommoder la paire `source=survey;survey:date=...` (+36 chars fixes) en plus des tags preset + variantes.
- `storage.c` (OSM XML writer) : `tag_copy` buffer passe de 96 à 160 octets pour parser les tags enrichis sans troncation.

### Fixed
- **Overlap `MIT License` / `Back` dans l'écran About** : les deux lignes étaient positionnées à y=52 et y=62 AlignBottom → chevauchement de ~5 px sur les dernières rows. Layout resserré (gaps de 10px entre chaque ligne, Back à y=63 AlignBottom) pour un rendu propre.

### Docs
- Screenshots mis à jour (v0.3 → v0.14) : 10 captures couvrant le flow complet (menu, catégories, presets, quick log, éditeur de note, post-save toast, track, last points, settings, about). README restructuré en 3 tableaux thématiques.

### Note pour les utilisateurs existants
Le setting est off par défaut → aucun changement de comportement. Pour l'activer : Settings → Survey mode → on. Recommandé dès qu'on est en condition de vérification terrain (plutôt que de mapping "from memory" ou d'import).

## 0.13 (tooling) — 2026-04-19

### Added
- **`scripts/build_presets.py`** — générateur de `presets.txt` depuis le schéma officiel OSM [id-tagging-schema](https://github.com/openstreetmap/id-tagging-schema) (celui utilisé par l'éditeur iD). Télécharge le `presets.json` maintenu par la communauté, filtre les POIs point, mappe vers nos 15 catégories via une table `CATEGORY_RULES` customisable, normalise les labels (ASCII, 18 chars, déaccentuation NFD), et émet un fichier compatible avec le loader SD de l'app. CLI : `--locale` (en/fr/de/es/…), `--max`, `-o`, `--input` (mode hors-ligne), `--stats`. Python 3.8+ stdlib uniquement, zéro dépendance externe.
- **Docs bilingues** `docs/CUSTOM_PRESETS.md` (FR) + `docs/CUSTOM_PRESETS.en.md` (EN) expliquant les deux méthodes (édition manuelle du `presets.txt` + script iD). Ajoute la page au `docs/README.md`.
- **README principal** : mention du script + lien vers les nouvelles docs, dans la section Features et la section Documentation.

### Rationale
Notre liste compilée de 65 presets est bonne pour démarrer mais plafonne vite en cartographie thématique (EV chargers, mobilier cyclable, pharmacies de nuit…). Le schéma iD contient 100+ presets par catégorie en 30+ langues, maintenu par la community. Permettre aux utilisateurs de régénérer leur `presets.txt` depuis cette source canonique :
- Élargit drastiquement la palette sans bloater l'app (fichier SD, pas de recompile).
- Donne gratuitement les traductions multilingues.
- Permet aux contributeurs de proposer des nouveaux tags "depuis le terrain" via PR sur id-tagging-schema → bénéfice pour tout OSM.

Pas de bump `fap_version` : aucun code Flipper modifié. Tooling + docs uniquement.

## 0.13 — 2026-04-19

### Added
- **Export OSM XML natif** (`points.osm`) — 6e format de sortie, au format OSM API 0.6. Chaque point devient un `<node>` avec ses tags OSM éclatés (multi-tag `amenity=bench;material=wood` → 2 balises `<tag>`) + altitude en `ele=` + note user + méta `flipper:hdop/sats/time`. IDs négatifs incrémentaux (convention OSM pour nouveaux nodes pas encore uploadés). Directement chargeable dans JOSM comme **data layer** éditable (pas juste des waypoints) → workflow pro : édition + upload OSM en quelques clics.
- **Support complet du nouveau fichier** dans :
  - `delete_last` : supprime le dernier `<node>` et réécrit le footer `</osm>`
  - `clear_all` : `storage_common_remove` du `points.osm`
  - `archive_session` : inclut `points.osm` dans l'archive datée
- Helper `xml_escape` : échappe les 5 caractères XML critiques (`& < > " '`) pour éviter les XML mal formés en cas de caractères spéciaux dans les notes.

### Changed
- `storage_write_all_formats` accepte désormais un 12e paramètre `node_seq` (compteur séquentiel pour l'ID OSM négatif).
- README et docs FORMATS mentionnent le nouveau format + workflow JOSM recommandé.

### Roadmap OSM étoffée
Nouvelle section **OSM-spécifiques** dans `docs/ROADMAP.md` listant : Survey mode, POI confidence rating, Preset search par lettres, Overpass pré-check (WiFi), OSM Notes API direct (WiFi), changeset upload direct (WiFi), autofill `addr:street`, ways/lignes, building outlines, import schema iD, MapComplete link, StreetComplete export, username setting.

## 0.12 — 2026-04-19

### Added
- **`points.gpx` enrichi au format OsmAnd Favorites** — le fichier GPX ajoute désormais des extensions OsmAnd (`<osmand:icon>`, `<osmand:background>`, `<osmand:color>`) + un `<type>` catégorie + un `<name>` humain lisible (ex. `"Bench"`) + un `<desc>` technique avec le tag OSM brut. Importé dans OsmAnd : chaque point devient un favori avec pastille colorée par catégorie et icône dérivée du tag (ex. icône banc pour `amenity=bench`). 100% rétro-compatible : JOSM, iD, QGIS, GPX Viewer ignorent silencieusement le namespace `osmand:` qu'ils ne connaissent pas.
- **Palette couleur par catégorie** : Street furniture = cyan, Emergency = rouge, Nature = vert foncé, Services = bleu, Tourism = rose, etc. (table complète `PRESET_CATEGORY_COLORS` dans `src/presets.c`). Navigation visuelle rapide dans OsmAnd.

### Changed
- **Format GPX : `<name>` devient humain lisible** au lieu du tag OSM technique. Ex. `<name>Bench</name>` au lieu de `<name>amenity=bench</name>`. Le tag complet reste accessible via `<desc>`. Breaking change mineur pour les scripts de post-traitement qui lisaient le tag depuis `<name>` — à migrer vers `<desc>`.
- `storage_write_all_formats` prend 4 nouveaux paramètres : `display_name`, `category_label`, `osmand_icon`, `osmand_color`. Seul appelant : `quick_log_write_point`.

## 0.11 — 2026-04-19

### Added
- **Seuil HDOP configurable** via Settings `HDOP max` : off / 2.5 / 3.0 / 5.0 / 10.0. Défaut 5.0, plus permissif que les 2.5 hardcodés avant — le NEO-6M consumer a typiquement un HDOP de 2.5-5 même avec un bon fix. Le seuil s'applique à Quick Log (gate OK court) et au mode trace quand `track_hdop_strict` est activé. Résout le bug "save refusé HDOP 2.6 > 2.5" en conditions normales d'usage.
- Nouvelle clé `hdop_max_x10` dans `settings.txt` (stockage en dixièmes pour permettre 2.5 sans float).

### Changed
- Le message de refus affiche désormais le seuil actif (`HDOP 6.2 > 5.0`) au lieu du `2.5` hardcodé.

## 0.10 — 2026-04-19

Itération de polish suite aux premiers tests terrain.

### Added
- **Précision en mètres affichée à l'écran** (plutôt que seulement HDOP qui est dimensionless). Sur Quick Log : `alt=45m prec=4m t=247` quand fix actif. Sur GPS Status : `alt=45m HDOP=1.3 ~4m`. Formule : `prec_m ≈ HDOP × 3` (approximation courante pour les modules ublox en conditions normales).

### Fixed
- **Save refusé avec "No GPS fix" alors que les coords étaient affichées à l'écran** : le chemin save utilisait `app->has_fix` brut (flip-flop à 2 Hz à cause de RMC V / GGA q>0) pendant que l'affichage utilisait l'hystérésis 5s. Résultat : le clic OK tombait parfois pile sur une trame RMC V et refusait. Corrigé en utilisant la même logique "fix effectif" (has_fix OU fix dans les 5 dernières secondes) pour le save et pour l'averaging.
- **Averaging perdait la moitié des samples** pour la même raison (condition `app->has_fix` dans `averaging_tick`). Maintenant déclenché sur le changement de `last_fix_tick` seul → 2× plus de samples collectés sur une même durée.
- **"SAVE FAILED / SD card not found" alors que la SD est en place** : `storage_pre_save_check` utilisait `storage_common_stat(s, "/ext", &fi)` qui peut échouer sur le mount point même avec la SD mountée. Remplacé par `storage_sd_status(s)` qui est l'API canonique Flipper pour tester la SD, + un test réel via `storage_common_mkdir` (si mkdir réussit, la SD est forcément accessible).

## 0.9 — 2026-04-19

Grosse itération qualité de vie pour l'utilisation terrain. Focus : stabiliser l'affichage GPS + donner du contexte au utilisateur + introduire le mode averaging pour des positions de niveau OSM.

### Added
- **Mode averaging** : collecte N secondes de samples GPS et sauvegarde un seul point avec la **moyenne des lat/lon** et le **meilleur HDOP observé**. Setting `Averaging` : off / 5s / 10s / 30s / 60s (défaut off). C'est LA technique utilisée par les mappeurs OSM sérieux pour réduire le bruit du GPS consumer (le NEO-6M a typiquement ±5 m d'erreur par sample, ramené à <2 m avec 30s de moyenne).
  - **Écran dédié** pendant la collecte : progress (elapsed/total), samples accumulés, HDOP min, moyenne live des coords.
  - **`OK court` = démarre l'averaging** si setting > 0, sinon save instantané.
  - **`OK long` = force save instantané** (bypass averaging + HDOP gate + duplicate check).
  - **`Back` pendant collecte = annule** sans sauver.
  - Marqueur `avg` ajouté automatiquement au champ `note` des points moyennés.
- **Toast de confirmation** après chaque save sur Quick Log : pendant 10 secondes, le footer affiche `> saved Bench @10:42` avec le preset sauvegardé et l'heure (HH:MM). Après 10s, retour au rappel des touches habituel.
- **Ligne "last save"** persistante en bas de l'écran GPS Status : `last: Bench @10:42` (remplace "Back" qui n'était pas utile).
- **Messages de refus de save explicites** : quand OK court est refusé, un overlay rouge sur Quick Log indique :
  - `No GPS fix / Hold OK to force` si pas de fix
  - `HDOP X.X > 2.5 / Hold OK to force` si HDOP trop haut
  
  Une touche efface l'overlay. Plus de mystère sur "pourquoi mon save n'a rien fait".

### Fixed
- **Clignotement de l'affichage GPS** entre "FIX OK" et "No GPS fix" à 1-2 Hz en condition de fix stable, causé par le flip-flop entre RMC status=V (void) et GGA quality>0 dans les trames consécutives. Corrigé par une **hystérésis de 5 secondes** sur l'affichage `has_fix` (Quick Log / Track / GPS Status). La logique d'écriture reste sur le `app->has_fix` brut pour préserver l'intégrité des gates (trkpts, notification fix acquis).
- **Coordonnées qui repassaient à 0,0** entre deux trames valides : l'ISR initialisait `lat/lon/hdop/sats` à 0 en local avant de passer au parser. Si la trame en cours ne contenait pas de fix, le parser ne touchait pas ces variables et on écrivait 0 dans `app->lat`. Maintenant les locaux sont seedés depuis `app->*` → on garde la dernière valeur connue.

### Changed
- `settings.txt` ajoute `avg_seconds=N`.
- Refactor interne : extraction du chemin d'écriture dans `quick_log_write_point(...)` pour éviter la duplication entre save instantané et save averaged.

## 0.8 — 2026-04-19

### Added
- **Catalogue OSM étoffé** : de 23 presets en v0.7 à **65 presets** en 15 catégories (v0.8).
  - **Street furniture** : +6 (Telephone, Shelter, Vending machine, Public bookcase, Clock, Bollard, Water tap)
  - **Roads & signs** : +4 (Traffic signals, Stop, Give way, Speed bump, Mile marker)
  - **Parking** : +1 (Disabled parking avec tag additionnel wheelchair=designated)
  - **Sports & leisure** : +7 (Football, Tennis, Ping pong, Skate park, Fitness station, Swimming pool, Climbing wall)
  - **Waste** : +1 (Composter) + variantes de recycling (glass/paper/clothes/plastic)
  - **Shops** (nouvelle catégorie dédiée) : Supermarket, Clothes, Hairdresser, Beauty, Bookshop, Hardware, Florist (+ Cafe/Restaurant/Pharmacy/Bakery préservés)
  - **Services** (nouvelle catégorie) : Bank, ATM, Post office, Doctor, Dentist, Veterinary, Gas station, Car wash, Laundry
  - **Emergency** : +4 (Fire station, Police, Hospital, Emergency phone)
  - **Tourism** (nouvelle catégorie) : Hotel, Museum, Viewpoint, Artwork, Monument, Camp site, Attraction
  - **Nature** (nouvelle catégorie) : Tree, Bush, Peak, Spring, Park
  - **Education** (nouvelle catégorie) : School, Kindergarten, University, Library
  - **Religion** (nouvelle catégorie) : Place of worship, Chapel, Cemetery, Wayside cross
  - **Transport** (nouvelle catégorie) : Bus stop, Tram stop, Subway entrance, Train station, Taxi stand, Bike rental, Ferry
  - **Address** (nouvelle catégorie) : House number, Building entry, Building — convention "note field = house number" documentée dans le tutoriel OSM
- **`PRESETS_MAX_ENTRIES`** bumped de 64 à 128 pour accommoder les extensions via `presets.txt`.

### Changed
- Categories enum passe de 8 à 15 valeurs.
- `presets.txt.sample` réécrit avec la table de catégories et des exemples par usage.
- `docs/GETTING_STARTED_OSM.md` : ajout du workflow "House number" (utiliser la note pour saisir le numéro, éditer le tag dans JOSM).

## 0.7 — 2026-04-19

### Added
- **Icônes dans le menu principal** : refactor `Submenu` → `Menu` Flipper module, 6 icônes 10×10 1-bit (Quick Log, Track, Last points, GPS status, Settings, About) générées dans `images/` et packagées via `fap_icon_assets`.
- **Sous-catégories de presets** : nouvelle vue intermédiaire entre le menu principal et la liste des presets. Les 23 presets par défaut sont regroupés en 7 catégories (Street furniture, Roads, Parking, Sports & leisure, Waste, Shops & services, Emergency). Les `presets.txt` peuvent inclure un champ catégorie optionnel `label;key;value[,variants];category` (0..7).
- **Archive session** : nouvelle action dans Last points → "Archive session" → renomme les 5 fichiers de sortie dans un sous-dossier `session_YYYYMMDDTHHMMSS/`. Plus simple qu'un ZIP compressé (pas de zlib dispo sur Flipper), même valeur UX : un dossier unique à transférer via qFlipper.

### Changed
- Flow de sélection : Main menu → Categories → Presets (filtrés par catégorie) → Quick Log. Back navigation cohérente à chaque niveau.

## 0.6 — 2026-04-19

### Added
- **Stats live en mode trace** : distance totale parcourue (cumul haversine entre trkpts), vitesse instantanée (depuis `$GPRMC` champ 7 — nœuds convertis en km/h), vitesse max observée dans la session. Affichées en 2 lignes sur l'écran Track.
- **GitHub Actions CI** : workflow `.github/workflows/build.yml` qui build l'app contre les canaux Release et RC du firmware Flipper à chaque push/PR, avec upload de l'artefact `.fap`.
- **Parser NMEA étendu** : extrait maintenant la vitesse depuis RMC champ 7.

### Fixed
- **Bloqueur runtime** : `atof` n'est plus dans l'API Flipper SDK 1.4.3 (désactivé). Remplacé par un `simple_atof` inline dans storage.c, évite l'erreur `MissingImports` au load de l'app.

## 0.5 — 2026-04-19

### Added
- **Détection de doublons** : au save, si un point avec le même tag existe à moins de X mètres, afficher un écran de confirmation ("Same tag Ym away — OK=save anyway, Back=cancel"). Seuil configurable dans Settings : `Dup check` = off / 5m / 10m / 25m (défaut 10m).
- **Écrans d'erreur explicites** : si le save échoue (SD absente, dossier impossible à créer, disque plein), un overlay rouge affiche `SAVE FAILED` + message précis. Une touche pour dismiss.
- **Vérification pré-save** : avant chaque sauvegarde, check de la présence SD + espace libre (seuil 10 Ko). Évite les échecs silencieux.

### Fixed
- Le tag multi-ligne (`amenity=bench;material=wood`) était wrappé par `elements_multiline_text_aligned` par-dessus la ligne coords. Maintenant splitté proprement sur 2 lignes en Quick Log et Preview, les autres lignes décalées en conséquence.

### Changed
- `settings.txt` ajoute `duplicate_check_m` (entier en mètres, 0 = désactivé).

## 0.4 — 2026-04-19

### Added
- **Notes persistantes par preset** : la note saisie pour un preset (ex. "wooden bench") est sauvegardée dans `notes_cache.txt` sur la SD et rechargée automatiquement à la prochaine sélection du même preset.
- **Auto photo ID** (toggle Settings) : append automatique de `photo:N` dans la note au save (N = total cumulatif + 1). Pratique pour aligner les saves Flipper avec des photos téléphone prises en parallèle.
- **Tutoriel OSM pour débutants** : `docs/GETTING_STARTED_OSM.md` — guide complet de A à Z pour un premier contributeur OSM (câblage, app, premier fix, logger un POI, récupérer les fichiers, importer dans JOSM / iD, bonnes pratiques).

### Changed
- `settings.txt` ajoute la clé `auto_photo_id`.
- Nouveau fichier `/ext/apps_data/osm_logger/notes_cache.txt` (format TAB-séparé `primary_tag<TAB>note`).

## 0.3 — 2026-04-19

### Added
- **Last points browser** : nouvelle vue "Last points (browse/undo)" accessible depuis le menu principal. Liste les 10 derniers saves sous la forme `HH:MM  tag`. Actions en bas de liste : **Delete last** (supprime le dernier point des 4 fichiers : JSONL, CSV, GPX, GeoJSON) et **Clear all** (wipe de tous les fichiers de points + reset des compteurs).
- **Point detail view** : clic sur un point dans la liste Last Points → écran avec détails complets (time ISO, tag, lat/lon 6 décimales, altitude, HDOP, sats, note).
- **Variantes = tags additionnels** : un preset peut désormais avoir des variantes qui ajoutent un tag secondaire (pas juste une valeur alternative). Exemple `Bench` → `amenity=bench` / `amenity=bench;material=wood` / `amenity=bench;material=metal`. Les variantes contenant `=` deviennent des tags additionnels, les autres restent des valeurs alternatives.
- **Filtre de distance en mode trace** : setting `Track min dist` (off / 2m / 5m / 10m) — skip les trkpts trop proches du précédent. Calcul haversine approx (équirectangulaire, <1% erreur à l'échelle km).
- **HDOP strict en mode trace** : toggle `Track HDOP strict` dans Settings — rejette les trkpts quand HDOP > 2.5 (comme le gate Quick Log).
- **Preview avant save** : toggle `Preview save` dans Settings. OK court sur Quick Log ouvre une vue de confirmation avec tag/coords/HDOP/note avant le vrai save. OK long court-circuite le preview.
- **Cap / heading** parsé depuis `$GPRMC` champ 8, affiché en mode trace (`hdg=225°`).
- **Badge batterie** `%%` en haut à droite sur Quick Log, Track et GPS Status.

### Changed
- **Tag multi-valué** dans les fichiers de sortie : quand une variante additionnelle est utilisée, le champ `tag` contient maintenant des tags séparés par `;` (ex. `amenity=bench;material=wood`).
- **`settings.txt`** : nouveau fichier `/ext/apps_data/osm_logger/settings.txt` persiste les préférences (baud_rate, track_interval_s, track_min_dist_m, track_hdop_strict, preview_before_save).
- **Parser NMEA** : extraction de lat/lon depuis GGA aussi (pas seulement RMC) — corrige un bug où les coordonnées restaient à 0 quand GGA indiquait un fix mais RMC pas encore.

### Fixed
- Cooldown de 10 s sur la notification "fix acquired" pour éviter le spam si le fix flippe entre trames RMC et GGA.

## 0.2 — 2026-04-19

### Added
- **Mode Trace (auto-log GPX)** : une nouvelle vue avec un timer périodique (5 s) qui écrit des `<trkpt>` dans `track.gpx`. Chaque session démarre un nouveau `<trkseg>` pour ne pas tracer de ligne fictive entre deux sessions.
- **Presets dynamiques depuis la microSD** : `/ext/apps_data/osm_logger/presets.txt` (format `label;key;value[,variant1,...]`) remplace la liste compilée si présent.
- **Variantes de preset** : `←` / `→` cyclent entre plusieurs valeurs alternatives pour le même key OSM (ex. `amenity=cafe` → `pub` → `bar` → `fast_food`).
- **Éditeur de note** : `Up` dans Quick Log ouvre un `TextInput` Flipper, `Down` efface. La note est incluse dans les fichiers de sortie.
- **Notification fix acquis** : vibration + LED verte automatiques quand le GPS passe de no-fix à fix.
- **Statut GPS** devient une vue dédiée (plus un one-off action) avec toutes les infos GPS live + compteurs diagnostiques NMEA (octets reçus / lignes parsées).
- **About** : écran auteur + lien GitHub + licence.
- **Total cumulatif** : compte les lignes de `points.jsonl` au démarrage, affiché sur Quick Log.
- **Métadonnées Apps Catalog** : `fap_category`, `fap_version`, `fap_icon`, `fap_author`, `fap_weburl`, `fap_description`.
- **Doc** : nouveau `docs/ROADMAP.md`, `docs/DEVELOPMENT.md` et `docs/FORMATS.md` à jour.
- **Anglais** : toute l'UI (menu, footers, headers, noms des presets par défaut) passe en anglais pour la soumission au catalogue Flipper. `presets.txt.sample` reste en français comme exemple de personnalisation.

### Changed
- **Format JSONL / CSV** enrichi avec `alt`, `hdop`, `sats` en plus de `time/lat/lon/tag/note`.

## 0.1 — 2026-04-19

### Added
- **Mode Rapide (Quick Log)** : sous-menu de 23 presets OSM → écran live avec coords, altitude, HDOP, satellites, âge du fix, compteurs.
- **HDOP gate** : `OK` court refuse la sauvegarde si pas de fix ou HDOP > 2.5, `OK` long force.
- **5 formats natifs** écrits dans `/ext/apps_data/osm_logger/` :
  - `points.jsonl` (JSON Lines)
  - `notes.csv` (tableur)
  - `points.gpx` (waypoints GPX 1.1)
  - `points.geojson` (FeatureCollection)
  - L'écriture utilise un **write-append-framed** pour garder GPX et GeoJSON valides à tout moment.
- **Parser NMEA** minimal (RMC + GGA), sans `atof` / `strtok`, ISR-safe.
- **Gestion du service Expansion** : désactivé pendant que l'app tourne pour libérer l'UART.
- **Doc** : `README.md`, `docs/HARDWARE.md`, `docs/FORMATS.md`, `docs/DEVELOPMENT.md`.
