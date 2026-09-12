# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

[English](README.md) | [Italiano](README.it.md) | [Espanol](README.es.md) | [Francais](README.fr.md) | **Deutsch**

**Zeiterfassungs-App** fuer den [Flipper Zero](https://flipperzero.one/). Weise
jedem Mitarbeiter einen **leeren Chip** (NFC oder RFID) zu, halte ihn an das
Geraet, und die Stempelung (Kommen/Gehen) wird mit Datum und Uhrzeit auf der
microSD als CSV gespeichert. Laeuft eigenstaendig, ohne Telefon oder PC.

> **Nur autorisierte Nutzung.** Die App liest **nur die UID** des Chips zur
> Erkennung. Sie emuliert und beschreibt keine Chips und umgeht kein
> Zugangssystem. Nutze nur Chips und Systeme, fuer die du berechtigt bist.

## Funktionen

- **Stempeln**: Chip anhalten, die App bucht automatisch abwechselnd **IN** und
  **OUT** (erst IN, dann OUT, dann IN...). Keine manuelle Auswahl.
- **Leser** in den Einstellungen waehlbar: **NFC** (13.56 MHz) oder **RFID LF**
  (125 kHz).
- **Registrierung** einer Person auf einem leeren Chip (nur Name). Jede Person ist
  an ihren Chip (UID) gebunden; Chip verloren: *Ausweise > Chip ersetzen*.
- **Arbeitsmodus**: Vollbild-Uhr, Gruss Willkommen/Tschuess; zum Verlassen ist die
  **Pfeil-PIN** noetig.
- **Verlauf** mit Filtern, **Heute** (Kommen/Gehen/Stunden/Pausen) und **Woche**.
- **Export** CSV/JSON, **datiertes CSV**, **Backup** und **Wiederherstellen** auf
  der microSD.
- **Rueckmeldung** Ton/Vibration/LED getrennt fuer IN und OUT (abschaltbar).
- **PIN** optional (Pfeilfolge), beim ersten Start angeboten.
- **Sprachen**: Englisch, Italienisch, Spanisch, Franzoesisch, Deutsch.

## Bauen und installieren

Externe App (FAP) fuer die **offizielle Firmware**, gebaut mit
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
ufbt            # baut (Ausgabe in dist/)
ufbt launch     # installiert und startet auf dem verbundenen Flipper
```

Oder lade aus der [Release](https://github.com/vladpereverzyev/flipper-timeclock/releases)
die passende `.fap` fuer deine Firmware und kopiere sie nach `apps/Tools/`.

| Firmware    | Datei                       |
|-------------|-----------------------------|
| Offiziell   | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

## Lizenz

Copyright (C) 2026 Vladyslav Pereverzyev. Lizenz **GNU GPL-3.0-or-later** - siehe
[LICENSE](LICENSE). Vollstaendige Doku im [englischen README](README.md).
