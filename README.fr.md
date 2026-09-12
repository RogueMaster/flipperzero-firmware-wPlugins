# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[English](README.md) | [Italiano](README.it.md) | [Espanol](README.es.md) | **Francais** | [Deutsch](README.de.md)

Application de **pointage du personnel** pour le [Flipper Zero](https://flipperzero.one/).
Attribuez a chaque collaborateur un **badge vierge** NFC ou RFID, approchez-le, et
le pointage (entree/sortie) est enregistre avec date et heure sur la microSD au
format CSV. Fonctionne en autonomie, sans telephone ni PC.

> **Usage autorise uniquement.** L'app lit **seulement l'UID** du badge pour le
> reconnaitre. Elle n'emule pas et n'ecrit pas les badges, et ne contourne aucun
> systeme d'acces. Utilisez uniquement des badges et systemes autorises.

## Fonctions

- **Pointer**: approchez le badge et l'app enregistre seule, en alternant **IN**
  et **OUT** (d'abord IN, puis OUT, puis IN...). Aucun choix manuel.
- **Lecteur** au choix dans Reglages: **NFC** (13.56 MHz) ou **RFID LF** (125 kHz).
- **Enregistrement** d'un collaborateur sur un badge vierge (nom seulement).
  Chaque personne est liee a son badge (UID); badge perdu: *Badges > Remplacer puce*.
- **Work mode**: horloge plein ecran, message Bienvenue/Au revoir; pour sortir il
  faut le **PIN a fleches**.
- **Historique** avec filtres, resume **Aujourd'hui** (entree/sortie/heures/pauses)
  et **Semaine**.
- **Export** CSV/JSON, **CSV date**, **Sauvegarde** et **Restauration** sur microSD.
- **Retour** son/vibration/LED distincts pour IN et OUT (desactivables).
- **PIN** optionnel (sequence de fleches) propose au premier lancement.
- **Langues**: anglais, italien, espagnol, francais, allemand.

## Compilation et installation

Application externe (FAP) pour le **firmware officiel**, compilee avec
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
ufbt            # compile (sortie dans dist/)
ufbt launch     # installe et lance sur le Flipper connecte
```

Ou telechargez depuis la [Release](https://github.com/vladpereverzyev/flipper-timeclock/releases)
le `.fap` de votre firmware et copiez-le dans `apps/Tools/`.

| Firmware    | Fichier                     |
|-------------|-----------------------------|
| Officiel    | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

## Licence

Copyright (C) 2026 Vladyslav Pereverzyev. Licence **GNU GPL-3.0-or-later** - voir
[LICENSE](LICENSE). Documentation complete dans le [README en anglais](README.md).
