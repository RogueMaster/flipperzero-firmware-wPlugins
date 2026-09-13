# Staff Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-staff-time-clock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-staff-time-clock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-staff-time-clock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases)
[![Downloads](https://img.shields.io/github/downloads/vladpereverzyev/flipper-staff-time-clock/total?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-staff-time-clock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[![en](https://img.shields.io/badge/lang-en-red.svg)](./README.md)
[![it](https://img.shields.io/badge/lang-it-green.svg)](./README.it.md)
[![es](https://img.shields.io/badge/lang-es-yellow.svg)](./README.es.md)
[![fr](https://img.shields.io/badge/lang-fr-blue.svg)](./README.fr.md)
[![de](https://img.shields.io/badge/lang-de-lightgrey.svg)](./README.de.md)

Application de **pointage du personnel** pour le [Flipper Zero](https://flipperzero.one/).
Elle sert a enregistrer vos collaborateurs et leurs entrees/sorties: attribuez a
chaque personne un badge - **NFC**, **RFID** ou **iButton** - approchez-le, et
chaque pointage est enregistre avec date et heure sur la microSD au format CSV
ouvrable dans Excel. Fonctionne en autonomie, sans telephone ni PC.

Le lecteur lit **NFC**, **RFID** et **iButton**: choisissez lequel utiliser
avec Gauche/Droite sur l'ecran de lecture (l'app retient votre dernier
choix). Dans une meme entreprise une personne peut avoir un badge NFC, une
autre un porte-cles RFID et une autre un iButton - chacun choisit juste sa
technologie avant de pointer.

**Une carte que la personne possede deja convient aussi.** Comme l'app **lit
seulement l'UID** et n'ecrit rien sur la carte, un badge deja utilise dans une
autre entreprise (carte d'acces de bureau, porte-cles de salle de sport, carte de
transport...) peut etre enregistre et utilise ici sans etre modifie ni ecrase
d'aucune facon - le systeme retient simplement son UID parmi les autres.

> **Identification seulement - usage autorise uniquement.** L'app lit l'UID du
> badge pour distinguer les personnes. Elle n'**ecrit** pas et n'**emule** pas les
> badges et ne **contourne** aucun systeme d'authentification; enregistrer une
> carte ici n'a aucun effet la ou elle sert par ailleurs. Utilisez seulement avec
> des personnes et badges que vous etes autorise a gerer. Voir
> [SECURITY.md](SECURITY.md).

## Ecrans

Maquettes facon Flipper des ecrans principaux (128x64):

<table>
  <tr>
    <td align="center"><img src="docs/img/menu.svg?v=4" width="240" alt="Menu principal"><br><b>Menu principal</b><br>Pointer, Work mode, Badges, Historique...</td>
    <td align="center"><img src="docs/img/work-clock.svg?v=4" width="240" alt="Work mode"><br><b>Work mode</b><br>Horloge, pointage a la puce</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/greeting.svg?v=4" width="240" alt="Message"><br><b>Passez le badge</b><br>Bienvenue / Au revoir par nom</td>
    <td align="center"><img src="docs/img/today.svg?v=4" width="240" alt="Resume du jour"><br><b>Aujourd'hui</b><br>Pointages + heures totales</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/pin.svg?v=4" width="240" alt="Verrou PIN"><br><b>Verrou PIN</b><br>Protege la sortie de l'app</td>
    <td align="center"><img src="docs/img/settings.svg?v=4" width="240" alt="Reglages"><br><b>Reglages</b><br>Son / Vibration / LED, PIN</td>
  </tr>
</table>

## Fonctions

- **Pointer** en approchant un badge - les badges connus sont reconnus par UID.
- **Lecteur multi-technologie**: **NFC** (13.56 MHz), **RFID LF** (125 kHz)
  et **iButton** (cles Dallas 1-Wire). Gauche/Droite sur l'ecran de lecture
  choisit lequel utiliser (retenu entre les sessions), donc NFC, RFID et
  iButton coexistent dans la meme installation.
- **Correction manuelle**: **Ajouter IN** / **Ajouter OUT** depuis le badge d'une
  personne ajoute un pointage manquant a l'heure actuelle.
- **Postes passant minuit** bien comptes; **objectif journalier** optionnel avec
  **heures sup** dans Aujourd'hui; **export du mois** en CSV.
- **Fonctionne avec des cartes existantes**: en ne lisant que l'UID (jamais
  d'ecriture), une carte deja utilisee ailleurs - meme d'une autre entreprise -
  peut etre enregistree et utilisee sans la modifier.
- **Enregistrer un collaborateur** au premier passage de son badge, avec un nom.
  Chaque personne est liee a cette puce (son UID): chaque pointage renvoie a cette
  puce.
- **Gerer les collaborateurs (badges)**: renommer, **remplacer la puce** si perdue
  (garde le nom et l'historique, seule la puce change), voir l'historique de la
  personne, **annuler le dernier pointage** (corriger une erreur), supprimer
  (l'historique reste).
- **IN/OUT automatique**: approcher un badge alterne tout seul (d'abord IN, puis
  OUT, puis IN...) - aucun choix manuel, le pointage est instantane.
- **Retour au pointage**: **son**, **vibration** et **LED** distincts pour IN et
  OUT (tonalite montante + 1 vibration + vert pour IN; tonalite descendante + 2
  vibrations + bleu pour OUT). Chacun desactivable dans Reglages (actifs par
  defaut).
- **Apercu**: ecran rapide par collaborateur (heures du jour / semaine / mois et
  la pause du jour), Gauche/Droite pour changer de personne.
- **Historique** (un seul bouton du menu) avec le journal complet (tous /
  aujourd'hui / semaine, ou par collaborateur depuis Badges), resume
  **Aujourd'hui** (premiere entree, derniere sortie, heures et pauses),
  **Semaine** (heures par jour + total) et **Mois** (heures par collaborateur).
- **Stockage sur microSD** en CSV, plus **export JSON**, **CSV date**, une
  **Sauvegarde** (copie datee des badges + pointages) et **Restauration**
  (recharger badges + pointages depuis une sauvegarde).
- **Mode protege (PIN)**: code optionnel en **sequence de fleches** (Haut / Bas /
  Gauche / Droite - rapide) qui verrouille la sortie de l'app; propose au premier
  lancement ou plus tard dans Reglages.
- **Langues**: anglais, italien, espagnol, francais, allemand - au choix dans
  Reglages (le firmware officiel n'expose pas de langue systeme).

Voir la [Roadmap](#roadmap) pour les idees v1.1 / v2.0.

### Une puce par personne (et puce perdue)

Chaque collaborateur est identifie par l'**UID** de sa puce: attribuez une puce
par personne et gardez-la comme reference - tous ses pointages renvoient a cette
puce. Si quelqu'un **perd sa puce**, ouvrez **Badges -> (personne) -> Remplacer
puce** et passez une nouvelle puce (vierge ou une qu'elle possede deja): nom et
pointages passes sont gardes, seule la puce de reference change.

## Fichiers de donnees

Tout est enregistre sur la microSD dans `/ext/apps_data/timeclock/`:

| Fichier       | Contenu                                                         |
|---------------|-----------------------------------------------------------------|
| `badges.csv`  | Badges: `uid,name,tech,created,last_used,last_event` (`tech`: `NFC`/`RFID`/`iBTN`) |
| `punches.csv` | Historique: `date,time,name,uid,type` (`IN`/`OUT`)              |
| `config.txt`  | Reglages + **hash** du PIN et sel (jamais le PIN en clair)      |
| `export.json` | Export JSON de l'historique (*Export -> Export JSON*)           |
| `punches-YYYY-MM-DD.csv` | Instantane CSV date (*Export -> Export CSV*)        |
| `punches-YYYY-MM.csv` | Export CSV du mois (*Export -> Export mois*)            |
| `backup/`     | Copies datees des badges + pointages (*Export -> Backup*)       |

`punches.csv` est la feuille de pointage interne: chaque entree/sortie de chaque
collaborateur, par jour et heure. Elle s'ouvre dans Excel, LibreOffice, Google
Sheets, etc.

Exemple de `punches.csv`:

```csv
date,time,name,uid,type
2026-09-12,08:02,Mario,04A1B2C3D4,IN
2026-09-12,12:31,Mario,04A1B2C3D4,OUT
```

## Compilation et installation

C'est une app externe (FAP) pour le **firmware officiel**. Compilez avec
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
```

Depuis le dossier du projet (celui avec `application.fam`):

```bash
ufbt
```

Installez et lancez sur un Flipper connecte:

```bash
ufbt launch
```

Le `.fap` compile arrive dans `dist/`. Vous pouvez aussi le copier dans
`SD Card/apps/Tools/` via qFlipper et le lancer depuis **Apps -> Tools -> Time
Clock**.

> **Note firmware.** La couche radio est dans `timeclock_reader.c` (NFC via le
> poller ISO14443-3A - MIFARE Classic/Ultralight, NTAG, DESFire - le worker LF
> RFID 125 kHz et le worker iButton pour les cles Dallas 1-Wire, demarres
> ensemble). C'est la partie la plus sensible aux changements d'API; si un symbole
> change, le correctif est dans ce seul fichier.

## Compatibilite

Staff Time Clock fonctionne sur le firmware **officiel** du Flipper Zero et sur les fork
populaires. Un FAP est compile pour l'API d'un firmware precis, donc chaque
Release fournit **un `.fap` par firmware** - telechargez celui qui correspond:

| Firmware    | Fichier de la release       |
|-------------|-----------------------------|
| Officiel    | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

RogueMaster est base sur le SDK Unleashed (compatible binaire). Pour un firmware
non liste, compilez depuis les sources avec `ufbt` (voir plus haut): le code
utilise des API standard et est portable.

## Mode protege et PIN - ce qu'il peut et ne peut pas

Le PIN est une **sequence rapide de 4 fleches** (ex. Haut, Haut, Gauche, Droite).
Il est propose au premier lancement, ou a tout moment via *Reglages -> Definir
PIN*. Quand il est actif, l'app demarre verrouillee et **Back ne quitte plus
l'app**; la seule voie logicielle est *Reglages -> Quitter* (ou Work mode ->
Back), qui demande la sequence. Il est stocke uniquement en **hash avec sel**,
jamais en clair.

**Limites honnetes (par conception):**

- Aucune app ne peut empecher un arret **materiel** ou un arret force au niveau
  firmware (ex. `Gauche` + `Back` pour redemarrer, ou couper l'alimentation). Le
  mode protege ne couvre que ce qui est controlable par logiciel.
- Le hash du PIN (FNV-1a) evite de le stocker en clair et protege l'interface,
  mais ce **n'est pas** une defense forte contre quelqu'un ayant un acces physique
  a la microSD qui teste hors ligne les sequences courtes.
- Il n'y a **aucun contournement cache**. Supprimer `config.txt` sur la SD
  reinitialise les reglages (et le PIN).

## Structure du projet

```
timeclock/
|-- application.fam            # manifeste de l'app
|-- timeclock.h / .c           # cycle de vie, entry point, helpers
|-- timeclock_storage.h / .c   # persistance microSD + modele de donnees
|-- timeclock_pin.h / .c       # hash du PIN avec sel
|-- views/
|   `-- pin_view.h / .c        # vue personnalisee du PIN
`-- scenes/
    |-- timeclock_scene*.{h,c} # scene manager (X-macro)
    `-- timeclock_scene_*.c    # un fichier par ecran
```

## Roadmap

- **v1.1 / v1.2** - resume hebdomadaire, calcul des pauses et filtres (faits).
- **v2.0** - sauvegarde et restauration (faites); ensuite: sync Bluetooth, app companion, import.

## Flipper App Catalog

Un manifest pret pour le catalogue se trouve dans [catalog/manifest.yml](catalog/manifest.yml)
et l'historique des versions dans [changelog.md](changelog.md). Pour publier sur
[apps.flipper.net](https://apps.flipper.net): forkez
[flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog),
copiez le manifest dans `applications/Tools/timeclock/manifest.yml`, definissez
son `commit_sha` sur le commit de la release taguee, ajoutez de vraies captures
prises avec qFlipper (voir [screenshots/README.md](screenshots/README.md)) et
ouvrez une pull request. L'app compile avec le dernier Release SDK `ufbt`
(cible f7).

## Contribuer

Les contributions sont bienvenues - voir [CONTRIBUTING.md](CONTRIBUTING.md) et le
[Code de Conduite](CODE_OF_CONDUCT.md).

## Soutenir

Si Staff Time Clock vous est utile, vous pouvez soutenir le developpement:

[![Sponsor sur GitHub](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/vladpereverzyev)
[![Offrir un cafe sur Ko-fi](https://img.shields.io/badge/Ko--fi-Offrir%20un%20cafe-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/vladpereverzyev)

## Licence

Staff Time Clock est **open source**, sous licence
[GNU General Public License v3.0 ou ulterieure](LICENSE).

- **Utilisez-le, modifiez-le et redistribuez-le librement** - pour du
  pointage personnel ou commercial, sur autant de Flipper que vous voulez.
- **Si vous redistribuez une version modifiee**, elle doit rester sous la
  meme licence et son code source doit etre disponible.
- Copyright © 2026 Vladyslav Pereverzyev. Les fichiers source portent
  l'en-tete `SPDX-License-Identifier: GPL-3.0-or-later`.
