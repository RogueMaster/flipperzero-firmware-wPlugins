# Formats de sortie

Tous les fichiers sont écrits dans `/ext/apps_data/osm_logger/` sur la microSD, en mode **append-only**.

## `points.jsonl` — JSON Lines

Une ligne JSON par point sauvegardé :

```json
{"time":"2026-04-19T08:45:12Z","lat":48.431234,"lon":-0.093210,"alt":45.2,"hdop":1.3,"sats":9,"tag":"amenity=bench","note":""}
```

| Champ | Type      | Notes                                                           |
|-------|-----------|-----------------------------------------------------------------|
| `time` | string   | ISO-8601 UTC depuis le RTC du Flipper                           |
| `lat`  | number   | Degrés décimaux, 6 décimales (~11 cm de précision)              |
| `lon`  | number   | Degrés décimaux, 6 décimales                                    |
| `alt`  | number   | Altitude en mètres (MSL depuis trame GGA), 1 décimale           |
| `hdop` | number   | HDOP au moment du save (2 → 4 m, 1 → 2 m, 5+ = douteux)         |
| `sats` | integer  | Nb de satellites utilisés pour le fix                           |
| `tag`  | string   | Tag OSM au format `key=value` ou tags multiples séparés par `;` (ex. `amenity=bench` ou `amenity=bench;material=wood` pour une variante à tag additionnel) |
| `note` | string   | Note libre. Peut contenir : la note utilisateur (via `Up` → TextInput), `photo:N` si `Auto photo ID` activé, `avg` si le point a été sauvé via le mode averaging. Exemple combiné : `"wooden photo:12 avg"` |

**Point forcé** (OK long sans fix) : `lat=0`, `lon=0`, `alt=0`, `hdop=99.9` → ignorer ces points au post-traitement si besoin.

**Point averaged** : enregistré via le mode Averaging (Settings → `Averaging` ≥ 5s). Les `lat`/`lon` sont la moyenne de N samples collectés pendant la capture. `hdop` reflète le **meilleur** HDOP observé pendant l'averaging (pas la moyenne). Le note contient `avg`.

## `notes.csv` — CSV sans header

Colonnes : `time, lat, lon, alt, hdop, sats, tag, note` dans cet ordre.

```csv
"2026-04-19T08:45:12Z",48.431234,-0.093210,45.2,1.3,9,"amenity=bench",""
```

Guillemets doublés autour des champs texte pour Excel/LibreOffice. Les `"` dans `note` sont remplacés par `'` pour ne pas casser le parsing.

> 💡 Pas de header → lors de l'import dans un tableur, ajouter une première ligne manuellement.

## `points.gpx` — GPX 1.1 (waypoints) avec extensions OsmAnd Favorites

Fichier XML valide à tout moment (après chaque OK). Enrichi avec le namespace **OsmAnd Favorites** (icône + couleur + catégorie) pour un rendu visuel propre dans OsmAnd, tout en restant 100% compatible avec **JOSM**, **iD**, **QGIS**, **GPX Viewer** (qui ignorent silencieusement les extensions inconnues).

```xml
<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="Flipper Zero OSM Logger"
     xmlns="http://www.topografix.com/GPX/1/1"
     xmlns:osmand="https://osmand.net">
  <wpt lat="48.431234" lon="-0.093210">
    <ele>45.2</ele>
    <time>2026-04-19T08:45:12Z</time>
    <name>Bench</name>
    <desc>amenity=bench  HDOP=1.3 sats=9</desc>
    <type>Street furniture</type>
    <extensions>
      <osmand:icon>bench</osmand:icon>
      <osmand:background>circle</osmand:background>
      <osmand:color>#10c0f0</osmand:color>
    </extensions>
  </wpt>
</gpx>
```

