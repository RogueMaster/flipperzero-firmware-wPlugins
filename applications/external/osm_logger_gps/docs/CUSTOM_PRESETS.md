# Presets personnalisés

L'app embarque **65 presets OSM** compilés par défaut, couvrant les POIs les plus fréquents (mobilier urbain, commerces, transports, etc.). C'est suffisant pour la plupart des sessions terrain.

Pour avoir **plus de presets** (jusqu'à **128**, la limite actuelle de `PRESETS_MAX_ENTRIES`) ou des presets **traduits**, il est possible de déposer un fichier `presets.txt` sur la microSD du Flipper :

```
/ext/apps_data/osm_logger/presets.txt
```

L'app détecte ce fichier au démarrage et **remplace** la liste compilée par celle du fichier. Pas besoin de recompiler l'app.

---

## Méthode 1 — Édition manuelle

Le format est simple, une ligne par preset :

```
Label;key;value[,variant1,variant2,...];category
```

- **Label** : nom affiché à l'écran, 18 caractères ASCII max (pas d'accents, pas de `;`, pas de `,`)
- **key / value** : le tag OSM principal (ex. `amenity=bench`)
- **variants** *(optionnel)* : valeurs alternatives (`pub,bar,fast_food`) **OU** tags additionnels (`material=wood`, cyclables via ←/→ sur Quick Log)
- **category** : entier 0-14 (voir table ci-dessous)

### Table des catégories

| ID | Catégorie          |
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

### Exemple (`presets.txt`)

```
# Lignes commençant par # ignorees
Bench;amenity;bench,material=wood,material=metal;0
Bin;amenity;waste_basket;0
Drinking water;amenity;drinking_water;0
Cafe;amenity;cafe,pub,bar,fast_food;5
Bike parking;amenity;bicycle_parking;2
```

Voir [presets.txt.sample](../presets.txt.sample) pour un exemple complet commenté.

---

## Méthode 2 — Générer depuis le schéma iD officiel (`build_presets.py`)

Pour bénéficier des **centaines de presets maintenus par la communauté OSM** (projet [id-tagging-schema](https://github.com/openstreetmap/id-tagging-schema) utilisé par l'éditeur iD), utilise le script `scripts/build_presets.py`.

Le script télécharge le schéma officiel, filtre les presets point, mappe vers nos 15 catégories, et génère un `presets.txt` compatible.

### Pré-requis

- Python **3.8+**
- Connexion réseau (ou un `presets.json` téléchargé à la main, voir `--input`)

### Utilisation

```bash
# 1. Depuis la racine du repo
cd scripts/

# 2. Génère un presets.txt en anglais (défaut)
python3 build_presets.py

# 3. Version française (noms traduits)
python3 build_presets.py --locale fr -o presets_fr.txt

# 4. Limite à 100 presets et affiche les stats par catégorie
python3 build_presets.py --locale fr --max 100 --stats -o presets_fr.txt

# 5. Mode hors-ligne (presets.json téléchargé séparément)
python3 build_presets.py --input presets.json --locale fr
```

Options :
- `--locale` : code locale iD (`en`, `fr`, `de`, `es`, `it`, `pt`, `nl`, …). Défaut : `en`.
- `--max N` : limite à N presets max (défaut 128, la limite de l'app).
- `-o FILE` : chemin du fichier de sortie (défaut `presets.txt` dans le dossier courant).
- `--input FILE` : utilise un `presets.json` local au lieu de télécharger.
- `--stats` : affiche la répartition par catégorie en sortie standard.

### Déploiement sur le Flipper

1. Lance le script → obtiens `presets.txt` en local.
2. Branche le Flipper, ouvre qFlipper (ou monte la microSD).
3. Copie `presets.txt` dans `/ext/apps_data/osm_logger/`.
4. Redémarrer l'app OSM Logger → les nouveaux presets sont chargés (visible dans la console série si consultée).

### Sous le capot

Le script :
1. Télécharge `presets.json` depuis `openstreetmap/id-tagging-schema` (branche `develop`).
2. Filtre les entrées : garde uniquement celles avec `geometry: ["point", ...]` et un tag concret.
3. Extrait le tag principal (`emergency`, `amenity`, `shop`, `tourism`, …).
4. Mappe vers une de nos 15 catégories via `CATEGORY_RULES` (first-match-wins).
5. Charge la locale demandée et remplace les noms anglais par les traductions si disponibles.
6. Normalise les labels : décompose les accents (`é → e`), retire les diacritiques, limite à 18 caractères ASCII.
7. Trie par catégorie puis label, limite au nombre demandé.
8. Écrit le `presets.txt` dans notre format.

### Personnaliser le mapping

Pour que certains tags tombent dans une catégorie différente, éditer la liste `CATEGORY_RULES` en tête du script. Les règles sont appliquées **dans l'ordre**, la première correspondance gagne. Exemple pour classer `amenity=fountain` comme Nature plutôt que Street furniture :

```python
CATEGORY_RULES = [
    ("amenity", "fountain", CAT_NATURE),  # ajouter AVANT la règle générique Street
    # ... reste inchangé
]
```

---

## Retour d'expérience / contribution

En cas de génération d'une liste particulièrement utile (ex. `presets_fr_urbain_complet.txt` pour cartographie urbaine), ne pas hésiter à ouvrir une PR pour l'ajouter à un futur dossier `presets_samples/` dans le repo.

Les ajouts de règles dans `CATEGORY_RULES` sont bienvenus : la logique actuelle est orientée "OSM generaliste", mais des cartographes spécialisés (bornes électriques, mobilier cyclable, etc.) auront des préférences différentes.
