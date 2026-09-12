# Time Clock - Flipper Zero

[![Build & Release](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml/badge.svg)](https://github.com/vladpereverzyev/flipper-timeclock/actions/workflows/build.yml)
[![Latest release](https://img.shields.io/github/v/release/vladpereverzyev/flipper-timeclock?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![Downloads](https://img.shields.io/github/downloads/vladpereverzyev/flipper-timeclock/total?cacheSeconds=300)](https://github.com/vladpereverzyev/flipper-timeclock/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Languages](https://img.shields.io/badge/lang-EN%20%7C%20IT%20%7C%20ES%20%7C%20FR%20%7C%20DE-blue)](#)

[English](README.md) | [Italiano](README.it.md) | [Espanol](README.es.md) | [Francais](README.fr.md) | **Deutsch**

**Zeiterfassungs-App** fuer den [Flipper Zero](https://flipperzero.one/). Damit
erfasst du deine Mitarbeiter und ihre Kommen-/Gehen-Zeiten: weise jeder Person
einen dedizierten **leeren** NFC- oder RFID-Ausweis zu, halte ihn an das Geraet,
die App waehlt selbst **IN** oder **OUT**, und jede Stempelung wird mit Datum und
Uhrzeit auf der microSD als CSV gespeichert, das du in Excel oeffnen kannst.
Laeuft eigenstaendig, ohne Telefon oder PC.

> **Nutze leere/dedizierte Ausweise - nur autorisierte Nutzung.** Die App ist fuer
> **leere Ausweise** gedacht, die du deinen Mitarbeitern zuweist. Sie liest **nur
> die UID** des Ausweises, um Personen zu unterscheiden: sie ist **nicht** zum
> Lesen fremder Zugangsausweise gedacht, sie **emuliert** keine Ausweise und
> **umgeht** kein Authentifizierungssystem. Nutze nur Ausweise und Systeme, fuer
> die du berechtigt bist. Siehe [SECURITY.md](SECURITY.md).

## Bildschirme

Flipper-artige Mockups der Hauptbildschirme (128x64):

<table>
  <tr>
    <td align="center"><img src="docs/img/menu.svg" width="240" alt="Hauptmenue"><br><b>Hauptmenue</b><br>Stempeln, Work mode, Ausweise, Verlauf...</td>
    <td align="center"><img src="docs/img/work-clock.svg" width="240" alt="Work mode"><br><b>Work mode</b><br>Uhr, Stempeln per Chip</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/greeting.svg" width="240" alt="Gruss"><br><b>Chip anhalten</b><br>Willkommen / Tschuess mit Name</td>
    <td align="center"><img src="docs/img/today.svg" width="240" alt="Tagesuebersicht"><br><b>Heute</b><br>Stempel + Gesamtstunden</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/img/pin.svg" width="240" alt="PIN-Sperre"><br><b>PIN-Sperre</b><br>Schuetzt das Verlassen der App</td>
    <td align="center"><img src="docs/img/settings.svg" width="240" alt="Einstellungen"><br><b>Einstellungen</b><br>Ton / Vibration / LED, PIN</td>
  </tr>
</table>

## Funktionen

- **Stempeln** durch Anhalten eines Ausweises - bekannte Ausweise werden per UID
  erkannt.
- **Leserwahl** in den Einstellungen: **NFC** (13.56 MHz) oder **RFID LF**
  (125 kHz). Der Leser erkennt die von der Firmware unterstuetzten Protokolle.
- **Mitarbeiter registrieren** beim ersten Anhalten des leeren Ausweises, mit
  Namen. Jede Person ist an diesen Chip (seine UID) gebunden: jede Stempelung
  verweist auf diesen Chip.
- **Mitarbeiter (Ausweise) verwalten**: umbenennen, **Chip ersetzen** bei Verlust
  (Name und Verlauf bleiben, nur der Chip aendert sich), Verlauf der Person
  ansehen, loeschen (Verlauf bleibt).
- **Automatisches IN/OUT**: Anhalten wechselt selbst (erst IN, dann OUT, dann
  IN...) - keine manuelle Wahl, sofort gebucht.
- **Rueckmeldung beim Stempeln**: getrennter **Ton**, **Vibration** und **LED**
  fuer IN und OUT (aufsteigender Ton + 1 Vibration + gruen fuer IN; absteigender
  Ton + 2 Vibrationen + blau fuer OUT). Jeweils in den Einstellungen abschaltbar
  (standardmaessig an).
- **Verlauf** mit Filtern (alle / heute / Woche, oder je Mitarbeiter aus
  Ausweise), **Heute**-Uebersicht (erstes Kommen, letztes Gehen, Stunden und
  Pausen) und **Woche** (Stunden pro Tag + Wochensumme).
- **Speicherung auf microSD** als CSV, plus **JSON-Export**, **datiertes CSV**,
  ein **Backup** (datierte Kopie von Ausweisen + Stempeln) und
  **Wiederherstellen** (Ausweise + Stempel aus einem Backup laden).
- **Geschuetzter Modus (PIN)**: optionaler Code als **Pfeilfolge** (Hoch / Runter
  / Links / Rechts - schnell) der das Verlassen der App sperrt; beim ersten Start
  angeboten oder spaeter in den Einstellungen.
- **Sprachen**: Englisch, Italienisch, Spanisch, Franzoesisch, Deutsch - in den
  Einstellungen waehlbar (die offizielle Firmware bietet keine Systemsprache).

Siehe die [Roadmap](#roadmap) fuer v1.1 / v2.0.

### Ein Chip pro Person (und verlorener Chip)

Jeder Mitarbeiter wird ueber die **UID** seines Chips erkannt: weise einen Chip
pro Person zu und behalte ihn als Referenz - alle Stempel verweisen auf diesen
Chip. Verliert jemand seinen Chip, oeffne **Ausweise -> (Person) -> Chip
ersetzen** und halte einen neuen leeren Chip an: Name und bisherige Stempel
bleiben, nur der Referenzchip aendert sich.

## Datendateien

Alles wird auf der microSD unter `/ext/apps_data/timeclock/` gespeichert:

| Datei         | Inhalt                                                          |
|---------------|-----------------------------------------------------------------|
| `badges.csv`  | Ausweise: `uid,name,tech,created,last_used,last_event`          |
| `punches.csv` | Verlauf: `date,time,name,uid,type` (`IN`/`OUT`)                 |
| `config.txt`  | Einstellungen + PIN-**Hash** und Salt (nie der PIN im Klartext) |
| `export.json` | JSON-Export des Verlaufs (*Export -> Export JSON*)              |
| `punches-YYYY-MM-DD.csv` | Datierter CSV-Schnappschuss (*Export -> Export CSV*) |
| `backup/`     | Datierte Kopien von Ausweisen + Stempeln (*Export -> Backup*)   |

`punches.csv` ist die interne Stempelliste: jedes Kommen/Gehen jedes Mitarbeiters
nach Tag und Uhrzeit. Oeffnet direkt in Excel, LibreOffice, Google Sheets usw.

Beispiel `punches.csv`:

```csv
date,time,name,uid,type
2026-09-12,08:02,Mario,04A1B2C3D4,IN
2026-09-12,12:31,Mario,04A1B2C3D4,OUT
```

## Bauen und installieren

Eine externe App (FAP) fuer die **offizielle Firmware**. Bauen mit
[`ufbt`](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
python3 -m pip install --upgrade ufbt
```

Aus dem Projektordner (mit `application.fam`):

```bash
ufbt
```

Auf einen verbundenen Flipper installieren und starten:

```bash
ufbt launch
```

Die gebaute `.fap` liegt in `dist/`. Du kannst sie auch per qFlipper nach
`SD Card/apps/Tools/` kopieren und ueber **Apps -> Tools -> Time Clock** starten.

> **Firmware-Hinweis.** Die Funkschicht liegt in `timeclock_reader.c` (NFC ueber
> den ISO14443-3A-Poller - MIFARE Classic/Ultralight, NTAG, DESFire, die leeren
> Ausweise, die du wirklich nutzt - plus der LF-RFID-Worker fuer 125 kHz). Sie ist
> am empfindlichsten gegenueber API-Aenderungen; aendert sich ein Symbol, ist der
> Fix auf diese eine Datei beschraenkt.

## Kompatibilitaet

Time Clock laeuft auf der **offiziellen** Flipper-Zero-Firmware und den beliebten
Custom-Firmwares. Eine FAP wird gegen die API einer bestimmten Firmware gebaut,
daher liefert jede Release **eine `.fap` pro Firmware** - lade die passende:

| Firmware    | Release-Datei               |
|-------------|-----------------------------|
| Offiziell   | `timeclock-official.fap`    |
| Momentum    | `timeclock-momentum.fap`    |
| Unleashed   | `timeclock-unleashed.fap`   |
| RogueMaster | `timeclock-roguemaster.fap` |

RogueMaster basiert auf dem Unleashed-SDK (binaerkompatibel). Fuer nicht
gelistete Firmware baue aus dem Quellcode mit `ufbt` (siehe oben): der Code nutzt
Standard-APIs und ist portabel.

## Geschuetzter Modus und PIN - was er kann und nicht kann

Der PIN ist eine schnelle **Folge aus 4 Pfeilen** (z. B. Hoch, Hoch, Links,
Rechts). Er wird beim ersten Start angeboten oder jederzeit ueber *Einstellungen
-> PIN setzen*. Wenn gesetzt, startet die App gesperrt und **Back verlaesst die
App nicht mehr**; der einzige Software-Weg ist *Einstellungen -> App beenden*
(oder Work mode -> Back), der die Folge abfragt. Gespeichert wird nur ein
**gesalzener Hash**, nie im Klartext.

**Ehrliche Grenzen (bewusst):**

- Keine App kann ein **Hardware**-Ausschalten oder einen Firmware-Force-Quit
  verhindern (z. B. `Links` + `Back` zum Neustart oder Strom trennen). Der
  geschuetzte Modus deckt nur software-steuerbare Aktionen ab.
- Der PIN-Hash (FNV-1a) verhindert Klartext-Speicherung und schuetzt die
  Oberflaeche, ist aber **keine** starke Abwehr gegen jemanden mit physischem
  Zugriff auf die microSD, der kurze Folgen offline durchprobiert.
- Es gibt bewusst **keine versteckte Umgehung**. Loeschen von `config.txt` auf der
  SD setzt Einstellungen (und PIN) zurueck.

## Projektstruktur

```
timeclock/
|-- application.fam            # App-Manifest
|-- timeclock.h / .c           # Lebenszyklus, Entry Point, Helfer
|-- timeclock_storage.h / .c   # microSD-Persistenz + Datenmodell
|-- timeclock_pin.h / .c       # gesalzener PIN-Hash
|-- views/
|   `-- pin_view.h / .c        # eigene PIN-Ansicht
`-- scenes/
    |-- timeclock_scene*.{h,c} # Scene Manager (X-Macro)
    `-- timeclock_scene_*.c    # eine Datei pro Bildschirm
```

## Roadmap

- **v1.1 / v1.2** - Wochenuebersicht, Pausenberechnung und Filter (fertig).
- **v2.0** - Backup und Wiederherstellung (fertig); dann: Bluetooth-Sync, Companion-App, Import.

## Mitwirken

Beitraege sind willkommen - siehe [CONTRIBUTING.md](CONTRIBUTING.md) und den
[Verhaltenskodex](CODE_OF_CONDUCT.md).

## Unterstuetzen

Wenn dir Time Clock nuetzlich ist, kannst du die Entwicklung unterstuetzen:

[![Auf GitHub sponsern](https://img.shields.io/badge/Sponsor-GitHub-ea4aaa?logo=githubsponsors&logoColor=white)](https://github.com/sponsors/vladpereverzyev)
[![Auf Ko-fi unterstuetzen](https://img.shields.io/badge/Ko--fi-Kaffee%20spendieren-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/vladpereverzyev)

## Lizenz

Copyright (C) 2026 Vladyslav Pereverzyev.

Lizenziert unter **GNU General Public License v3.0 oder spaeter** - siehe
[LICENSE](LICENSE). Die Quelldateien tragen den Header
`SPDX-License-Identifier: GPL-3.0-or-later`.