**Mapping des champs** :
- `<name>` : **label humain** du preset (ex. `"Bench"`, `"Drinking water"`). Plus lisible que l'ancien format `amenity=bench`.
- `<desc>` : **détails techniques** → le tag OSM complet + HDOP + nb satellites.
- `<type>` : **catégorie** (Street furniture, Roads & signs, Tourism, Emergency, …). OsmAnd les utilise pour regrouper les favoris dans des dossiers.
- `<extensions>` : directives OsmAnd pour l'affichage visuel :
  - `icon` dérivé de la valeur primaire OSM (ex. `bench`, `atm`, `cafe`) — OsmAnd cherche une icône correspondante dans sa bibliothèque, sinon fallback générique.
  - `background` = `circle` (forme de la pastille autour de l'icône).
  - `color` par catégorie : ex. rouge pour Emergency, vert pour Nature, cyan pour Street furniture.

**Import dans OsmAnd** : fichier > Importer > ce `.gpx` → chaque point apparaît comme un favori avec sa pastille colorée et son icône.

L'app utilise un *write-append-framed* : à chaque save elle ouvre le fichier en R/W, seek juste avant `</gpx>`, truncate, écrit le nouveau `<wpt>` et réécrit le footer. Le fichier reste donc valide XML même après une coupure de courant **entre deux saves**.

## `points.osm` — OSM XML natif (API 0.6)

Fichier au format **OSM API 0.6** chargeable directement dans **JOSM** comme **data layer** (pas juste un layer de waypoints). Chaque point devient un `<node>` avec ses tags OSM réels, prêt à être édité et uploadé sur openstreetmap.org en quelques clics.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<osm version="0.6" generator="Flipper Zero OSM Logger" upload="false">
  <node id="-1" lat="48.431234" lon="-0.093210" visible="true">
    <tag k="amenity" v="bench"/>
    <tag k="material" v="wood"/>
    <tag k="ele" v="45.2"/>
    <tag k="note" v="wooden slats, needs repair"/>
    <tag k="flipper:hdop" v="1.3"/>
    <tag k="flipper:sats" v="9"/>
    <tag k="flipper:time" v="2026-04-19T08:45:12Z"/>
  </node>
</osm>
```

### Convention des IDs

Les IDs sont négatifs incrémentaux (`-1`, `-2`, `-3`…) — c'est la **convention OSM API** pour les nodes "nouveaux" pas encore uploadés. JOSM les reconnaît automatiquement et propose de les créer réellement sur le serveur au moment de l'upload.

### Tags générés

- **Tags primaires OSM** : le contenu de `tag` (ex. `amenity=bench`) est éclaté en plusieurs `<tag>` si multi-valué (`amenity=bench;material=wood` → 2 balises `<tag>`)
- **`ele=`** : altitude (mètres) si disponible
- **`note=`** : contenu de la note utilisateur (y compris `photo:N`, `avg` si activés)
- **`flipper:hdop`, `flipper:sats`, `flipper:time`** : métadonnées Flipper, préfixées par `flipper:` pour qu'elles soient **supprimables en un clic** dans JOSM avant l'upload final (convention OSM : les tags avec préfixe custom sont non-standard)

### Workflow recommandé

1. Transférer `points.osm` vers l'ordinateur via qFlipper
2. Ouvrir JOSM → **Fichier → Ouvrir** → choisir le `.osm` → accepter "Ouvrir comme fichier OSM (data layer)"
3. Chaque point apparaît comme un vrai node OSM éditable. Modifier les tags si besoin.
4. Avant upload : **Édition → Rechercher** → `type:node flipper:*` pour sélectionner les tags Flipper, puis **Supprimer** pour les retirer (uniquement si souhaité).
5. **Fichier → Uploader** → les nodes sont créés sur OSM avec leurs bons IDs.

C'est **LE format pro** pour un workflow OSM complet. `points.gpx` reste utile pour la visualisation rapide dans OsmAnd/QGIS, `points.osm` est pour l'édition et l'upload.

## `points.geojson` — GeoJSON FeatureCollection

Également valide à tout moment, prêt pour import direct dans **QGIS**, **geojson.io**, **iD**, etc.

```json
{"type":"FeatureCollection","features":[
  {"type":"Feature","geometry":{"type":"Point","coordinates":[-0.093210,48.431234,45.2]},"properties":{"time":"2026-04-19T08:45:12Z","tag":"amenity=bench","hdop":1.3,"sats":9}}
]}
```

> ⚠️ Convention GeoJSON : coordinates = `[lon, lat, alt]` (lon d'abord, contraire du GPX).

## Post-traitement optionnel

Le script [scripts/jsonl_to_geojson.py](../scripts/jsonl_to_geojson.py) reste dispo pour partir du JSONL (plus riche — garde `note` et timestamp séparés). Sinon le GeoJSON natif est directement utilisable.

```bash
python3 scripts/jsonl_to_geojson.py points.jsonl > points_from_jsonl.geojson
```

## `track.gpx` — Mode trace (auto-log GPX)

Écrit par le **Mode trace** du menu principal. Un timer périodique (intervalle configurable via Settings → `Track interval` : 1 / 5 / 10 / 30 / 60 s) ajoute un `<trkpt>` à chaque tick tant que la vue est active et qu'un fix GPS est disponible.

Deux filtres optionnels pour la qualité des tracks :
- `Track min dist` (off / 2 / 5 / 10 m) : skip les trkpts qui n'ont pas assez bougé depuis le précédent (évite les GPX pollués par les arrêts à un feu rouge)
- `Track HDOP strict` (on/off) : rejette les trkpts avec HDOP supérieur au seuil configuré

Chaque entrée dans le mode trace démarre un nouveau `<trkseg>` (segment), ce qui permet à JOSM/QGIS de ne pas tracer de ligne entre deux sessions de tracking pausées.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<gpx version="1.1" creator="Flipper Zero OSM Logger" xmlns="http://www.topografix.com/GPX/1/1">
<trk><name>Track</name>
<trkseg>
  <trkpt lat="48.431234" lon="-0.093210"><ele>45.2</ele><time>2026-04-19T08:45:00Z</time></trkpt>
  <trkpt lat="48.431267" lon="-0.093185"><ele>45.3</ele><time>2026-04-19T08:45:05Z</time></trkpt>
</trkseg>
<trkseg>
  <trkpt lat="48.432000" lon="-0.094000"><ele>47.0</ele><time>2026-04-19T08:55:00Z</time></trkpt>
</trkseg>
</trk>
</gpx>
```

Même stratégie **write-append-framed** que `points.gpx` : le fichier reste valide XML après chaque trkpt écrit, même en cas de coupure d'alim.

Usage typique : activer le mode trace avant de démarrer un trajet à vélo, laisser tourner. À l'arrivée, importer `track.gpx` dans JOSM ou un visualiseur pour voir le parcours, et ajouter les waypoints OSM par-dessus.

## `notes_cache.txt` — cache des notes par preset (interne)

Fichier interne, format TAB-séparé :
```
amenity=bench<TAB>wooden seat
amenity=drinking_water<TAB>public fountain
```

Rempli automatiquement à chaque save si une note a été tapée via l'éditeur `Up`. La prochaine fois que ce même preset est sélectionné, la note associée est pré-chargée. Utile pour ne pas retaper `"out of order"` ou `"material=wood"` à chaque banc.

Il est possible de l'éditer manuellement via qFlipper pour pré-remplir les notes.

## Archive session

Depuis le menu **Last points → Archive session**, l'app déplace les 5 fichiers courants (JSONL / CSV / GPX / GeoJSON / track.gpx) dans un sous-dossier daté :

```
/ext/apps_data/osm_logger/session_20260419T143025/
  ├── points.jsonl
  ├── notes.csv
  ├── points.gpx
  ├── points.geojson
  └── track.gpx
```

Après archivage, les compteurs session et total sont remis à zéro, et la session suivante repart de zéro. Pratique pour séparer les sessions (ex. « matin sur Paris », « après-midi sur Brest »).
